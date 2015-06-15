// Copyright (c) 2014-2015, Intel Corporation
//
// Redistribution  and  use  in source  and  binary  forms,  with  or  without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of  source code  must retain the  above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name  of Intel Corporation  nor the names of its contributors
//   may be used to  endorse or promote  products derived  from this  software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,  BUT NOT LIMITED TO,  THE
// IMPLIED WARRANTIES OF  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT  SHALL THE COPYRIGHT OWNER  OR CONTRIBUTORS BE
// LIABLE  FOR  ANY  DIRECT,  INDIRECT,  INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR
// CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT LIMITED  TO,  PROCUREMENT  OF
// SUBSTITUTE GOODS OR SERVICES;  LOSS OF USE,  DATA, OR PROFITS;  OR BUSINESS
// INTERRUPTION)  HOWEVER CAUSED  AND ON ANY THEORY  OF LIABILITY,  WHETHER IN
// CONTRACT,  STRICT LIABILITY,  OR TORT  (INCLUDING NEGLIGENCE  OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,  EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
// **************************************************************************
/*
 * Module Info: Generic protocol backend for keeping IPCs alive,
 * interfacing with DPI-C, messages and SW application
 *
 * Language   : C/C++
 * Owner      : Rahul R Sharma
 *              rahul.r.sharma@intel.com
 *              Intel Corporation
 *
 */

#include "ase_common.h"

// ---------------------------------------------------------------
// Message queues descriptors
// ---------------------------------------------------------------
int app2sim_rx;           // app2sim mesaage queue in RX mode
int sim2app_tx;           // sim2app mesaage queue in TX mode
int app2sim_csr_wr_rx;    // CSR Write listener MQ in RX mode
int app2sim_umsg_rx;      // UMSG    message queue in RX mode
int app2sim_simkill_rx;   // app2sim message queue in RX mode
int sim2app_intr_tx;      // sim2app message queue in TX mode

// Global test complete counter
// Keeps tabs of how many session_deinits were received
int glbl_test_cmplt_cnt;

/*
 * Generate scope data
 */
svScope scope;
void scope_function()
{
  scope = svGetScope();
}


/*
 * DPI: WriteLine Data exchange
 */
void wr_memline_dex(cci_pkt *pkt, int *cl_addr, int *mdata, char *wr_data )
{
  FUNC_CALL_ENTRY;
  uint64_t* wr_target_vaddr = (uint64_t*)NULL;
  uint64_t fake_wr_addr = 0;
  int i;

  // Generate fake byte address
  fake_wr_addr = (uint64_t)(*cl_addr) << 6;
  // Decode virtual address
  wr_target_vaddr = ase_fakeaddr_to_vaddr((uint64_t)fake_wr_addr);

  // Mem-copy from TX1 packet to system memory
  memcpy(wr_target_vaddr, wr_data, CL_BYTE_WIDTH);

  //////////// Write this to RX-path //////////////
  // Zero out data buffer
  for(i = 0; i < 8; i++)
    pkt->qword[i] = 0x0;

  // Loop around metadata
  pkt->meta = (ASE_RX0_WR_RESP << 14) | (*mdata);

  // Valid signals
  pkt->cfgvalid = 0;
  pkt->wrvalid  = 1;
  pkt->rdvalid  = 0;
  pkt->intrvalid = 0;
  pkt->umsgvalid = 0;

  // ase_write_cnt++;

  FUNC_CALL_EXIT;
}


/*
 * DPI: ReadLine Data exchange
 */
void rd_memline_dex(cci_pkt *pkt, int *cl_addr, int *mdata )
{
  FUNC_CALL_ENTRY;

  uint64_t fake_rd_addr = 0;
  uint64_t* rd_target_vaddr = (uint64_t*) NULL;

  // Fake CL address to fake address conversion
  fake_rd_addr = (uint64_t)(*cl_addr) << 6;

  // Calculate Virtualized SHIM address (translation table)
  rd_target_vaddr = ase_fakeaddr_to_vaddr((uint64_t)fake_rd_addr);

  // Copy data to memory
  memcpy(pkt->qword, rd_target_vaddr, CL_BYTE_WIDTH);

  // Loop around metadata
  pkt->meta = (ASE_RX0_RD_RESP << 14) | (*mdata);

  // Valid signals
  pkt->cfgvalid = 0;
  pkt->wrvalid  = 0;
  pkt->rdvalid  = 1;
  pkt->intrvalid = 0;
  pkt->umsgvalid = 0;

  // ase_read_cnt++;

  FUNC_CALL_EXIT;
}


