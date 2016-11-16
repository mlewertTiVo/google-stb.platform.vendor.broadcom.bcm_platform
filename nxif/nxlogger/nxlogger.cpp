/***************************************************************************
 *     (c)2011-2015 Broadcom Corporation
 *
 *  This program is the proprietary software of Broadcom Corporation and/or its licensors,
 *  and may only be used, duplicated, modified or distributed pursuant to the terms and
 *  conditions of a separate, written license agreement executed between you and Broadcom
 *  (an "Authorized License").  Except as set forth in an Authorized License, Broadcom grants
 *  no license (express or implied), right to use, or waiver of any kind with respect to the
 *  Software, and Broadcom expressly reserves all rights in and to the Software and all
 *  intellectual property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
 *  HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
 *  NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
 *
 *  Except as expressly set forth in the Authorized License,
 *
 *  1.     This program, including its structure, sequence and organization, constitutes the valuable trade
 *  secrets of Broadcom, and you shall use all reasonable efforts to protect the confidentiality thereof,
 *  and to use this information only in connection with your use of Broadcom integrated circuit products.
 *
 *  2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 *  AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
 *  WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO
 *  THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED WARRANTIES
 *  OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE,
 *  LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION
 *  OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF
 *  USE OR PERFORMANCE OF THE SOFTWARE.
 *
 *  3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS
 *  LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT, OR
 *  EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO YOUR
 *  USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN ADVISED OF
 *  THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT
 *  ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE
 *  LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF
 *  ANY LIMITED REMEDY.
 *
 **************************************************************************/
#include "bstd.h"
#include "bkni.h"
#include "bdbg_fifo.h"
#include "bdbg_log.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include "nexus_driver_ioctl.h"

#define LOG_TAG "NEXUS"
#include <cutils/log.h>
#include <ctype.h>
#include <sched.h>
#include <cutils/properties.h>

BDBG_MODULE(logger);

#define PROP_LOGGER_PRIORITY "sys.nx.logger_priority"

#define PRIORITY_REFRESH_INTERVAL 300

static bool sigusr1_fired=false;

static void sigusr1_handler(int unused)
{
    BSTD_UNUSED(unused);
    sigusr1_fired = true;
    return;
}

static void usage(const char *name)
{
    ALOGE("Usage:\n%s log_file [device_node]\n", name);
    exit(0);
}


static BDBG_Level get_log_message_level(char *pMsgBuf, size_t msgLen)
{
    BDBG_Level level = BDBG_P_eUnknown;
    char ch;

    /* We're going to look at the first three characters, so make
     * sure we have that many.  */
    if (msgLen < 3) {
        return BDBG_P_eUnknown;
    }

    /* Now make sure the first three characters are all same. */
    ch = *pMsgBuf++;
    if (ch != *pMsgBuf++  || ch != *pMsgBuf) {
        return BDBG_P_eUnknown;
    }

    /* Now just convert the 3-char prefix into a BDBG_Level, just
     * the reverse of what is done by the gDbgPrefix[] array in
     * bdbg.c.  */
    switch (ch) {
        case '.':   level = BDBG_eTrace;    /* ... */   break;
        case '-':   level = BDBG_eMsg;      /* --- */   break;
        case '*':   level = BDBG_eWrn;      /* *** */   break;
        case '#':                           /* ### */
        case '!':   level = BDBG_eErr;      /* !!! */   break;
        case ' ':   level = BDBG_eLog;      /*     */   break;
        default:    level = BDBG_P_eUnknown;            break;
    }
    return level;
}

static BERR_Code get_usermode_log_message(BDBG_FifoReader_Handle logReader, unsigned *pTimeout, char *pBuf, size_t bufLen, size_t *pMsgLen)
{
    BERR_Code rc;

    rc = BDBG_Log_Dequeue(logReader, pTimeout, pBuf, bufLen, pMsgLen);
    if(rc!=BERR_SUCCESS) {rc=BERR_TRACE(rc);return rc;}

    return BERR_SUCCESS;
}


