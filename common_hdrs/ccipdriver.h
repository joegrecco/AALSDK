//******************************************************************************
// Part of the Intel(R) QuickAssist Technology Accelerator Abstraction Layer
//
// This  file  is  provided  under  a  dual BSD/GPLv2  license.  When using or
//         redistributing this file, you may do so under either license.
//
//                            GPL LICENSE SUMMARY
//
//  Copyright(c) 2015, Intel Corporation.
//
//  This program  is  free software;  you  can redistribute it  and/or  modify
//  it  under  the  terms of  version 2 of  the GNU General Public License  as
//  published by the Free Software Foundation.
//
//  This  program  is distributed  in the  hope that it  will  be useful,  but
//  WITHOUT   ANY   WARRANTY;   without   even  the   implied   warranty    of
//  MERCHANTABILITY  or  FITNESS  FOR  A  PARTICULAR  PURPOSE.  See  the   GNU
//  General Public License for more details.
//
//  The  full  GNU  General Public License is  included in  this  distribution
//  in the file called README.GPLV2-LICENSE.TXT.
//
//  Contact Information:
//  Henry Mitchel, henry.mitchel at intel.com
//  77 Reed Rd., Hudson, MA  01749
//
//                                BSD LICENSE
//
//  Copyright(c) 2015, Intel Corporation.
//
//  Redistribution and  use  in source  and  binary  forms,  with  or  without
//  modification,  are   permitted  provided  that  the  following  conditions
//  are met:
//
//    * Redistributions  of  source  code  must  retain  the  above  copyright
//      notice, this list of conditions and the following disclaimer.
//    * Redistributions in  binary form  must  reproduce  the  above copyright
//      notice,  this  list of  conditions  and  the  following disclaimer  in
//      the   documentation   and/or   other   materials   provided  with  the
//      distribution.
//    * Neither   the  name   of  Intel  Corporation  nor  the  names  of  its
//      contributors  may  be  used  to  endorse  or promote  products derived
//      from this software without specific prior written permission.
//
//  THIS  SOFTWARE  IS  PROVIDED  BY  THE  COPYRIGHT HOLDERS  AND CONTRIBUTORS
//  "AS IS"  AND  ANY  EXPRESS  OR  IMPLIED  WARRANTIES,  INCLUDING,  BUT  NOT
//  LIMITED  TO, THE  IMPLIED WARRANTIES OF  MERCHANTABILITY  AND FITNESS  FOR
//  A  PARTICULAR  PURPOSE  ARE  DISCLAIMED.  IN NO EVENT  SHALL THE COPYRIGHT
//  OWNER OR CONTRIBUTORS BE LIABLE  FOR  ANY  DIRECT,  INDIRECT,  INCIDENTAL,
//  SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL   DAMAGES  (INCLUDING,   BUT   NOT
//  LIMITED  TO,  PROCUREMENT  OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF USE,
//  DATA,  OR PROFITS;  OR BUSINESS INTERRUPTION)  HOWEVER  CAUSED  AND ON ANY
//  THEORY  OF  LIABILITY,  WHETHER  IN  CONTRACT,  STRICT LIABILITY,  OR TORT
//  (INCLUDING  NEGLIGENCE  OR OTHERWISE) ARISING  IN ANY WAY  OUT  OF THE USE
//  OF  THIS  SOFTWARE, EVEN IF ADVISED  OF  THE  POSSIBILITY  OF SUCH DAMAGE.
//******************************************************************************
//****************************************************************************
//        FILE: ccipdriver.h
//     CREATED: Nov. 2, 2015
//      AUTHOR: Joseph Grecco, Intel  <joe.grecco@intel.com>
//
// PURPOSE: Definitions for the CCIP device driver
// HISTORY:
// COMMENTS:
// WHEN:          WHO:     WHAT:
// 11/02/15       JG       Initial version.
//****************************************************************************
#ifndef __AALSDK_CCIP_DRIVER_H__
#define __AALSDK_CCIP_DRIVER_H__
#include <aalsdk/kernel/AALWorkspace.h>
#include <aalsdk/kernel/AALTransactionID_s.h>