// -----------------------------------------------------------------------
// vbase/pbase exchange THREAD
// when an allocate request is received, the buffer is copied into a
// linked list. The reply consists of the pbase, fakeaddr and fd_ase.
// When a deallocate message is received, the buffer is invalidated.
// -----------------------------------------------------------------------
int ase_listener()
{
  //   FUNC_CALL_ENTRY;

   /*
    * Buffer Replicator
    */
  // DPI buffer
  struct buffer_t ase_buffer;

  // Prepare an empty buffer
  ase_empty_buffer(&ase_buffer);
  // Receive a DPI message and get information from replicated buffer
  if (ase_recv_msg(&ase_buffer)==ASE_MSG_PRESENT)
    {
      // ALLOC request received
      if(ase_buffer.metadata == HDR_MEM_ALLOC_REQ)
	{
	  ase_alloc_action(&ase_buffer);
	  ase_buffer.is_privmem = 0;
	  if (ase_buffer.index == 0)
	    ase_buffer.is_csrmap = 1;
	  else
	    ase_buffer.is_csrmap = 0;
	}
      // if DEALLOC request is received
      else if(ase_buffer.metadata == HDR_MEM_DEALLOC_REQ)
	{
	  ase_dealloc_action(&ase_buffer);
	}
      
      // Standard oneline message ---> Hides internal info
      ase_buffer_oneline(&ase_buffer);
      
      // Write buffer information to file
      if ( (ase_buffer.is_csrmap == 0) || (ase_buffer.is_privmem == 0) )
	{
	  // Write Workspace info to workspace log file
	  fprintf(fp_workspace_log, "Workspace %d =>\n", ase_buffer.index);
	  fprintf(fp_workspace_log, "             Host App Virtual Addr  = %p\n", (uint64_t*)ase_buffer.vbase);
	  fprintf(fp_workspace_log, "             HW Physical Addr       = %p\n", (uint64_t*)ase_buffer.fake_paddr);
	  fprintf(fp_workspace_log, "             HW CacheAligned Addr   = %p\n", (uint32_t*)(ase_buffer.fake_paddr >> 6));
	  fprintf(fp_workspace_log, "             Workspace Size (bytes) = %d\n", ase_buffer.memsize);
	  fprintf(fp_workspace_log, "\n");
	  
	  // Flush info to file
	  fflush(fp_workspace_log);
	}
      
      // Debug only
    #ifdef ASE_DEBUG
      ase_buffer_info(&ase_buffer);
    #endif
    }


  /*
   * CSR Write listener
   */
  // Message string
  char csr_wr_str[ASE_MQ_MSGSIZE];
  char *pch;
  char ase_msg_data[CL_BYTE_WIDTH];
  uint32_t csr_offset;
  uint32_t csr_data;

  // Cleanse receptacle string
  memset(ase_msg_data, '\0', sizeof(ase_msg_data));

  // Receive csr_write packet
  if(mqueue_recv(app2sim_csr_wr_rx, (char*)csr_wr_str)==ASE_MSG_PRESENT)
    {
      // Tokenize message to get CSR offset and data
      pch = strtok(csr_wr_str, " ");
      csr_offset = atoi(pch);
      pch = strtok(NULL, " ");
      csr_data = atoi(pch);

      // CSRWrite Dispatch
      csr_write_dispatch ( 0, csr_offset, csr_data );

      // *FIXME*: Synchronizer must go here... TEST CODE
      ase_memory_barrier();
    }


  /*
   * UMSG listener
   */
  // Message string
  char umsg_str[SIZEOF_UMSG_PACK_T];
  /* int ii; */
  umsg_pack_t inst;

  // Cleanse receptacle string
  /* umsg_data = malloc(CL_BYTE_WIDTH); */
  memset (umsg_str, '\0', SIZEOF_UMSG_PACK_T );
  /* memset (umsg_data, '\0', CL_BYTE_WIDTH ); */
  
  if (mqueue_recv(app2sim_umsg_rx, (char*)umsg_str ) == ASE_MSG_PRESENT)
    {
/* #ifdef ASE_DEBUG */
/*       printf("ASERxMsg => UMSG Received \n");       */
/* #endif */
      // Tokenize messgae to get msg_id & umsg_data
      // sscanf (umsg_str, "%d %d %s", &umsg_id, &umsg_hint, umsg_data );
      memcpy(&inst, umsg_str, SIZEOF_UMSG_PACK_T);
      
/* #ifdef ASE_DEBUG */
/*       printf("SIM-C : [ASE_DEBUG] Ready for UMSG dispatch %d %d \n", inst.id, inst.hint); */
/*       for(ii = 0 ; ii < 64; ii++) */
/* 	printf("%02X", (int)inst.data[ii]); */
/*       printf("\n"); */
/* #endif */
      
      // UMSG dispatch
      umsg_dispatch(0, 1, inst.hint, inst.id, inst.data);
    }


  /*
   * SIMKILL message handler
   */
  char ase_simkill_str[ASE_MQ_MSGSIZE];
  memset (ase_simkill_str, '\0', ASE_MQ_MSGSIZE);
  if(mqueue_recv(app2sim_simkill_rx, (char*)ase_simkill_str)==ASE_MSG_PRESENT)
    {
      // if (memcmp (ase_simkill_str, (char*)ASE_SIMKILL_MSG, ASE_MQ_MSGSIZE) == 0)
      // Update regression counter
      glbl_test_cmplt_cnt = glbl_test_cmplt_cnt + 1;

      // If in regression mode or SW-simkill mode
      if (  (cfg->ase_mode == ASE_MODE_DAEMON_SW_SIMKILL) ||
	   ((cfg->ase_mode == ASE_MODE_REGRESSION) && (cfg->ase_num_tests == glbl_test_cmplt_cnt))
	   )
	{
	  printf("\n");
	  printf("SIM-C : ASE Session Deinitialization was detected... Simulator will EXIT\n");
	  run_clocks (500);
	  ase_perror_teardown();
	  start_simkill_countdown();
	}
    }

  //  FUNC_CALL_EXIT;
  return 0;
}