static BERR_Code get_driver_log_message(int device_fd, PROXY_NEXUS_Log_Instance *pProxyInstance, unsigned *pTimeout, char *pBuf, size_t bufLen, size_t *pMsgLen)
{
    int         urc;
    PROXY_NEXUS_Log_Dequeue dequeue;

    dequeue.instance    = *pProxyInstance;
    dequeue.buffer_size = bufLen-1;
    dequeue.buffer      = (uint64_t)(intptr_t)pBuf;
    dequeue.timeout     = 0;
    urc = ioctl(device_fd, IOCTL_PROXY_NEXUS_Log_Dequeue, &dequeue);
    if(urc!=0) {
        BDBG_MSG(("Can't read data from the driver"));
        *pMsgLen = 0;
        BERR_TRACE(urc);
        return BERR_UNKNOWN;
    }

    *pTimeout = dequeue.timeout;
    *pMsgLen  = dequeue.buffer_size;

/*  if(*pMsgLen) {                                          */
/*      BDBG_ERR(("%u %u", *pMsgLen, strlen(pBuf)));        */
/*  }                                                       */

    return BERR_SUCCESS;
}


#define MAX_PREFIX_LEN    16      /* Reserve this many bytes in front of message. */
#define MAX_MSG_LEN      256      /* Reserve this many bytes for message.         */
#define MAX_SUFFIX_LEN     8      /* Reserve this many bytes after message.       */

static BERR_Code print_log_message(BDBG_FifoReader_Handle logReader, int deviceFd, PROXY_NEXUS_Log_Instance *pProxyInstance,  unsigned *pTimeout )
{
    BERR_Code   rc;
    int         urc;

    /* Allocate a buffer that can hold the message, but reserve
     * some space for a prefix and suffix.  */
    char        buf[MAX_PREFIX_LEN + MAX_MSG_LEN + MAX_SUFFIX_LEN + 1]; /* + 1 for ending newline char */
    char       *pMsgBuf = buf + MAX_PREFIX_LEN;
    size_t      msgLen;

    /* Retrieve the next log message as requested.  Depending on
     * what arguments were passed, we'll either get the message
     * directly from BDBG, or we'll have to go through the proxy
     * to get it.  */
    BDBG_ASSERT(logReader || pProxyInstance);
    rc = BERR_SUCCESS;
    if (logReader) {
        rc = get_usermode_log_message(logReader, pTimeout, pMsgBuf, MAX_MSG_LEN, &msgLen);
    }
    else if (pProxyInstance) {
        rc = get_driver_log_message(deviceFd, pProxyInstance, pTimeout, pMsgBuf, MAX_MSG_LEN, &msgLen);
    }
    if (rc != BERR_SUCCESS) {
        return BERR_TRACE(rc);
    }

    if (msgLen > 0) {
        BDBG_Level msgLevel = get_log_message_level(pMsgBuf, msgLen);

        /* BDBG_ERR(("%u %u", dbgStrLen, strlen(dbgStr))); */
        pMsgBuf[msgLen]  = '\n';
        msgLen++;

        pMsgBuf[msgLen]  = '\0';
        msgLen++;

        /* skip header and timestamp */
        while ( msgLen > 1 && !isalpha(pMsgBuf[0]) )
        {
            pMsgBuf++;
            msgLen--;
        }

        switch ( msgLevel )
        {
        case BDBG_eWrn:
            ALOGW("%s", pMsgBuf);
            break;
        case BDBG_eErr:
            ALOGE("%s", pMsgBuf);
            break;
        default:
            ALOGI("%s", pMsgBuf);
            break;
        }
    }

    return BERR_SUCCESS;
}


