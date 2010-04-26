/*
 *  Copyright 2001-2009 Texas Instruments - http://www.ti.com/
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
/*==============================================================================
 *  @file   MessageQApp.c
 *
 *  @brief  Sample application for MessageQ module between MPU & SysM3
 *
 *  ============================================================================
 */


/* Standard headers */
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <Std.h>

/* OSAL & Utils headers */
#include <Trace.h>
#include <OsalPrint.h>
#include <Memory.h>
#include <String.h>

/* Module level headers */
#include <IpcUsr.h>
#include <ti/ipc/MessageQ.h>
#include <ti/ipc/MultiProc.h>
#include <ti/ipc/HeapBufMP.h>
#include <ti/ipc/NameServer.h>
#include <ti/ipc/SharedRegion.h>
#include <ti/ipc/ListMP.h>
#include <ti/ipc/Notify.h>

/* Application header */
#include "MessageQApp_config.h"


#if defined (__cplusplus)
extern "C" {
#endif /* defined (__cplusplus) */


/** ============================================================================
 *  Macros and types
 *  ============================================================================
 */
/*!
 *  @brief  Number of transfers to be tested.
 */
//#define  MESSAGEQAPP_NUM_TRANSFERS  1000
#define  MESSAGEQAPP_NUM_TRANSFERS  10


/** ============================================================================
 *  Globals
 *  ============================================================================
 */
MessageQ_Handle                MessageQApp_messageQ;
HeapBufMP_Handle               MessageQApp_heapHandle;
MessageQ_QueueId               MessageQApp_queueId;
UInt16                         MessageQApp_procId;
UInt32                         MessageQApp_curShAddr;
ProcMgr_Handle                 MessageQApp_procMgrHandle;
SizeT                          MessageQApp_heapSize         = 0;
Ptr                            MessageQApp_ptr              = NULL;

/** ============================================================================
 *  Functions
 *  ============================================================================
 */
/*!
 *  @brief  Function to execute the startup for MessageQApp sample application
 *
 *  @sa
 */
Int
MessageQApp_startup (Void)
{
    Int32                          status           = 0;
    Ipc_Config                     config;
    HeapBufMP_Params               heapbufmpParams;
    UInt32                         srCount;
    SharedRegion_Entry             srEntry;
    Int                            i;
    IHeap_Handle                   srHeap           = NULL;
#if !defined (SYSLINK_USE_DAEMON)
    UInt32                         entryPoint       = 0;
    ProcMgr_StartParams            startParams;
#if defined(SYSLINK_USE_LOADER)
    Char *                         imageName;
    UInt32                         fileId;
#endif /* if defined(SYSLINK_USE_LOADER) */
#endif /* if !defined(SYSLINK_USE_DAEMON) */
    ProcMgr_AttachParams           attachParams;
    ProcMgr_State                  state;

    Osal_printf ("Entered MessageQApp_startup\n");

    Ipc_getConfig (&config);
    status = Ipc_setup (&config);
    if (status < 0) {
        Osal_printf ("Error in Ipc_setup [0x%x]\n", status);
    }

    /* Open a handle to the SysM3 ProcMgr instance. */
    MessageQApp_procId = MultiProc_getId ("SysM3");
    status = ProcMgr_open (&MessageQApp_procMgrHandle,
                           MessageQApp_procId);
    if (status < 0) {
        Osal_printf ("Error in ProcMgr_open [0x%x]\n", status);
    }
    else {
        Osal_printf ("ProcMgr_open Status [0x%x]\n", status);
        ProcMgr_getAttachParams (NULL, &attachParams);
        /* Default params will be used if NULL is passed. */
        status = ProcMgr_attach (MessageQApp_procMgrHandle, &attachParams);
        if (status < 0) {
            Osal_printf ("ProcMgr_attach failed [0x%x]\n", status);
        }
        else {
            Osal_printf ("ProcMgr_attach status: [0x%x]\n", status);
            state = ProcMgr_getState (MessageQApp_procMgrHandle);
            Osal_printf ("After attach: ProcMgr_getState\n"
                         "    state [0x%x]\n", status);
        }
    }


#if !defined (SYSLINK_USE_DAEMON)
    startParams.proc_id = MessageQApp_procId;
#ifdef SYSLINK_USE_LOADER
    imageName = "./MessageQ_MPUSYS_Test_Core0.xem3";
    Osal_printf ("Loading image (%s) onto Ducati with ProcId %d\n", imageName,
                startParams.proc_id);
    status = ProcMgr_load (MessageQApp_procMgrHandle, imageName, 2,
                            (String *) &imageName, &entryPoint, &fileId,
                            startParams.proc_id);
    Osal_printf ("ProcMgr_load SysM3 Status [0x%x]\n", status);
#endif /* SYSLINK_USE_LOADER */
    status = ProcMgr_start (MessageQApp_procMgrHandle, entryPoint,
                            &startParams);
    Osal_printf ("ProcMgr_start SysM3 Status [0x%x]\n", status);
#endif /* !SYSLINK_USE_DAEMON */

    srCount = SharedRegion_getNumRegions();
    Osal_printf ("SharedRegion_getNumRegions = %d\n", srCount);
    for (i = 0; i < srCount; i++) {
        status = SharedRegion_getEntry (i, &srEntry);
        Osal_printf ("SharedRegion_entry #%d: base = 0x%x len = 0x%x "
                        "ownerProcId = %d isValid = %d cacheEnable = %d "
                        "cacheLineSize = 0x%x createHeap = %d name = %s\n",
                        i, srEntry.base, srEntry.len, srEntry.ownerProcId,
                        (Int)srEntry.isValid, (Int)srEntry.cacheEnable,
                        srEntry.cacheLineSize, (Int)srEntry.createHeap,
                        srEntry.name);
    }

    /* Create Heap and register it with MessageQ */
    if (status >= 0) {
        HeapBufMP_Params_init (&heapbufmpParams);
        heapbufmpParams.sharedAddr = NULL;
        heapbufmpParams.align      = 128;
        heapbufmpParams.numBlocks  = 4;
        heapbufmpParams.blockSize  = MSGSIZE;
        MessageQApp_heapSize = HeapBufMP_sharedMemReq (&heapbufmpParams);
        Osal_printf ("heapSize = 0x%x\n", MessageQApp_heapSize);

        srHeap = SharedRegion_getHeap (0);
        if (srHeap == NULL) {
            status = MessageQ_E_FAIL;
            Osal_printf ("SharedRegion_getHeap failed for %d processor."
                         " srHeap: [0x%x]\n",
                         MessageQApp_procId,
                         srHeap);
        }
        else {
            Osal_printf ("Before Memory_alloc = 0x%x\n", srHeap);
            MessageQApp_ptr = Memory_alloc (srHeap,
                                MessageQApp_heapSize,
                                0);
            if (MessageQApp_ptr == NULL) {
                status = MessageQ_E_FAIL;
                Osal_printf ("Memory_alloc failed for %d processor."
                             " ptr: [0x%x]\n",
                             MessageQApp_procId,
                             MessageQApp_ptr);
            }
            else {
                heapbufmpParams.name           = "Heap0";
                heapbufmpParams.sharedAddr     = MessageQApp_ptr;
                Osal_printf ("Before HeapBufMP_Create: [0x%x]\n",
                                MessageQApp_ptr);
                MessageQApp_heapHandle = HeapBufMP_create (&heapbufmpParams);
                if (MessageQApp_heapHandle == NULL) {
                    status = MessageQ_E_FAIL;
                    Osal_printf ("HeapBufMP_create failed for %d processor."
                                 " Handle: [0x%x]\n",
                                 MessageQApp_procId,
                                 MessageQApp_heapHandle);
                }
                else {
                    /* Register this heap with MessageQ */
                    status = MessageQ_registerHeap (MessageQApp_heapHandle,
                                                    HEAPID);
                    if (status < 0) {
                        Osal_printf ("MessageQ_registerHeap failed!\n");
                    }
                }
            }
        }
    }

    Osal_printf ("Leaving MessageQApp_startup: status = 0x%x\n", status);

    return (status);
}


/*!
 *  @brief  Function to execute the MessageQApp sample application
 *
 *  @sa
 */
Int
MessageQApp_execute (Void)
{
    Int32                    status     = 0;
    MessageQ_Msg             msg        = NULL;
    MessageQ_Params          msgParams;
    UInt16                   i;

    Osal_printf ("Entered MessageQApp_execute\n");

    /* Create the Message Queue. */
    MessageQ_Params_init (&msgParams);
    MessageQApp_messageQ = MessageQ_create (ARM_MESSAGEQNAME, &msgParams);
    if (MessageQApp_messageQ == NULL) {
        Osal_printf ("Error in MessageQ_create\n");
    }
    else {
        Osal_printf ("MessageQ_create handle [0x%x]\n",
                     MessageQApp_messageQ);
    }

    sleep (1); /* Adding a small delay to resolve infinite nameserver issue */
    if (status >=0) {
        do {
            status = MessageQ_open (DUCATI_CORE0_MESSAGEQNAME,
                                    &MessageQApp_queueId);
        } while (status == MessageQ_E_NOTFOUND);
        if (status < 0) {
            Osal_printf ("Error in MessageQ_open [0x%x]\n", status);
        }
        else {
            Osal_printf ("MessageQ_open Status [0x%x]\n", status);
            Osal_printf ("MessageQApp_queueId  [0x%x]\n", MessageQApp_queueId);
        }
    }

    if (status >= 0) {
        Osal_printf ("\nExchanging messages with remote processor\n");
        for (i = 0 ; i < MESSAGEQAPP_NUM_TRANSFERS ; i++) {
            /* Allocate message. */
            msg = MessageQ_alloc (HEAPID, MSGSIZE);
            if (msg == NULL) {
                Osal_printf ("Error in MessageQ_alloc\n");
                break;
            }
            else {
                Osal_printf ("MessageQ_alloc msg [0x%x]\n", msg);
            }

            MessageQ_setMsgId (msg, (i % 16));

            /* Have the DSP reply to this message queue */
            MessageQ_setReplyQueue (MessageQApp_messageQ, msg);

            status = MessageQ_put (MessageQApp_queueId, msg);
            if (status < 0) {
                Osal_printf ("Error in MessageQ_put [0x%x]\n",
                             status);
                break;
            }
            else {
                Osal_printf ("MessageQ_put #%d Status [0x%x]\n", i, status);
            }

            status = MessageQ_get(MessageQApp_messageQ, &msg, MessageQ_FOREVER);
            if (status < 0) {
                Osal_printf ("Error in MessageQ_get\n");
                break;
            }
            else {
                Osal_printf ("MessageQ_get #%d Msg = 0x%x\n", i, msg);

                /* Validate the returned message. */
                if (msg != NULL) {
                    if (MessageQ_getMsgId (msg) != ((i % 16) + 1) ) {
                        Osal_printf ("Data integrity failure!\n"
                                     "    Expected %d\n"
                                     "    Received %d\n",
                                     ((i % 16) + 1),
                                     MessageQ_getMsgId (msg));
                        break;
                    }
                }

                status = MessageQ_free (msg);
                Osal_printf ("MessageQ_free status [0x%x]\n", status);
            }

            if ((i % 2) == 0) {
                Osal_printf ("Exchanged %d messages with remote processor\n",
                             i);
            }
        }
    }

    /* Keep the Ducati application running. */
#if !defined (SYSLINK_USE_DAEMON)
    /* Send die message */
    msg = MessageQ_alloc (HEAPID, MSGSIZE);
    if (msg == NULL) {
        Osal_printf ("MessageQ_alloc (die message) failed\n");
    }
    else {
        Osal_printf ("MessageQ_alloc (die message) msg = [0x%x]\n", msg);

        /* Send a die message */
        MessageQ_setMsgId(msg, DIEMESSAGE);

        /* Have the DSP reply to this message queue */
        MessageQ_setReplyQueue (MessageQApp_messageQ, msg);

        /* Send the message off */
        status = MessageQ_put (MessageQApp_queueId, msg);
        if (status < 0) {
            Osal_printf ("Error in MessageQ_put (die message) [0x%x]\n",
                         status);
        }
        else {
            Osal_printf ("MessageQ_put (die message) Status [0x%x]\n", status);
        }

        /* Wait for the final message. */
        status = MessageQ_get(MessageQApp_messageQ, &msg, MessageQ_FOREVER);
        if (status < 0) {
            Osal_printf ("\nError in MessageQ_get (die message)!\n");
        }
        else {
            /* Validate the returned message. */
            if (msg != NULL) {
                if (MessageQ_getMsgId (msg) == DIEMESSAGE) {
                    Osal_printf ("\nSuccessfully received die response from the "
                                 "remote processor\n");
                    Osal_printf ("Sample application successfully completed!\n");
                }
                else {
                    Osal_printf("\nUnsuccessful run of the sample "
                                "application!\n");
                }
            }
            else {
                Osal_printf("\nUnsuccessful run of the sample application msg "
                          "is NULL!\n");
            }
        }
        MessageQ_free(msg);
    }
#else
    Osal_printf ("Sample application successfully completed!\n");
#endif /* !SYSLINK_USE_DAEMON */

    /* Clean-up */
    if (MessageQApp_messageQ != NULL) {
        status = MessageQ_delete (&MessageQApp_messageQ);
        if (status < 0) {
            Osal_printf ("Error in MessageQ_delete [0x%x]\n",
                         status);
        }
        else {
            Osal_printf ("MessageQ_delete Status [0x%x]\n", status);
        }
    }

    if (MessageQApp_messageQ != NULL) {
        MessageQ_close (&MessageQApp_queueId);
    }

    Osal_printf ("Leaving MessageQApp_execute\n");

    return (status);
}


/*!
 *  @brief  Function to execute the shutdown for MessageQApp sample application
 *
 *  @sa
 */
Int
MessageQApp_shutdown (Void)
{
    Int32               status = 0;
#if !defined (SYSLINK_USE_DAEMON)
    ProcMgr_StopParams  stopParams;
#endif /* !SYSLINK_USE_DAEMON */
    IHeap_Handle        srHeap = NULL;

    Osal_printf ("Entered MessageQApp_shutdown()\n");

    status = MessageQ_unregisterHeap (HEAPID);
    Osal_printf ("MessageQ_unregisterHeap status: [0x%x]\n", status);

    if (MessageQApp_heapHandle) {
        status = HeapBufMP_delete (&MessageQApp_heapHandle);
        Osal_printf ("HeapBufMP_delete status: [0x%x]\n", status);
    }

    if (MessageQApp_ptr) {
        srHeap = SharedRegion_getHeap (0);
        Memory_free (srHeap, MessageQApp_ptr, MessageQApp_heapSize);
    }

#if !defined (SYSLINK_USE_DAEMON)
    stopParams.proc_id = MessageQApp_procId;
    status = ProcMgr_stop (MessageQApp_procMgrHandle, &stopParams);
    Osal_printf ("ProcMgr_stop status: [0x%x]\n", status);
#endif /* !SYSLINK_USE_DAEMON */

    status =  ProcMgr_detach (MessageQApp_procMgrHandle);
    Osal_printf ("ProcMgr_detach status [0x%x]\n", status);

    status = ProcMgr_close (&MessageQApp_procMgrHandle);
    Osal_printf ("ProcMgr_close status: [0x%x]\n", status);

    status = Ipc_destroy ();
    Osal_printf ("Ipc_destroy status: [0x%x]\n", status);

    Osal_printf ("Leave MessageQApp_shutdown()\n");

    return (status);
}