/*
 * Calculate Sysmem & CAPCM ranges to be used by ASE
 */
void calc_phys_memory_ranges()
{
  uint32_t cipuctl_22;
  uint32_t cipuctl_21_19;

  cipuctl_22    = (cfg->memmap_sad_setting & 0xF) >> 3;
  cipuctl_21_19 = (cfg->memmap_sad_setting & 0x7);

#ifdef ASE_DEBUG
  printf("        CIPUCTL[22] = %d | CIPUCTL[21:19] = %d\n", cipuctl_22, cipuctl_21_19 );
#endif

  // Memmory map calculation
  if (cfg->enable_capcm)
    {
      capcm_size = (uint64_t)( pow(2, cipuctl_21_19 + 1) * 1024 * 1024 * 1024);
      sysmem_size = (uint64_t)( (uint64_t)pow(2, FPGA_ADDR_WIDTH) - capcm_size);

      // Place CAPCM based on CIPUCTL[22]
      if (cipuctl_22 == 0)
	{
	  capcm_phys_lo = 0;
	  capcm_phys_hi = capcm_size - 1;
	  sysmem_phys_lo = capcm_size;
	  sysmem_phys_hi = (uint64_t)pow(2, FPGA_ADDR_WIDTH) - 1;
	}
      else
	{
	  capcm_phys_hi = (uint64_t)pow(2,FPGA_ADDR_WIDTH) - 1;
	  capcm_phys_lo = capcm_phys_hi + 1 - capcm_size;
	  sysmem_phys_lo = 0;
	  sysmem_phys_hi = sysmem_phys_lo + sysmem_size;
	}
    }
  else
    {
      sysmem_size = (uint64_t)pow(2, FPGA_ADDR_WIDTH);
      sysmem_phys_lo = 0;
      sysmem_phys_hi = sysmem_size-1;
      capcm_size = 0;
      capcm_phys_lo = 0;
      capcm_phys_hi = 0;
    }

  BEGIN_YELLOW_FONTCOLOR;
  printf("        System memory range  => 0x%016lx-0x%016lx | %ld~%ld GB \n",
	 sysmem_phys_lo, sysmem_phys_hi, sysmem_phys_lo/(uint64_t)pow(1024, 3), (uint64_t)(sysmem_phys_hi+1)/(uint64_t)pow(1024, 3) );
  if (cfg->enable_capcm)
    printf("        Private memory range => 0x%016lx-0x%016lx | %ld~%ld GB\n",
	   capcm_phys_lo, capcm_phys_hi, capcm_phys_lo/(uint64_t)pow(1024, 3), (uint64_t)(capcm_phys_hi+1)/(uint64_t)pow(1024, 3) );
  END_YELLOW_FONTCOLOR;

  // Internal check messages
  if (cfg->enable_capcm)
    {
      if (capcm_size > (uint64_t)8*1024*1024*1024 )
	{
	  BEGIN_RED_FONTCOLOR;
	  printf("SIM-C : WARNING =>\n");
	  printf("        Caching agent private memory size > 8 GB, this can cause a virtual memory hog\n");
	  printf("        Consider using a smaller memory for simulation !! \n");
	  printf("        Simulation will continue with requested setting, change to a smaller CAPCM in ase.cfg !!\n");
	  END_RED_FONTCOLOR;
	}
      if (sysmem_size == 0)
	{
	  BEGIN_RED_FONTCOLOR;
	  printf("SIM-C : WARNING =>\n");
	  printf("        System SAD setting has set System Memory size to 0 bytes. Please check that this is intended !\n");
	  printf("        Any SW Workspace Allocate action will FAIL !!\n");
	  printf("        Simulation will continue with requested setting...\n");
	  END_RED_FONTCOLOR;
	}
      if (capcm_size == 0)
	{
	  BEGIN_RED_FONTCOLOR;
	  printf("SIM-C : WARNING =>\n");
	  printf("        CAPCM is enabled and has size set to 0 bytes. Please check that this is intended !!\n");
	  printf("        Simulation will continue, but NO CAPCM regions will be created");
	  END_RED_FONTCOLOR;
	}
    }
}