int main(int argc, const char *argv[])
{
    BERR_Code rc;
    BDBG_FifoReader_Handle logReader=NULL;
    BDBG_Fifo_Handle logWriter=NULL;
    int fd=-1;
    int device_fd=-1;
    int sched_rc;
    bool driver_ready = false;
    const char *fname;
    size_t size;
    void *shared;
    struct stat st;
    int urc;
    pid_t parent;
    PROXY_NEXUS_Log_Instance instance;
    int32_t priority=0;
    int32_t new_priority;
    unsigned acc_timeout=0;

    if(argc<2) {
        usage(argv[0]);
    }
    BSTD_UNUSED(argv);

    rc = BKNI_Init();
    BDBG_ASSERT(rc==BERR_SUCCESS);
    rc = BDBG_Init();
    BDBG_ASSERT(rc==BERR_SUCCESS);

    priority = property_get_int32(PROP_LOGGER_PRIORITY, 0);
    if ( priority > 0 )
    {
        struct sched_param pri;
        pri.sched_priority = priority;
        sched_rc = sched_setscheduler(0, SCHED_FIFO, &pri);
        if ( sched_rc )
        {
            ALOGE("Error updating scheduler %d", sched_rc);
        }
    }

    /* coverity[var_assign_var] */
    fname = argv[1];
    BDBG_ASSERT(fname);
    /* coverity[tainted_string] */
    if ( strcmp(fname, "disabled") ) {
        fd = open(fname, O_RDONLY);
        if(fd<0) {
            perror(fname);
            usage(argv[0]);
        }
    }

    if(argc>2 && argv[2][0]!='\0') {
        int ready;
        /*( device_fd = open(argv[2],O_RDWR); */
        device_fd = atoi(argv[2]);
        if(device_fd<0) {
            perror(argv[2]);
            usage(argv[0]);
        }
        urc = ioctl(device_fd, IOCTL_PROXY_NEXUS_Log_Test, &ready);
        if(urc!=0) {
            perror(argv[2]);
            usage(argv[0]);
        }
        if(ready) {
            urc = ioctl(device_fd, IOCTL_PROXY_NEXUS_Log_Create, &instance);
            if(urc==0) {
                driver_ready = true;
            }
        }
    }
    /* unlink(fname); don't remove file, allow multiple copies of logger */
    if ( fd >= 0 ) {
        urc = fstat(fd, &st);
        if(urc<0) {
            perror("stat");
            usage(argv[0]);
        }
    }
    parent = getppid();
    signal(SIGUSR1,sigusr1_handler);
    if ( fd >= 0 ) {
        size = st.st_size;
        shared = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
        if(shared==MAP_FAILED) {
            perror("mmap");
            usage(argv[0]);
        }
        logWriter = (BDBG_Fifo_Handle)shared;
        rc = BDBG_FifoReader_Create(&logReader, logWriter);
        BDBG_ASSERT(rc==BERR_SUCCESS);
    }
    if(argc>3) {
        int ready_fd = atoi(argv[3]);
        char data[1]={'\0'};
        int rc;
        rc = write(ready_fd,data,1); /* signal parent that we've started */
        close(ready_fd);
    }
    for(;;) {
        unsigned timeout;
        unsigned driverTimeout=0;

        for(;;) {
            if ( logReader ) {
                timeout = 0;
                rc = print_log_message(logReader, -1, NULL, &timeout );
            } else {
                timeout = 5;
            }

            if(driver_ready) {

                rc = print_log_message(NULL, device_fd, &instance, &driverTimeout );

                if (rc == BERR_SUCCESS && timeout>driverTimeout) {
                    timeout = driverTimeout;
                }
            }
            if(timeout!=0) {
                break;
            }
        }
        BDBG_ASSERT(timeout);
        if(sigusr1_fired) {
            goto done;
        }
        if(kill(parent, 0)) {
            static const char terminated[] = "_____ TERMINATED _____\n";
            ALOGE(terminated);
            break;
        }
        BKNI_Sleep(timeout);

        acc_timeout += timeout;
        if ( acc_timeout > PRIORITY_REFRESH_INTERVAL )
        {
            /* Check latest logger priority */
            new_priority = property_get_int32(PROP_LOGGER_PRIORITY, 0);
            if ( new_priority > 0 && new_priority != priority )
            {
                struct sched_param pri;
                pri.sched_priority = new_priority;
                if ( priority )
                {
                    sched_rc = sched_setparam(0, &pri);
                }
                else
                {
                    /* Scheduler has not been specified yet */
                    sched_rc = sched_setscheduler(0, SCHED_FIFO, &pri);
                }
                if ( sched_rc )
                {
                    ALOGE("Error updating priority %d", sched_rc);
                }
                else
                {
                    priority = new_priority;
                }
            }

            acc_timeout = 0;
        }

        if(device_fd>=0 && !driver_ready) {
            int ready;
            /* if parent was delayed keep trying to attach */
            urc = ioctl(device_fd, IOCTL_PROXY_NEXUS_Log_Test, &ready);
            if(urc!=0) {
                goto done;
            }
            if(ready) {
                urc = ioctl(device_fd, IOCTL_PROXY_NEXUS_Log_Create, &instance);
                if(urc!=0) {
                    goto done;
                }
                driver_ready = true;
            }
        }
    }
done:
    if(device_fd>=0) {
        close(device_fd);
    }
    close(fd);
    BDBG_FifoReader_Destroy(logReader);

    BDBG_Uninit();
    /* BKNI_Uninit(); Don't call it since this would print memory allocation stats */
    return 0;
}
