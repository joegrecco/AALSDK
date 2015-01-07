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
//****************************************************************************
/// @file _xlMessageDelivery.cpp
/// @brief Definitions for the XL Runtime internal default Message Delivery facility.
/// @ingroup MDS
///
/// @verbatim
/// Intel(R) QuickAssist Technology Accelerator Abstraction Layer
///
/// AUTHORS: Joseph Grecco, Intel Corporation
///
/// HISTORY:
/// WHEN:          WHO:     WHAT:
/// 03/13/2014     JG       Initial version@endverbatim
//****************************************************************************
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif // HAVE_CONFIG_H

#include "aalsdk/AALDefs.h"
#include "aalsdk/aas/XLRuntimeModule.h"
#include "aalsdk/osal/OSServiceModule.h"
#include "aalsdk/aas/AALInProcServiceFactory.h"  // Defines InProc Service Factory
#include "aalsdk/aas/XLRuntimeMessages.h"
#include "_xlMessageDelivery.h"


// Define the factory to use for this service.

//  Use the standard AAL Service SDK to implement the built-ins.  This makes the
//   the system design more consistent by treating built-ins the same as plug-ins.
//   The only difference in defining a plug-in vs a built-in is that the entry point
//   (i.e., ServiceModule accessor) is a single well known name for a plug-in (_ServiceModule).
//   This name must be unique for a built-in.
//   The name is generated by the DEFINE_SERVICE_PROVIDER_2_0_ACCESSOR() macro. By default the
//   well known name is generated.


#define SERVICE_FACTORY AAL::AAS::InProcSvcsFact< AAL::XL::RT::_xlMessageDelivery >


#if defined ( __AAL_WINDOWS__ )
# pragma warning(push)
# pragma warning(disable : 4996) // destination of copy is unsafe
#endif // __AAL_WINDOWS__

AAL_BEGIN_SVC_MOD(SERVICE_FACTORY, localMDS, XLRT_API, XLRT_VERSION, XLRT_VERSION_CURRENT, XLRT_VERSION_REVISION, XLRT_VERSION_AGE)
   // Only default commands for now.
AAL_END_SVC_MOD()

#if defined ( __AAL_WINDOWS__ )
#pragma warning( pop )
#endif // __AAL_WINDOWS__


BEGIN_NAMESPACE(AAL)
   BEGIN_NAMESPACE(XL)
      BEGIN_NAMESPACE(RT)

/// @addtogroup MDS
/// @{


// Hook to allow the object to initialize
//=============================================================================
// Name: init
// Description: Object Initialization
// Interface: public
// Comments: The only thing required is to send the message
//=============================================================================
void _xlMessageDelivery::init(TransactionID const &rtid)
{
   // Sends a Service Client serviceAllocated callback
   QueueAASEvent(new AAL::AAS::ObjectCreatedEvent( getRuntimeClient(),
                                                   Client(),
                                                   dynamic_cast<IBase*>(this),
                                                   rtid));
}

//=============================================================================
// Name: Release
// Description: Release the service
// Interface: public
// Comments:
//=============================================================================
btBool _xlMessageDelivery::Release(TransactionID const &rTranID, btTime timeout)
{
   AutoLock(this);
   // Delete the Dispatcher. This will cause all messages to be processed
   if ( NULL != m_Dispatcher ) {
      delete m_Dispatcher;
      m_Dispatcher = NULL;
   }
   return ServiceBase::Release(rTranID, timeout);
}

// Quiet Release. Used when Service is unloaded.
btBool _xlMessageDelivery::Release(btTime timeout)
{
   AutoLock(this);
   // Delete the Dispatcher. This will cause all messages to be processed
   if ( NULL != m_Dispatcher ) {
      delete m_Dispatcher;
      m_Dispatcher = NULL;
   }
   return ServiceBase::Release(timeout);
}


//=============================================================================
// Name: Schedule
// Description: Manually process some work
// Interface: public
// Comments: Unsupported
//=============================================================================
AAL::AAS::EDS_Status _xlMessageDelivery::Schedule()
{
   return AAL::AAS::EDS_statusUnsupportedModel;
}

//=============================================================================
// Name: StartEventDelivery
// Description: Start the service
// Interface: public
// Comments:
//=============================================================================
void _xlMessageDelivery::StartEventDelivery()
{
   AutoLock(this);
   if ( NULL == m_Dispatcher ) {
      m_Dispatcher = new OSLThreadGroup();
   }
}

//=============================================================================
// Name: StopEventDelivery
// Description: Stop the service
// Interface: public
// Comments: Brute force method
//=============================================================================
void _xlMessageDelivery::StopEventDelivery()
{
   AutoLock(this);
   if( NULL != m_Dispatcher ) {
     m_Dispatcher->Drain();
   }
}

//=============================================================================
// Name: SetParms
// Description: Set service parameters
// Interface: public
// Comments: Unsupported
//=============================================================================
btBool _xlMessageDelivery::SetParms(NamedValueSet const &rparms)
{
   return false;
}


//=============================================================================
// Name: QueueEvent
// Description: Queue and Event or Funtor object for scheduled dispatch
// Interface: public
// Comments: 3 variants.
//           1 - for events to event handlers
//           2 - for functors wrapped as events (deprecated)
//           3 - for proper dispatchable functors.
//=============================================================================
btBool _xlMessageDelivery::QueueEvent(btEventHandler       handler,
                                      AAL::AAS::CAALEvent *pevent )
{
   AutoLock(this);
   if ( NULL == m_Dispatcher ) {
      return false;
   }
   pevent->setHandler(handler);
   m_Dispatcher->Add(pevent);
   return true;
}

btBool _xlMessageDelivery::QueueEvent(btObjectType         parm,
                                      AAL::AAS::CAALEvent *pevent )
{
   AutoLock(this);
   if ( NULL == m_Dispatcher ) {
      return false;
   }
   m_Dispatcher->Add(pevent);
   return true;
}

btBool _xlMessageDelivery::QueueEvent(btObjectType   parm,
                                      IDispatchable *pfunctor)
{
   AutoLock(this);
   if ( NULL == m_Dispatcher ) {
      return false;
   }
   m_Dispatcher->Add(pfunctor);
   return true;
}


//=============================================================================
// Name: GetEventDispatcher
// Description: Retrieve the IEventDispatcher interface.
// Interface: public
// Comments: Unsupported
//=============================================================================
AAL::AAS::IEventDispatcher *_xlMessageDelivery::GetEventDispatcher(AAL::AAS::EDSDispatchClass disclass)
{
   return NULL;
}

//=============================================================================
// Name: ~_xlMessageDelivery
// Description: Retrieve the IEventDispatcher interface.
// Interface: public
// Comments:
//=============================================================================
_xlMessageDelivery::~_xlMessageDelivery()
{
   StopEventDelivery();
}

/// @} group MDS


      END_NAMESPACE(RT)
   END_NAMESPACE(XL)
END_NAMESPACE(AAL)