BEGIN_NAMESPACE(AAL)

BEGIN_C_DECLS

///=================================================================
/// IDs used by the devices and objects
///=================================================================

// FPGA Management Engine GUID
#define CCIP_FME_GUIDL              (0x82FE38F0F9E17764ULL)
#define CCIP_FME_GUIDH              (0xBFAf2AE94A5246E3ULL)
#define CCIP_FME_PIPIID             (0x4DDEA2705E7344D1ULL)

#define CCIP_DEV_FME_SUBDEV         0
#define CCIP_DEV_PORT_SUBDEV(s)     (s + 0x10)
#define CCIP_DEV_AFU_SUBDEV(s)      (s + 0x20)

#define PORT_SUBDEV(s)              (s - 0x10 )
#define AFU_SUBDEV(s)               (s - 0x20 )

/// FPGA Port GUID
#define CCIP_PORT_GUIDL             (0x9642B06C6B355B87ULL)
#define CCIP_PORT_GUIDH             (0x3AB49893138D42EBULL)
#define CCIP_PORT_PIPIID            (0x5E82B04A50E59F20ULL)

/// AFU GUID
#define CCIP_AFU_PIPIID             (0x26F67D4CAD054DFCULL)

/// Signal Tap GUID
#define CCIP_STAP_GUIDL             (0xB6B03A385883AB8DULL)
#define CCIP_STAP_GUIDH             (0x022F85B12CC24C9DULL)
#define CCIP_STAP_PIPIID            (0xA710C842F06E45E0ULL)

/// Partial Reconfiguration GUID
#define CCIP_PR_GUIDL               (0x83B54FD5E5216870ULL)
#define CCIP_PR_GUIDH               (0xA3AAB28579A04572ULL)
#define CCIP_PR_PIPIID              (0x7C4D41EA156C4D81ULL)


/// Vender ID and Device ID
#define CCIP_FPGA_VENDER_ID         0x8086

/// PCI Device ID
#define PCIe_DEVICE_ID_RCiEP0       0xBCBD
#define PCIe_DEVICE_ID_RCiEP1       0xBCBE

/// QPI Device ID
#define PCIe_DEVICE_ID_RCiEP2       0xBCBC

/// MMIO Space map
#define FME_DFH_AFUIDL  0x8
#define FME_DFH_AFUIDH  0x10
#define FME_DFH_NEXTAFU 0x18

//-----------------------------------------------------------------------------
// Request message IDs
//-----------------------------------------------------------------------------
typedef enum
{
   uid_errnumOK = 0,
   uid_errnumBadDevHandle,                       // 1
   uid_errnumCouldNotClaimDevice,                // 2
   uid_errnumNoAppropriateInterface,             // 3
   uid_errnumDeviceHasNoPIPAssigned,             // 4
   uid_errnumCouldNotBindPipInterface,           // 5
   uid_errnumCouldNotUnBindPipInterface,         // 6
   uid_errnumNotDeviceOwner,                     // 7
   uid_errnumSystem,                             // 8
   uid_errnumAFUTransaction,                     // 9
   uid_errnumAFUTransactionNotFound,             // 10
   uid_errnumDuplicateStartingAFUTransactionID,  // 11
   uid_errnumBadParameter,                       // 12
   uid_errnumNoMem,                              // 13
   uid_errnumNoMap,                              // 14
   uid_errnumBadMapping,                         // 15
   uid_errnumPermission,                         // 16
   uid_errnumInvalidOpOnMAFU,                    // 17
   uid_errnumPointerOutOfWorkspace,              // 18
   uid_errnumNoAFUBindToChannel,                 // 19
   uid_errnumCopyFromUser,                       // 20
   uid_errnumDescArrayEmpty,                     // 21
   uid_errnumCouldNotCreate,                     // 22
   uid_errnumInvalidRequest,                     // 23
   uid_errnumInvalidDeviceAddr,                  // 24
   uid_errnumCouldNotDestroy,                    // 25
   uid_errnumDeviceBusy                          // 26
} uid_errnum_e;

