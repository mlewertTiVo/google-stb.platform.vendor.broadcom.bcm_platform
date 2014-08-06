
#include <utils/Log.h>
#include "PESConverter.h"

#define LOG_ERROR               LOGD
#define LOG_WARNING             LOGD
#define LOG_INFO                LOGD

void
PESConverter::CommonInit()
{
    if (ObjState ==  state_init_success)
    {
        LOG_ERROR("%s: Object Already Initialized",__FUNCTION__);
        return;
    }

    AtomFactory = batom_factory_create(bkni_alloc, 16);
    if (!AtomFactory)
    {
        AtomFactory = 0;
        LOG_ERROR("Constructor Failed batom_factory_create failed!!");
        ObjState = state_init_err_factory_create;
        return;
    }

    AccumulatorObject = batom_accum_create(AtomFactory);
    if (!AccumulatorObject) 
    { 
        AccumulatorObject = 0;
        LOG_ERROR("Constructor Failed Failed to create accumulator object!!");
        ObjState = state_init_err_accum_create;
        return; 
    }
    ObjState = state_init_success;
    return;
}

PESConverter::PESConverter(unsigned int vPID) :
    VideoPID(vPID),
    ObjState(state_init_uninitialized)
{
    CommonInit();
}

PESConverter :: PESConverter(unsigned int vPID, NEXUS_PlaypumpHandle nxPPHandle):
    VideoPID(vPID),
    ObjState(state_init_uninitialized),
    NxPlayPumpHandle(nxPPHandle)
{
    NEXUS_PlaypumpStatus playpumpStatus;
    CommonInit();
    NEXUS_Playpump_GetStatus(NxPlayPumpHandle,&playpumpStatus);
    TshFifoFull = playpumpStatus.fifoSize - (3 * playpumpStatus.fifoSize) / 4;
}


PESConverter::~PESConverter()
{
    if(AccumulatorObject)
    {
        batom_accum_destroy(AccumulatorObject);
    }

    if(AtomFactory)
    {
        batom_factory_destroy(AtomFactory);
    }

    ObjState = state_init_uninitialized;
}

bool PESConverter::InitiatePESHeader(unsigned int pts, size_t pktSz)
{
    bmedia_pes_info pes_info;
    size_t pes_header_len;

    if(!ValidateState())
    {
        LOG_ERROR("%s Invalid Object State :%d",__FUNCTION__,ObjState);
        return false;
    }

    /*Initialize the PES info structure*/
    bmedia_pes_info_init(&pes_info, VideoPID);

    if(pts)
    {
        uint64_t pts_45Khz = CALCULATE_PTS(pts);
        
//         LOG_INFO("%s Calculated PTS for PES :%lld For VideoPID:%d",
//             __FUNCTION__,pts_45Khz,VideoPID);

        pes_info.pts_valid = true;
        pes_info.pts = (uint32_t)pts_45Khz;
    }

    pes_header_len = bmedia_pes_header_init(pesHeader, pktSz, &pes_info);
    batom_accum_clear(AccumulatorObject);
    batom_accum_add_range(AccumulatorObject, pesHeader, pes_header_len); 
    return true;
}   

size_t
PESConverter::ProcessESData(unsigned int pts, 
                            unsigned char *pBuffer, 
                            size_t bufferSz)
{
    size_t   PESDataSz=0;
    
    if(!ValidateState())
    {
        LOG_ERROR("%s Invalid Object State :%d",__FUNCTION__,ObjState);
        return 0;
    }
    
    if( (!pBuffer) || (!bufferSz))
    {
        LOG_ERROR("%s: Invalid Parameters",__FUNCTION__);
        return 0;
    }

    if(!InitiatePESHeader(pts,bufferSz))
    {
        LOG_ERROR("%s: Failed to Initiate the PES header",__FUNCTION__);
        return 0;
    }

    batom_accum_add_range(AccumulatorObject, pBuffer, bufferSz);
    batom_cursor_from_accum(&CursorToAccum, AccumulatorObject);

    PESDataSz =  getPESDataLen();
    return PESDataSz;
}

size_t
PESConverter::getPESDataLen()
{
    if(!ValidateState())
    {
        LOG_ERROR("%s Invalid Object State :%d",__FUNCTION__,ObjState);
        return 0;
    }

    return batom_cursor_size(&CursorToAccum);
}


/************************************************************************
* Summary: The function copies the PES data to the provided buffer. 
* Returns Amount of data to be copied which may be less than the 
* required size in case there is less data. This is a safe copy
* In case the data is more and the provided buffer is less only 
* the data equal to the size of the provided buffer will be 
* copied.   
*************************************************************************/

size_t 
PESConverter::CopyPESData(void *pBuffer, size_t bufferSz)
{
    size_t copiedSz=0;
    
    if(!ValidateState())
    {
        LOG_ERROR("%s Invalid Object State :%d",__FUNCTION__,ObjState);
        return 0;
    }
    
    if( (!pBuffer) || (!bufferSz))
    {
        LOG_ERROR("%s: Invalid Parameters",__FUNCTION__);
        return 0;
    }

    copiedSz = batom_cursor_copy(&CursorToAccum, pBuffer, bufferSz);
    return copiedSz;
}