// -----------------------------------------------------------------------
// DPI Initialize routine
// - Setup message queues
// - Start buffer replicator, csr_write listener thread
// -----------------------------------------------------------------------
int ase_init()
{
  FUNC_CALL_ENTRY;

  // Register SIGINT and listen to it
  signal(SIGTERM, start_simkill_countdown);
  signal(SIGINT , start_simkill_countdown);
  signal(SIGQUIT, start_simkill_countdown);
  signal(SIGKILL, start_simkill_countdown); // *FIXME*: This possibly doesnt work //
  signal(SIGHUP,  start_simkill_countdown);

  // Get PID
  ase_pid = getpid();
  printf("SIM-C : PID of simulator is %d\n", ase_pid);

  // Evaluate PWD
  ase_run_path = malloc(ASE_FILEPATH_LEN);
  ase_run_path = getenv("PWD");

  // ASE configuration management
  ase_config_parse(ASE_CONFIG_FILE);

  // Evaluate Session directory
  ase_workdir_path = malloc(ASE_FILEPATH_LEN);
  /* ase_workdir_path = ase_eval_session_directory();   */
  sprintf(ase_workdir_path, "%s/work/", ase_run_path);
  printf("SIM-C : ASE Session Directory located at =>\n");
  printf("        %s\n", ase_workdir_path);
  printf("SIM-C : ASE Run path =>\n");
  printf("        %s\n", ase_run_path);

  // Evaluate IPCs
  ipc_init();

  // Generate timstamp (used as session ID)
  put_timestamp();
  tstamp_filepath = malloc(ASE_FILEPATH_LEN);
  strcpy(tstamp_filepath, ase_workdir_path);
  strcat(tstamp_filepath, TSTAMP_FILENAME);

  // Print timestamp
  printf("SIM-C : Session ID => %s\n", get_timestamp(0) );

  // Create IPC cleanup setup
  create_ipc_listfile();

  // Set up message queues
  printf("SIM-C : Creating Messaging IPCs...\n");
  int ipc_iter;
  for( ipc_iter = 0; ipc_iter < ASE_MQ_INSTANCES; ipc_iter++)
    mqueue_create( mq_array[ipc_iter].name );
  // ase_mqueue_setup();

  // Open message queues
  app2sim_rx         = mqueue_open(mq_array[0].name,  mq_array[0].perm_flag);
  app2sim_csr_wr_rx  = mqueue_open(mq_array[1].name,  mq_array[1].perm_flag);
  app2sim_umsg_rx    = mqueue_open(mq_array[2].name,  mq_array[2].perm_flag);
  app2sim_simkill_rx = mqueue_open(mq_array[3].name,  mq_array[3].perm_flag);
  sim2app_tx         = mqueue_open(mq_array[4].name,  mq_array[4].perm_flag);

  // Calculate memory map regions
  printf("SIM-C : Calculating memory map...\n");
  calc_phys_memory_ranges();

  // Random number for csr_pinned_addr
  if (cfg->enable_reuse_seed)
    {
      ase_addr_seed = ase_read_seed ();
     }
  else
    {
      ase_addr_seed = time(NULL);
      ase_write_seed ( ase_addr_seed );
    }
  srand ( ase_addr_seed );

  // Open Buffer info log
  fp_workspace_log = fopen("workspace_info.log", "wb");
  if (fp_workspace_log == NULL) 
    {
      ase_error_report("fopen", errno, ASE_OS_FOPEN_ERR);
    }
  else
    {
      printf("SIM-C : Information about opened workspaces => workspace_info.log \n");
    }

  fflush(stdout);

  FUNC_CALL_EXIT;
  return 0;
}


