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

#ifndef __HWCLISTENER__H__INCLUDED__
#define __HWCLISTENER__H__INCLUDED__

#include "IHwcListener.h"
#include <binder/Binder.h>

using namespace android;

class HwcListener: public IBinder::DeathRecipient, public BnHwcListener
{
public:
      HwcListener();
      ~HwcListener();

     virtual void binderDied(const wp<IBinder>& who);
     virtual void notify(int msg, int param1, int param2);
};

#endif // __HWCLISTENER__H__INCLUDED__
