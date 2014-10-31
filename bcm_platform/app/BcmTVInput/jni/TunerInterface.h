#ifndef _TUNER_INTERFACE_H_
#define _TUNER_INTERFACE_H_

#define GET_TUNER_CONTEXT       0x1112
#define TUNER_INTERFACE_NAME    "com.broadcom.tunerhal.TunerInterface"

#define android_DECLARE_META_INTERFACE(INTERFACE)                               \
        static const android::String16 descriptor;                      \
        static android::sp<I##INTERFACE> asInterface(const android::sp<android::IBinder>& obj); \
        static android::String16 getInterfaceDescriptor();

#define android_IMPLEMENT_META_INTERFACE(INTERFACE, NAME)                       \
        const android::String16 I##INTERFACE::descriptor(NAME);         \
        android::String16 I##INTERFACE::getInterfaceDescriptor() { \
        return I##INTERFACE::descriptor;                                \
    }                                                                   \
        android::sp<I##INTERFACE> I##INTERFACE::asInterface(const android::sp<android::IBinder>& obj) \
    {                                                                   \
            android::sp<I##INTERFACE> intr;                             \
        if (obj != NULL) {                                              \
            intr = static_cast<I##INTERFACE*>(                          \
                obj->queryLocalInterface(                               \
                        I##INTERFACE::descriptor).get());               \
            if (intr == NULL) {                                         \
                intr = new Bp##INTERFACE(obj);                          \
            }                                                           \
        }                                                               \
        return intr;                                                    \
    }

class INexusTunerClient: public android::IInterface {
public:
    android_DECLARE_META_INTERFACE(NexusTunerClient)

    virtual android::IBinder* get_remote() = 0;
};
#endif