#define LOG_TAG "bcm.hardware.nexus@1.0::NexusDspCb"

#include <android/log.h>
#include <cutils/trace.h>
#include <hidl/HidlTransportSupport.h>

#include <android/hidl/manager/1.0/IServiceManager.h>
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
    return ::android::hardware::details::castInterface<INexusDspCb, ::android::hidl::base::V1_0::IBase, BpHwNexusDspCb>(
            parent, "bcm.hardware.nexus@1.0::INexusDspCb", emitError);
}

BpHwNexusDspCb::BpHwNexusDspCb(const ::android::sp<::android::hardware::IBinder> &_hidl_impl)
        : BpInterface<INexusDspCb>(_hidl_impl),
          ::android::hardware::details::HidlInstrumentor("bcm.hardware.nexus@1.0", "INexusDspCb") {
}

// Methods from INexusDspCb follow.
::android::hardware::Return<void> BpHwNexusDspCb::_hidl_onDsp(::android::hardware::IInterface *_hidl_this, ::android::hardware::details::HidlInstrumentor *_hidl_this_instrumentor) {
    #ifdef __ANDROID_DEBUGGABLE__
    bool mEnableInstrumentation = _hidl_this_instrumentor->isInstrumentationEnabled();
    const auto &mInstrumentationCallbacks = _hidl_this_instrumentor->getInstrumentationCallbacks();
    #else
    (void) _hidl_this_instrumentor;
    #endif // __ANDROID_DEBUGGABLE__
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

    _hidl_err = _hidl_data.writeInterfaceToken(BpHwNexusDspCb::descriptor);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_err = ::android::hardware::IInterface::asBinder(_hidl_this)->transact(1 /* onDsp */, _hidl_data, &_hidl_reply, ::android::hardware::IBinder::FLAG_ONEWAY);
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


// Methods from INexusDspCb follow.
::android::hardware::Return<void> BpHwNexusDspCb::onDsp(){
    ::android::hardware::Return<void>  _hidl_out = ::bcm::hardware::nexus::V1_0::BpHwNexusDspCb::_hidl_onDsp(this, this);

    return _hidl_out;
}


// Methods from ::android::hidl::base::V1_0::IBase follow.
::android::hardware::Return<void> BpHwNexusDspCb::interfaceChain(interfaceChain_cb _hidl_cb){
    ::android::hardware::Return<void>  _hidl_out = ::android::hidl::base::V1_0::BpHwBase::_hidl_interfaceChain(this, this, _hidl_cb);

    return _hidl_out;
}

::android::hardware::Return<void> BpHwNexusDspCb::debug(const ::android::hardware::hidl_handle& fd, const ::android::hardware::hidl_vec<::android::hardware::hidl_string>& options){
    ::android::hardware::Return<void>  _hidl_out = ::android::hidl::base::V1_0::BpHwBase::_hidl_debug(this, this, fd, options);

    return _hidl_out;
}

::android::hardware::Return<void> BpHwNexusDspCb::interfaceDescriptor(interfaceDescriptor_cb _hidl_cb){
    ::android::hardware::Return<void>  _hidl_out = ::android::hidl::base::V1_0::BpHwBase::_hidl_interfaceDescriptor(this, this, _hidl_cb);

    return _hidl_out;
}

::android::hardware::Return<void> BpHwNexusDspCb::getHashChain(getHashChain_cb _hidl_cb){
    ::android::hardware::Return<void>  _hidl_out = ::android::hidl::base::V1_0::BpHwBase::_hidl_getHashChain(this, this, _hidl_cb);

    return _hidl_out;
}

::android::hardware::Return<void> BpHwNexusDspCb::setHALInstrumentation(){
    ::android::hardware::Return<void>  _hidl_out = ::android::hidl::base::V1_0::BpHwBase::_hidl_setHALInstrumentation(this, this);

    return _hidl_out;
}

::android::hardware::Return<bool> BpHwNexusDspCb::linkToDeath(const ::android::sp<::android::hardware::hidl_death_recipient>& recipient, uint64_t cookie){
    ::android::hardware::ProcessState::self()->startThreadPool();
    ::android::hardware::hidl_binder_death_recipient *binder_recipient = new ::android::hardware::hidl_binder_death_recipient(recipient, cookie, this);
    std::unique_lock<std::mutex> lock(_hidl_mMutex);
    _hidl_mDeathRecipients.push_back(binder_recipient);
    return (remote()->linkToDeath(binder_recipient) == ::android::OK);
}

::android::hardware::Return<void> BpHwNexusDspCb::ping(){
    ::android::hardware::Return<void>  _hidl_out = ::android::hidl::base::V1_0::BpHwBase::_hidl_ping(this, this);

    return _hidl_out;
}

::android::hardware::Return<void> BpHwNexusDspCb::getDebugInfo(getDebugInfo_cb _hidl_cb){
    ::android::hardware::Return<void>  _hidl_out = ::android::hidl::base::V1_0::BpHwBase::_hidl_getDebugInfo(this, this, _hidl_cb);

    return _hidl_out;
}

::android::hardware::Return<void> BpHwNexusDspCb::notifySyspropsChanged(){
    ::android::hardware::Return<void>  _hidl_out = ::android::hidl::base::V1_0::BpHwBase::_hidl_notifySyspropsChanged(this, this);

    return _hidl_out;
}

::android::hardware::Return<bool> BpHwNexusDspCb::unlinkToDeath(const ::android::sp<::android::hardware::hidl_death_recipient>& recipient){
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

BnHwNexusDspCb::~BnHwNexusDspCb() {
    ::android::hardware::details::gBnMap.eraseIfEqual(_hidl_mImpl.get(), this);
}

// Methods from INexusDspCb follow.
::android::status_t BnHwNexusDspCb::_hidl_onDsp(
        ::android::hidl::base::V1_0::BnHwBase* _hidl_this,
        const ::android::hardware::Parcel &_hidl_data,
        ::android::hardware::Parcel *_hidl_reply,
        TransactCallback _hidl_cb) {
    #ifdef __ANDROID_DEBUGGABLE__
    bool mEnableInstrumentation = _hidl_this->isInstrumentationEnabled();
    const auto &mInstrumentationCallbacks = _hidl_this->getInstrumentationCallbacks();
    #endif // __ANDROID_DEBUGGABLE__

    ::android::status_t _hidl_err = ::android::OK;
    if (!_hidl_data.enforceInterface(BnHwNexusDspCb::Pure::descriptor)) {
        _hidl_err = ::android::BAD_TYPE;
        return _hidl_err;
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

    static_cast<BnHwNexusDspCb*>(_hidl_this)->_hidl_mImpl->onDsp();

    (void) _hidl_cb;

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

    return _hidl_err;
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
            _hidl_err = ::bcm::hardware::nexus::V1_0::BnHwNexusDspCb::_hidl_onDsp(this, _hidl_data, _hidl_reply, _hidl_cb);
            break;
        }

        case 256067662 /* interfaceChain */:
        {
            _hidl_err = ::android::hidl::base::V1_0::BnHwBase::_hidl_interfaceChain(this, _hidl_data, _hidl_reply, _hidl_cb);
            break;
        }

        case 256131655 /* debug */:
        {
            _hidl_err = ::android::hidl::base::V1_0::BnHwBase::_hidl_debug(this, _hidl_data, _hidl_reply, _hidl_cb);
            break;
        }

        case 256136003 /* interfaceDescriptor */:
        {
            _hidl_err = ::android::hidl::base::V1_0::BnHwBase::_hidl_interfaceDescriptor(this, _hidl_data, _hidl_reply, _hidl_cb);
            break;
        }

        case 256398152 /* getHashChain */:
        {
            _hidl_err = ::android::hidl::base::V1_0::BnHwBase::_hidl_getHashChain(this, _hidl_data, _hidl_reply, _hidl_cb);
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
            _hidl_err = ::android::hidl::base::V1_0::BnHwBase::_hidl_ping(this, _hidl_data, _hidl_reply, _hidl_cb);
            break;
        }

        case 257049926 /* getDebugInfo */:
        {
            _hidl_err = ::android::hidl::base::V1_0::BnHwBase::_hidl_getDebugInfo(this, _hidl_data, _hidl_reply, _hidl_cb);
            break;
        }

        case 257120595 /* notifySyspropsChanged */:
        {
            _hidl_err = ::android::hidl::base::V1_0::BnHwBase::_hidl_notifySyspropsChanged(this, _hidl_data, _hidl_reply, _hidl_cb);
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
                ::android::hardware::Status::EX_TRANSACTION_FAILED,
                "Passthrough oneway function queue exceeds maximum size.");
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
    const bool trebleTestingOverride =  env && !strcmp(env, "true");
    const bool vintfLegacy = (transport == Transport::EMPTY) && trebleTestingOverride;
    #else // __ANDROID_TREBLE__ but not __ANDROID_DEBUGGABLE__
    const bool trebleTestingOverride = false;
    const bool vintfLegacy = false;
    #endif // __ANDROID_DEBUGGABLE__

    #else // not __ANDROID_TREBLE__
    const char* env = std::getenv("TREBLE_TESTING_OVERRIDE");
    const bool trebleTestingOverride =  env && !strcmp(env, "true");
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
                    iface = INexusDspCb::castFrom(baseInterface);
                    if (!getStub || trebleTestingOverride) {
                        iface = new BsNexusDspCb(iface);
                    }
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
    const bool trebleTestingOverride =  env && !strcmp(env, "true");
    const bool vintfLegacy = (transport == Transport::EMPTY) && trebleTestingOverride;
    #else // __ANDROID_TREBLE__ but not __ANDROID_DEBUGGABLE__
    const bool trebleTestingOverride = false;
    const bool vintfLegacy = false;
    #endif // __ANDROID_DEBUGGABLE__

    #else // not __ANDROID_TREBLE__
    const char* env = std::getenv("TREBLE_TESTING_OVERRIDE");
    const bool trebleTestingOverride =  env && !strcmp(env, "true");
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
                    iface = INexusDspCb::castFrom(baseInterface);
                    if (!getStub || trebleTestingOverride) {
                        iface = new BsNexusDspCb(iface);
                    }
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
