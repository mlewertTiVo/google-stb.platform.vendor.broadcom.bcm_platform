#ifndef _BCM_DEBUG_H_
#define _BCM_DEBUG_H_

#include <utils/Log.h>
#include <stdio.h>

//#define ENABLE_FUNCTION_TRACE   ALOGE

#define BCMOMX_DBG_ASSERT(__exp_) if(__exp_);\
else { \
    ALOGE("%s:%d !!======ASSERTION FAILED======!!",__PRETTY_FUNCTION__,__LINE__); \
    BDBG_ASSERT(__exp_);\
}

class DumpData
{
public:
    DumpData(const char *);
    ~DumpData();
    unsigned int WriteData(void *,size_t);
private:
    FILE *OpenFile;
    unsigned int NumberOfWrites;
    /*Operations Hidden from outside world*/
    DumpData (DumpData& CpyFromMe);
    void operator=(DumpData& Rhs);
};


class trace
{
public:
    trace(char const * functionName);
    trace(char const * functionName, char * params);
    ~trace();

private:
    char *funcName;
};
#endif