typedef enum
{
   // Management
   reqid_UID_Bind=1,                // Use default API Version
   reqid_UID_ExtendedBindInfo,      // Pass additional Bind parms
   reqid_UID_UnBind,                // Release a device

   // Provision
   reqid_UID_Activate,              // Activate the device
   reqid_UID_Deactivate,            // Deactivate the device

   // Administration
   reqid_UID_Shutdown,              // Request that the Service session shutdown

   reqid_UID_SendAFU,               // Send AFU a message

   // Response and Event IDs
   rspid_UID_Shutdown=0xF000,       // Service is shutdown
   rspid_UID_UnbindComplete,        // Release Device Response
   rspid_UID_BindComplete,          // Bind has completed

   rspid_UID_Activate,              // Activate the device
   rspid_UID_Deactivate,            // Deactivate the device

   rspid_AFU_Response,              // Response from AFU request
   rspid_AFU_Event,                 // Event from AFU

   rspid_PIP_Event,                 // Event from PIP

   rspid_WSM_Response,              // Event from Workspace manager

   rspid_UID_Response,
   rspid_UID_Event,

} uid_msgIDs_e;

typedef enum
{
    ccipdrv_afucmdWKSP_ALLOC=1,
    ccipdrv_afucmdPort_AssertReset,
    ccipdrv_afucmdPort_DeassertReset,
    ccipdrv_afucmdPort_QuiesceAndAssertReset,
    ccipdrv_afucmdWKSP_FREE,
    ccipdrv_getMMIORmap,
    ccipdrv_getFeatureRegion
} ccipdrv_afuCmdID_e;

struct aalui_CCIdrvMessage
{
   btUnsigned64bitInt  apiver;     // Version of message handler [IN]
   btUnsigned64bitInt  pipver;     // Version of PIP interface [IN]
   btUnsigned64bitInt  cmd;        // Command [IN]
   btWSSize            size;       // size of payload [IN]
   btByte              payload[1]; // data [IN/OUT]
};

#if   defined( __AAL_LINUX__ )
# define AALUID_IOCTL_SENDMSG       _IOR ('x', 0x00, struct ccipui_ioctlreq)
# define AALUID_IOCTL_GETMSG_DESC   _IOR ('x', 0x01, struct ccipui_ioctlreq)
# define AALUID_IOCTL_GETMSG        _IOWR('x', 0x02, struct ccipui_ioctlreq)
# define AALUID_IOCTL_BINDDEV       _IOWR('x', 0x03, struct ccipui_ioctlreq)
# define AALUID_IOCTL_ACTIVATEDEV   _IOWR('x', 0x04, struct ccipui_ioctlreq)
# define AALUID_IOCTL_DEACTIVATEDEV _IOWR('x', 0x05, struct ccipui_ioctlreq)
#elif defined( __AAL_WINDOWS__ )
# ifdef __AAL_USER__
#    include <winioctl.h>
# endif // __AAL_USER__

// CTL_CODE() bits:
//     Common[31]
// DeviceType[30:16]
//     Access[15:14]
//     Custom[13]
//   Function[12:2] (or [10:0] << 2)
//     Method[1:0]

# define UAIA_DRV_ID                  0x2
# define UAIA_DEVICE_TYPE             FILE_DEVICE_BUS_EXTENDER
# define UAIA_ACCESS                  FILE_ANY_ACCESS
# define UAIA_CUSTOM                  1
# define UAIA_METHOD                  METHOD_BUFFERED

