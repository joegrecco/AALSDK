// ***************************************************************************
//
//        Copyright (C) 2008-2013 Intel Corporation All Rights Reserved.
//
// Engineer:            Narayanan Ravichandran, Pratik Marolia
// Create Date:         Thu Nov 20 11:28:01 PDT 2014
// Module Name:         test_sw1.v
// Project:             mb_2_3 AFU 
// Description:         hw + sw ping pong test. FPGA initializes a location X,
//                      flag the SW. The SW in turn copies the data to location
//                      Y and flag the FPGA. FPGA reads the data from location Y.
// ***************************************************************************
// ---------------------------------------------------------------------------------------------------------------------------------------------------
//                                         SW test 1
// ---------------------------------------------------------------------------------------------------------------------------------------------------
// Goal:
// Characterize 3 methods of notification from CPU to FPGA:
// 1. polling from AFU
// 2. UMsg with Data
// 3. UMsgHint followed by UMsg with Data
// 
//
// Test flow:
// 1. Wait on test_go
// 2. Start of Iteration. Write N cache lines (CL), WrData= {16'h abcd} 
// 3. Wait for N responses.
// 4. FPGA -> CPU Message. Write to address N+1, WrData = Iteration Count [1 to 2047]
// 5. CPU  -> FPGA Message. Configure one of the following methods:
//   a. Poll on Addr N+1. Expected Data = Iteration Count [1 to 2047]
//   b. UMsg Mode 0 (with data). UMsg ID = [0 to 31]
//   c. UMsg Mode 1 (Hint). UMsg ID = [0 to 31]
// 6. Read N cache lines. Wait for all read responses.
// 7. End of Iteration
// 8. Repeat steps 2 to 7 (re2xy_Numrepeat_sw) times. 
// 9. Send Test completion 
// 
// test mode selection:
// re2xy_test_cfg[7:6]  Description
// ----------------------------------------
// 2'h0                 Polling method
// 2'h2                 UMsg Mode 0 (with data)
// 2'h3                 UMsg Mode 1 (Hint)
//
// UMsgs : Mode 0 and Mode 1
// -------------------------		   
// It is possible to have 32 UMsgs configured in one of these 2 modes. ab2s1_RdRsp[5:0] denotes the UMsg ID [0 to 31]
// 2 variants of UMsgs : UMsg (Mode 0) and UMsgH (Mode 1)
// UMsg directly reads value of the Cache Line
// UMsgH serves as an early hint indicating the CL has been modified
// Note that there is always a UMsg (reading the modified value) following an UMsgH. 
//
// UMSg response definition
// ------------------------
// ab2s1_RdRsp
//	[13]			Reserved
//	[12]			0 -- UMsg, 1 -- UMsgH
//  [11:6]			Reserved
//	[5:0]			UMsg ID
//
// Use UMsgHint with caution!
// ---------------------------
// UMsgH has to be used 'only' as an early hint (to denote CL modified).
// Let's assume CPU writes CL 'x'=A that maps to UMsg Address Space(UMAS). 
// At the FPGA CCI interface, this will generate a UMsgH followed by a UMsg with Data=A
// Now let's assume CPU writes CL 'x'=B, again Write CL 'x'=C, within a short period of time.
// At the FPGA CCI interface, this may generated only 1 UMsgH followed by the UMsg with Data=C
// The important thing to note is that UMsg will provide the most recent data, the intermediate UMsgH may get dropped.
//
// In case of microbenchmark, UMsgH for CL 'x'=B could trigger a control action of FPGA writing a flag to memory (which initiates another CPU write to same CL 'x'=C).
// To handle the case where the UMsgH (for CL 'x'=C) gets dropped, the test checks for either UMsgH for CL 'x'=C or UMsg for CL 'x'=C to trigger the next control action.
// Therefore, in cases where the UMsgH of a line gets dropped, the corresponding UMsg will initiate subsequent control action.
//
// Source and Destination Buffers
// ------------------------------
// The source and destination buffers (4MB = 65536 Cache Lines) are organized as follows: 
// There are 32 threads and each thread has 2047 CLs for data and 1 CL for flag in SRC, DST buffer.
//
// Note:
// ----
// The terms INSTANCE and thread are used interchangeably 
//

