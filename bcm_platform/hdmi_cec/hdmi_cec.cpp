/*
 * Copyright (C) 2014 The Android Open Source Project
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

// Verbose messages removed
//#define LOG_NDEBUG 0

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "Brcmstb-HDMI-CEC"
#include <utils/Log.h>

#include <hardware/hardware.h>
#include <hardware/hdmi_cec.h>

#include "nexus_hdmi_cec.h"

#define HDMI_CEC_TRACE_ENTER ALOGV("%s: Enter\n", __FUNCTION__)

using namespace android;

typedef struct hdmi_device {
    struct hdmi_cec_device device;   // Must be first member
    sp<NexusHdmiCecDevice> mNexusHdmiCecDevice;
} hdmi_device_t;


/*
 * Helper function to convert from a hdmi_cec_device to a NexusHdmiCecDevice.
 */
sp<NexusHdmiCecDevice>
get_nexus_hdmi_cec_device(const hdmi_cec_device *dev)
{
    hdmi_device_t* ctx = reinterpret_cast<hdmi_device_t *>(const_cast<hdmi_cec_device *>(dev));
    return ctx->mNexusHdmiCecDevice;
}

/*
 * (*add_logical_address)() passes the logical address that will be used
 * in this system.
 *
 * HAL may use it to configure the hardware so that the CEC commands addressed
 * the given logical address can be filtered in. This method can be called
 * as many times as necessary in order to support multiple logical devices.
 * addr should be in the range of valid logical addresses for the call
 * to succeed.
 *
 * Returns 0 on success or -errno on error.
 */
static int hdmi_cec_add_logical_address(const struct hdmi_cec_device* dev, cec_logical_address_t addr)
{
    sp<NexusHdmiCecDevice> device = get_nexus_hdmi_cec_device(dev);

    ALOGV("%s: logical address=%d", __PRETTY_FUNCTION__, (unsigned)addr);

    return (device->setCecLogicalAddress((uint8_t)addr) == NO_ERROR) ? 0 : -EINVAL;
}

/*
 * (*clear_logical_address)() tells HAL to reset all the logical addresses.
 *
 * It is used when the system doesn't need to process CEC command any more,
 * hence to tell HAL to stop receiving commands from the CEC bus, and change
 * the state back to the beginning.
 */
static void hdmi_cec_clear_logical_address(const struct hdmi_cec_device* dev)
{
    HDMI_CEC_TRACE_ENTER;

    sp<NexusHdmiCecDevice> device = get_nexus_hdmi_cec_device(dev);

    ALOGV("%s: clear logical address to 0xFF", __PRETTY_FUNCTION__);

    device->setCecLogicalAddress(0xFF);
}

/*
 * (*get_physical_address)() returns the CEC physical address. The
 * address is written to addr.
 *
 * The physical address depends on the topology of the network formed
 * by connected HDMI devices. It is therefore likely to change if the cable
 * is plugged off and on again. It is advised to call get_physical_address
 * to get the updated address when hot plug event takes place.
 *
 * Returns 0 on success or -errno on error.
 */
static int hdmi_cec_get_physical_address(const struct hdmi_cec_device* dev, uint16_t* addr)
{
    int status;
    sp<NexusHdmiCecDevice> device = get_nexus_hdmi_cec_device(dev);

    HDMI_CEC_TRACE_ENTER;

    status = device->getCecPhysicalAddress(addr);

    if (status == NO_ERROR) {
        ALOGV("%s: CEC physical address = %d.%d.%d.%d", __PRETTY_FUNCTION__,
              (*addr >> 12) & 0x0f, (*addr >> 8) & 0x0f, (*addr >> 4) & 0x0f, *addr & 0x0f);
    }

    return (status == NO_ERROR) ? 0 : -EINVAL;
}

/*
 * (*send_message)() transmits HDMI-CEC message to other HDMI device.
 *
 * The method should be designed to return in a certain amount of time not
 * hanging forever, which can happen if CEC signal line is pulled low for
 * some reason. HAL implementation should take the situation into account
 * so as not to wait forever for the message to get sent out.
 *
 * It should try retransmission at least once as specified in the standard.
 *
 * Returns error code. See HDMI_RESULT_SUCCESS, HDMI_RESULT_NACK, and
 * HDMI_RESULT_BUSY.
 */
static int hdmi_cec_send_message(const struct hdmi_cec_device* dev, const cec_message_t* message)
{
    sp<NexusHdmiCecDevice> device = get_nexus_hdmi_cec_device(dev);

    ALOGV("%s: initiator=%d, destination=%d, length=%d, opcode=0x%02x", __PRETTY_FUNCTION__,
            message->initiator, message->destination, message->length, message->body[0]);

    if (device->sendCecMessage(message) != NO_ERROR) {
        ALOGE("%s: Could not send CEC message!!!", __PRETTY_FUNCTION__);
        return HDMI_RESULT_FAIL;
    }
    return HDMI_RESULT_SUCCESS;
}

