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

#ifndef __INXSERVER__H__INCLUDED__
#define __INXSERVER__H__INCLUDED__

#include <utils/RefBase.h>
#include <utils/Errors.h>
#include <binder/IInterface.h>

#include <INxHpdEvtSrc.h>
#include <INxDspEvtSrc.h>

using namespace android;

class INxServer : public IInterface
{
public:
   DECLARE_META_INTERFACE(NxServer);

   virtual void getNxClient(int pid, uint64_t &client) = 0;

   virtual status_t regHpdEvt(const sp<INxHpdEvtSrc> &listener) = 0;
   virtual status_t unregHpdEvt(const sp<INxHpdEvtSrc> &listener) = 0;

   virtual status_t regDspEvt(const sp<INxDspEvtSrc> &listener) = 0;
   virtual status_t unregDspEvt(const sp<INxDspEvtSrc> &listener) = 0;
};

class BnNxServer: public BnInterface<INxServer>
{
public:
    virtual status_t onTransact( uint32_t code,
                                 const Parcel& data,
                                 Parcel* reply,
                                 uint32_t flags = 0);
};

#endif

