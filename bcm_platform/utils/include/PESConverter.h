#ifndef _PES_CONVERSION_CLASS_
#define _PES_CONVERSION_CLASS_


/*Required Nexus Files....*/
#include "nexus_platform.h"
#include "nexus_platform_client.h"
#include "nexus_core_utils.h"
#include "nexus_playpump.h"
#include "bioatom.h"
#include "bmedia_util.h"


#define CALCULATE_PTS(t)        (uint64_t(t) * 45LL)


class PESConverter 
{
public :
    PESConverter(unsigned int vPID);

    /* 
     * In case the user wants to send the data directly to the hardware
     * Use this constructor instead and then the send data calls would 
     * succeed.
     */
    PESConverter(unsigned int vPID, NEXUS_PlaypumpHandle nxPPHandle);

    ~PESConverter();
    size_t ProcessESData(unsigned int pts, unsigned char *pBuffer, size_t bufferSz);
    size_t getPESDataLen();
    size_t CopyPESData(void *pBuffer, size_t bufferSz);
    bool SendPESDataToHardware();
    void DumpPESDataToFile(char * FileName);
    bool NotifyEOS();

private :
    
    typedef enum {
        state_init_success=0,
        state_init_uninitialized,
        state_init_err_factory_create,
        state_init_err_accum_create,
    }initState;

    
    /*Instance Variables*/
    initState               ObjState;
    unsigned int            VideoPID;
    batom_factory_t         AtomFactory;
    batom_accum_t           AccumulatorObject;
    uint8_t                 pesHeader[64];  
    batom_cursor            CursorToAccum;
    NEXUS_PlaypumpHandle    NxPlayPumpHandle;
    


    /* TshFifoFull : FIFO Full Threshold
     * If the number of continuous bytes is less than this threshold value
     * we treat it as a temp fifo full condition and do not pump in anymore
     * data. 
     */
    size_t                  TshFifoFull;        

    /*Operations Hidden from outside world*/
    PESConverter (PESConverter& CpyFromMe);
    void operator=(PESConverter& Rhs);

    /*Private operators*/
    void CommonInit();
    bool InitiatePESHeader(unsigned int pts, size_t pktSz); 
    bool ValidateState();
    bool DetectInputFifioFull(size_t requestedSz);
    
};

#endif