// field extractors
# define AAL_IOCTL_DRV_ID(__ctl_code) (((__ctl_code) >> 10) & 0x7)
# define AAL_IOCTL_FN(__ctl_code)     (((__ctl_code) >> 2) & 0xff)

                  // 0 - 0xff
# define UAIA_IOCTL(__index)          CTL_CODE(UAIA_DEVICE_TYPE, (UAIA_CUSTOM << 11) | (UAIA_DRV_ID << 8) | (__index), UAIA_METHOD, UAIA_ACCESS)

# define AALUID_IOCTL_SENDMSG         UAIA_IOCTL(0x00)
# define AALUID_IOCTL_GETMSG_DESC     UAIA_IOCTL(0x01)
# define AALUID_IOCTL_GETMSG          UAIA_IOCTL(0x02)
# define AALUID_IOCTL_BINDDEV         UAIA_IOCTL(0x03)
# define AALUID_IOCTL_ACTIVATEDEV     UAIA_IOCTL(0x04)
# define AALUID_IOCTL_DEACTIVATEDEV   UAIA_IOCTL(0x05)
# define AALUID_IOCTL_POLL            UAIA_IOCTL(0x06)
# define AALUID_IOCTL_MMAP            UAIA_IOCTL(0x07)

#endif // OS

//=============================================================================
// Name: ccipui_ioctlreq
// Description: IOCTL message block
//=============================================================================
struct ccipui_ioctlreq
{
   uid_msgIDs_e       id;           // ID of UI request [IN]
   stTransactionID_t  tranID;       // Transaction ID to identify result [IN]
   btObjectType       context;      // Optional token [IN]
   uid_errnum_e       errcode;      // Driver specific error number
   btHANDLE           handle;       // Device handle
   btWSSize           size;         // Size of payload section [IN]
   btByte             payload[];    // Beginning of optional payload [IN]
};

// TODO CASSERT( sizeof(struct ccipui_ioctlreq)

#define aalui_ioAFUmessagep(i)   (struct aalui_CCIdrvMessage *)(i->payload )
#define aalui_ioctlPayload(i)    ((void *)(i->payload))
#define aalui_ioctlPayloadSize(i)   ((i)->size)


struct ahm_req
{
   union {
      // mem_alloc
      struct {
         btWSID   m_wsid;     // IN
         btWSSize m_size;     // IN
         btWSSize m_pgsize;
      } wksp;

// Special workspace IDs for CSR Aperture mapping
// XXX These must match aaldevice.h:AAL_DEV_APIMAP_CSR*
#define WSID_CSRMAP_READAREA  0x00000001
#define WSID_CSRMAP_WRITEAREA 0x00000002
#define WSID_MAP_MMIOR        0x00000003
#define WSID_MAP_UMSG         0x00000004
      // mem_get_cookie
      struct {
         btWSID             m_wsid;   /* IN  */
         btUnsigned64bitInt m_cookie; /* OUT */
      } wksp_cookie;

      struct {
         btVirtAddr         vaddr; /* IN   */
         btWSSize           size;   /* IN   */
         btUnsigned64bitInt mem_id; /* OUT  */
      } mem_uv2id;
   } u;
};