module test_sw1 #(parameter PEND_THRESH=1, ADDR_LMT=20, MDATA=14, INSTANCE=32)
(
   //---------------------------global signals-------------------------------------------------
   Clk_32UI,                // input -- Core clock
   Resetb,                  // input -- Use SPARINGLY only for control
        
   s12ab_WrAddr,            // output   [ADDR_LMT-1:0]    
   s12ab_WrTID,             // output   [ADDR_LMT-1:0]      
   s12ab_WrDin,             // output   [511:0]             
   s12ab_WrFence,           // output   write fence.
   s12ab_WrEn,              // output   write enable        
   ab2s1_WrSent,            // input	write sent                          
   ab2s1_WrAlmFull,         // input    write buffer almost full                      
   s12ab_Hist_Ctrl,          // output   Control signal to Arbitrar to tell when it should write to Histogram Workspace
  
   s12ab_RdAddr,            // output   [ADDR_LMT-1:0]
   s12ab_RdTID,             // output   [13:0]
   s12ab_RdEn,              // output 
   ab2s1_RdSent,            // input
   
   ab2s1_RdRspValid,        // input                    
   ab2s1_UMsgValid,         // input                    
   ab2s1_RdRsp,             // input    [13:0]          
   ab2s1_RdRspAddr,         // input    [ADDR_LMT-1:0]  
   ab2s1_RdData,            // input    [511:0]         
    
   ab2s1_WrRspValid,        // input                  
   ab2s1_WrRsp,             // input    [13:0]            
   ab2s1_WrRspAddr,         // input    [ADDR_LMT-1:0]    

   re2xy_go,                // input                 
   re2xy_NumLines,          // input    [31:0]              
   re2xy_test_cfg,          // input    [7:0]  
   re2xy_Numrepeat_sw,		// input	[20:0]
    
   s12ab_TestCmp,           // output           
   test_Resetb,             // input
   data_startAddr,			// input	[15:0]		= 2047*INSTANCE
   flag_Addr				// input	[15:0]		= (2047*32) + INSTANCE
);

   input                      Clk_32UI;               // csi_top:    Clk_32UI
   input                      Resetb;                 // csi_top:    system Resetb
   
   output   [ADDR_LMT-1:0]    s12ab_WrAddr;           // arb:        write address
   output   [13:0]            s12ab_WrTID;            // arb:        meta data
   output   [511:0]           s12ab_WrDin;            // arb:        Cache line data
   output                     s12ab_WrFence;          // arb:        write fence
   output                     s12ab_WrEn;             // arb:        write enable.
   input                      ab2s1_WrSent;           // arb:        write issued
   input                      ab2s1_WrAlmFull;        // arb:        write fifo almost full
   
   output   [ADDR_LMT-1:0]    s12ab_RdAddr;           // arb:        Reads may yield to writes
   output   [13:0]            s12ab_RdTID;            // arb:        meta data
   output                     s12ab_RdEn;             // arb:        read enable
   input                      ab2s1_RdSent;           // arb:        read issued
   
   input                      ab2s1_RdRspValid;       // arb:        read response valid
   input                      ab2s1_UMsgValid;        // arb:        UMsg valid
   input    [13:0]            ab2s1_RdRsp;            // arb:        read response header
   input    [ADDR_LMT-1:0]    ab2s1_RdRspAddr;        // arb:        read response address
   input    [511:0]           ab2s1_RdData;           // arb:        read data
   
   input                      ab2s1_WrRspValid;       // arb:        write response valid
   input    [13:0]            ab2s1_WrRsp;            // arb:        write response header
   input    [ADDR_LMT-1:0]    ab2s1_WrRspAddr;        // arb:        write response address

   input                      re2xy_go;               // requestor:  start of frame recvd
   input    [31:0]            re2xy_NumLines;         // requestor:  number of cache lines
   input    [7:0]             re2xy_test_cfg;         // requestor:  8-bit test cfg register.
   input	[20:0]			  re2xy_Numrepeat_sw;	  // requestor:  11 bits	

   output                     s12ab_TestCmp;          // arb:        Test completion flag
   input                      test_Resetb;
   input	[15:0]			  data_startAddr;		  // input		 Starting Address of buffer = 2047*INSTANCE
   input	[15:0]			  flag_Addr;			  // input	     (2047*32) + INSTANCE 
  
   output   				  s12ab_Hist_Ctrl;         // arb:        Control signal to Arb to tell when it should write to Histogram Workspace
   //------------------------------------------------------------------------------------------------------------------------
   
   // Rd FSM states
   // Histogram Compute
   localparam Vrdfsm_WAIT 			= 2'h0;
   localparam Vrdfsm_RESP 			= 2'h1;
   localparam Vrdfsm_READ 			= 2'h2;
   localparam Vrdfsm_DONE 			= 2'h3;
   
   // Wr FSM states
   localparam Vwrfsm_WAIT 			= 3'h0;
   localparam Vwrfsm_WRITE 			= 3'h1;
   localparam Vwrfsm_WRFENCE 		= 3'h2;
   localparam Vwrfsm_UPDTFLAG 		= 3'h3;
   localparam Vwrfsm_DONE 			= 3'h4;
   localparam Vwrfsm_HIST_COMPUTE	= 3'h5;
   localparam Vwrfsm_HIST_DECODE 	= 3'h6;

   // Rd Poll FSM states
   localparam Vpollfsm_WAIT   		= 2'h0;
   localparam Vpollfsm_READ 		= 2'h1;
   localparam Vpollfsm_RESP 		= 2'h2;
   localparam Vpollfsm_DONE 		= 2'h3;

   reg      [ADDR_LMT-1:0]    s12ab_WrAddr;           // arb:        Writes are guaranteed to be accepted
   wire     [13:0]            s12ab_WrTID;            // arb:        meta data
   reg      [511:0]           s12ab_WrDin;            // arb:        Cache line data
   reg                        s12ab_WrEn;             // arb:        write enable
   reg                        s12ab_WrFence;          // arb:        write fence
   reg      [ADDR_LMT-1:0]    s12ab_RdAddr;           // arb:        Reads may yield to writes
   wire     [13:0]            s12ab_RdTID;            // arb:        meta data
   reg                        s12ab_RdEn;             // arb:        read enable
   reg                        s12ab_TestCmp_c1;       // arb:        Test completion flag
   reg                        s12ab_TestCmp_c2;       // arb:        Test completion flag
   reg                        s12ab_TestCmp;          // arb:        Test completion flag
   reg 						  s12ab_Hist_Ctrl;		  // arb:        Control signal to Arbitrar to tell when it should write to Histogram workspace						  
   // Local Variables   
   reg      [11:0]            Num_RdReqs;				// No. of Reads issued
   reg      [11:0]            Num_RdRsp;				// No. of Read Responses
   reg      [11:0]            Num_WrRsp;				// No. of Write Responses
   reg      [11:0]            Num_WrReqs;				// No. of Writes issued
   
   reg      [1:0]             RdFSM;
   reg      [1:0]             PollFSM;   
   reg      [2:0]             WrFSM;
   reg                        rd_go;
            
   reg      [11:0]    		  read_count; 				
   reg      [11:0]    		  wr_count; 				
   reg      [21:0]			  count_sw;  				// To track current Iteration 
   reg      [21:0]			  count_sw_reg;   

   wire     [MDATA-4:0]       Wrmdata = s12ab_WrAddr[MDATA-4:0];
   wire     [MDATA-4:0]       Rdmdata = s12ab_RdAddr[MDATA-4:0];
   // Added for Histogram
   reg     [20:0] 			  lockid;
   reg   					  lockvalid;
   reg     [ADDR_LMT-1:0]	  lock_RdAddr;
   reg 	   [511:0]			  lock_RdData;
   reg 						  hist_compute_go;
   reg 						  hist_compute_done;
   reg 						  poll_switch;
   reg						  set_wren;
   reg 						  set_ren;
   
   
   assign s12ab_RdTID = {(6'b000000 + INSTANCE), Rdmdata}; 			// Bits [13:8] = {1'b0, 5'bINSTANCE} 
   assign s12ab_WrTID = {(6'b000000 + INSTANCE), Wrmdata};          // INSTANCE ranges between [0 and 31]
      
   always @(*)
   begin
    s12ab_TestCmp_c1 = (RdFSM==Vrdfsm_DONE) && (count_sw[20:0] == re2xy_Numrepeat_sw [20:0]);
    s12ab_RdEn       =  RdFSM == Vrdfsm_READ  || PollFSM == Vpollfsm_READ;
    s12ab_WrEn       =  
	//WrFSM == Vwrfsm_WRITE || WrFSM == Vwrfsm_UPDTFLAG ||WrFSM== Vwrfsm_HIST_COMPUTE;
	set_wren;
    s12ab_WrFence    =  1'b0;
   end

   // Write FSM   
   always @(posedge Clk_32UI)
   begin
		 set_wren <= 0;
         s12ab_TestCmp     <= s12ab_TestCmp_c2;
         s12ab_TestCmp_c2  <= s12ab_TestCmp_c1;
         case(WrFSM)       				/* synthesis parallel_case */
            Vwrfsm_WAIT:            	// Wait for CPU to start the test
            begin
               Num_WrReqs     <= 0;
			   Num_WrRsp      <= 0;
			   wr_count		  <= 1'b1;
               if(re2xy_go)
					if(hist_compute_go)
						begin
							//hist_compute_done	<= 0;
							WrFSM 				<= Vwrfsm_HIST_DECODE;
							s12ab_WrAddr		<= (lockid>>3) + data_startAddr;
						end
					else
					begin
							WrFSM   		<= Vwrfsm_UPDTFLAG;
							s12ab_WrDin		<= {count_sw[20:0]+1};
							s12ab_WrAddr    <= flag_Addr;
							set_wren <= 1;
					end
					
			   end

            Vwrfsm_WRITE:           // Move data from FPGA to CPU
            begin
              if(ab2s1_WrSent)
              begin
                  s12ab_WrAddr        <= wr_count + data_startAddr;
				  wr_count			  <= wr_count + 1'b1;
                  Num_WrReqs          <= Num_WrReqs + 1'b1;
                          
                  if(Num_WrReqs >= (re2xy_NumLines[10:0]-1'b1))
                  begin
                      WrFSM     <= Vwrfsm_WRFENCE;
                  end
              end
            end

            Vwrfsm_WRFENCE:         // Fence - guarantees data is written
            begin
              if(Num_WrRsp==re2xy_NumLines[10:0])
              begin
                    WrFSM       	<= Vwrfsm_UPDTFLAG;
                    s12ab_WrDin 	<= {count_sw[20:0]+1};	// Write Flag [1 to 2047]
					s12ab_WrAddr	<= flag_Addr;
					set_wren <= 1;
              end
            end
           
            Vwrfsm_UPDTFLAG:        // FPGA -> CPU Message saying data is available
            begin
              if(ab2s1_WrSent)
              begin
                      WrFSM        	<= Vwrfsm_DONE;
					  poll_switch	<= 1;
			  end
			  else
					set_wren <= 1;
					  //update_flag  <= 0;
              
            end
			Vwrfsm_HIST_DECODE:  // Do Histogram Compute
			begin
						//if(ab2s1_RdRspValid)//hist_compute_go           			<= 0;
						//begin
						/*	s12ab_WrDin	<= lock_RdData;
							case(lockid[2:0])
								3'h0:	s12ab_WrDin[0:+64] <= lock_RdData[0:+64] + 1;
								3'h1:	s12ab_WrDin[64:+64] <= lock_RdData[64:+64] + 1;
								3'h2:	s12ab_WrDin[128:+64] <= lock_RdData[128:+64] + 1;
							endcase
						*/	
							if(lockid[2:0] == 3'h0)
							begin
								s12ab_WrDin[63:0] 			<= 	lock_RdData[63:0]			+1;
								s12ab_WrDin[511:64] 		<= 	lock_RdData[511:64];
							end
							if(lockid[2:0] == 3'h1)
							begin
								s12ab_WrDin[127:64] 	<=	lock_RdData[127:64]	+1;
								s12ab_WrDin[511:128] 	<=  lock_RdData[511:128];
								s12ab_WrDin[63:0] 		<=  lock_RdData[63:0];
								
							end
							if(lockid[2:0] == 3'h2)
							begin
								s12ab_WrDin[127:0] 		<=  lock_RdData[127:0];
								s12ab_WrDin[191:128] 	<=	lock_RdData[191:128]	+1;
								s12ab_WrDin[511:192] 	<=  lock_RdData[511:192];
								
							end
							if(lockid[2:0] == 3'h3)
							begin
								s12ab_WrDin[191:0]      <=  lock_RdData[191:0];
								s12ab_WrDin[255:192] 	<=	lock_RdData[255:192]	+1;
								s12ab_WrDin[511:256]    <=  lock_RdData[511:256];
								
							end
							if(lockid[2:0] == 3'h4)
							begin
								s12ab_WrDin[255:0] 		<=  lock_RdData[255:0];
								s12ab_WrDin[319:256] 	<=	lock_RdData[319:256]	+1;
								s12ab_WrDin[511:320] 	<=  lock_RdData[511:320];
								
							end
							if(lockid[2:0] == 3'h5)
							begin
								s12ab_WrDin[319:0] 		<=  lock_RdData[319:0];
								s12ab_WrDin[383:320] 	<=	lock_RdData[383:320] 	+1;
								s12ab_WrDin[511:384] 	<=  lock_RdData[511:384];
								
							end
							if(lockid[2:0] == 3'h6)
							begin
								s12ab_WrDin[383:0]		<=  lock_RdData[383:0];
								s12ab_WrDin[447:384] 	<=	lock_RdData[447:384]	+1;
								s12ab_WrDin[511:448] 	<=  lock_RdData[511:448];
								
							end
							if(lockid[2:0]==3'h7)
							begin
								s12ab_WrDin[447:0]		<=  lock_RdData[447:0];
								s12ab_WrDin[511:448] 	<=	lock_RdData[511:448]	+1;
								
							end
							WrFSM			<= Vwrfsm_HIST_COMPUTE;
							set_wren		<= 1;
						//end
                   /* if(ab2s1_WrSent)
						begin
						WrFSM 				<= Vwrfsm_DONE;
						hist_compute_done	<= 1;
						poll_switch 		<= 0;
						end
						*/
			
			end
			Vwrfsm_HIST_COMPUTE:
			if(ab2s1_WrSent)
						begin
						WrFSM 				<= Vwrfsm_DONE;
						hist_compute_done	<= 1;
						poll_switch 		<= 0;
						end
			else
						set_wren <= 1;
			
			Vwrfsm_DONE:
			begin
					if((RdFSM == Vrdfsm_DONE && (count_sw[20:0] != re2xy_Numrepeat_sw [20:0])) || hist_compute_go ==1)  
					begin
						WrFSM   			<= Vwrfsm_WAIT;
						hist_compute_done	<= 0;
					end
					
					
			end			
            
            default:
            begin
                WrFSM     <= WrFSM;
            end
         endcase
        
		 if(ab2s1_WrRspValid)
           Num_WrRsp        <= Num_WrRsp + 1'b1;


     if (!test_Resetb)
     begin
         WrFSM          		<= Vwrfsm_WAIT;
         s12ab_TestCmp  		<= 0;
		 //update_flag 			<= 1;
		 hist_compute_done 		<= 0;
		 poll_switch 			<= 0;
		 //s12ab_Hist_Ctrl		<= 0;
		 //lock_RdData 			<=0;
     end
   end

// Read FSM   
   always @(posedge Clk_32UI)
   begin
       set_ren 		<= 0;
       case(re2xy_test_cfg[7:6])
           2'h0:            // polling method
           begin
               case(PollFSM)
                   Vpollfsm_WAIT:
                   begin												
                        s12ab_RdAddr <= flag_Addr;
                        if(WrFSM==Vwrfsm_DONE && poll_switch==1)
                            begin
							PollFSM <= Vpollfsm_READ;
							set_ren <= 1;
							end
                   end
				   
                   Vpollfsm_READ:
                   begin
                        if(ab2s1_RdSent)
                           PollFSM <= Vpollfsm_RESP;
                   end
				   
                   Vpollfsm_RESP:
                   begin
                        if(ab2s1_RdRspValid)
                        begin
                            if(ab2s1_RdData[23:3]==(count_sw[20:0]+1'b1))
								begin
									rd_go 		<= 1;
									lockid 		<= ab2s1_RdData[63:35];
									lockvalid	<= ab2s1_RdData[0];
									PollFSM <= Vpollfsm_DONE;
								end
						else 
								begin
									PollFSM 	<= Vpollfsm_READ;
									rd_go 		<= 0;
									set_ren 	<= 1;
							end
						end
				   end
				   
                   default: //Vpollfsm_DONE
                   begin
                       PollFSM <= PollFSM;
                   end
               endcase
           end       
		   
           2'h2:      
		   // UMsg Mode 0 (with data)
				//rd_go 			<= ab2s1_UMsgValid && ab2s1_RdRsp[12]==1'b0 && ab2s1_RdRsp[5:0]==INSTANCE && (ab2s1_RdData[20:0]==(count_sw[20:0]+1'b1));	// UMsg ID [0 to 31]
				begin
					if(ab2s1_UMsgValid && ab2s1_RdRsp[12]==1'b0 && ab2s1_RdRsp[5:0]==INSTANCE && ab2s1_RdData[23:3]==count_sw[20:0]+1'b1)
						begin
							rd_go    	<= 1;
							lockid 		<= ab2s1_RdData[63:35];
							lockvalid	<= ab2s1_RdData[0];
							//PollFSM 	<= Vpollfsm_DONE;
						end
						
				end
           2'h3:			// UMsg Mode 1 (Hint followed by data)
			   begin
				   if ((ab2s1_UMsgValid && ab2s1_RdRsp[12]==1'b1 && ab2s1_RdRsp[5:0]==INSTANCE) 
				    || (ab2s1_UMsgValid && ab2s1_RdRsp[12]==1'b0 && ab2s1_RdRsp[5:0]==INSTANCE && (ab2s1_RdData[20:0]==(count_sw_reg[20:0]+2'b10))))
				   begin	  
						count_sw_reg <= count_sw;
						rd_go 		 <= 1; 
						lockid 		<= ab2s1_RdData[63:35];
						lockvalid	<= ab2s1_RdData[0];
				   end
				   
				   else
				   begin	  
						count_sw_reg <= count_sw_reg;
						rd_go  		 <= 0; 
				   end 
			   end
       endcase
	   //set_ren 		<= 0;
       case(RdFSM)       /* synthesis parallel_case */
            
			Vrdfsm_WAIT:                            // Read Data payload
            begin
                
				Num_RdReqs   <= 0;
                Num_RdRsp    <= 0;
                if(rd_go)
                begin
					read_count <=1'b1;
					if(lockvalid !=0)
						begin
							RdFSM 		<= Vrdfsm_READ;
							s12ab_RdAddr <= (lockid>>3) +data_startAddr;
							set_ren 	<= 1;
						end
					else
						begin
							count_sw <= count_sw + 1; 
							RdFSM <= Vrdfsm_DONE;
						end
				end
            end
			// to do: 1) Change the read N cache lines to read the memory location pointed by 
			//			 lock id.
			//		  2) Whatever data is returned, need to increament it by 1.
            Vrdfsm_READ:                             // Read N cache lines
            begin
                if(ab2s1_RdSent)
					begin
						RdFSM     		<= Vrdfsm_RESP;
						s12ab_Hist_Ctrl <= 1;
						//hist_compute_go <= 1;
						
					end
				end
			// to do: 1) Change it do histogram compute
			//        2) Write the data 
            Vrdfsm_RESP:                            // Wait untill all reads complete
            begin
                if(ab2s1_RdRspValid)
					begin
						count_sw 		<= count_sw + 1; 
						hist_compute_go <= 1;
						lock_RdData 	<= ab2s1_RdData;
					end
				if(hist_compute_done)
						begin
							RdFSM    			<= Vrdfsm_DONE;
							hist_compute_go             <= 0;
						end
			end
            
			Vrdfsm_DONE:
			begin
				
				if(count_sw[20:0] != re2xy_Numrepeat_sw [20:0])  
					begin
						RdFSM   			<= Vrdfsm_WAIT;
						PollFSM 			<= Vpollfsm_WAIT;
						rd_go           	<= 0;
						s12ab_RdAddr    	<= 0;
						Num_RdRsp    		<= 0;
						s12ab_Hist_Ctrl 	<= 0;
						
					end
			end
			default:
            begin
              RdFSM     <= RdFSM;
            end
       endcase         


         if(ab2s1_RdRspValid)
           Num_RdRsp        <= Num_RdRsp + 1'b1;
           
       if (!test_Resetb)
       begin
		 count_sw			<= 0;
		 count_sw_reg		<= 0;
         s12ab_RdAddr   	<= 0;
         RdFSM          	<= Vrdfsm_WAIT;          
         PollFSM        	<= Vpollfsm_WAIT;
         rd_go          	<= 0;
		 hist_compute_go 	<= 0;
		 s12ab_Hist_Ctrl	<= 0;
		 lock_RdData 		<= 0;
		 end
       
   end

   
endmodule