/*
 * (*register_event_callback)() registers a callback that HDMI-CEC HAL
 * can later use for incoming CEC messages or internal HDMI events.
 * When calling from C++, use the argument arg to pass the calling object.
 * It will be passed back when the callback is invoked so that the context
 * can be retrieved.
 */
static void hdmi_cec_register_event_callback(const struct hdmi_cec_device* dev, event_callback_t callback, void* arg)
{
    sp<NexusHdmiCecDevice> device = get_nexus_hdmi_cec_device(dev);

    HDMI_CEC_TRACE_ENTER;

    device->registerEventCallback(dev, callback, arg);
}

/*
 * (*get_version)() returns the CEC version supported by underlying hardware.
 */
static void hdmi_cec_get_version(const struct hdmi_cec_device* dev, int* version)
{
    sp<NexusHdmiCecDevice> device = get_nexus_hdmi_cec_device(dev);

    HDMI_CEC_TRACE_ENTER;

    device->getCecVersion(version);

    ALOGV("%s: CEC version=%d", __PRETTY_FUNCTION__, *version);
}

/*
 * (*get_vendor_id)() returns the identifier of the vendor. It is
 * the 24-bit unique company ID obtained from the IEEE Registration
 * Authority Committee (RAC).
 */
static void hdmi_cec_get_vendor_id(const struct hdmi_cec_device* dev, uint32_t* vendor_id)
{
    sp<NexusHdmiCecDevice> device = get_nexus_hdmi_cec_device(dev);

    HDMI_CEC_TRACE_ENTER;

    device->getCecVendorId(vendor_id);

    ALOGV("%s: CEC vendor id=0x%08x", __PRETTY_FUNCTION__, *vendor_id);
}

/*
 * (*get_port_info)() returns the hdmi port information of underlying hardware.
 * info is the list of HDMI port information, and 'total' is the number of
 * HDMI ports in the system.
 */
static void hdmi_cec_get_port_info(const struct hdmi_cec_device* dev, struct hdmi_port_info* list[], int* total)
{
    sp<NexusHdmiCecDevice> device = get_nexus_hdmi_cec_device(dev);

    HDMI_CEC_TRACE_ENTER;

    hdmi_port_info* ports;
    int numPorts;
    device->getCecPortInfo(&ports, &numPorts);

    for (int i = 0; i < numPorts; ++i) {
        hdmi_port_info* info = &ports[i];
        ALOGV("%s: port[%d]: type=%d, phy addr=%d.%d.%d.%d, cec=%d, arc=%d", __FUNCTION__, i, info->type,
                (info->physical_address >> 12) & 0x0f, 
                (info->physical_address >>  8) & 0x0f, 
                (info->physical_address >>  4) & 0x0f, 
                (info->physical_address      ) & 0x0f, 
                info->cec_supported, info->arc_supported);
    }

    *list=ports;
    *total=numPorts;
}

/*
 * (*set_option)() passes flags controlling the way HDMI-CEC service works down
 * to HAL implementation. Those flags will be used in case the feature needs
 * update in HAL itself, firmware or microcontroller.
 */
static void hdmi_cec_set_option(const struct hdmi_cec_device* dev, int flag, int value)
{
    sp<NexusHdmiCecDevice> device = get_nexus_hdmi_cec_device(dev);

    HDMI_CEC_TRACE_ENTER;

    switch (flag) {
        case HDMI_OPTION_ENABLE_CEC: {
            ALOGV("%s: HDMI_OPTION_ENABLE_CEC=%d", __PRETTY_FUNCTION__, value);
            device->setEnableState(!!value);
        } break;

        case HDMI_OPTION_WAKEUP: {
            ALOGV("%s: HDMI_OPTION_WAKEUP=%d", __PRETTY_FUNCTION__, value);
            device->setAutoWakeupState(!!value);
        } break;

        case HDMI_OPTION_SYSTEM_CEC_CONTROL: {
            ALOGV("%s: HDMI_OPTION_SYSTEM_CEC_CONTROL=%d", __PRETTY_FUNCTION__, value);
            device->setControlState(!!value);
        } break;

        default: {
            ALOGW("%s: unknown flag %d!", __PRETTY_FUNCTION__, flag);
        }
    }
}

/*
 * (*set_audio_return_channel)() configures ARC circuit in the hardware logic
 * to start or stop the feature. Flag can be either 1 to start the feature
 * or 0 to stop it.
 *
 * Returns 0 on success or -errno on error.
 */