// -----------------------------------------------------------------------
// ASE ready indicator:  Print a message that ASE is ready to go.
// Controls run-modes
// -----------------------------------------------------------------------
int ase_ready()
{
  FUNC_CALL_ENTRY;

  // Set test_cnt to 0
  glbl_test_cmplt_cnt = 0;

  // Indicate readiness with .ase_ready file
  ase_ready_filepath = malloc (ASE_FILEPATH_LEN);
  sprintf(ase_ready_filepath, "%s/%s", ase_workdir_path, ASE_READY_FILENAME);
  ase_ready_fd = fopen( ase_ready_filepath, "w");
  fprintf(ase_ready_fd, "%d", ase_pid);
  fclose(ase_ready_fd);

  // Display "Ready for simulation"
  BEGIN_GREEN_FONTCOLOR;
  printf("SIM-C : ** ATTENTION : BEFORE running the software application **\n");
  printf("        Run the following command into terminal where application will run (copy-and-paste) =>\n");
  printf("        $SHELL   | Run:\n");
  printf("        ---------+-----------------------------------------\n");
  printf("        bash     | export ASE_WORKDIR=%s\n", ase_run_path);
  printf("        tcsh/csh | setenv ASE_WORKDIR %s\n", ase_run_path);
  printf("        For any other $SHELL, consult your Linux administrator\n");
  printf("\n");
  END_GREEN_FONTCOLOR;
  
  // Run ase_regress.sh here
  if (cfg->ase_mode == ASE_MODE_REGRESSION) 
    {
      printf("Starting ase_regress.sh script...\n");
      system("./ase_regress.sh &");  
    }
  else
    {
      BEGIN_GREEN_FONTCOLOR;
      printf("SIM-C : Ready for simulation...\n");
      printf("SIM-C : Press CTRL-C to close simulator...\n");
      END_GREEN_FONTCOLOR;
    }

  fflush(stdout);

  FUNC_CALL_EXIT;
  return 0;
}


/*
 * DPI simulation timeout counter
 * - When CTRL-C is pressed, start teardown sequence
 * - TEARDOWN SEQUENCE:
 *   - Close and unlink message queues
 *   - Close and unlink shared memories
 *   - Destroy linked list
 *   - Delete .ase_ready
 *   - Send $finish to VCS
 */
