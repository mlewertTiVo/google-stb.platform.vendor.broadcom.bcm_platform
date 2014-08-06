#include "BcmDebug.h"

DumpData::DumpData(const char * FileName)
            :NumberOfWrites(0)
{
    char FileNameString[256];
    sprintf(FileNameString,"/mnt/sdcard1/%s",FileName);
    ALOGD("Opening File %s",FileNameString);
    OpenFile = fopen(FileNameString,"wb");
    if (!OpenFile) 
    {
        ALOGD("Opening File %s Failed",FileNameString);
    }
}

DumpData::~DumpData()
{
    ALOGD("Close File Now");
    fclose(OpenFile);
}

unsigned int
DumpData::WriteData(void * StartAddr, size_t Sz)
{
    size_t WrittenSz=0;
    ALOGD("Write: File:%p Addr:%p Size:%d",OpenFile,StartAddr,Sz);
    WrittenSz = fwrite(StartAddr,1,Sz,OpenFile);
    if (WrittenSz != Sz) 
    {
       ALOGD("Failed To Write Data On TO File");
       return 0;
    }

    NumberOfWrites++;
    return NumberOfWrites;
}


// Trace Class Implementation

trace::trace(char const * functionName)
{
#ifdef ENABLE_FUNCTION_TRACE
    funcName = strdup(functionName);
    ENABLE_FUNCTION_TRACE("%s : %s : ENTER \n",LOG_TAG, funcName);
#endif
}

trace::trace(char const * functionName, char * params)
{
#ifdef ENABLE_FUNCTION_TRACE
    funcName = strdup(functionName);
    ENABLE_FUNCTION_TRACE("%s : %s : ENTER Params [%s]\n",LOG_TAG, funcName,params);
#endif
}

trace::~trace()
{
#ifdef ENABLE_FUNCTION_TRACE
    ENABLE_FUNCTION_TRACE("%s : %s : EXIT \n",LOG_TAG, funcName);
    delete funcName;
#endif
}