struct ccidrvreq
{
   struct ahm_req    ahmreq;
   stTransactionID_t afutskTranID;
   btTime            pollrate;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
////////////////////                                     //////////////////////
/////////////////          EVENT SUPPORT FUNCTIONS         ////////////////////
////////////////////                                     //////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

//=============================================================================
// Name: aalui_AFUResponse
// Type[Dir]: Event/Response [OUT]
// Object: AFU engine
// Command ID: reqid_UID_SendAFU
// fields: context - user defined data associated with descriptor
//         afutskTranID - transaction ID of AFU task
//         respID - ID code of response
//         mode - mode the descriptor ran
//         data - 32 buyte data returned by AFU
// Description: Returned AFU response to a request
// Comments:
//=============================================================================
typedef enum
{
   uid_afurespUndefinedResponse=0,
   uid_afurespInputDescriptorComplete,
   uid_afurespOutputDescriptorComplete,
   uid_afurespEndofTask,
   uid_afurespTaskStarted,
   uid_afurespTaskStopped,
   uid_afurespSetContext,
   uid_afurespTaskComplete,
   uid_afurespSetGetCSRComplete,
   uid_afurespAFUCreateComplete,
   uid_afurespAFUDestroyComplete,
   uid_afurespActivateComplete,
   uid_afurespDeactivateComplete,
   uid_afurespInitializeComplete,
   uid_afurespFreeComplete,
   uid_afurespUndefinedRequest,
   uid_afurespFirstUserResponse = 0xf000
} uid_afurespID_e;

struct aalui_AFUResponse
{
   btUnsigned32bitInt  respID;
   btUnsigned64bitInt  evtData;
   btUnsignedInt       payloadsize;
};
#define aalui_AFURespPayload(__ptr) ( ((btVirtAddr)(__ptr)) + sizeof(struct aalui_AFUResponse) )

//=============================================================================
// Name: aalui_Shutdown
// Type[Dir]: Request[IN]
// Object: UI Driver
// Description: Request to the AFU
// Comments:
//=============================================================================
typedef enum
{
   ui_shutdownReasonNormal=0,
   ui_shutdownReasonMaint,
   ui_shutdownFailure,
   ui_shutdownReasonRestart
} ui_shutdownreason_e;


//=============================================================================
// Name: aalui_Shutdown
// Description: UI driver shutdown event.
//=============================================================================
//=============================================================================
struct aalui_Shutdown {
   ui_shutdownreason_e m_reason;
   btTime              m_timeout;
};


//=============================================================================
// Name: aalui_WSMParms
// Object: Kernel Workspace Manager Service
// Command ID: reqid_UID_SendWSM
// Input: wsid  - Workspace ID
//        ptr   - Pointer to start of workspace
//        physptr - Physical address
//        size  - size in bytes of workspace
// Description: Parameters describing a workspace
// Comments: Used in WSM interface
//=============================================================================
struct aalui_WSMParms
{
   btWSID     wsid;        // Workspace ID
   btVirtAddr ptr;         // Virtual Workspace pointer
   btPhysAddr physptr;     // Depends on use
   btWSSize   size;        // Workspace size
   btWSSize   itemsize;    // Workspace item size
   btWSSize   itemspacing; // Workspace item spacing
   TTASK_MODE type;        // Task mode this workspace is compatible with
};


//=============================================================================
// Name: aalui_WSMEvent
// Type[Dir]: Event/Response [OUT]
// Object: Kernel Workspace Manager Service
// Command ID: reqid_UID_SendWSM
// fields: wsid - Workspace ID
//         ptr  - Pointer tostart of workspace
//         size - size in bytes of workspace
// Description: Parameters describing a workspace
// Comments: Used in WSM interface
//=============================================================================
typedef enum
{
   uid_wseventAllocate=0,
   uid_wseventFree,
   uid_wseventGetPhys,
   uid_wseventCSRMap,
   uid_wseventMMIOMap
} uid_wseventID_e;

struct aalui_WSMEvent
{
   uid_wseventID_e       evtID;
   struct aalui_WSMParms wsParms;
};


//=============================================================================
// Name: ccipdrv_DeviceAttributes
// Request[IN] Response[OUT]
// Description: Debice attirpbues.
// Comments: This object is used to notify the device owner of attributes
//           any specific attributes the device publishes
//=============================================================================
struct ccipdrv_DeviceAttributes
{
   btWSSize           m_size;             // Size of the attibute block
   btUnsigned32bitInt m_mappableAPI;      // Permits direct CSR mapping To be Deprecated
   btByte             m_devattrib[];      // Attribute block (TBD)
};

END_C_DECLS

END_NAMESPACE(AAL)


#endif // __AALSDK_CCIP_DRIVER_H__