void start_simkill_countdown()
{
  FUNC_CALL_ENTRY;

  // Close and unlink message queue
  printf("SIM-C : Closing message queue and unlinking...\n");
  // ase_mqueue_teardown();

  // Destroy all open shared memory regions
  printf("SIM-C : Unlinking Shared memory regions.... \n");
  // ase_destroy();

  // *FIXME* Remove the ASE timestamp file
  if (unlink(tstamp_filepath) == -1)
    {
      printf("SIM-C : %s could not be deleted, please delete manually... \n", TSTAMP_FILENAME);
    }

  // Final clean of IPC
  final_ipc_cleanup();

  // Close workspace log
  fclose(fp_workspace_log);

  // Remove session files
  printf("SIM-C : Cleaning session files...\n");
  if ( unlink(ase_ready_filepath) == -1 )
    {
      BEGIN_RED_FONTCOLOR;
      printf("Session file %s could not be removed, please remove manually !!\n", ASE_READY_FILENAME);
      END_RED_FONTCOLOR;
    }

  // Print location of log files
  BEGIN_GREEN_FONTCOLOR;
  printf("SIM-C : Simulation generated log files\n");
  printf("        Transactions file   | $ASE_WORKDIR/transactions.tsv\n");
  printf("        Workspaces info     | $ASE_WORKDIR/workspace_info.log\n");
  printf("        Protocol Warnings   | $ASE_WORKDIR/warnings.txt\n");
  printf("        ASE seed            | $ASE_WORKDIR/ase_seed.txt\n");
  END_GREEN_FONTCOLOR;

  // Send a simulation kill command
  printf("SIM-C : Sending kill command...\n");
  svSetScope(scope);
  simkill();

  FUNC_CALL_EXIT;
}


/*
 * ase_umsg_init : SIM_SIDE UMSG setup
 *                 Set up CSR addresses to indicate existance
 *                 and features of the UMSG system
 */
void ase_umsg_init(uint64_t dsm_base)
{
  FUNC_CALL_ENTRY;

  uint32_t *cirbstat;

  printf ("SIM-C : Enabling UMSG subsystem in ASE...\n");

  // Calculate CIRBSTAT address
  cirbstat = (uint32_t*)((uint64_t)(dsm_base + ASE_CIRBSTAT_CSROFF));

  // CIRBSTAT setup (completed / ready)
  *cirbstat = cfg->num_umsg_log2 << 4 | 0x1 << 0;
  printf ("        DSM base      = %p\n", (uint32_t*)dsm_base);
  printf ("        CIRBSTAT addr = %p\n", cirbstat);
  printf ("        *cirbstat     = %08x\n", *cirbstat);

  FUNC_CALL_EXIT;
}


/*
 * Parse strings and remove unnecessary characters
 */
// Remove spaces
void remove_spaces(char* in_str)
{
  char* i;
  char* j;
  i = in_str;
  j = in_str;
  while(*j != 0)
    {
      *i = *j++;
      if(*i != ' ')
	i++;
    }
  *i = 0;
}


// Remove tabs
void remove_tabs(char* in_str)
{
  char *i = in_str;
  char *j = in_str;
  while(*j != 0)
    {
      *i = *j++;
      if(*i != '\t')
  	i++;
    }
  *i = 0;
}

// Remove newline
void remove_newline(char* in_str)
{
  char *i = in_str;
  char *j = in_str;
  while(*j != 0)
    {
      *i = *j++;
      if(*i != '\n')
  	i++;
    }
  *i = 0;
}


/*
 * ASE config parsing
 * - Set default values for ASE configuration
 * - See if a ase.cfg is available for overriding global values
 *   - If YES, parse and configure the cfg (ase_cfg_t) structure
 */