bool
PESConverter::SendPESDataToHardware()
{
    void            *pPlayPumpBuffer=NULL;
    size_t          PlayPumpBuffSz=0, PESDataLen=0;
    unsigned int    RetryCnt=700;
    
    if(!ValidateState())
    {
        LOG_ERROR("%s Invalid Object State :%d",__FUNCTION__,ObjState);
        return false;
    }
    
    if (!NxPlayPumpHandle)
    {
        LOG_ERROR("%s: Invalid Parameters [NULL PlayPump To Send Data]",__FUNCTION__);
        return false;
    }

    PESDataLen = getPESDataLen();
    while(PESDataLen)
    {
        size_t   copiedSz=0;
        pPlayPumpBuffer=NULL;
        PlayPumpBuffSz = 0;

        if(!RetryCnt)
        {
           LOG_ERROR("%s: Retry Count Expired, SEND FUNCTION FAILED.... ",__FUNCTION__);
           return false;
        }

//          if(DetectInputFifioFull(PESDataLen))
//          {
//              //LOG_ERROR("%s: FIFO Full Detected......", __FUNCTION__);
//              RetryCnt--;
//              continue;
//          }

        NEXUS_Playpump_GetBuffer(NxPlayPumpHandle,&pPlayPumpBuffer,&PlayPumpBuffSz);

        if (!pPlayPumpBuffer || !PlayPumpBuffSz)
        {
            //LOG_ERROR("%s: NEXUS_Playpump_GetBuffer Retrying", __FUNCTION__);
            RetryCnt--;
            if ( (RetryCnt > 50) && (RetryCnt < 60)) BKNI_Sleep(3);
            continue;
        }
                
        copiedSz = CopyPESData((unsigned char *)pPlayPumpBuffer,PlayPumpBuffSz);
        
        if(copiedSz)
        {
            NEXUS_Error rc = NEXUS_Playpump_WriteComplete(NxPlayPumpHandle, 0, copiedSz);
            if(rc != NEXUS_SUCCESS)
            {
                LOG_ERROR("%s %d : NEXUS_Playpump_WriteComplete FAILED", __FUNCTION__,__LINE__);
                return false;
            }
        }

        PESDataLen -= copiedSz;
        //LOG_INFO("%s PES-Data-Len II :%d",__FUNCTION__,PESDataLen);

        if(PESDataLen)
        { 
            LOG_INFO("%s More Data To Write:%d",__FUNCTION__,PESDataLen);
        }
    }

    return true;
}

bool
PESConverter::DetectInputFifioFull(size_t requestedSz)
{
    size_t  SizeToChk = requestedSz + TshFifoFull;
    size_t  EmptySz=0;

    NEXUS_Error rc = NEXUS_SUCCESS;
    NEXUS_PlaypumpStatus playpumpStatus;

    if(!ValidateState())
    {
        LOG_ERROR("%s Invalid Object State :%d",__FUNCTION__,ObjState);
        return false;
    }

    rc = NEXUS_Playpump_GetStatus(NxPlayPumpHandle, &playpumpStatus);
    if (rc != NEXUS_SUCCESS) 
    {
        LOG_ERROR("%s : BCMESPlayer::sendData: NEXUS_Playpump_GetStatus failed!!",__FUNCTION__);
        return true; /*Assume FIFO is FULL*/
    }

    //Reduce the fifo size by threshold amount.....

    //playpumpStatus.fifoSize = playpumpStatus.fifoSize - TshFifoFull;
    EmptySz = playpumpStatus.fifoSize - playpumpStatus.fifoDepth;

    if(SizeToChk > EmptySz)
    {
//         ..LOG_ERROR("%s : FIFO FULL CONDITION HIT [FD:%d TSH:%d FS:%d] SizeToChk:%d EmptySz:%d !!",__FUNCTION__,
//             playpumpStatus.fifoDepth, TshFifoFull, playpumpStatus.fifoSize,SizeToChk,EmptySz);
        
        return true;
    }

    return false;
}

bool
PESConverter::NotifyEOS()
{
    unsigned char * b_eos_filler;
    unsigned char b_eos_h264[4] = 
    {
        0x00, 0x00, 0x01, 0x0A
    };

    if(!ValidateState())
    {
        LOG_ERROR("%s Invalid Object State :%d",__FUNCTION__,ObjState);
        return false;
    }

    b_eos_filler = (unsigned char *) malloc(256);
    if(!b_eos_filler)
    {
        LOG_ERROR("%s Memory Allocation Failure",__FUNCTION__);
        return false;
    }

    memset(b_eos_filler,0,256);
    InitiatePESHeader(0,sizeof(b_eos_h264));
    batom_accum_add_range(AccumulatorObject, b_eos_h264, sizeof(b_eos_h264));
    batom_cursor_from_accum(&CursorToAccum, AccumulatorObject);

    if(!SendPESDataToHardware())
    {
        LOG_ERROR("%s Sending EOS Failed !!",__FUNCTION__);
        return false;
    }

    InitiatePESHeader(0, 16 * 256);

    for(unsigned i = 0; i < 16; i++) 
    {
        batom_accum_add_range(AccumulatorObject, b_eos_h264, 256);
    }

    batom_cursor_from_accum(&CursorToAccum, AccumulatorObject);
    if(!SendPESDataToHardware())
    {
        LOG_ERROR("%s Sending EOS Fillers Failed !!",__FUNCTION__);
        return false;
    }

    return true;
}

bool
PESConverter::ValidateState()
{
    if(ObjState !=  state_init_success)
        return false;

    return true;
}

/*Implement this later*/
void
PESConverter::DumpPESDataToFile(char * FileName)
{
    return ;
}

