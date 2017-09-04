/*
 * Copyright (C) 2014 Broadcom Canada Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sys/resource.h>
#include <cutils/sched_policy.h>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include "Hwc.h"

using namespace android;

int main( int argc, char** argv )
{
    (void) argc;
    (void) argv;

#if defined(BCM_FULL_TREBLE)
    ProcessState::initWithDriver("/dev/vndbinder");
#endif

    // binder threads limit: 3 x video window (max) + 1 hwc.
    ProcessState::self()->setThreadPoolMaxThreadCount(4);

    setpriority(PRIO_PROCESS, 0, PRIORITY_URGENT_DISPLAY);
    set_sched_policy(0, SP_FOREGROUND);

    ProcessState::self()->startThreadPool();

    // init and publish the interface.
    Hwc::instantiate();

    IPCThreadState::self()->joinThreadPool();
}

