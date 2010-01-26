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

/*============================================================================
 *  @file   Client.c
 *
 *  @brief  RCM Client Sample application for RCM module between MPU & Ducati
 *
 *  ============================================================================
 */

 /* OS-specific headers */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>


/* Standard headers */
#include <Std.h>

/* OSAL & Utils headers */
#include <OsalPrint.h>
#include <String.h>

/* IPC headers */
#if defined (SYSLINK_USE_SYSMGR)
#include <SysMgr.h>
#else /* if defined (SYSLINK_USE_SYSMGR) */
#include <UsrUtilsDrv.h>
#include <HeapBuf.h>
#include <MultiProc.h>
#include <GatePeterson.h>
#include <NameServer.h>
#include <SharedRegion.h>
#include <ListMPSharedMemory.h>
#include <HeapBuf.h>
#include <Notify.h>
#include <NotifyDriverShm.h>
#include <NameServerRemoteNotify.h>
#endif /* if defined(SYSLINK_USE_SYSMGR) */
#include <ProcMgr.h>

/* RCM headers */
#include <RcmClient.h>

/* Sample headers */
#include <Client.h>
#include <RcmTest_Config.h>

#if defined (__cplusplus)
extern "C" {
#endif /* defined (__cplusplus) */

/** ============================================================================
 *  Macros and types
 *  ============================================================================
 */
/*!
 *  @brief  Interrupt ID of physical interrupt handled by the Notify driver to
 *          receive events.
 */
#define BASE_DSP2ARM_INTID      26

/*!
 *  @brief  Interrupt ID of physical interrupt handled by the Notify driver to
 *          send events.
 */
#define BASE_ARM2DSP_INTID      55

typedef struct {
    Int a;
} RCM_Remote_FxnArgs;

UInt                            fxnDoubleIdx;
UInt                            fxnExitIdx;
RcmClient_Handle                rcmClientHandle;
NotifyDriverShm_Handle          notifyDrvHandle_client;
GatePeterson_Handle             gateHandle_client;
NameServerRemoteNotify_Handle   nsrnHandle_client;
HeapBuf_Handle                  heapHandle_client;
MessageQTransportShm_Handle     transportShmHandle_client;
ProcMgr_Handle                  procMgrHandle_client;
UInt16                          remoteId_client;

/*
 *  ======== GetSymbolIndex ========
 */
Int GetSymbolIndex (Void)
{
    Int status = 0;

    /* get remote function index */
    Osal_printf ("\nQuerying server for fxnDouble() function index \n");
    status = RcmClient_getSymbolIndex (rcmClientHandle, "fxnDouble",
                                            &fxnDoubleIdx);
    if (status < 0)
        Osal_printf ("Error getting fxnDouble symbol index [0x%x]\n", status);
    else
        Osal_printf ("fxnDouble() symbol index [0x%x]\n", fxnDoubleIdx);

    Osal_printf ("Querying server for fxnExit() function index \n");
    status = RcmClient_getSymbolIndex (rcmClientHandle, "fxnExit", &fxnExitIdx);
    if (status < 0)
        Osal_printf ("Error getting fxnExit symbol index [0x%x]\n", status);
    else
        Osal_printf ("fxnExit() symbol index [0x%x]\n", fxnExitIdx);

    return status;
}

/*
 *  ======== TestExec ========
 */
Int TestExec (Void)
{
    Int status = 0;
    Int loop;
    RcmClient_Message *rcmMsg = NULL;
    UInt rcmMsgSize;
    RCM_Remote_FxnArgs *fxnDoubleArgs;

    Osal_printf ("\nTesting exec API\n");

    /* work loop: exec() */
    for (loop = 1; loop <= LOOP_COUNT; loop++) {

        // allocate a remote command message
        rcmMsgSize = sizeof(RCM_Remote_FxnArgs);

        Osal_printf ("TestExec: calling RcmClient_alloc \n");
        rcmMsg = RcmClient_alloc (rcmClientHandle, rcmMsgSize);
        if (rcmMsg == NULL) {
            Osal_printf ("Error allocating RCM message\n");
            goto exit;
        }

        // fill in the remote command message
        rcmMsg->fxnIdx = fxnDoubleIdx;
        fxnDoubleArgs = (RCM_Remote_FxnArgs *)(&rcmMsg->data);
        fxnDoubleArgs->a = loop;

        // execute the remote command message
        Osal_printf ("TestExec: calling RcmClient_exec \n");
        status = RcmClient_exec (rcmClientHandle, rcmMsg);
        if (status < 0) {
            Osal_printf (" RcmClient_exec error. \n");
            goto exit;
        }

        fxnDoubleArgs = (RCM_Remote_FxnArgs *)(&(rcmMsg->data));
        Osal_printf ("exec(fxnDouble(%d)), result = %d\n",
            fxnDoubleArgs->a, rcmMsg->result);

        // return message to the heap
        Osal_printf ("TestExec: calling RcmClient_free \n");
        RcmClient_free (rcmClientHandle, rcmMsg);
    }

exit:
    Osal_printf ("Leaving TestExec()\n");
    return status;
}

/*
 *  ======== TestExecDpc ========
 */
Int TestExecDpc (Void)
{
    Int status = 0;
    Int loop;
    RcmClient_Message *rcmMsg = NULL;
    UInt rcmMsgSize;
    RCM_Remote_FxnArgs *fxnDoubleArgs;

    Osal_printf ("Testing execDpc API\n");

    /* work loop: RcmClient_execDpc () */
    for (loop = 1; loop <= LOOP_COUNT; loop++) {

        // allocate a remote command message
        rcmMsgSize = sizeof(RCM_Remote_FxnArgs);

        Osal_printf ("TestExecDpc: calling RcmClient_alloc \n");
        rcmMsg = RcmClient_alloc (rcmClientHandle, rcmMsgSize);
        if (rcmMsg == NULL) {
            Osal_printf ("Error allocating RCM message\n");
            goto exit;
        }

        // fill in the remote command message
        rcmMsg->fxnIdx = fxnDoubleIdx;
        fxnDoubleArgs = (RCM_Remote_FxnArgs *)(&rcmMsg->data);
        fxnDoubleArgs->a = loop;

        // execute the remote command message
        Osal_printf ("TestExecDpc: calling RcmClient_execDpc \n");
        status = RcmClient_execDpc (rcmClientHandle, rcmMsg);
        if (status < 0) {
            Osal_printf (" RcmClient_execDpc error. \n");
            goto exit;
        }

        fxnDoubleArgs = (RCM_Remote_FxnArgs *)(&(rcmMsg->data));
        Osal_printf ("exec(fxnDouble(%d)), result = %d",
            fxnDoubleArgs->a, rcmMsg->result);

        // return message to the heap
        Osal_printf ("TestExecDpc: calling RcmClient_free \n");
        RcmClient_free (rcmClientHandle, rcmMsg);
    }

exit:
    Osal_printf ("Leaving TestExecDpc()\n");
    return status;
}

/*
 *  ======== TestExecNoWait ========
 */
Int TestExecNoWait(void)
{
    Int                     status = 0;
    Int                     loop, job;
    unsigned short          msgIdAry[JOB_COUNT];
    RcmClient_Message *     rcmMsg = NULL;
    UInt                    rcmMsgSize;
    RCM_Remote_FxnArgs *    fxnDoubleArgs;

    Osal_printf ("\nTesting TestExecNoWait API\n");

    /* work loop: execNoWait(), exec() */
    for (loop = 1; loop <= LOOP_COUNT; loop++) {
        /*
         * issue process jobs
         */
        for (job = 1; job <= JOB_COUNT; job++) {
            // allocate a remote command message
            rcmMsgSize = sizeof(RCM_Remote_FxnArgs);
            Osal_printf ("TestExecNoWait: calling RcmClient_alloc \n");
            rcmMsg = RcmClient_alloc (rcmClientHandle, rcmMsgSize);
            if (rcmMsg == NULL) {
                Osal_printf ("Error allocating RCM message\n");
                goto exit;
            }
            // fill in the remote command message
            rcmMsg->fxnIdx = fxnDoubleIdx;
            fxnDoubleArgs = (RCM_Remote_FxnArgs *)(&rcmMsg->data);
            fxnDoubleArgs->a = job;

            // execute the remote command message
            Osal_printf ("TestExecNoWait: calling RcmClient_execNoWait \n");
            status = RcmClient_execNoWait (rcmClientHandle, rcmMsg,
                                            &msgIdAry[job-1]);
            if (status < 0) {
                Osal_printf (" RcmClient_execNoWait error. \n");
                goto exit;
            }
        }

        /*
         * reclaim process jobs
         */
        for (job = 1; job <= JOB_COUNT; job++) {
            Osal_printf ("TestExecNoWait: calling RcmClient_waitUntilDone \n");
            status = RcmClient_waitUntilDone (rcmClientHandle, msgIdAry[job-1],
                                                rcmMsg);
            if (status < 0) {
                   Osal_printf (" RcmClient_waitUntilDone error. \n");
                goto exit;
            }

            Osal_printf ("msgId: %d, result = %d", msgIdAry[job-1],
                            rcmMsg->result);

            // return message to the heap
            Osal_printf ("TestExecNoWait: calling RcmClient_free \n");
            RcmClient_free (rcmClientHandle, rcmMsg);
        }
    }

exit:
    Osal_printf ("Leaving TestExecNoWait()\n");
    return status;
}

/*
 *  ======== create_RcmClient ========
 */
Int CreateRcmClient(Int testCase)
{
    RcmClient_Config                cfgParams;
    RcmClient_Params                rcmClient_Params;
    Int                             count = 0;
    Int                             status = 0;
    Char *                          remoteServerName;
    Char *                          procName;
    UInt16                          procId;
    UInt32                          shAddrBase;
    UInt32                          shAddrBase1;
    UInt32                          curShAddr;
    UInt32                          nsrnEventNo;
    UInt32                          mqtEventNo;
#if !defined (SYSLINK_USE_DAEMON)
#if defined(SYSLINK_USE_LOADER) || defined(SYSLINK_USE_SYSMGR)
    UInt32                          entry_point = 0;
    ProcMgr_StartParams             start_params;
#endif
#if defined(SYSLINK_USE_LOADER)
    Char *                          sysm3_image_name;
    Char *                          appm3_image_name;
    Char                            uProcId;
    UInt32                          fileId;
#endif
#endif
#if defined(SYSLINK_USE_SYSMGR)
    SysMgr_Config                   config;
#else
    Notify_Config                   notifyConfig;
    NotifyDriverShm_Config          notifyDrvShmConfig;
    NameServerRemoteNotify_Config   nsrConfig;
    SharedRegion_Config             sharedRegConfig;
    ListMPSharedMemory_Config       listMPSharedConfig;
    GatePeterson_Config             gpConfig;
    HeapBuf_Config                  heapbufConfig;
    MessageQTransportShm_Config     msgqTransportConfig;
    MessageQ_Config                 messageqConfig;
    MultiProc_Config                 multiProcConfig;
    NotifyDriverShm_Params          notifyShmParams;
    GatePeterson_Params             gateParams;
    NameServerRemoteNotify_Params   nsrParams;
    HeapBuf_Params                  heapbufParams;
    MessageQTransportShm_Params     msgqTransportParams;
#endif /* if defined(SYSLINK_USE_SYSMGR) */

    Osal_printf ("\nEntering CreateRcmClient()\n");

    rcmClientHandle = NULL;

    switch(testCase) {
    case 0:
        Osal_printf ("Local RCM test\n");
        remoteServerName = RCMSERVER_NAME;
        procName = MPU_PROC_NAME;
        break;
    case 1:
        Osal_printf ("RCM test with RCM server on Sys M3\n");
        remoteServerName = SYSM3_SERVER_NAME;
        procName = SYSM3_PROC_NAME;
        nsrnEventNo = NSRN_NOTIFY_EVENTNO;
        mqtEventNo = TRANSPORT_NOTIFY_EVENTNO;
        break;
    case 2:
        Osal_printf ("RCM test with RCM server on App M3\n");
        remoteServerName = APPM3_SERVER_NAME;
        procName = APPM3_PROC_NAME;
        nsrnEventNo = NSRN_NOTIFY_EVENTNO1;
        mqtEventNo = TRANSPORT_NOTIFY_EVENTNO1;
        break;
    default:
        Osal_printf ("Please pass valid arg (0-local, 1-Sys M3, 2-App M3) \n");
        goto exit;
        break;
    }

    /* size (in bytes) of RCM header including the messageQ header */
    /* RcmClient_Message member data[1] is the start of the payload */
    Osal_printf ("Size of RCM header in bytes = %d \n",
                            RcmClient_getHeaderSize());

#if defined(SYSLINK_USE_SYSMGR)
    SysMgr_getConfig (&config);
    status = SysMgr_setup (&config);
    if (status < 0) {
        Osal_printf ("Error in SysMgr_setup [0x%x]\n", status);
        goto exit;
    }
    else
        Osal_printf("SysMgr_setup status [0x%x]\n", status);
#else /* if defined(SYSLINK_USE_SYSMGR) */
    UsrUtilsDrv_setup ();

    /* Get and set GPP MultiProc ID by name. */
    multiProcConfig.maxProcessors = 4;
    multiProcConfig.id = 0;
    String_cpy (multiProcConfig.nameList [0], "MPU");
    String_cpy (multiProcConfig.nameList [1], "Tesla");
    String_cpy (multiProcConfig.nameList [2], "SysM3");
    String_cpy (multiProcConfig.nameList [3], "AppM3");
    status = MultiProc_setup (&multiProcConfig);
    if (status < 0) {
        Osal_printf ("Error in MultiProc_setup [0x%x]\n", status);
        goto exit;
    }

    /* NameServer and NameServerRemoteNotify module setup */
    status = NameServer_setup ();
    if (status < 0) {
        Osal_printf ("Error in NameServer_setup [0x%x]\n", status);
        goto exit;
    }
    Osal_printf ("NameServer_setup Status [0x%x]\n", status);

    NameServerRemoteNotify_getConfig (&nsrConfig);
    status = NameServerRemoteNotify_setup (&nsrConfig);
    if (status < 0) {
        Osal_printf ("Error in NameServerRemoteNotify_setup [0x%x]\n",
                        status);
        goto exit;
    }
    Osal_printf ("NameServerRemoteNotify_setup Status [0x%x]\n",
                     status);

    /*
     *  Need to define the shared region. The IPC modules use this
     *  to make portable pointers. All processors need to add this
     *  same call with their base address of the shared memory region.
     *  If the processor cannot access the memory, do not add it.
     */

    /* SharedRegion module setup */
    sharedRegConfig.gateHandle = NULL;
    sharedRegConfig.heapHandle = NULL;
    sharedRegConfig.maxRegions = 4;
    status = SharedRegion_setup (&sharedRegConfig);
    if (status < 0) {
        Osal_printf ("Error in SharedRegion_setup. Status [0x%x]\n",
                     status);
        goto exit;
    }
    Osal_printf ("SharedRegion_setup Status [0x%x]\n", status);

    /* ListMPSharedMemory module setup */
    ListMPSharedMemory_getConfig (&listMPSharedConfig);
    status = ListMPSharedMemory_setup (&listMPSharedConfig);
    if (status < 0) {
        Osal_printf ("Error in ListMPSharedMemory_setup."
                       " Status [0x%x]\n", status);
        goto exit;
    }
    Osal_printf ("ListMPSharedMemory_setup Status [0x%x]\n", status);

    /* HeapBuf module setup */
    HeapBuf_getConfig (&heapbufConfig);
    status = HeapBuf_setup (&heapbufConfig);
    if (status < 0) {
    Osal_printf ("Error in HeapBuf_setup. Status [0x%x]\n",
         status);
         goto exit;
    }
    Osal_printf ("HeapBuf_setup Status [0x%x]\n", status);

    /* GatePeterson module setup */
    GatePeterson_getConfig (&gpConfig);
    status = GatePeterson_setup (&gpConfig);
    if (status < 0) {
        Osal_printf ("Error in GatePeterson_setup. Status [0x%x]\n",
                        status);
        goto exit;
    }
    Osal_printf ("GatePeterson_setup Status [0x%x]\n", status);

    /* Setup Notify module and NotifyDriverShm module */
    Notify_getConfig (&notifyConfig);
    notifyConfig.maxDrivers = 2;
    status = Notify_setup (&notifyConfig);
    if (status < 0) {
        Osal_printf ("Error in Notify_setup [0x%x]\n", status);
        goto exit;
    }
    Osal_printf ("Notify_setup Status [0x%x]\n", status);

    NotifyDriverShm_getConfig (&notifyDrvShmConfig);
    status = NotifyDriverShm_setup (&notifyDrvShmConfig);
    if (status < 0) {
        Osal_printf ("Error in NotifyDriverShm_setup [0x%x]\n",
                       status);
        goto exit;
    }
    Osal_printf ("NotifyDriverShm_setup Status [0x%x]\n", status);

    /* Setup MessageQ module and MessageQTransportShm module */
    MessageQ_getConfig (&messageqConfig);
    status = MessageQ_setup (&messageqConfig);
    if (status < 0) {
        Osal_printf ("Error in MessageQ_setup [0x%x]\n", status);
        goto exit;
    }
    Osal_printf ("MessageQ_setup Status [0x%x]\n", status);

    MessageQTransportShm_getConfig (&msgqTransportConfig);
    status = MessageQTransportShm_setup (&msgqTransportConfig);
    if (status < 0) {
        Osal_printf ("Error in MessageQTransportShm_setup [0x%x]\n",
                        status);
        goto exit;
    }
    Osal_printf ("MessageQTransportShm_setup Status [0x%x]\n",
                      status);

#endif /* if !defined(SYSLINK_USE_SYSMGR) */

    /* Open a handle to the ProcMgr instance. */
    procId = MultiProc_getId (SYSM3_PROC_NAME);
    /* Open a handle to the ProcMgr instance. */
    status = ProcMgr_open (&procMgrHandle_client, procId);
        if (status < 0) {
        Osal_printf ("Error in ProcMgr_open [0x%x]\n", status);
        goto exit;
    }

    Osal_printf ("ProcMgr_open Status [0x%x]\n", status);
    /* Get the address of the shared region in kernel space. */
    status = ProcMgr_translateAddr (procMgrHandle_client,
                                    (Ptr) &shAddrBase,
                                    ProcMgr_AddrType_MasterUsrVirt,
                                    (Ptr) SHAREDMEM,
                                    ProcMgr_AddrType_SlaveVirt);
    if (status < 0) {
        Osal_printf ("Error in ProcMgr_translateAddr [0x%x]\n",
                     status);
        goto exit;
    }
    Osal_printf ("Virt address of shared address base #1:"
                     " [0x%x]\n", shAddrBase);

    /* Get the address of the shared region in kernel space. */
    status = ProcMgr_translateAddr (procMgrHandle_client,
                                    (Ptr) &shAddrBase1,
                                    ProcMgr_AddrType_MasterUsrVirt,
                                    (Ptr) SHAREDMEM1,
                                    ProcMgr_AddrType_SlaveVirt);
    if (status < 0) {
        Osal_printf ("Error in ProcMgr_translateAddr [0x%x]\n", status);
        goto exit;
    }
    Osal_printf ("Virt address of shared address base #2:"
                            " [0x%x]\n", shAddrBase1);

    curShAddr = shAddrBase;
    /* Add the region to SharedRegion module. */
    status = SharedRegion_add (0,
                               (Ptr) curShAddr,
                               SHAREDMEMSIZE);
    if (status < 0) {
        Osal_printf ("Error in SharedRegion_add [0x%x]\n", status);
        goto exit;
    }
    Osal_printf ("SharedRegion_add [0x%x]\n", status);

    /* Add the region to SharedRegion module. */
    status = SharedRegion_add (1,
                               (Ptr) shAddrBase1,
                               SHAREDMEMSIZE1);
    if (status < 0) {
        Osal_printf ("Error in SharedRegion_add1 [0x%x]\n", status);
        goto exit;
    }
    Osal_printf ("SharedRegion_add1 [0x%x]\n", status);

#if !defined(SYSLINK_USE_SYSMGR)
    /* Create instance of NotifyDriverShm */
    NotifyDriverShm_Params_init (NULL, &notifyShmParams);
    //notifyShmParams.sharedAddr= (UInt32)curShAddr;
    //notifyShmParams.sharedAddrSize = 0x4000;
    /* NotifyDriverShm_sharedMemReq (&notifyShmParams); */
    notifyShmParams.numEvents          = 32;
    notifyShmParams.numReservedEvents  = 0;
    notifyShmParams.sendEventPollCount = (UInt32) -1;
    notifyShmParams.recvIntId          = BASE_DSP2ARM_INTID;
    notifyShmParams.sendIntId          = BASE_ARM2DSP_INTID;
    notifyShmParams.remoteProcId       = procId;

    if (testCase == 1) {
        /* Increment the offset for the next allocation for MPU-SysM3 */
        curShAddr += NOTIFYMEMSIZE;
    }
    else if (testCase == 2) {
        /* Reset the address for the next allocation for MPU-AppM3 */
        curShAddr = shAddrBase1;
    }

    /* Create instance of NotifyDriverShm */
    notifyDrvHandle_client = NotifyDriverShm_create (
                                             "NOTIFYDRIVER_DUCATI",
                                             &notifyShmParams);
    Osal_printf ("NotifyDriverShm_create Handle: [0x%x]\n",
                 notifyDrvHandle_client);
    if (notifyDrvHandle_client == NULL) {
        Osal_printf ("Error in NotifyDriverShm_create\n");
        goto exit;
    }

#endif

    remoteId_client = MultiProc_getId (procName);
    Osal_printf("remoteId_client = %d\n", remoteId_client);

#if !defined(SYSLINK_USE_DAEMON)
#if defined(SYSLINK_USE_LOADER)
    if (testCase == 1)
        sysm3_image_name = "./RCMServer_MPUSYS_Test_Core0.xem3";
    else if (testCase == 2)
        sysm3_image_name = "./Notify_MPUSYS_reroute_Test_Core0.xem3";
    uProcId = remoteId_client;
    Osal_printf ("SYSM3 Load: loading the SYSM3 image %s\n", sysm3_image_name);
    Osal_printf ("SYSM3 Load: uProcId = %d\n", uProcId);
    status = ProcMgr_load (procMgrHandle_client, sysm3_image_name, 2,
                          &sysm3_image_name, &entry_point, &fileId, PROC_SYSM3);
    if (status < 0) {
        Osal_printf ("Error in ProcMgr_load SysM3 image [0x%x]\n", status);
        goto exit;
    }
    Osal_printf ("ProcMgr_load SysM3 image Status [0x%x]\n", status);
#endif
#if defined(SYSLINK_USE_SYSMGR) || defined(SYSLINK_USE_LOADER)
    start_params.proc_id = PROC_SYSM3;
    Osal_printf ("SYSM3 Load: start_params.proc_id = %d\n",
                start_params.proc_id);
    status = ProcMgr_start (procMgrHandle_client, entry_point, &start_params);
    if (status < 0) {
        Osal_printf ("Error in ProcMgr_start SysM3 [0x%x]\n", status);
        goto exit;
    }
    Osal_printf ("ProcMgr_start SysM3 Status [0x%x]\n", status);
#endif

    if(remoteId_client == PROC_APPM3) {
#if defined(SYSLINK_USE_LOADER)
        appm3_image_name = "./RCMServer_MPUAPP_Test_Core1.xem3";
        uProcId = remoteId_client;
        Osal_printf ("APPM3 Load: loading the APPM3 image %s\n",
                    appm3_image_name);
        Osal_printf ("APPM3 Load: uProcId = %d\n", uProcId);
        status = ProcMgr_load (procMgrHandle_client, appm3_image_name, 2,
                                &appm3_image_name, &entry_point, &fileId,
                                uProcId);
        if (status < 0) {
            Osal_printf ("Error in ProcMgr_load AppM3 image [0x%x]\n", status);
            goto exit;
        }
        Osal_printf ("ProcMgr_load AppM3 image Status [0x%x]\n", status);
#endif
#if defined(SYSLINK_USE_SYSMGR) || defined(SYSLINK_USE_LOADER)
        start_params.proc_id = PROC_APPM3;
        Osal_printf("APPM3 Load: start_params.proc_id = %d\n",
                    start_params.proc_id);
        status = ProcMgr_start (procMgrHandle_client, entry_point,
                                &start_params);
        if (status < 0) {
            Osal_printf ("Error in ProcMgr_start AppM3 [0x%x]\n", status);
            goto exit;
        }
        Osal_printf ("ProcMgr_start AppM3 Status [0x%x]\n", status);
#endif
    }
#endif /* !SYSLINK_USE_DAEMON */

#if !defined(SYSLINK_USE_SYSMGR)
    Osal_printf ("\nPlease break the platform and load the Ducati image now."
                    "Press any key to continue ...\n");
    getchar ();

    Osal_printf ("Opening the Gate\n");

    GatePeterson_Params_init (gateHandle_client, &gateParams);
    gateParams.sharedAddrSize = GatePeterson_sharedMemReq (&gateParams);
    Osal_printf ("Memory required for GatePeterson instance [0x%x]"
                 " bytes \n",
                 gateParams.sharedAddrSize);

    do {
        gateParams.sharedAddr     = (Ptr)(curShAddr);
        status = GatePeterson_open (&gateHandle_client,
                                    &gateParams);
    }
    while ((status == GATEPETERSON_E_NOTFOUND) ||
            (status == GATEPETERSON_E_VERSION));

    if (status < 0) {
        Osal_printf ("Error in GatePeterson_open [0x%x]\n", status);
        goto exit;
    }
    Osal_printf ("GatePeterson_open Status [0x%x]\n", status);

    /* Increment the offset for the next allocation */
    curShAddr += GATEPETERSONMEMSIZE;

    /* Create the heap. */
    HeapBuf_Params_init(NULL, &heapbufParams);
    if (testCase == 1)
        heapbufParams.name       = HEAPNAME_SYSM3;
    else if (testCase == 2)
        heapbufParams.name       = HEAPNAME_APPM3;
    heapbufParams.sharedAddr     = (Ptr)(curShAddr);
    heapbufParams.align          = 128;
    heapbufParams.numBlocks      = 4;
    heapbufParams.blockSize      = MSGSIZE;
    heapbufParams.gate           = (Gate_Handle) gateHandle_client;
    heapbufParams.sharedAddrSize = HeapBuf_sharedMemReq (&heapbufParams,
                    &heapbufParams.sharedBufSize);
    heapbufParams.sharedBuf      = (Ptr)(curShAddr + \
                                        heapbufParams.sharedAddrSize);
    heapHandle_client = HeapBuf_create (&heapbufParams);
    if (heapHandle_client == NULL) {
        Osal_printf ("Error in HeapBuf_create\n");
        goto exit;
    }
    Osal_printf ("HeapBuf_create Handle [0x%x]\n", heapHandle_client);

    /* Register this heap with MessageQ */
    if (testCase == 1)
        MessageQ_registerHeap (heapHandle_client, HEAPID_SYSM3);
    else if (testCase == 2)
        MessageQ_registerHeap (heapHandle_client, HEAPID_APPM3);
    }

    /* Increment the offset for the next allocation */
    curShAddr += HEAPBUFMEMSIZE;
    /*
     *  Create the NameServerRemote implementation that is used to
     *  communicate with the remote processor. It uses some shared
     *  memory and the Notify module.
     *
     *  Note that this implementation uses Notify to communicate, so
     *  interrupts need to be enabled. On BIOS, that does not occur
     *  until after main returns.
     */
    NameServerRemoteNotify_Params_init(NULL, &nsrParams);
    nsrParams.notifyDriver   = notifyDrvHandle_client;
    nsrParams.notifyEventNo  = nsrnEventNo;
    nsrParams.sharedAddr     = (Ptr)curShAddr;
    nsrParams.gate           = (Ptr)gateHandle_client;
    nsrParams.sharedAddrSize = NSRN_MEMSIZE;
    nsrnHandle_client = NameServerRemoteNotify_create (remoteId_client,
                                                        &nsrParams);
    if (nsrnHandle_client == NULL) {
        Osal_printf ("Error in NotifyDriverShm_create\n");
        goto exit;
    }
    Osal_printf ("NameServerRemoteNotify_create handle [0x%x]\n",
                         nsrnHandle_client);

    /* Increment the offset for the next allocation */
    curShAddr += NSRN_MEMSIZE;

    MessageQTransportShm_Params_init (NULL, &msgqTransportParams);

    msgqTransportParams.sharedAddr     = (Ptr)curShAddr;
    msgqTransportParams.gate           = (Gate_Handle) gateHandle_client;
    msgqTransportParams.notifyEventNo  = mqtEventNo;
    msgqTransportParams.notifyDriver   = notifyDrvHandle_client;
    msgqTransportParams.sharedAddrSize =
         MessageQTransportShm_sharedMemReq (&msgqTransportParams);
    transportShmHandle_client = MessageQTransportShm_create (remoteId_client,
                                               &msgqTransportParams);
    if (transportShmHandle_client == NULL) {
        Osal_printf ("Error in MessageQTransportShm_create\n");
        goto exit;
    }
    Osal_printf ("MessageQTransportShm_create handle [0x%x]\n",
                         transportShmHandle_client);
#endif

    /* Get default config for rcm client module */
    Osal_printf ("Get default config for rcm client module.\n");
    status = RcmClient_getConfig(&cfgParams);
    if (status < 0) {
        Osal_printf ("Error in RCM Client module get config \n");
        goto exit;
    }
    Osal_printf ("RCM Client module get config passed \n");

    cfgParams.defaultHeapBlockSize = MSGSIZE;

    /* rcm client module setup*/
    Osal_printf ("RCM Client module setup.\n");
    status = RcmClient_setup (&cfgParams);
    if (status < 0) {
        Osal_printf ("Error in RCM Client module setup \n");
        goto exit;
    }
    Osal_printf ("RCM Client module setup passed \n");

    /* rcm client module params init*/
    Osal_printf ("RCM Client module params init.\n");
    status = RcmClient_Params_init(NULL, &rcmClient_Params);
    if (status < 0) {
        Osal_printf ("Error in RCM Client instance params init \n");
        goto exit;
    }
    Osal_printf ("RCM Client instance params init passed \n");

    /* create an rcm client instance */
    Osal_printf ("Creating RcmClient instance \n");
    rcmClient_Params.callbackNotification = 0; /* disable asynchronous exec */
    if (testCase == 1)
        rcmClient_Params.heapId = HEAPID_SYSM3;
    else if (testCase == 2)
        rcmClient_Params.heapId = HEAPID_APPM3;

    while ((rcmClientHandle == NULL) && (count++ < MAX_CREATE_ATTEMPTS)) {
        status = RcmClient_create (remoteServerName, &rcmClient_Params,
                                    &rcmClientHandle);
        if (status < 0) {
            if (status == RCMCLIENT_ESERVER) {
                Osal_printf ("Unable to open remote server %d time\n", count);
            }
            else {
                Osal_printf ("Error in RCM Client create \n");
                goto exit;
            }
        }
        Osal_printf ("RCM Client create passed \n");
    }
    if (MAX_CREATE_ATTEMPTS <= count)
        Osal_printf ("Timeout... could not connect with remote server\n");

exit:
    Osal_printf ("Leaving CreateRcmClient() \n");
    return status;
}