static void hdmi_cec_set_audio_return_channel(const struct hdmi_cec_device* dev __unused, int flag __unused)
{
    ALOGV("%s: flag=0x%08x", __PRETTY_FUNCTION__, flag);
}

/*
 * (*is_connected)() returns the connection status of the specified port.
 * Returns HDMI_CONNECTED if a device is connected, otherwise HDMI_NOT_CONNECTED.
 * The HAL should watch for +5V power signal to determine the status.
 */
static int hdmi_cec_is_connected(const struct hdmi_cec_device* dev, int port_id)
{
    sp<NexusHdmiCecDevice> device = get_nexus_hdmi_cec_device(dev);
    int connected = HDMI_NOT_CONNECTED;

    HDMI_CEC_TRACE_ENTER;

    // TODO support multiple HDMI CEC ports
    if (port_id == 1) {
        if (device->getConnectedState(&connected) != NO_ERROR) {
            ALOGE("%s: Could not get HDMI%d connected state!!!", __PRETTY_FUNCTION__, port_id);
        }
    }
    ALOGV("%s: HDMI%d is %s", __PRETTY_FUNCTION__, port_id, (connected == HDMI_CONNECTED) ? "connected" : "disconnected");
    return connected;
}

static int hdmi_cec_device_close(struct hw_device_t *dev)
{
    int status = 0;
    hdmi_device_t* ctx = reinterpret_cast<hdmi_device_t *>(dev);

    HDMI_CEC_TRACE_ENTER;

    if (ctx) {
        if (ctx->mNexusHdmiCecDevice != NULL) {
            if (ctx->mNexusHdmiCecDevice->uninitialise() != NO_ERROR) {
                status = -EINVAL;
            }
            else {
                ALOGI("%s: Successfully destroyed NexusHdmiCecDevice", __PRETTY_FUNCTION__);
            }
            ctx->mNexusHdmiCecDevice = NULL;
        }
        free(ctx);
    }
    return status;
}

static int hdmi_cec_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device)
{
    int status = -EINVAL;

    HDMI_CEC_TRACE_ENTER;

    if (!strcmp(name, HDMI_CEC_HARDWARE_INTERFACE)) {
        hdmi_device_t *dev;
        dev = (hdmi_device_t*)calloc(1, sizeof(*dev));

        /* initialize the procs */
        dev->device.common.tag = HARDWARE_DEVICE_TAG;
        dev->device.common.version = HDMI_CEC_DEVICE_API_VERSION_1_0;
        dev->device.common.module = const_cast<hw_module_t*>(module);
        dev->device.common.close = hdmi_cec_device_close;

        dev->device.add_logical_address = hdmi_cec_add_logical_address;
        dev->device.clear_logical_address = hdmi_cec_clear_logical_address;
        dev->device.get_physical_address = hdmi_cec_get_physical_address;
        dev->device.send_message = hdmi_cec_send_message;
        dev->device.register_event_callback = hdmi_cec_register_event_callback;
        dev->device.get_version = hdmi_cec_get_version;
        dev->device.get_vendor_id = hdmi_cec_get_vendor_id;
        dev->device.get_port_info = hdmi_cec_get_port_info;
        dev->device.set_option = hdmi_cec_set_option;
        dev->device.set_audio_return_channel = hdmi_cec_set_audio_return_channel;
        dev->device.is_connected = hdmi_cec_is_connected;

        *device = &dev->device.common;

        dev->mNexusHdmiCecDevice = new NexusHdmiCecDevice();
        if (dev->mNexusHdmiCecDevice == NULL) {
            ALOGE("%s: cannot instantiate NexusHdmiCecDevice!!!", __PRETTY_FUNCTION__);
            free(dev);
        }
        else {
            status = dev->mNexusHdmiCecDevice->initialise();
            if (status == NO_ERROR) {
                ALOGI("%s: Successfully instantiated NexusHdmiCecDevice. :)", __PRETTY_FUNCTION__);
            }
            else {
                ALOGE("%s: Could not initialise NexusHdmiCecDevice!!!", __PRETTY_FUNCTION__);
                dev->mNexusHdmiCecDevice = NULL;
                free(dev);
            }
        }
    }
    return status;
}

static struct hw_module_methods_t hdmi_cec_module_methods = {
    open: hdmi_cec_device_open
};

hdmi_module_t HAL_MODULE_INFO_SYM = {
    common: {
        tag: HARDWARE_MODULE_TAG,
        module_api_version: HDMI_CEC_MODULE_API_VERSION_1_0,
        hal_api_version: HARDWARE_HAL_API_VERSION,
        id: HDMI_CEC_HARDWARE_MODULE_ID,
        name: "Broadcom HDMI CEC module",
        author: "The Android Open Source Project",
        methods: &hdmi_cec_module_methods,
    }
};