void ase_config_parse(char *filename)
{
  FUNC_CALL_ENTRY;

  FILE *fp;
  char *line;
  size_t len = 0;
  ssize_t read;
  char *parameter;
  int value;
  // int tmp_umsg_log2;

  char *ase_cfg_filepath;
  ase_cfg_filepath = malloc(256);
  sprintf(ase_cfg_filepath, "%s/%s", ase_run_path, ASE_CONFIG_FILE);

  // Allocate space to store ASE config
  cfg = (struct ase_cfg_t *)malloc( sizeof(struct ase_cfg_t) );
  if (cfg == NULL)
    {
      BEGIN_RED_FONTCOLOR;
      printf("SIM-C : ASE config structure could not be allocated... EXITING\n");
      END_RED_FONTCOLOR;
      ase_error_report("malloc", errno, ASE_OS_MALLOC_ERR);
    #ifdef SIM_SIDE
      start_simkill_countdown();
    #else
      exit(1);
    #endif
    }
  line = malloc(sizeof(char) * 80);

  // Default values
  cfg->ase_mode = ASE_MODE_DAEMON_NO_SIMKILL;
  cfg->ase_timeout = 500;
  cfg->ase_num_tests = 1;
  cfg->enable_reuse_seed = 0;
  cfg->enable_capcm = 0;
  cfg->memmap_sad_setting = 0;
  cfg->num_umsg_log2 = 5;
  cfg->enable_cl_view = 1;

  // Find ase.cfg OR not
  // if ( access (ASE_CONFIG_FILE, F_OK) != -1 )
  if ( access (ase_cfg_filepath, F_OK) != -1 )
    {
      // FILE exists, overwrite
      printf("SIM-C : Reading %s configuration file\n", ASE_CONFIG_FILE);
      fp = fopen(ase_cfg_filepath, "r");

      // Parse file line by line
      while ((read = getline(&line, &len, fp)) != -1)
	{
	  // Remove all invalid characters
	  remove_spaces (line);
	  remove_tabs (line);
	  remove_newline (line);
	  // Ignore strings begining with '#' OR NULL (compound NOR)
	  if ( (line[0] != '#') && (line[0] != '\0') )
	    {
	      parameter = strtok(line, "=\n");
	      value = atoi(strtok(NULL, ""));
	      if (strcmp (parameter,"ASE_MODE") == 0)
	      	cfg->ase_mode = value;
	      else if (strcmp (parameter,"ASE_TIMEOUT") == 0)
	      	cfg->ase_timeout = value;
	      else if (strcmp (parameter,"ASE_NUM_TESTS") == 0)
	      	cfg->ase_num_tests = value;
	      else if (strcmp (parameter, "ENABLE_REUSE_SEED") == 0)
		cfg->enable_reuse_seed = value;
	      else if (strcmp (parameter,"ENABLE_CAPCM") == 0)
	      	cfg->enable_capcm = value;
	      else if (strcmp (parameter,"MEMMAP_SAD_SETTING") == 0)
	      	cfg->memmap_sad_setting = value;
	      else if (strcmp (parameter,"NUM_UMSG_LOG2") == 0)
		cfg->num_umsg_log2 = value;
	      else if (strcmp (parameter,"ENABLE_CL_VIEW") == 0)
		cfg->enable_cl_view = value;
	      else
	      	printf("SIM-C : In config file %s, Parameter type %s is unidentified \n", ASE_CONFIG_FILE, parameter);
	    }
	}

      // ASE mode control
      switch (cfg->ase_mode)
	{
	  // Classic Server client mode
	case ASE_MODE_DAEMON_NO_SIMKILL:
	  printf("SIM-C : ASE was started in Mode 1 (Server-Client without SIMKILL)\n");
	  cfg->ase_timeout = 0;
	  cfg->ase_num_tests = 0;
	  break;

	  // Server Client mode with SIMKILL
	case ASE_MODE_DAEMON_SIMKILL:
	  printf("SIM-C : ASE was started in Mode 2 (Server-Client with SIMKILL)\n");
	  cfg->ase_num_tests = 0;
	  break;

	  // Long runtime mode (SW kills SIM)
	case ASE_MODE_DAEMON_SW_SIMKILL:
	  printf("SIM-C : ASE was started in Mode 3 (Server-Client with Sw SIMKILL (long runs)\n");
	  cfg->ase_timeout = 0;
	  cfg->ase_num_tests = 0;
	  break;

	  // Regression mode (lets an SH file with
	case ASE_MODE_REGRESSION:
	  printf("SIM-C : ASE was started in Mode 4 (Regression mode)\n");
	  cfg->ase_timeout = 0;
	  break;

	  // Illegal modes
	default:
	  printf("SIM-C : ASE mode could not be identified, will revert to ASE_MODE = 1 (Server client w/o SIMKILL)\n");
	  cfg->ase_mode = ASE_MODE_DAEMON_NO_SIMKILL;
	  cfg->ase_timeout = 0;
	  cfg->ase_num_tests = 0;
	}


      // CAPCM size implementation
      if (cfg->enable_capcm != 0)
	{
	  if ((cfg->memmap_sad_setting > 15) || (cfg->memmap_sad_setting < 0))
	    {
	      BEGIN_YELLOW_FONTCOLOR;
	      printf("SIM-C : In config file %s, there was an error in setting MEMMAP_SAD_SETTING\n", ASE_CONFIG_FILE);
	      printf("        MEMMAP_SAD_SETTING was %d\n", cfg->memmap_sad_setting);
	      printf("        Setting default MEMMAP_SAD_SETTING to default '2', see ase.cfg and ASE User Guide \n");
	      cfg->memmap_sad_setting = 2;
	      END_YELLOW_FONTCOLOR;
	    }
	}

      // UMSG implementation
      if (cfg->num_umsg_log2 == 0)
	{
	  BEGIN_YELLOW_FONTCOLOR;
	  printf("SIM-C : In config file %s, there was an error in setting NUM_UMSG_LOG2\n", ASE_CONFIG_FILE);
	  printf("        NUM_UMSG_LOG2 was %d\n", cfg->num_umsg_log2);
	  printf("        Setting default NUM_UMSG_LOG2 to default 5\n");
	  cfg->num_umsg_log2 = 5;
	  END_YELLOW_FONTCOLOR;
	}

      // Close file
      fclose(fp);
    }
  else
    {
      // FILE does not exist
      printf("SIM-C : %s not found, using default values\n", ASE_CONFIG_FILE);
    }

  // Print configurations
  BEGIN_YELLOW_FONTCOLOR;

  // ASE mode
  printf("        ASE mode                   ... ");
  switch (cfg->ase_mode)
    {
    case ASE_MODE_DAEMON_NO_SIMKILL : printf("Server-Client mode without SIMKILL\n") ; break ;
    case ASE_MODE_DAEMON_SIMKILL    : printf("Server-Client mode with SIMKILL\n") ; break ;
    case ASE_MODE_DAEMON_SW_SIMKILL : printf("Server-Client mode with SW SIMKILL (long runs)\n") ; break ;
    case ASE_MODE_REGRESSION        : printf("ASE Regression mode\n") ; break ;
    }  

  // Inactivity
  if (cfg->ase_timeout != 0)
    printf("        Inactivity kill-switch     ... ENABLED after %d clocks \n", cfg->ase_timeout);
  else
    printf("        Inactivity kill-switch     ... DISABLED \n");
  
  // Reuse seed
  if (cfg->enable_reuse_seed != 0)
    printf("        Reuse simulation seed      ... ENABLED \n");
  else
    printf("        Reuse simulation seed      ... DISABLED \n");

  // UMSG
  printf("        Number of UMSG buffers     ... %d (NUM_UMSG_LOG2 = %d) \n", (int)pow((float)2, (float)cfg->num_umsg_log2), cfg->num_umsg_log2);

  // CAPCM
  if (cfg->enable_capcm != 0)
    {
      printf("        CA Private memory          ... ENABLED\n");
    }
  else
    printf("        CA Private memory          ... DISABLED\n");

  // CL view
  if (cfg->enable_cl_view != 0)
    printf("        ASE Transaction view       ... ENABLED\n");
  else
    printf("        ASE Transaction view       ... DISABLED\n");

  END_YELLOW_FONTCOLOR;

  // Transfer data to hardware (for simulation only)
#ifdef SIM_SIDE
  ase_config_dex(cfg);
#endif

  FUNC_CALL_EXIT;
}
