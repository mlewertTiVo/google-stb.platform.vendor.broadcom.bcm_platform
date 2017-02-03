/*
 * Copyright (C) 2017 Broadcom Canada Ltd.
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

#ifndef __NXSERVERSVC__H__INCLUDED__
#define __NXSERVERSVC__H__INCLUDED__

#include <INxServer.h>

using namespace android;

const sp<INxServer>& get_nxs(bool retry);

#endif

