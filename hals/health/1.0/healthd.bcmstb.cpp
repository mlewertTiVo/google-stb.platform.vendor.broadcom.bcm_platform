/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include <healthd/healthd.h>

void healthd_board_init(struct healthd_config*) {
    // use defaults
}

int healthd_board_battery_update(struct android::BatteryProperties* p) {
   p->chargerAcOnline = true;
   p->batteryPresent  = true;
   p->batteryStatus   = android::BATTERY_STATUS_CHARGING;
   p->batteryHealth   = android::BATTERY_HEALTH_GOOD;
   p->batteryLevel    = 100;
   p->batteryChargeCounter = 1000000;
   p->batteryFullCharge = 100;
   // return 0 to log periodic polled battery status to kernel log
   return 0;
}