#define LOG_TAG "bcm.hardware.nexus@1.0::NexusDspCb"

#include <android/log.h>
#include <cutils/trace.h>
#include <hidl/HidlTransportSupport.h>

#include <android/hidl/manager/1.0/IServiceManager.h>
#include <hidl/LegacySupport.h>
#include <bcm/hardware/nexus/1.0/BpHwNexusDspCb.h>
#include <bcm/hardware/nexus/1.0/BnHwNexusDspCb.h>
#include <bcm/hardware/nexus/1.0/BsNexusDspCb.h>
#include <android/hidl/base/1.0/BpHwBase.h>
#include <hidl/ServiceManagement.h>

namespace bcm {
namespace hardware {
namespace nexus {
namespace V1_0 {

std::string toString(const ::android::sp<INexusDspCb>& o) {
    std::string os = "[class or subclass of ";
    os += INexusDspCb::descriptor;
    os += "]";
    os += o->isRemote() ? "@remote" : "@local";
    return os;
}

const char* INexusDspCb::descriptor("bcm.hardware.nexus@1.0::INexusDspCb");

__attribute__((constructor))static void static_constructor() {
    ::android::hardware::details::gBnConstructorMap.set(INexusDspCb::descriptor,
            [](void *iIntf) -> ::android::sp<::android::hardware::IBinder> {
                return new BnHwNexusDspCb(static_cast<INexusDspCb *>(iIntf));
            });
    ::android::hardware::details::gBsConstructorMap.set(INexusDspCb::descriptor,
            [](void *iIntf) -> ::android::sp<::android::hidl::base::V1_0::IBase> {
                return new BsNexusDspCb(static_cast<INexusDspCb *>(iIntf));
            });
};

__attribute__((destructor))static void static_destructor() {
    ::android::hardware::details::gBnConstructorMap.erase(INexusDspCb::descriptor);
    ::android::hardware::details::gBsConstructorMap.erase(INexusDspCb::descriptor);
};

// Methods from INexusDspCb follow.
// no default implementation for: ::android::hardware::Return<void> INexusDspCb::onDsp()

// Methods from ::android::hidl::base::V1_0::IBase follow.
::android::hardware::Return<void> INexusDspCb::interfaceChain(interfaceChain_cb _hidl_cb){
    _hidl_cb({
        INexusDspCb::descriptor,
        ::android::hidl::base::V1_0::IBase::descriptor,
    });
    return ::android::hardware::Void();}

::android::hardware::Return<void> INexusDspCb::debug(const ::android::hardware::hidl_handle& fd, const ::android::hardware::hidl_vec<::android::hardware::hidl_string>& options){
    (void)fd;
    (void)options;
    return ::android::hardware::Void();}

::android::hardware::Return<void> INexusDspCb::interfaceDescriptor(interfaceDescriptor_cb _hidl_cb){
    _hidl_cb(INexusDspCb::descriptor);
    return ::android::hardware::Void();}

::android::hardware::Return<void> INexusDspCb::getHashChain(getHashChain_cb _hidl_cb){
    _hidl_cb({
        (uint8_t[32]){113,118,137,255,192,94,99,101,185,105,5,186,101,249,172,201,173,173,153,34,160,7,232,215,129,208,161,209,1,42,106,99} /* 717689ffc05e6365b96905ba65f9acc9adad9922a007e8d781d0a1d1012a6a63 */,
        (uint8_t[32]){189,218,182,24,77,122,52,109,166,160,125,192,130,140,241,154,105,111,76,170,54,17,197,31,46,20,86,90,20,180,15,217} /* bddab6184d7a346da6a07dc0828cf19a696f4caa3611c51f2e14565a14b40fd9 */});
    return ::android::hardware::Void();
}

::android::hardware::Return<void> INexusDspCb::setHALInstrumentation(){
    return ::android::hardware::Void();
}

::android::hardware::Return<bool> INexusDspCb::linkToDeath(const ::android::sp<::android::hardware::hidl_death_recipient>& recipient, uint64_t cookie){
    (void)cookie;
    return (recipient != nullptr);
}

::android::hardware::Return<void> INexusDspCb::ping(){
    return ::android::hardware::Void();
}

::android::hardware::Return<void> INexusDspCb::getDebugInfo(getDebugInfo_cb _hidl_cb){
    _hidl_cb({ -1 /* pid */, 0 /* ptr */, 
    #if defined(__LP64__)
    ::android::hidl::base::V1_0::DebugInfo::Architecture::IS_64BIT
    #else
    ::android::hidl::base::V1_0::DebugInfo::Architecture::IS_32BIT
    #endif
    });
    return ::android::hardware::Void();}

::android::hardware::Return<void> INexusDspCb::notifySyspropsChanged(){
    ::android::report_sysprop_change();
    return ::android::hardware::Void();}

::android::hardware::Return<bool> INexusDspCb::unlinkToDeath(const ::android::sp<::android::hardware::hidl_death_recipient>& recipient){
    return (recipient != nullptr);
}


// static 
::android::hardware::Return<::android::sp<INexusDspCb>> INexusDspCb::castFrom(const ::android::sp<INexusDspCb>& parent, bool /* emitError */) {
    return parent;
}

// static 
::android::hardware::Return<::android::sp<INexusDspCb>> INexusDspCb::castFrom(const ::android::sp<::android::hidl::base::V1_0::IBase>& parent, bool emitError) {
    return ::android::hardware::details::castInterface<INexusDspCb, ::android::hidl::base::V1_0::IBase, BpHwNexusDspCb, ::android::hidl::base::V1_0::BpHwBase>(
            parent, "bcm.hardware.nexus@1.0::INexusDspCb", emitError);
}

BpHwNexusDspCb::BpHwNexusDspCb(const ::android::sp<::android::hardware::IBinder> &_hidl_impl)
        : BpInterface<INexusDspCb>(_hidl_impl),
          ::android::hardware::details::HidlInstrumentor("bcm.hardware.nexus@1.0", "INexusDspCb") {
}

// Methods from INexusDspCb follow.
::android::hardware::Return<void> BpHwNexusDspCb::onDsp() {
    atrace_begin(ATRACE_TAG_HAL, "HIDL::INexusDspCb::onDsp::client");
    #ifdef __ANDROID_DEBUGGABLE__
    if (UNLIKELY(mEnableInstrumentation)) {
        std::vector<void *> _hidl_args;
        for (const auto &callback: mInstrumentationCallbacks) {
            callback(InstrumentationEvent::CLIENT_API_ENTRY, "bcm.hardware.nexus", "1.0", "INexusDspCb", "onDsp", &_hidl_args);
        }
    }
    #endif // __ANDROID_DEBUGGABLE__

    ::android::hardware::Parcel _hidl_data;
    ::android::hardware::Parcel _hidl_reply;
    ::android::status_t _hidl_err;
    ::android::hardware::Status _hidl_status;

    _hidl_err = _hidl_data.writeInterfaceToken(INexusDspCb::descriptor);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_err = remote()->transact(1 /* onDsp */, _hidl_data, &_hidl_reply, ::android::hardware::IBinder::FLAG_ONEWAY);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    atrace_end(ATRACE_TAG_HAL);
    #ifdef __ANDROID_DEBUGGABLE__
    if (UNLIKELY(mEnableInstrumentation)) {
        std::vector<void *> _hidl_args;
        for (const auto &callback: mInstrumentationCallbacks) {
            callback(InstrumentationEvent::CLIENT_API_EXIT, "bcm.hardware.nexus", "1.0", "INexusDspCb", "onDsp", &_hidl_args);
        }
    }
    #endif // __ANDROID_DEBUGGABLE__

    _hidl_status.setFromStatusT(_hidl_err);
    return ::android::hardware::Return<void>();

_hidl_error:
    _hidl_status.setFromStatusT(_hidl_err);
    return ::android::hardware::Return<void>(_hidl_status);
}


// Methods from ::android::hidl::base::V1_0::IBase follow.
::android::hardware::Return<void> BpHwNexusDspCb::interfaceChain(interfaceChain_cb _hidl_cb) {
    if (_hidl_cb == nullptr) {
        return ::android::hardware::Status::fromExceptionCode(
                ::android::hardware::Status::EX_ILLEGAL_ARGUMENT);
    }

    atrace_begin(ATRACE_TAG_HAL, "HIDL::INexusDspCb::interfaceChain::client");
    #ifdef __ANDROID_DEBUGGABLE__
    if (UNLIKELY(mEnableInstrumentation)) {
        std::vector<void *> _hidl_args;
        for (const auto &callback: mInstrumentationCallbacks) {
            callback(InstrumentationEvent::CLIENT_API_ENTRY, "bcm.hardware.nexus", "1.0", "INexusDspCb", "interfaceChain", &_hidl_args);
        }
    }
    #endif // __ANDROID_DEBUGGABLE__

    ::android::hardware::Parcel _hidl_data;
    ::android::hardware::Parcel _hidl_reply;
    ::android::status_t _hidl_err;
    ::android::hardware::Status _hidl_status;

    const ::android::hardware::hidl_vec<::android::hardware::hidl_string>* _hidl_out_descriptors;

    _hidl_err = _hidl_data.writeInterfaceToken(::android::hidl::base::V1_0::IBase::descriptor);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_err = remote()->transact(256067662 /* interfaceChain */, _hidl_data, &_hidl_reply);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_err = ::android::hardware::readFromParcel(&_hidl_status, _hidl_reply);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    if (!_hidl_status.isOk()) { return _hidl_status; }

    size_t _hidl__hidl_out_descriptors_parent;

    _hidl_err = _hidl_reply.readBuffer(sizeof(*_hidl_out_descriptors), &_hidl__hidl_out_descriptors_parent,  reinterpret_cast<const void **>(&_hidl_out_descriptors));

    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    size_t _hidl__hidl_out_descriptors_child;

    _hidl_err = ::android::hardware::readEmbeddedFromParcel(
            const_cast<::android::hardware::hidl_vec<::android::hardware::hidl_string> &>(*_hidl_out_descriptors),
            _hidl_reply,
            _hidl__hidl_out_descriptors_parent,
            0 /* parentOffset */, &_hidl__hidl_out_descriptors_child);

    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    for (size_t _hidl_index_0 = 0; _hidl_index_0 < _hidl_out_descriptors->size(); ++_hidl_index_0) {
        _hidl_err = ::android::hardware::readEmbeddedFromParcel(
                const_cast<::android::hardware::hidl_string &>((*_hidl_out_descriptors)[_hidl_index_0]),
                _hidl_reply,
                _hidl__hidl_out_descriptors_child,
                _hidl_index_0 * sizeof(::android::hardware::hidl_string));

        if (_hidl_err != ::android::OK) { goto _hidl_error; }

    }

    _hidl_cb(*_hidl_out_descriptors);

    atrace_end(ATRACE_TAG_HAL);
    #ifdef __ANDROID_DEBUGGABLE__
    if (UNLIKELY(mEnableInstrumentation)) {
        std::vector<void *> _hidl_args;
        _hidl_args.push_back((void *)_hidl_out_descriptors);
        for (const auto &callback: mInstrumentationCallbacks) {
            callback(InstrumentationEvent::CLIENT_API_EXIT, "bcm.hardware.nexus", "1.0", "INexusDspCb", "interfaceChain", &_hidl_args);
        }
    }
    #endif // __ANDROID_DEBUGGABLE__

    _hidl_status.setFromStatusT(_hidl_err);
    return ::android::hardware::Return<void>();

_hidl_error:
    _hidl_status.setFromStatusT(_hidl_err);
    return ::android::hardware::Return<void>(_hidl_status);
}

::android::hardware::Return<void> BpHwNexusDspCb::debug(const ::android::hardware::hidl_handle& fd, const ::android::hardware::hidl_vec<::android::hardware::hidl_string>& options) {
    atrace_begin(ATRACE_TAG_HAL, "HIDL::INexusDspCb::debug::client");
    #ifdef __ANDROID_DEBUGGABLE__
    if (UNLIKELY(mEnableInstrumentation)) {
        std::vector<void *> _hidl_args;
        _hidl_args.push_back((void *)&fd);
        _hidl_args.push_back((void *)&options);
        for (const auto &callback: mInstrumentationCallbacks) {
            callback(InstrumentationEvent::CLIENT_API_ENTRY, "bcm.hardware.nexus", "1.0", "INexusDspCb", "debug", &_hidl_args);
        }
    }
    #endif // __ANDROID_DEBUGGABLE__

    ::android::hardware::Parcel _hidl_data;
    ::android::hardware::Parcel _hidl_reply;
    ::android::status_t _hidl_err;
    ::android::hardware::Status _hidl_status;

    _hidl_err = _hidl_data.writeInterfaceToken(::android::hidl::base::V1_0::IBase::descriptor);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_err = _hidl_data.writeNativeHandleNoDup(fd);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    size_t _hidl_options_parent;

    _hidl_err = _hidl_data.writeBuffer(&options, sizeof(options), &_hidl_options_parent);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    size_t _hidl_options_child;

    _hidl_err = ::android::hardware::writeEmbeddedToParcel(
            options,
            &_hidl_data,
            _hidl_options_parent,
            0 /* parentOffset */, &_hidl_options_child);

    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    for (size_t _hidl_index_0 = 0; _hidl_index_0 < options.size(); ++_hidl_index_0) {
        _hidl_err = ::android::hardware::writeEmbeddedToParcel(
                options[_hidl_index_0],
                &_hidl_data,
                _hidl_options_child,
                _hidl_index_0 * sizeof(::android::hardware::hidl_string));

        if (_hidl_err != ::android::OK) { goto _hidl_error; }

    }

    _hidl_err = remote()->transact(256131655 /* debug */, _hidl_data, &_hidl_reply);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_err = ::android::hardware::readFromParcel(&_hidl_status, _hidl_reply);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    if (!_hidl_status.isOk()) { return _hidl_status; }

    atrace_end(ATRACE_TAG_HAL);
    #ifdef __ANDROID_DEBUGGABLE__
    if (UNLIKELY(mEnableInstrumentation)) {
        std::vector<void *> _hidl_args;
        for (const auto &callback: mInstrumentationCallbacks) {
            callback(InstrumentationEvent::CLIENT_API_EXIT, "bcm.hardware.nexus", "1.0", "INexusDspCb", "debug", &_hidl_args);
        }
    }
    #endif // __ANDROID_DEBUGGABLE__

    _hidl_status.setFromStatusT(_hidl_err);
    return ::android::hardware::Return<void>();

_hidl_error:
    _hidl_status.setFromStatusT(_hidl_err);
    return ::android::hardware::Return<void>(_hidl_status);
}

::android::hardware::Return<void> BpHwNexusDspCb::interfaceDescriptor(interfaceDescriptor_cb _hidl_cb) {
    if (_hidl_cb == nullptr) {
        return ::android::hardware::Status::fromExceptionCode(
                ::android::hardware::Status::EX_ILLEGAL_ARGUMENT);
    }

    atrace_begin(ATRACE_TAG_HAL, "HIDL::INexusDspCb::interfaceDescriptor::client");
    #ifdef __ANDROID_DEBUGGABLE__
    if (UNLIKELY(mEnableInstrumentation)) {
        std::vector<void *> _hidl_args;
        for (const auto &callback: mInstrumentationCallbacks) {
            callback(InstrumentationEvent::CLIENT_API_ENTRY, "bcm.hardware.nexus", "1.0", "INexusDspCb", "interfaceDescriptor", &_hidl_args);
        }
    }
    #endif // __ANDROID_DEBUGGABLE__

    ::android::hardware::Parcel _hidl_data;
    ::android::hardware::Parcel _hidl_reply;
    ::android::status_t _hidl_err;
    ::android::hardware::Status _hidl_status;

    const ::android::hardware::hidl_string* _hidl_out_descriptor;

    _hidl_err = _hidl_data.writeInterfaceToken(::android::hidl::base::V1_0::IBase::descriptor);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_err = remote()->transact(256136003 /* interfaceDescriptor */, _hidl_data, &_hidl_reply);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_err = ::android::hardware::readFromParcel(&_hidl_status, _hidl_reply);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    if (!_hidl_status.isOk()) { return _hidl_status; }

    size_t _hidl__hidl_out_descriptor_parent;

    _hidl_err = _hidl_reply.readBuffer(sizeof(*_hidl_out_descriptor), &_hidl__hidl_out_descriptor_parent,  reinterpret_cast<const void **>(&_hidl_out_descriptor));

    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_err = ::android::hardware::readEmbeddedFromParcel(
            const_cast<::android::hardware::hidl_string &>(*_hidl_out_descriptor),
            _hidl_reply,
            _hidl__hidl_out_descriptor_parent,
            0 /* parentOffset */);

    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_cb(*_hidl_out_descriptor);

    atrace_end(ATRACE_TAG_HAL);
    #ifdef __ANDROID_DEBUGGABLE__
    if (UNLIKELY(mEnableInstrumentation)) {
        std::vector<void *> _hidl_args;
        _hidl_args.push_back((void *)_hidl_out_descriptor);
        for (const auto &callback: mInstrumentationCallbacks) {
            callback(InstrumentationEvent::CLIENT_API_EXIT, "bcm.hardware.nexus", "1.0", "INexusDspCb", "interfaceDescriptor", &_hidl_args);
        }
    }
    #endif // __ANDROID_DEBUGGABLE__

    _hidl_status.setFromStatusT(_hidl_err);
    return ::android::hardware::Return<void>();

_hidl_error:
    _hidl_status.setFromStatusT(_hidl_err);
    return ::android::hardware::Return<void>(_hidl_status);
}

::android::hardware::Return<void> BpHwNexusDspCb::getHashChain(getHashChain_cb _hidl_cb) {
    if (_hidl_cb == nullptr) {
        return ::android::hardware::Status::fromExceptionCode(
                ::android::hardware::Status::EX_ILLEGAL_ARGUMENT);
    }

    atrace_begin(ATRACE_TAG_HAL, "HIDL::INexusDspCb::getHashChain::client");
    #ifdef __ANDROID_DEBUGGABLE__
    if (UNLIKELY(mEnableInstrumentation)) {
        std::vector<void *> _hidl_args;
        for (const auto &callback: mInstrumentationCallbacks) {
            callback(InstrumentationEvent::CLIENT_API_ENTRY, "bcm.hardware.nexus", "1.0", "INexusDspCb", "getHashChain", &_hidl_args);
        }
    }
    #endif // __ANDROID_DEBUGGABLE__

    ::android::hardware::Parcel _hidl_data;
    ::android::hardware::Parcel _hidl_reply;
    ::android::status_t _hidl_err;
    ::android::hardware::Status _hidl_status;

    const ::android::hardware::hidl_vec<::android::hardware::hidl_array<uint8_t, 32>>* _hidl_out_hashchain;

    _hidl_err = _hidl_data.writeInterfaceToken(::android::hidl::base::V1_0::IBase::descriptor);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_err = remote()->transact(256398152 /* getHashChain */, _hidl_data, &_hidl_reply);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_err = ::android::hardware::readFromParcel(&_hidl_status, _hidl_reply);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    if (!_hidl_status.isOk()) { return _hidl_status; }

    size_t _hidl__hidl_out_hashchain_parent;

    _hidl_err = _hidl_reply.readBuffer(sizeof(*_hidl_out_hashchain), &_hidl__hidl_out_hashchain_parent,  reinterpret_cast<const void **>(&_hidl_out_hashchain));

    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    size_t _hidl__hidl_out_hashchain_child;

    _hidl_err = ::android::hardware::readEmbeddedFromParcel(
            const_cast<::android::hardware::hidl_vec<::android::hardware::hidl_array<uint8_t, 32>> &>(*_hidl_out_hashchain),
            _hidl_reply,
            _hidl__hidl_out_hashchain_parent,
            0 /* parentOffset */, &_hidl__hidl_out_hashchain_child);

    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_cb(*_hidl_out_hashchain);

    atrace_end(ATRACE_TAG_HAL);
    #ifdef __ANDROID_DEBUGGABLE__
    if (UNLIKELY(mEnableInstrumentation)) {
        std::vector<void *> _hidl_args;
        _hidl_args.push_back((void *)_hidl_out_hashchain);
        for (const auto &callback: mInstrumentationCallbacks) {
            callback(InstrumentationEvent::CLIENT_API_EXIT, "bcm.hardware.nexus", "1.0", "INexusDspCb", "getHashChain", &_hidl_args);
        }
    }
    #endif // __ANDROID_DEBUGGABLE__

    _hidl_status.setFromStatusT(_hidl_err);
    return ::android::hardware::Return<void>();

_hidl_error:
    _hidl_status.setFromStatusT(_hidl_err);
    return ::android::hardware::Return<void>(_hidl_status);
}

::android::hardware::Return<void> BpHwNexusDspCb::setHALInstrumentation() {
    atrace_begin(ATRACE_TAG_HAL, "HIDL::INexusDspCb::setHALInstrumentation::client");
    #ifdef __ANDROID_DEBUGGABLE__
    if (UNLIKELY(mEnableInstrumentation)) {
        std::vector<void *> _hidl_args;
        for (const auto &callback: mInstrumentationCallbacks) {
            callback(InstrumentationEvent::CLIENT_API_ENTRY, "bcm.hardware.nexus", "1.0", "INexusDspCb", "setHALInstrumentation", &_hidl_args);
        }
    }
    #endif // __ANDROID_DEBUGGABLE__

    ::android::hardware::Parcel _hidl_data;
    ::android::hardware::Parcel _hidl_reply;
    ::android::status_t _hidl_err;
    ::android::hardware::Status _hidl_status;

    _hidl_err = _hidl_data.writeInterfaceToken(::android::hidl::base::V1_0::IBase::descriptor);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_err = remote()->transact(256462420 /* setHALInstrumentation */, _hidl_data, &_hidl_reply, ::android::hardware::IBinder::FLAG_ONEWAY);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    atrace_end(ATRACE_TAG_HAL);
    #ifdef __ANDROID_DEBUGGABLE__
    if (UNLIKELY(mEnableInstrumentation)) {
        std::vector<void *> _hidl_args;
        for (const auto &callback: mInstrumentationCallbacks) {
            callback(InstrumentationEvent::CLIENT_API_EXIT, "bcm.hardware.nexus", "1.0", "INexusDspCb", "setHALInstrumentation", &_hidl_args);
        }
    }
    #endif // __ANDROID_DEBUGGABLE__

    _hidl_status.setFromStatusT(_hidl_err);
    return ::android::hardware::Return<void>();

_hidl_error:
    _hidl_status.setFromStatusT(_hidl_err);
    return ::android::hardware::Return<void>(_hidl_status);
}

::android::hardware::Return<bool> BpHwNexusDspCb::linkToDeath(const ::android::sp<::android::hardware::hidl_death_recipient>& recipient, uint64_t cookie) {
    ::android::hardware::ProcessState::self()->startThreadPool();
    ::android::hardware::hidl_binder_death_recipient *binder_recipient = new ::android::hardware::hidl_binder_death_recipient(recipient, cookie, this);
    std::unique_lock<std::mutex> lock(_hidl_mMutex);
    _hidl_mDeathRecipients.push_back(binder_recipient);
    return (remote()->linkToDeath(binder_recipient) == ::android::OK);
}

::android::hardware::Return<void> BpHwNexusDspCb::ping() {
    atrace_begin(ATRACE_TAG_HAL, "HIDL::INexusDspCb::ping::client");
    #ifdef __ANDROID_DEBUGGABLE__
    if (UNLIKELY(mEnableInstrumentation)) {
        std::vector<void *> _hidl_args;
        for (const auto &callback: mInstrumentationCallbacks) {
            callback(InstrumentationEvent::CLIENT_API_ENTRY, "bcm.hardware.nexus", "1.0", "INexusDspCb", "ping", &_hidl_args);
        }
    }
    #endif // __ANDROID_DEBUGGABLE__

    ::android::hardware::Parcel _hidl_data;
    ::android::hardware::Parcel _hidl_reply;
    ::android::status_t _hidl_err;
    ::android::hardware::Status _hidl_status;

    _hidl_err = _hidl_data.writeInterfaceToken(::android::hidl::base::V1_0::IBase::descriptor);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_err = remote()->transact(256921159 /* ping */, _hidl_data, &_hidl_reply);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_err = ::android::hardware::readFromParcel(&_hidl_status, _hidl_reply);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    if (!_hidl_status.isOk()) { return _hidl_status; }

    atrace_end(ATRACE_TAG_HAL);
    #ifdef __ANDROID_DEBUGGABLE__
    if (UNLIKELY(mEnableInstrumentation)) {
        std::vector<void *> _hidl_args;
        for (const auto &callback: mInstrumentationCallbacks) {
            callback(InstrumentationEvent::CLIENT_API_EXIT, "bcm.hardware.nexus", "1.0", "INexusDspCb", "ping", &_hidl_args);
        }
    }
    #endif // __ANDROID_DEBUGGABLE__

    _hidl_status.setFromStatusT(_hidl_err);
    return ::android::hardware::Return<void>();

_hidl_error:
    _hidl_status.setFromStatusT(_hidl_err);
    return ::android::hardware::Return<void>(_hidl_status);
}

::android::hardware::Return<void> BpHwNexusDspCb::getDebugInfo(getDebugInfo_cb _hidl_cb) {
    if (_hidl_cb == nullptr) {
        return ::android::hardware::Status::fromExceptionCode(
                ::android::hardware::Status::EX_ILLEGAL_ARGUMENT);
    }

    atrace_begin(ATRACE_TAG_HAL, "HIDL::INexusDspCb::getDebugInfo::client");
    #ifdef __ANDROID_DEBUGGABLE__
    if (UNLIKELY(mEnableInstrumentation)) {
        std::vector<void *> _hidl_args;
        for (const auto &callback: mInstrumentationCallbacks) {
            callback(InstrumentationEvent::CLIENT_API_ENTRY, "bcm.hardware.nexus", "1.0", "INexusDspCb", "getDebugInfo", &_hidl_args);
        }
    }
    #endif // __ANDROID_DEBUGGABLE__

    ::android::hardware::Parcel _hidl_data;
    ::android::hardware::Parcel _hidl_reply;
    ::android::status_t _hidl_err;
    ::android::hardware::Status _hidl_status;

    const ::android::hidl::base::V1_0::DebugInfo* _hidl_out_info;

    _hidl_err = _hidl_data.writeInterfaceToken(::android::hidl::base::V1_0::IBase::descriptor);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_err = remote()->transact(257049926 /* getDebugInfo */, _hidl_data, &_hidl_reply);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_err = ::android::hardware::readFromParcel(&_hidl_status, _hidl_reply);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    if (!_hidl_status.isOk()) { return _hidl_status; }

    size_t _hidl__hidl_out_info_parent;

    _hidl_err = _hidl_reply.readBuffer(sizeof(*_hidl_out_info), &_hidl__hidl_out_info_parent,  reinterpret_cast<const void **>(&_hidl_out_info));
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_cb(*_hidl_out_info);

    atrace_end(ATRACE_TAG_HAL);
    #ifdef __ANDROID_DEBUGGABLE__
    if (UNLIKELY(mEnableInstrumentation)) {
        std::vector<void *> _hidl_args;
        _hidl_args.push_back((void *)_hidl_out_info);
        for (const auto &callback: mInstrumentationCallbacks) {
            callback(InstrumentationEvent::CLIENT_API_EXIT, "bcm.hardware.nexus", "1.0", "INexusDspCb", "getDebugInfo", &_hidl_args);
        }
    }
    #endif // __ANDROID_DEBUGGABLE__

    _hidl_status.setFromStatusT(_hidl_err);
    return ::android::hardware::Return<void>();

_hidl_error:
    _hidl_status.setFromStatusT(_hidl_err);
    return ::android::hardware::Return<void>(_hidl_status);
}

::android::hardware::Return<void> BpHwNexusDspCb::notifySyspropsChanged() {
    atrace_begin(ATRACE_TAG_HAL, "HIDL::INexusDspCb::notifySyspropsChanged::client");
    #ifdef __ANDROID_DEBUGGABLE__
    if (UNLIKELY(mEnableInstrumentation)) {
        std::vector<void *> _hidl_args;
        for (const auto &callback: mInstrumentationCallbacks) {
            callback(InstrumentationEvent::CLIENT_API_ENTRY, "bcm.hardware.nexus", "1.0", "INexusDspCb", "notifySyspropsChanged", &_hidl_args);
        }
    }
    #endif // __ANDROID_DEBUGGABLE__

    ::android::hardware::Parcel _hidl_data;
    ::android::hardware::Parcel _hidl_reply;
    ::android::status_t _hidl_err;
    ::android::hardware::Status _hidl_status;

    _hidl_err = _hidl_data.writeInterfaceToken(::android::hidl::base::V1_0::IBase::descriptor);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_err = remote()->transact(257120595 /* notifySyspropsChanged */, _hidl_data, &_hidl_reply, ::android::hardware::IBinder::FLAG_ONEWAY);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    atrace_end(ATRACE_TAG_HAL);
    #ifdef __ANDROID_DEBUGGABLE__
    if (UNLIKELY(mEnableInstrumentation)) {
        std::vector<void *> _hidl_args;
        for (const auto &callback: mInstrumentationCallbacks) {
            callback(InstrumentationEvent::CLIENT_API_EXIT, "bcm.hardware.nexus", "1.0", "INexusDspCb", "notifySyspropsChanged", &_hidl_args);
        }
    }
    #endif // __ANDROID_DEBUGGABLE__

    _hidl_status.setFromStatusT(_hidl_err);
    return ::android::hardware::Return<void>();

_hidl_error:
    _hidl_status.setFromStatusT(_hidl_err);
    return ::android::hardware::Return<void>(_hidl_status);
}

::android::hardware::Return<bool> BpHwNexusDspCb::unlinkToDeath(const ::android::sp<::android::hardware::hidl_death_recipient>& recipient) {
    std::unique_lock<std::mutex> lock(_hidl_mMutex);
    for (auto it = _hidl_mDeathRecipients.begin();it != _hidl_mDeathRecipients.end();++it) {
        if ((*it)->getRecipient() == recipient) {
            ::android::status_t status = remote()->unlinkToDeath(*it);
            _hidl_mDeathRecipients.erase(it);
            return status == ::android::OK;
        }}
    return false;
}


BnHwNexusDspCb::BnHwNexusDspCb(const ::android::sp<INexusDspCb> &_hidl_impl)
        : ::android::hidl::base::V1_0::BnHwBase(_hidl_impl, "bcm.hardware.nexus@1.0", "INexusDspCb") { 
            _hidl_mImpl = _hidl_impl;
            auto prio = ::android::hardware::details::gServicePrioMap.get(_hidl_impl, {SCHED_NORMAL, 0});
            mSchedPolicy = prio.sched_policy;
            mSchedPriority = prio.prio;
}

// Methods from INexusDspCb follow.

// Methods from ::android::hidl::base::V1_0::IBase follow.
::android::hardware::Return<void> BnHwNexusDspCb::ping() {
    return ::android::hardware::Void();
}
::android::hardware::Return<void> BnHwNexusDspCb::getDebugInfo(getDebugInfo_cb _hidl_cb) {
    _hidl_cb({
        ::android::hardware::details::debuggable()? getpid() : -1 /* pid */,
        ::android::hardware::details::debuggable()? reinterpret_cast<uint64_t>(this) : 0 /* ptr */,
        #if defined(__LP64__)
        ::android::hidl::base::V1_0::DebugInfo::Architecture::IS_64BIT
        #else
        ::android::hidl::base::V1_0::DebugInfo::Architecture::IS_32BIT
        #endif

    });
    return ::android::hardware::Void();}

::android::status_t BnHwNexusDspCb::onTransact(
        uint32_t _hidl_code,
        const ::android::hardware::Parcel &_hidl_data,
        ::android::hardware::Parcel *_hidl_reply,
        uint32_t _hidl_flags,
        TransactCallback _hidl_cb) {
    ::android::status_t _hidl_err = ::android::OK;

    switch (_hidl_code) {
        case 1 /* onDsp */:
        {
            if (!_hidl_data.enforceInterface(INexusDspCb::descriptor)) {
                _hidl_err = ::android::BAD_TYPE;
                break;
            }

            atrace_begin(ATRACE_TAG_HAL, "HIDL::INexusDspCb::onDsp::server");
            #ifdef __ANDROID_DEBUGGABLE__
            if (UNLIKELY(mEnableInstrumentation)) {
                std::vector<void *> _hidl_args;
                for (const auto &callback: mInstrumentationCallbacks) {
                    callback(InstrumentationEvent::SERVER_API_ENTRY, "bcm.hardware.nexus", "1.0", "INexusDspCb", "onDsp", &_hidl_args);
                }
            }
            #endif // __ANDROID_DEBUGGABLE__

            _hidl_mImpl->onDsp();

            atrace_end(ATRACE_TAG_HAL);
            #ifdef __ANDROID_DEBUGGABLE__
            if (UNLIKELY(mEnableInstrumentation)) {
                std::vector<void *> _hidl_args;
                for (const auto &callback: mInstrumentationCallbacks) {
                    callback(InstrumentationEvent::SERVER_API_EXIT, "bcm.hardware.nexus", "1.0", "INexusDspCb", "onDsp", &_hidl_args);
                }
            }
            #endif // __ANDROID_DEBUGGABLE__

            ::android::hardware::writeToParcel(::android::hardware::Status::ok(), _hidl_reply);

            break;
        }

        case 256067662 /* interfaceChain */:
        {
            if (!_hidl_data.enforceInterface(::android::hidl::base::V1_0::IBase::descriptor)) {
                _hidl_err = ::android::BAD_TYPE;
                break;
            }

            atrace_begin(ATRACE_TAG_HAL, "HIDL::INexusDspCb::interfaceChain::server");
            #ifdef __ANDROID_DEBUGGABLE__
            if (UNLIKELY(mEnableInstrumentation)) {
                std::vector<void *> _hidl_args;
                for (const auto &callback: mInstrumentationCallbacks) {
                    callback(InstrumentationEvent::SERVER_API_ENTRY, "bcm.hardware.nexus", "1.0", "INexusDspCb", "interfaceChain", &_hidl_args);
                }
            }
            #endif // __ANDROID_DEBUGGABLE__

            bool _hidl_callbackCalled = false;

            _hidl_mImpl->interfaceChain([&](const auto &_hidl_out_descriptors) {
                if (_hidl_callbackCalled) {
                    LOG_ALWAYS_FATAL("interfaceChain: _hidl_cb called a second time, but must be called once.");
                }
                _hidl_callbackCalled = true;

                ::android::hardware::writeToParcel(::android::hardware::Status::ok(), _hidl_reply);

                size_t _hidl__hidl_out_descriptors_parent;

                _hidl_err = _hidl_reply->writeBuffer(&_hidl_out_descriptors, sizeof(_hidl_out_descriptors), &_hidl__hidl_out_descriptors_parent);
                /* _hidl_err ignored! */

                size_t _hidl__hidl_out_descriptors_child;

                _hidl_err = ::android::hardware::writeEmbeddedToParcel(
                        _hidl_out_descriptors,
                        _hidl_reply,
                        _hidl__hidl_out_descriptors_parent,
                        0 /* parentOffset */, &_hidl__hidl_out_descriptors_child);

                /* _hidl_err ignored! */

                for (size_t _hidl_index_0 = 0; _hidl_index_0 < _hidl_out_descriptors.size(); ++_hidl_index_0) {
                    _hidl_err = ::android::hardware::writeEmbeddedToParcel(
                            _hidl_out_descriptors[_hidl_index_0],
                            _hidl_reply,
                            _hidl__hidl_out_descriptors_child,
                            _hidl_index_0 * sizeof(::android::hardware::hidl_string));

                    /* _hidl_err ignored! */

                }

                atrace_end(ATRACE_TAG_HAL);
                #ifdef __ANDROID_DEBUGGABLE__
                if (UNLIKELY(mEnableInstrumentation)) {
                    std::vector<void *> _hidl_args;
                    _hidl_args.push_back((void *)&_hidl_out_descriptors);
                    for (const auto &callback: mInstrumentationCallbacks) {
                        callback(InstrumentationEvent::SERVER_API_EXIT, "bcm.hardware.nexus", "1.0", "INexusDspCb", "interfaceChain", &_hidl_args);
                    }
                }
                #endif // __ANDROID_DEBUGGABLE__

                _hidl_cb(*_hidl_reply);
            });

            if (!_hidl_callbackCalled) {
                LOG_ALWAYS_FATAL("interfaceChain: _hidl_cb not called, but must be called once.");
            }

            break;
        }

        case 256131655 /* debug */:
        {
            if (!_hidl_data.enforceInterface(::android::hidl::base::V1_0::IBase::descriptor)) {
                _hidl_err = ::android::BAD_TYPE;
                break;
            }

            ::android::hardware::hidl_handle fd;
            const ::android::hardware::hidl_vec<::android::hardware::hidl_string>* options;

            const native_handle_t *fd_ptr;

            _hidl_err = _hidl_data.readNullableNativeHandleNoDup(&fd_ptr);

            if (_hidl_err != ::android::OK) { break; }

            fd = fd_ptr;
            size_t _hidl_options_parent;

            _hidl_err = _hidl_data.readBuffer(sizeof(*options), &_hidl_options_parent,  reinterpret_cast<const void **>(&options));

            if (_hidl_err != ::android::OK) { break; }

            size_t _hidl_options_child;

            _hidl_err = ::android::hardware::readEmbeddedFromParcel(
                    const_cast<::android::hardware::hidl_vec<::android::hardware::hidl_string> &>(*options),
                    _hidl_data,
                    _hidl_options_parent,
                    0 /* parentOffset */, &_hidl_options_child);

            if (_hidl_err != ::android::OK) { break; }

            for (size_t _hidl_index_0 = 0; _hidl_index_0 < options->size(); ++_hidl_index_0) {
                _hidl_err = ::android::hardware::readEmbeddedFromParcel(
                        const_cast<::android::hardware::hidl_string &>((*options)[_hidl_index_0]),
                        _hidl_data,
                        _hidl_options_child,
                        _hidl_index_0 * sizeof(::android::hardware::hidl_string));

                if (_hidl_err != ::android::OK) { break; }

            }

            atrace_begin(ATRACE_TAG_HAL, "HIDL::INexusDspCb::debug::server");
            #ifdef __ANDROID_DEBUGGABLE__
            if (UNLIKELY(mEnableInstrumentation)) {
                std::vector<void *> _hidl_args;
                _hidl_args.push_back((void *)&fd);
                _hidl_args.push_back((void *)options);
                for (const auto &callback: mInstrumentationCallbacks) {
                    callback(InstrumentationEvent::SERVER_API_ENTRY, "bcm.hardware.nexus", "1.0", "INexusDspCb", "debug", &_hidl_args);
                }
            }
            #endif // __ANDROID_DEBUGGABLE__

            _hidl_mImpl->debug(fd, *options);

            atrace_end(ATRACE_TAG_HAL);
            #ifdef __ANDROID_DEBUGGABLE__
            if (UNLIKELY(mEnableInstrumentation)) {
                std::vector<void *> _hidl_args;
                for (const auto &callback: mInstrumentationCallbacks) {
                    callback(InstrumentationEvent::SERVER_API_EXIT, "bcm.hardware.nexus", "1.0", "INexusDspCb", "debug", &_hidl_args);
                }
            }
            #endif // __ANDROID_DEBUGGABLE__

            ::android::hardware::writeToParcel(::android::hardware::Status::ok(), _hidl_reply);

            break;
        }

        case 256136003 /* interfaceDescriptor */:
        {
            if (!_hidl_data.enforceInterface(::android::hidl::base::V1_0::IBase::descriptor)) {
                _hidl_err = ::android::BAD_TYPE;
                break;
            }

            atrace_begin(ATRACE_TAG_HAL, "HIDL::INexusDspCb::interfaceDescriptor::server");
            #ifdef __ANDROID_DEBUGGABLE__
            if (UNLIKELY(mEnableInstrumentation)) {
                std::vector<void *> _hidl_args;
                for (const auto &callback: mInstrumentationCallbacks) {
                    callback(InstrumentationEvent::SERVER_API_ENTRY, "bcm.hardware.nexus", "1.0", "INexusDspCb", "interfaceDescriptor", &_hidl_args);
                }
            }
            #endif // __ANDROID_DEBUGGABLE__

            bool _hidl_callbackCalled = false;

            _hidl_mImpl->interfaceDescriptor([&](const auto &_hidl_out_descriptor) {
                if (_hidl_callbackCalled) {
                    LOG_ALWAYS_FATAL("interfaceDescriptor: _hidl_cb called a second time, but must be called once.");
                }
                _hidl_callbackCalled = true;

                ::android::hardware::writeToParcel(::android::hardware::Status::ok(), _hidl_reply);

                size_t _hidl__hidl_out_descriptor_parent;

                _hidl_err = _hidl_reply->writeBuffer(&_hidl_out_descriptor, sizeof(_hidl_out_descriptor), &_hidl__hidl_out_descriptor_parent);
                /* _hidl_err ignored! */

                _hidl_err = ::android::hardware::writeEmbeddedToParcel(
                        _hidl_out_descriptor,
                        _hidl_reply,
                        _hidl__hidl_out_descriptor_parent,
                        0 /* parentOffset */);

                /* _hidl_err ignored! */

                atrace_end(ATRACE_TAG_HAL);
                #ifdef __ANDROID_DEBUGGABLE__
                if (UNLIKELY(mEnableInstrumentation)) {
                    std::vector<void *> _hidl_args;
                    _hidl_args.push_back((void *)&_hidl_out_descriptor);
                    for (const auto &callback: mInstrumentationCallbacks) {
                        callback(InstrumentationEvent::SERVER_API_EXIT, "bcm.hardware.nexus", "1.0", "INexusDspCb", "interfaceDescriptor", &_hidl_args);
                    }
                }
                #endif // __ANDROID_DEBUGGABLE__

                _hidl_cb(*_hidl_reply);
            });

            if (!_hidl_callbackCalled) {
                LOG_ALWAYS_FATAL("interfaceDescriptor: _hidl_cb not called, but must be called once.");
            }

            break;
        }

        case 256398152 /* getHashChain */:
        {
            if (!_hidl_data.enforceInterface(::android::hidl::base::V1_0::IBase::descriptor)) {
                _hidl_err = ::android::BAD_TYPE;
                break;
            }

            atrace_begin(ATRACE_TAG_HAL, "HIDL::INexusDspCb::getHashChain::server");
            #ifdef __ANDROID_DEBUGGABLE__
            if (UNLIKELY(mEnableInstrumentation)) {
                std::vector<void *> _hidl_args;
                for (const auto &callback: mInstrumentationCallbacks) {
                    callback(InstrumentationEvent::SERVER_API_ENTRY, "bcm.hardware.nexus", "1.0", "INexusDspCb", "getHashChain", &_hidl_args);
                }
            }
            #endif // __ANDROID_DEBUGGABLE__

            bool _hidl_callbackCalled = false;

            _hidl_mImpl->getHashChain([&](const auto &_hidl_out_hashchain) {
                if (_hidl_callbackCalled) {
                    LOG_ALWAYS_FATAL("getHashChain: _hidl_cb called a second time, but must be called once.");
                }
                _hidl_callbackCalled = true;

                ::android::hardware::writeToParcel(::android::hardware::Status::ok(), _hidl_reply);

                size_t _hidl__hidl_out_hashchain_parent;

                _hidl_err = _hidl_reply->writeBuffer(&_hidl_out_hashchain, sizeof(_hidl_out_hashchain), &_hidl__hidl_out_hashchain_parent);
                /* _hidl_err ignored! */

                size_t _hidl__hidl_out_hashchain_child;

                _hidl_err = ::android::hardware::writeEmbeddedToParcel(
                        _hidl_out_hashchain,
                        _hidl_reply,
                        _hidl__hidl_out_hashchain_parent,
                        0 /* parentOffset */, &_hidl__hidl_out_hashchain_child);

                /* _hidl_err ignored! */

                atrace_end(ATRACE_TAG_HAL);
                #ifdef __ANDROID_DEBUGGABLE__
                if (UNLIKELY(mEnableInstrumentation)) {
                    std::vector<void *> _hidl_args;
                    _hidl_args.push_back((void *)&_hidl_out_hashchain);
                    for (const auto &callback: mInstrumentationCallbacks) {
                        callback(InstrumentationEvent::SERVER_API_EXIT, "bcm.hardware.nexus", "1.0", "INexusDspCb", "getHashChain", &_hidl_args);
                    }
                }
                #endif // __ANDROID_DEBUGGABLE__

                _hidl_cb(*_hidl_reply);
            });

            if (!_hidl_callbackCalled) {
                LOG_ALWAYS_FATAL("getHashChain: _hidl_cb not called, but must be called once.");
            }

            break;
        }

        case 256462420 /* setHALInstrumentation */:
        {
            configureInstrumentation();
            break;
        }

        case 256660548 /* linkToDeath */:
        {
            break;
        }

        case 256921159 /* ping */:
        {
            if (!_hidl_data.enforceInterface(::android::hidl::base::V1_0::IBase::descriptor)) {
                _hidl_err = ::android::BAD_TYPE;
                break;
            }

            atrace_begin(ATRACE_TAG_HAL, "HIDL::INexusDspCb::ping::server");
            #ifdef __ANDROID_DEBUGGABLE__
            if (UNLIKELY(mEnableInstrumentation)) {
                std::vector<void *> _hidl_args;
                for (const auto &callback: mInstrumentationCallbacks) {
                    callback(InstrumentationEvent::SERVER_API_ENTRY, "bcm.hardware.nexus", "1.0", "INexusDspCb", "ping", &_hidl_args);
                }
            }
            #endif // __ANDROID_DEBUGGABLE__

            this->ping();

            atrace_end(ATRACE_TAG_HAL);
            #ifdef __ANDROID_DEBUGGABLE__
            if (UNLIKELY(mEnableInstrumentation)) {
                std::vector<void *> _hidl_args;
                for (const auto &callback: mInstrumentationCallbacks) {
                    callback(InstrumentationEvent::SERVER_API_EXIT, "bcm.hardware.nexus", "1.0", "INexusDspCb", "ping", &_hidl_args);
                }
            }
            #endif // __ANDROID_DEBUGGABLE__

            ::android::hardware::writeToParcel(::android::hardware::Status::ok(), _hidl_reply);

            break;
        }

        case 257049926 /* getDebugInfo */:
        {
            if (!_hidl_data.enforceInterface(::android::hidl::base::V1_0::IBase::descriptor)) {
                _hidl_err = ::android::BAD_TYPE;
                break;
            }

            atrace_begin(ATRACE_TAG_HAL, "HIDL::INexusDspCb::getDebugInfo::server");
            #ifdef __ANDROID_DEBUGGABLE__
            if (UNLIKELY(mEnableInstrumentation)) {
                std::vector<void *> _hidl_args;
                for (const auto &callback: mInstrumentationCallbacks) {
                    callback(InstrumentationEvent::SERVER_API_ENTRY, "bcm.hardware.nexus", "1.0", "INexusDspCb", "getDebugInfo", &_hidl_args);
                }
            }
            #endif // __ANDROID_DEBUGGABLE__

            bool _hidl_callbackCalled = false;

            this->getDebugInfo([&](const auto &_hidl_out_info) {
                if (_hidl_callbackCalled) {
                    LOG_ALWAYS_FATAL("getDebugInfo: _hidl_cb called a second time, but must be called once.");
                }
                _hidl_callbackCalled = true;

                ::android::hardware::writeToParcel(::android::hardware::Status::ok(), _hidl_reply);

                size_t _hidl__hidl_out_info_parent;

                _hidl_err = _hidl_reply->writeBuffer(&_hidl_out_info, sizeof(_hidl_out_info), &_hidl__hidl_out_info_parent);
                /* _hidl_err ignored! */

                atrace_end(ATRACE_TAG_HAL);
                #ifdef __ANDROID_DEBUGGABLE__
                if (UNLIKELY(mEnableInstrumentation)) {
                    std::vector<void *> _hidl_args;
                    _hidl_args.push_back((void *)&_hidl_out_info);
                    for (const auto &callback: mInstrumentationCallbacks) {
                        callback(InstrumentationEvent::SERVER_API_EXIT, "bcm.hardware.nexus", "1.0", "INexusDspCb", "getDebugInfo", &_hidl_args);
                    }
                }
                #endif // __ANDROID_DEBUGGABLE__

                _hidl_cb(*_hidl_reply);
            });

            if (!_hidl_callbackCalled) {
                LOG_ALWAYS_FATAL("getDebugInfo: _hidl_cb not called, but must be called once.");
            }

            break;
        }

        case 257120595 /* notifySyspropsChanged */:
        {
            if (!_hidl_data.enforceInterface(::android::hidl::base::V1_0::IBase::descriptor)) {
                _hidl_err = ::android::BAD_TYPE;
                break;
            }

            atrace_begin(ATRACE_TAG_HAL, "HIDL::INexusDspCb::notifySyspropsChanged::server");
            #ifdef __ANDROID_DEBUGGABLE__
            if (UNLIKELY(mEnableInstrumentation)) {
                std::vector<void *> _hidl_args;
                for (const auto &callback: mInstrumentationCallbacks) {
                    callback(InstrumentationEvent::SERVER_API_ENTRY, "bcm.hardware.nexus", "1.0", "INexusDspCb", "notifySyspropsChanged", &_hidl_args);
                }
            }
            #endif // __ANDROID_DEBUGGABLE__

            _hidl_mImpl->notifySyspropsChanged();

            atrace_end(ATRACE_TAG_HAL);
            #ifdef __ANDROID_DEBUGGABLE__
            if (UNLIKELY(mEnableInstrumentation)) {
                std::vector<void *> _hidl_args;
                for (const auto &callback: mInstrumentationCallbacks) {
                    callback(InstrumentationEvent::SERVER_API_EXIT, "bcm.hardware.nexus", "1.0", "INexusDspCb", "notifySyspropsChanged", &_hidl_args);
                }
            }
            #endif // __ANDROID_DEBUGGABLE__

            ::android::hardware::writeToParcel(::android::hardware::Status::ok(), _hidl_reply);

            break;
        }

        case 257250372 /* unlinkToDeath */:
        {
            break;
        }

        default:
        {
            return ::android::hidl::base::V1_0::BnHwBase::onTransact(
                    _hidl_code, _hidl_data, _hidl_reply, _hidl_flags, _hidl_cb);
        }
    }

    if (_hidl_err == ::android::UNEXPECTED_NULL) {
        _hidl_err = ::android::hardware::writeToParcel(
                ::android::hardware::Status::fromExceptionCode(::android::hardware::Status::EX_NULL_POINTER),
                _hidl_reply);
    }return _hidl_err;
}

BsNexusDspCb::BsNexusDspCb(const ::android::sp<INexusDspCb> impl) : ::android::hardware::details::HidlInstrumentor("bcm.hardware.nexus@1.0", "INexusDspCb"), mImpl(impl) {
    mOnewayQueue.start(3000 /* similar limit to binderized */);
}

::android::hardware::Return<void> BsNexusDspCb::addOnewayTask(std::function<void(void)> fun) {
    if (!mOnewayQueue.push(fun)) {
        return ::android::hardware::Status::fromExceptionCode(
                ::android::hardware::Status::EX_TRANSACTION_FAILED);
    }
    return ::android::hardware::Status();
}

// static
::android::sp<INexusDspCb> INexusDspCb::tryGetService(const std::string &serviceName, const bool getStub) {
    using ::android::hardware::defaultServiceManager;
    using ::android::hardware::details::waitForHwService;
    using ::android::hardware::getPassthroughServiceManager;
    using ::android::hardware::Return;
    using ::android::sp;
    using Transport = ::android::hidl::manager::V1_0::IServiceManager::Transport;

    sp<INexusDspCb> iface = nullptr;

    const sp<::android::hidl::manager::V1_0::IServiceManager> sm = defaultServiceManager();
    if (sm == nullptr) {
        ALOGE("getService: defaultServiceManager() is null");
        return nullptr;
    }

    Return<Transport> transportRet = sm->getTransport(INexusDspCb::descriptor, serviceName);

    if (!transportRet.isOk()) {
        ALOGE("getService: defaultServiceManager()->getTransport returns %s", transportRet.description().c_str());
        return nullptr;
    }
    Transport transport = transportRet;
    const bool vintfHwbinder = (transport == Transport::HWBINDER);
    const bool vintfPassthru = (transport == Transport::PASSTHROUGH);

    #ifdef __ANDROID_TREBLE__

    #ifdef __ANDROID_DEBUGGABLE__
    const char* env = std::getenv("TREBLE_TESTING_OVERRIDE");
    const bool vintfLegacy = (transport == Transport::EMPTY) && env && !strcmp(env, "true");
    #else // __ANDROID_TREBLE__ but not __ANDROID_DEBUGGABLE__
    const bool vintfLegacy = false;
    #endif // __ANDROID_DEBUGGABLE__

    #else // not __ANDROID_TREBLE__
    const bool vintfLegacy = (transport == Transport::EMPTY);

    #endif // __ANDROID_TREBLE__

    for (int tries = 0; !getStub && (vintfHwbinder || (vintfLegacy && tries == 0)); tries++) {
        Return<sp<::android::hidl::base::V1_0::IBase>> ret = 
                sm->get(INexusDspCb::descriptor, serviceName);
        if (!ret.isOk()) {
            ALOGE("INexusDspCb: defaultServiceManager()->get returns %s", ret.description().c_str());
            break;
        }
        sp<::android::hidl::base::V1_0::IBase> base = ret;
        if (base == nullptr) {
            if (tries > 0) {
                ALOGW("INexusDspCb: found null hwbinder interface");
            }break;
        }
        Return<sp<INexusDspCb>> castRet = INexusDspCb::castFrom(base, true /* emitError */);
        if (!castRet.isOk()) {
            if (castRet.isDeadObject()) {
                ALOGW("INexusDspCb: found dead hwbinder service");
                break;
            } else {
                ALOGW("INexusDspCb: cannot call into hwbinder service: %s; No permission? Check for selinux denials.", castRet.description().c_str());
                break;
            }
        }
        iface = castRet;
        if (iface == nullptr) {
            ALOGW("INexusDspCb: received incompatible service; bug in hwservicemanager?");
            break;
        }
        return iface;
    }
    if (getStub || vintfPassthru || vintfLegacy) {
        const sp<::android::hidl::manager::V1_0::IServiceManager> pm = getPassthroughServiceManager();
        if (pm != nullptr) {
            Return<sp<::android::hidl::base::V1_0::IBase>> ret = 
                    pm->get(INexusDspCb::descriptor, serviceName);
            if (ret.isOk()) {
                sp<::android::hidl::base::V1_0::IBase> baseInterface = ret;
                if (baseInterface != nullptr) {
                    iface = new BsNexusDspCb(INexusDspCb::castFrom(baseInterface));
                }
            }
        }
    }
    return iface;
}

// static
::android::sp<INexusDspCb> INexusDspCb::getService(const std::string &serviceName, const bool getStub) {
    using ::android::hardware::defaultServiceManager;
    using ::android::hardware::details::waitForHwService;
    using ::android::hardware::getPassthroughServiceManager;
    using ::android::hardware::Return;
    using ::android::sp;
    using Transport = ::android::hidl::manager::V1_0::IServiceManager::Transport;

    sp<INexusDspCb> iface = nullptr;

    const sp<::android::hidl::manager::V1_0::IServiceManager> sm = defaultServiceManager();
    if (sm == nullptr) {
        ALOGE("getService: defaultServiceManager() is null");
        return nullptr;
    }

    Return<Transport> transportRet = sm->getTransport(INexusDspCb::descriptor, serviceName);

    if (!transportRet.isOk()) {
        ALOGE("getService: defaultServiceManager()->getTransport returns %s", transportRet.description().c_str());
        return nullptr;
    }
    Transport transport = transportRet;
    const bool vintfHwbinder = (transport == Transport::HWBINDER);
    const bool vintfPassthru = (transport == Transport::PASSTHROUGH);

    #ifdef __ANDROID_TREBLE__

    #ifdef __ANDROID_DEBUGGABLE__
    const char* env = std::getenv("TREBLE_TESTING_OVERRIDE");
    const bool vintfLegacy = (transport == Transport::EMPTY) && env && !strcmp(env, "true");
    #else // __ANDROID_TREBLE__ but not __ANDROID_DEBUGGABLE__
    const bool vintfLegacy = false;
    #endif // __ANDROID_DEBUGGABLE__

    #else // not __ANDROID_TREBLE__
    const bool vintfLegacy = (transport == Transport::EMPTY);

    #endif // __ANDROID_TREBLE__

    for (int tries = 0; !getStub && (vintfHwbinder || (vintfLegacy && tries == 0)); tries++) {
        if (tries > 1) {
            ALOGI("getService: Will do try %d for %s/%s in 1s...", tries, INexusDspCb::descriptor, serviceName.c_str());
            sleep(1);
        }
        if (vintfHwbinder && tries > 0) {
            waitForHwService(INexusDspCb::descriptor, serviceName);
        }
        Return<sp<::android::hidl::base::V1_0::IBase>> ret = 
                sm->get(INexusDspCb::descriptor, serviceName);
        if (!ret.isOk()) {
            ALOGE("INexusDspCb: defaultServiceManager()->get returns %s", ret.description().c_str());
            break;
        }
        sp<::android::hidl::base::V1_0::IBase> base = ret;
        if (base == nullptr) {
            if (tries > 0) {
                ALOGW("INexusDspCb: found null hwbinder interface");
            }continue;
        }
        Return<sp<INexusDspCb>> castRet = INexusDspCb::castFrom(base, true /* emitError */);
        if (!castRet.isOk()) {
            if (castRet.isDeadObject()) {
                ALOGW("INexusDspCb: found dead hwbinder service");
                continue;
            } else {
                ALOGW("INexusDspCb: cannot call into hwbinder service: %s; No permission? Check for selinux denials.", castRet.description().c_str());
                break;
            }
        }
        iface = castRet;
        if (iface == nullptr) {
            ALOGW("INexusDspCb: received incompatible service; bug in hwservicemanager?");
            break;
        }
        return iface;
    }
    if (getStub || vintfPassthru || vintfLegacy) {
        const sp<::android::hidl::manager::V1_0::IServiceManager> pm = getPassthroughServiceManager();
        if (pm != nullptr) {
            Return<sp<::android::hidl::base::V1_0::IBase>> ret = 
                    pm->get(INexusDspCb::descriptor, serviceName);
            if (ret.isOk()) {
                sp<::android::hidl::base::V1_0::IBase> baseInterface = ret;
                if (baseInterface != nullptr) {
                    iface = new BsNexusDspCb(INexusDspCb::castFrom(baseInterface));
                }
            }
        }
    }
    return iface;
}

::android::status_t INexusDspCb::registerAsService(const std::string &serviceName) {
    ::android::hardware::details::onRegistration("bcm.hardware.nexus@1.0", "INexusDspCb", serviceName);

    const ::android::sp<::android::hidl::manager::V1_0::IServiceManager> sm
            = ::android::hardware::defaultServiceManager();
    if (sm == nullptr) {
        return ::android::INVALID_OPERATION;
    }
    ::android::hardware::Return<bool> ret = sm->add(serviceName.c_str(), this);
    return ret.isOk() && ret ? ::android::OK : ::android::UNKNOWN_ERROR;
}

bool INexusDspCb::registerForNotifications(
        const std::string &serviceName,
        const ::android::sp<::android::hidl::manager::V1_0::IServiceNotification> &notification) {
    const ::android::sp<::android::hidl::manager::V1_0::IServiceManager> sm
            = ::android::hardware::defaultServiceManager();
    if (sm == nullptr) {
        return false;
    }
    ::android::hardware::Return<bool> success =
            sm->registerForNotifications("bcm.hardware.nexus@1.0::INexusDspCb",
                    serviceName, notification);
    return success.isOk() && success;
}

static_assert(sizeof(::android::hardware::MQDescriptor<char, ::android::hardware::kSynchronizedReadWrite>) == 32, "wrong size");
static_assert(sizeof(::android::hardware::hidl_handle) == 16, "wrong size");
static_assert(sizeof(::android::hardware::hidl_memory) == 40, "wrong size");
static_assert(sizeof(::android::hardware::hidl_string) == 16, "wrong size");
static_assert(sizeof(::android::hardware::hidl_vec<char>) == 16, "wrong size");

}  // namespace V1_0
}  // namespace nexus
}  // namespace hardware
}  // namespace bcm