/*
 *  ======== RcmServerCleanup ========
 */
Int RcmClientCleanup (Int testCase)
{
    Int                     status = 0;
    RcmClient_Message *     rcmMsg = NULL;
    UInt                    rcmMsgSize;
    RCM_Remote_FxnArgs *    fxnExitArgs;
#if defined (SYSLINK_USE_SYSMGR)
#if !defined(SYSLINK_USE_DAEMON)
    ProcMgr_StopParams      stop_params;
#endif
#endif

    Osal_printf ("\nEntering RcmClientCleanup()\n");

    // send terminate message
    // allocate a remote command message
    rcmMsgSize = sizeof(RCM_Remote_FxnArgs);
    rcmMsg = RcmClient_alloc (rcmClientHandle, rcmMsgSize);
    if (rcmMsg == NULL) {
        Osal_printf ("RcmClientCleanup: Error allocating RCM message\n");
        goto exit;
    }

    // fill in the remote command message
    rcmMsg->fxnIdx = fxnExitIdx;
    fxnExitArgs = (RCM_Remote_FxnArgs *)(&rcmMsg->data);
    fxnExitArgs->a = 0xFFFF;

#ifndef SYSLINK_USE_DAEMON
    // execute the remote command message
    Osal_printf ("RcmClientCleanup: calling RcmClient_execDpc \n");
    status = RcmClient_execDpc (rcmClientHandle, rcmMsg);
    if (status < 0) {
        Osal_printf ("RcmClientCleanup: RcmClient_execDpc error [0x%x]\n"
                            , status);
        goto exit;
    }
#endif /* !SYSLINK_USE_DAEMON */

    // return message to the heap
    Osal_printf ("RcmClientCleanup: calling RcmClient_free \n");
    RcmClient_free (rcmClientHandle, rcmMsg);

    /* delete the rcm client */
    Osal_printf ("Delete RCM client instance \n");
    status = RcmClient_delete (&rcmClientHandle);
    if (status < 0)
        Osal_printf ("Error in RCM Client instance delete [0x%x]\n"
                            , status);
    else
        Osal_printf ("RcmClient_delete status: [0x%x]\n", status);

    /* rcm client module destroy*/
    Osal_printf ("RcmClientCleanup: Destroy RCM client module \n");
    status = RcmClient_destroy ();
    if (status < 0)
        Osal_printf ("Error in RCM Client module destroy [0x%x]\n"
                            , status);
    else
        Osal_printf ("RcmClient_destroy status: [0x%x]\n", status);

    /* Finalize modules */
#if defined (SYSLINK_USE_SYSMGR)
    SharedRegion_remove (0);
    SharedRegion_remove (1);

#if !defined(SYSLINK_USE_DAEMON)    // Do not call ProcMgr_stop if using daemon
    stop_params.proc_id = remoteId_client;
    status = ProcMgr_stop(procMgrHandle_client, &stop_params);
    if (status < 0)
        Osal_printf("Error in ProcMgr_stop [0x%x]\n", status);
    else
        Osal_printf("ProcMgr_stop status: [0x%x]\n", status);

    if (testCase == 2) {
        stop_params.proc_id = PROC_SYSM3;
        status = ProcMgr_stop(procMgrHandle_client, &stop_params);
        if (status < 0)
            Osal_printf("Error in ProcMgr_stop [0x%x]\n", status);
        else
            Osal_printf("ProcMgr_stop status: [0x%x]\n", status);
    }
#endif

    status = ProcMgr_close (&procMgrHandle_client);
    if (status < 0)
        Osal_printf ("Error in ProcMgr_close [0x%x]\n", status);
    else
        Osal_printf ("ProcMgr_close status: [0x%x]\n", status);

    status = SysMgr_destroy ();
    if (status < 0)
        Osal_printf("Error in SysMgr_destroy [0x%x]\n", status);
    else
        Osal_printf("SysMgr_destroy status: [0x%x]\n", status);
#else /* if defined (SYSLINK_USE_SYSMGR) */

    status = MessageQTransportShm_delete (&transportShmHandle_client);
    if (status < 0)
        Osal_printf ("Error in MessageQTransportShm_delete [0x%x]\n"
                            , status);
    else
        Osal_printf ("MessageQTransportShm_delete status: [0x%x]\n"
                            , status);

    status = NameServerRemoteNotify_delete (&nsrnHandle_client);
    if (status < 0)
        Osal_printf ("Error in NameServerRemoteNotify_delete [0x%x]\n"
                            , status);
    else
        Osal_printf ("NameServerRemoteNotify_delete status: [0x%x]\n"
                            , status);

    if (testCase == 1)
        status = MessageQ_unregisterHeap (HEAPID_SYSM3);
    else if (testCase == 2)
        status = MessageQ_unregisterHeap (HEAPID_APPM3);
    if (status < 0)
        Osal_printf ("Error in MessageQ_unregisterHeap [0x%x]\n"
                            , status);
    else
        Osal_printf ("MessageQ_unregisterHeap status: [0x%x]\n"
                            , status);

    status = HeapBuf_delete (&heapHandle_client);
    if (status < 0)
        Osal_printf ("Error in HeapBuf_delete [0x%x]\n", status);
    else
        Osal_printf ("HeapBuf_delete status: [0x%x]\n", status);

    status = NotifyDriverShm_delete (&notifyDrvHandle_client);
    if (status < 0)
        Osal_printf ("Error in NotifyDriverShm_delete [0x%x]\n", status);
    else
        Osal_printf ("NotifyDriverShm_delete status: [0x%x]\n", status);

    SharedRegion_remove (0);
    SharedRegion_remove (1);

    status = GatePeterson_close (&gateHandle_client);
    if (status < 0)
        Osal_printf ("Error in GatePeterson_close [0x%x]\n", status);
    else
        Osal_printf ("GatePeterson_close status: [0x%x]\n", status);

    status = ProcMgr_close (&procMgrHandle_client);
    if (status < 0)
        Osal_printf ("Error in ProcMgr_close [0x%x]\n", status);
    else
        Osal_printf ("ProcMgr_close status: [0x%x]\n", status);

    status = MessageQTransportShm_destroy ();
    if (status < 0)
        Osal_printf ("Error in MessageQTransportShm_destroy [0x%x]\n"
                            , status);
    else
        Osal_printf ("MessageQTransportShm_destroy status: [0x%x]\n"
                            , status);

    status = MessageQ_destroy ();
    if (status < 0)
        Osal_printf ("Error in MessageQ_destroy [0x%x]\n", status);
    else
        Osal_printf ("MessageQ_destroy status: [0x%x]\n", status);

    status = NotifyDriverShm_destroy ();
    if (status < 0)
        Osal_printf ("Error in NotifyDriverShm_destroy [0x%x]\n", status);
    else
        Osal_printf ("NotifyDriverShm_destroy status: [0x%x]\n", status);

    status = Notify_destroy ();
    if (status < 0)
        Osal_printf ("Error in Notify_destroy [0x%x]\n", status);
    else
        Osal_printf ("Notify_destroy status: [0x%x]\n", status);

    status = HeapBuf_destroy ();
    if (status < 0)
        Osal_printf ("Error in HeapBuf_destroy [0x%x]\n", status);
    else
        Osal_printf ("HeapBuf_destroy status: [0x%x]\n", status);

    status = ListMPSharedMemory_destroy();
    if (status < 0)
        Osal_printf ("Error in ListMPSharedMemory_destroy [0x%x]\n"
                            , status);
    else
        Osal_printf ("ListMPSharedMemory_destroy status: [0x%x]\n"
                            , status);

    status = GatePeterson_destroy ();
    if (status < 0)
        Osal_printf ("Error in GatePeterson_destroy [0x%x]\n", status);
    else
        Osal_printf ("GatePeterson_destroy status: [0x%x]\n", status);

    status = SharedRegion_destroy ();
    if (status < 0)
        Osal_printf ("Error in SharedRegion_destroy [0x%x]\n", status);
    else
        Osal_printf ("SharedRegion_destroy status: [0x%x]\n", status);

    status = NameServerRemoteNotify_destroy ();
    if (status < 0)
        Osal_printf ("Error in NameServerRemoteNotify_destroy [0x%x]\n"
                            , status);
    else
        Osal_printf ("NameServerRemoteNotify_destroy status: [0x%x]\n"
                            , status);

    status = NameServer_destroy ();
    if (status < 0)
        Osal_printf ("Error in NameServer_destroy [0x%x]\n", status);
    else
        Osal_printf ("NameServer_destroy status: [0x%x]\n", status);

    status = MultiProc_destroy ();
    if (status < 0)
        Osal_printf ("Error in Multiproc_destroy [0x%x]\n", status);
    else
        Osal_printf ("Multiproc_destroy status: [0x%x]\n", status);

    UsrUtilsDrv_destroy ();
#endif /* if !defined(SYSLINK_USE_SYSMGR) */

exit:
    Osal_printf ("Leaving RcmClientCleanup()\n");
    return status;
}

/*
 *  ======== MpuRcmClientTest ========
 */
Int MpuRcmClientTest(Int testCase)
{
    Int status = 0;

    Osal_printf ("Testing RCM client on MPU\n");

    status = CreateRcmClient (testCase);
    if (status < 0) {
        Osal_printf ("Error in creating RcmClient \n");
        goto exit;
    }

    status = GetSymbolIndex ();
    if (status < 0) {
        Osal_printf ("Error in GetSymbolIndex \n");
        goto exit;
    }

    status = TestExec ();
    if (status < 0) {
        Osal_printf ("Error in TestExec \n");
        goto exit;
    }

    /* status = TestExecDpc ();
    if (status < 0) {
        Osal_printf ("Error in TestExecDpc \n");
        goto exit;
    }

    status = TestExecNoWait ();
    if (status < 0) {
        Osal_printf ("Error in TestExecNoWait \n");
        goto exit;
    } */

    status = RcmClientCleanup (testCase);
    if (status < 0)
        Osal_printf ("Error in RcmClientCleanup \n");


exit:
    Osal_printf ("Leaving MpuRcmClientTest()\n");
    return status;
}

#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */
