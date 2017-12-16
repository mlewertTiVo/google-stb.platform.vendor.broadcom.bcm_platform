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

#define LOG_TAG "bcm-nxs-ib"

#include <binder/Parcel.h>
#include "INxServer.h"

using namespace android;

enum {
   GET_NX_CLIENT = IBinder::FIRST_CALL_TRANSACTION,
   REG_HPD_LISTENER,
   UNREG_HPD_LISTENER,
   REG_DSP_LISTENER,
   UNREG_DSP_LISTENER,
   RMLMK_CLIENT,
};

class BpNxServer : public BpInterface<INxServer>
{
public:
    BpNxServer(const sp<IBinder>& impl)
        : BpInterface<INxServer>(impl) {
    }

    void getNxClient(int pid, uint64_t &client) {
       Parcel data, reply;
       data.writeInterfaceToken(INxServer::getInterfaceDescriptor());
       data.writeInt32(pid);
       remote()->transact(GET_NX_CLIENT, data, &reply);
       client = reply.readUint64();
    }

    status_t regHpdEvt(const sp<INxHpdEvtSrc> &listener) {
       Parcel data, reply;
       data.writeInterfaceToken(INxServer::getInterfaceDescriptor());
       data.writeStrongBinder(listener->asBinder(listener));
       remote()->transact(REG_HPD_LISTENER, data, &reply);
       return reply.readInt32();
    }
    status_t unregHpdEvt(const sp<INxHpdEvtSrc> &listener) {
       Parcel data, reply;
       data.writeInterfaceToken(INxServer::getInterfaceDescriptor());
       data.writeStrongBinder(listener->asBinder(listener));
       remote()->transact(UNREG_HPD_LISTENER, data, &reply);
       return reply.readInt32();
    }

    status_t regDspEvt(const sp<INxDspEvtSrc> &listener) {
       Parcel data, reply;
       data.writeInterfaceToken(INxServer::getInterfaceDescriptor());
       data.writeStrongBinder(listener->asBinder(listener));
       remote()->transact(REG_DSP_LISTENER, data, &reply);
       return reply.readInt32();
    }
    status_t unregDspEvt(const sp<INxDspEvtSrc> &listener) {
       Parcel data, reply;
       data.writeInterfaceToken(INxServer::getInterfaceDescriptor());
       data.writeStrongBinder(listener->asBinder(listener));
       remote()->transact(UNREG_DSP_LISTENER, data, &reply);
       return reply.readInt32();
    }

    void rmlmk(uint64_t client) {
       Parcel data, reply;
       data.writeInterfaceToken(INxServer::getInterfaceDescriptor());
       data.writeUint64(client);
       remote()->transact(RMLMK_CLIENT, data, &reply);
    }
};

IMPLEMENT_META_INTERFACE(NxServer, "broadcom.nxserver");

status_t BnNxServer::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
   switch (code) {
      case GET_NX_CLIENT: {
         CHECK_INTERFACE(INxServer, data, reply);
         uint64_t value = 0;
         int index = data.readInt32();
         getNxClient(index, value);
         reply->writeUint64(value);
         return NO_ERROR;
      }
      break;

      case REG_HPD_LISTENER: {
         CHECK_INTERFACE(INxServer, data, reply);
         sp<INxHpdEvtSrc> listener = interface_cast<INxHpdEvtSrc>(data.readStrongBinder());
         return reply->writeInt32(regHpdEvt(listener));
      }
      break;
      case UNREG_HPD_LISTENER: {
         CHECK_INTERFACE(INxServer, data, reply);
         sp<INxHpdEvtSrc> listener = interface_cast<INxHpdEvtSrc>(data.readStrongBinder());
         return reply->writeInt32(unregHpdEvt(listener));
      }
      break;

      case REG_DSP_LISTENER: {
         CHECK_INTERFACE(INxServer, data, reply);
         sp<INxDspEvtSrc> listener = interface_cast<INxDspEvtSrc>(data.readStrongBinder());
         return reply->writeInt32(regDspEvt(listener));
      }
      break;
      case UNREG_DSP_LISTENER: {
         CHECK_INTERFACE(INxServer, data, reply);
         sp<INxDspEvtSrc> listener = interface_cast<INxDspEvtSrc>(data.readStrongBinder());
         return reply->writeInt32(unregDspEvt(listener));
      }
      break;

      case RMLMK_CLIENT: {
         CHECK_INTERFACE(INxServer, data, reply);
         uint64_t client = data.readUint64();
         rmlmk(client);
         return NO_ERROR;
      }
      break;

      default:
         return BBinder::onTransact(code, data, reply, flags);
      break;
   }

   return NO_ERROR;
}
