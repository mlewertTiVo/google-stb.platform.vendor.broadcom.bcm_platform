/******************************************************************************
 * (c) 2014 Broadcom Corporation
 *
 * This program is the proprietary software of Broadcom Corporation and/or its
 * licensors, and may only be used, duplicated, modified or distributed pursuant
 * to the terms and conditions of a separate, written license agreement executed
 * between you and Broadcom (an "Authorized License").  Except as set forth in
 * an Authorized License, Broadcom grants no license (express or implied), right
 * to use, or waiver of any kind with respect to the Software, and Broadcom
 * expressly reserves all rights in and to the Software and all intellectual
 * property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
 * HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
 * NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
 *
 * Except as expressly set forth in the Authorized License,
 *
 * 1. This program, including its structure, sequence and organization,
 *    constitutes the valuable trade secrets of Broadcom, and you shall use all
 *    reasonable efforts to protect the confidentiality thereof, and to use
 *    this information only in connection with your use of Broadcom integrated
 *    circuit products.
 *
 * 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 *    AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
 *    WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT
 *    TO THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED
 *    WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A
 *    PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET
 *    ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME
 *    THE ENTIRE RISK ARISING OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
 *
 * 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS
 *    LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT,
 *    OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO
 *    YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN
 *    ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS
 *    OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER
 *    IS GREATER. THESE LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF
 *    ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
 ******************************************************************************/

#include <pthread.h>        /*for POSIX calls */

#include "TuneableDefines.h"

#ifdef ENABLE_BCM_OMX_PROTOTYPE
#include "OMXNexusDecoder.h"
#include "OMXNexusAudioDecoder.h"
#include "PESFeeder.h"
#include "AndroidVideoWindow.h"
#include "HardwareAPI.h"
#ifdef BCM_OMX_SUPPORT_ENCODER
#include "OMXNexusVideoEncoder.h"
#endif
#endif
#ifdef ENABLE_SECURE_DECODERS
#include "sage_srai.h"
#endif

/*
 *     D E F I N I T I O N S
 */

typedef struct _BufferList BufferList;


/*
 * The main structure for buffer management.
 *
 *   pBufHdr     - An array of pointers to buffer headers.
 *                 The size of the array is set dynamically using the nBufferCountActual value
 *                   send by the client.
 *   nListEnd    - Marker to the boundary of the array. This points to the last index of the
 *                   pBufHdr array.
 *   nSizeOfList - Count of valid data in the list.
 *   nAllocSize  - Size of the allocated list. This is equal to (nListEnd + 1) in most of
 *                   the times. When the list is freed this is decremented and at that
 *                   time the value is not equal to (nListEnd + 1). This is because
 *                   the list is not freed from the end and hence we cannot decrement
 *                   nListEnd each time we free an element in the list. When nAllocSize is zero,
 *                   the list is completely freed and the other paramaters of the list are
 *                   initialized.
 *                 If the client crashes before freeing up the buffers, this parameter is
 *                   checked (for nonzero value) to see if there are still elements on the list.
 *                   If yes, then the remaining elements are freed.
 *    nWritePos  - The position where the next buffer would be written. The value is incremented
 *                   after the write. It is wrapped around when it is greater than nListEnd.
 *    nReadPos   - The position from where the next buffer would be read. The value is incremented
 *                   after the read. It is wrapped around when it is greater than nListEnd.
 *    eDir       - Type of BufferList.
 *                            OMX_DirInput  =  Input  Buffer List
 *                           OMX_DirOutput  =  Output Buffer List
 *    mutex      - Mutex used for BufferList synchronization.
 */
struct _BufferList{
   OMX_BUFFERHEADERTYPE **pBufHdr;
   OMX_S32 nListEnd;
   OMX_S32 nSizeOfList;
   OMX_S32 nAllocSize;
   OMX_S32 nWritePos;
   OMX_S32 nReadPos;
   OMX_DIRTYPE eDir;
   pthread_mutex_t mutex;
};

static void* ComponentThread(void* pThreadData);


typedef struct _CMD_DATA_
{
    OMX_U32 Cmd;
    OMX_U32 CmdData;
}CMD_DATA,*PCMD_DATA;

/*
 * Enumeration for the commands processed by the component
 */

typedef enum ThrCmdType
{
    SetState,
    Flush,
    StopPort,
    RestartPort,
    MarkBuf,
    Stop,
    FillBuf,
    EmptyBuf,
    SendFillBuffDoneNtf,
    Undefined
} ThrCmdType;

#define SIZEOF_BCMADTS_HEADER   7

//
// This Structure holds the final AAC parameters
// which can be used to generate the ADTS header
// for RAW AAC. ADTS Header is aslo in the same structure.
//
typedef struct _BCM_AAC_PARAMETERS_
{
    unsigned int                    nSampleRate;
    unsigned int                    nChannels;
    OMX_AUDIO_AACSTREAMFORMATTYPE   eAACStreamFormat;
    OMX_AUDIO_AACPROFILETYPE        eAACProfileType;
    unsigned char                   BCMAdtsHeader[7];
}BCM_AAC_PARAMETERS,*PBCM_AAC_PARAMETERS;

typedef struct bcmOmxTimestampEntry
{
    bool entryUsed;
    uint64_t originalTimestamp;
    uint32_t convertedTimestamp;
} bcmOmxTimestampEntry;

class bcmOmxTimestampTable
{
public:
    bcmOmxTimestampTable(unsigned int size);
    ~bcmOmxTimestampTable();
    uint32_t store(uint64_t original_ts);
    uint64_t retrieve(uint32_t converted_ts);
    void flush();
    bool isFull();

private:
    Mutex mMutex;
    bcmOmxTimestampEntry *pTimestamps;
    unsigned int size;
    unsigned int usage_count;
};

/*
 * Private data of the component.
 * It includes input and output port related information (Port definition, format),
 *   buffer related information, specifications for the video decoder, the command and data pipes
 *   and the BufferList structure for storing input and output buffers.
 */

typedef struct _BCM_OMX_CONTEXT_
{
    OMX_STATETYPE                   state;
    OMX_CALLBACKTYPE                *pCallbacks;
    OMX_PTR                         pAppData;
    OMX_HANDLETYPE                  hSelf;
    OMX_PORT_PARAM_TYPE             sPortParam;
    OMX_PARAM_PORTDEFINITIONTYPE    sInPortDef;
    OMX_PARAM_PORTDEFINITIONTYPE    sOutPortDef;
    OMX_VIDEO_PARAM_PORTFORMATTYPE  sInPortFormat;
    OMX_VIDEO_PARAM_PORTFORMATTYPE  sOutPortFormat;
    OMX_PRIORITYMGMTTYPE            sPriorityMgmt;
    OMX_PARAM_BUFFERSUPPLIERTYPE    sInBufSupplier;
    OMX_PARAM_BUFFERSUPPLIERTYPE    sOutBufSupplier;
    OMX_VIDEO_PARAM_MPEG2TYPE       sMpeg2;
    struct EnableAndroidNativeBuffersParams sNativeBufParam;
    pthread_t                       thread_id;
    OMX_U32                         cmdpipe[2];
    ThrCmdType                      eTCmd;
    BufferList                      sInBufList;
    BufferList                      sOutBufList;
    OMX_BUFFERHEADERTYPE            **pOutBufHdrRes;
    OMX_BUFFERHEADERTYPE            **pInBufHdrRes;
#ifdef ENABLE_BCM_OMX_PROTOTYPE
    BCM_AAC_PARAMETERS              BCMAacParams;
    OMXNexusDecoder                 *pOMXNxDecoder;
    OMXNexusAudioDecoder            *pOMXNxAudioDec;
    PESFeeder                       *pPESFeeder;
    AndroidVideoWindow              *pAndVidWindow;
    bcmOmxTimestampTable            *pTstable;
    bool                            bSetStartUpTimeDone;
    bool                            bHwNeedsFlush;
    bool                            bUseNoDataCopy;
#endif
#ifdef BCM_OMX_SUPPORT_ENCODER
    OMXNexusVideoEncoder            *pOMXNxVidEnc;
    OMX_VIDEO_PARAM_AVCTYPE         sAvcVideoParams;
    OMX_VIDEO_PARAM_BITRATETYPE     sRateParams;
    bool                            bMetaDataInBuffer;
#endif
#ifdef ENABLE_SECURE_DECODERS
    SRAI_PlatformHandle sage_platformHandle;
    BSAGElib_InOutContainer *container;
#endif
} BCM_OMX_CONTEXT;


/*
 *     M A C R O S
 */



/*
 * Initializes a data structure using a pointer to the structure.
 * The initialization of OMX structures always sets up the nSize and nVersion fields
 *   of the structure.
 */
#define OMX_CONF_INIT_STRUCT_PTR(_s_, _name_)   \
    memset((_s_), 0x0, sizeof(_name_)); \
    (_s_)->nSize = sizeof(_name_);      \
    (_s_)->nVersion.s.nVersionMajor = 0x1;  \
    (_s_)->nVersion.s.nVersionMinor = 0x0;  \
    (_s_)->nVersion.s.nRevision = 0x0;      \
    (_s_)->nVersion.s.nStep = 0x0



/*
 * Checking for version compliance.
 * If the nSize of the OMX structure is not set, raises bad parameter error.
 * In case of version mismatch, raises a version mismatch error.
 */
#define OMX_CONF_CHK_VERSION(_s_, _name_, _e_)              \
    if((_s_)->nSize != sizeof(_name_)) _e_ = OMX_ErrorBadParameter; \
    if(((_s_)->nVersion.s.nVersionMajor != 0x1)||           \
       ((_s_)->nVersion.s.nVersionMinor != 0x0)||           \
       ((_s_)->nVersion.s.nRevision != 0x0)||               \
       ((_s_)->nVersion.s.nStep != 0x0)) _e_ = OMX_ErrorVersionMismatch;\
    if(_e_ != OMX_ErrorNone) goto OMX_CONF_CMD_BAIL;



/*
 * Checking paramaters for non-NULL values.
 * The macro takes three parameters because inside the code the highest
 *   number of parameters passed for checking in a single instance is three.
 * In case one or two parameters are passed, the ramaining parameters
 *   are set to 1 (or a nonzero value).
 */
#define OMX_CONF_CHECK_CMD(_ptr1, _ptr2, _ptr3) \
{                       \
    if(!_ptr1 || !_ptr2 || !_ptr3){     \
        eError = OMX_ErrorBadParameter;     \
    goto OMX_CONF_CMD_BAIL;         \
    }                       \
}



/*
 * Redirects control flow in an error situation.
 * The OMX_CONF_CMD_BAIL label is defined inside the calling function.
 */
#define OMX_CONF_BAIL_IF_ERROR(_eError)     \
{                       \
    if(_eError != OMX_ErrorNone)        \
        goto OMX_CONF_CMD_BAIL;         \
}



/*
 * Sets error type and redirects control flow to error handling and cleanup section
 */
#define OMX_CONF_SET_ERROR_BAIL(_eError, _eCode)\
{                       \
    _eError = _eCode;               \
    goto OMX_CONF_CMD_BAIL;         \
}


/*
 * Allocates a new entry in a BufferList.
 * Finds the position where memory has to be allocated.
 * Actual allocation happens in the caller function.
 */
#define ListAllocate(_pH, _nIndex)              \
   pthread_mutex_lock(&_pH.mutex);              \
   if (_pH.nListEnd == -1) {                    \
      _pH.nListEnd = 0;                         \
      _pH.nWritePos = 0;                        \
   }                                            \
   else {                                       \
      _pH.nListEnd++;                           \
   }                                            \
   _pH.nAllocSize++;                            \
   _nIndex = _pH.nListEnd;                      \
   pthread_mutex_unlock(&_pH.mutex);




/*
 * Sets an entry in the BufferList.
 * The entry set is a BufferHeader.
 * The nWritePos value is incremented after the write.
 * It is wrapped around when it is greater than nListEnd.
 */
#define ListSetEntry(_pH, _pB)                  \
   pthread_mutex_lock(&_pH.mutex);              \
   if (_pH.nSizeOfList < (_pH.nListEnd + 1)) {  \
      _pH.nSizeOfList++;                        \
      _pH.pBufHdr[_pH.nWritePos++] = _pB;       \
      if (_pH.nReadPos <= -1)                   \
         _pH.nReadPos = 0;                      \
      if (_pH.nWritePos > _pH.nListEnd)         \
         _pH.nWritePos = 0;                     \
   }                                            \
   pthread_mutex_unlock(&_pH.mutex);




/*
 * Gets an entry from the BufferList
 * The entry is a BufferHeader
 * The nReadPos value is incremented after the read.
 * It is wrapped around when it is greater than nListEnd.
 */
#define ListGetEntry(_pH, _pB)                  \
   pthread_mutex_lock(&_pH.mutex);              \
   if (_pH.nSizeOfList > 0){                    \
      _pH.nSizeOfList--;                        \
      _pB = _pH.pBufHdr[_pH.nReadPos++];        \
      if (_pH.nReadPos > _pH.nListEnd)          \
         _pH.nReadPos = 0;                      \
      }                                         \
   pthread_mutex_unlock(&_pH.mutex);


typedef void (*CleanUpFunc)(OMX_BUFFERHEADERTYPE *);

/*
 * Flushes all entries from the BufferList structure.
 * The nSizeOfList gives the number of valid entries in the list.
 * The nReadPos value is incremented after the read.
 * It is wrapped around when it is greater than nListEnd.
 * CleanUpFunc is always called before the buffer is returned                                                             .
 * To give a chance to the client to cleanup.                                                                                                                        .
 */

#define ListFlushEntriesWithCleanup(_pH, _pC, CleanUpFunc)                                          \
{                                                                                                   \
    ALOGD("ListFlushEntriesWithCleanup: ListSizeToFlush:%d \n",_pH.nSizeOfList);                    \
    pthread_mutex_lock(&_pH.mutex);                                                                 \
    while (_pH.nSizeOfList > 0)                                                                     \
    {                                                                                               \
        _pH.nSizeOfList--;                                                                          \
        if (_pH.eDir == OMX_DirInput)                                                               \
        {                                                                                           \
            pthread_mutex_unlock(&_pH.mutex);                                                       \
            CleanUpFunc(_pH.pBufHdr[_pH.nReadPos++]);                                               \
            _pC->pCallbacks->EmptyBufferDone(_pC->hSelf,_pC->pAppData,_pH.pBufHdr[_pH.nReadPos++]); \
            pthread_mutex_lock(&_pH.mutex);                                                         \
        } else if (_pH.eDir == OMX_DirOutput){                                                      \
            pthread_mutex_unlock(&_pH.mutex);                                                       \
            CleanUpFunc(_pH.pBufHdr[_pH.nReadPos++]);                                               \
            _pC->pCallbacks->FillBufferDone(_pC->hSelf, _pC->pAppData, _pH.pBufHdr[_pH.nReadPos++]);\
            pthread_mutex_lock(&_pH.mutex);                                                         \
        }                                                                                           \
                                                                                                    \
        if (_pH.nReadPos > _pH.nListEnd)                                                            \
        {                                                                                           \
            _pH.nReadPos = 0;                                                                       \
        }                                                                                           \
    }                                                                                               \
    pthread_mutex_unlock(&_pH.mutex);                                                               \
}
/*
 * Flushes all entries from the BufferList structure.
 * The nSizeOfList gives the number of valid entries in the list.
 * The nReadPos value is incremented after the read.
 * It is wrapped around when it is greater than nListEnd.
 */
#define ListFlushEntries(_pH, _pC)                                                                  \
    pthread_mutex_lock(&_pH.mutex);                                                                 \
    while (_pH.nSizeOfList > 0)                                                                     \
    {                                                                                               \
       _pH.nSizeOfList--;                                                                           \
       if (_pH.eDir == OMX_DirInput)                                                                \
       {                                                                                            \
           pthread_mutex_unlock(&_pH.mutex);                                                        \
           _pC->pCallbacks->EmptyBufferDone(_pC->hSelf,_pC->pAppData,_pH.pBufHdr[_pH.nReadPos++]);  \
           pthread_mutex_lock(&_pH.mutex);                                                          \
       } else if (_pH.eDir == OMX_DirOutput){                                                       \
           pthread_mutex_unlock(&_pH.mutex);                                                        \
           _pC->pCallbacks->FillBufferDone(_pC->hSelf,_pC->pAppData,_pH.pBufHdr[_pH.nReadPos++]);   \
           pthread_mutex_lock(&_pH.mutex);                                                          \
       }                                                                                            \
       if (_pH.nReadPos > _pH.nListEnd)                                                             \
       {                                                                                            \
          _pH.nReadPos = 0;                                                                         \
       }                                                                                            \
    }                                                                                               \
    pthread_mutex_unlock(&_pH.mutex);


/*
 * Frees the memory allocated for BufferList entries
 *   by comparing with client supplied buffer header.
 * The nAllocSize value gives the number of allocated (i.e. not free'd) entries in the list.
 * When nAllocSize is zero, the list is completely freed
 *   and the other paramaters of the list are initialized.
 */
#define ListFreeBuffer(_pH, _pB, _pP)                                   \
    pthread_mutex_lock(&_pH.mutex);                                     \
    for (OMX_S32 _nIndex = 0; _nIndex <= _pH.nListEnd; _nIndex++)       \
    {                                                                   \
        if (_pH.pBufHdr[_nIndex] == _pB)                                \
        {                                                               \
           _pH.nAllocSize--;                                            \
           if (_pH.pBufHdr[_nIndex])                                    \
           {                                                            \
              if (_pH.pBufHdr[_nIndex]->pBuffer)                        \
                 _pH.pBufHdr[_nIndex]->pBuffer = NULL;                  \
              OMX_BUFFERHEADERTYPE *bufhdr =                            \
                (OMX_BUFFERHEADERTYPE *)_pH.pBufHdr[_nIndex];           \
              free(bufhdr);                                             \
              _pH.pBufHdr[_nIndex] = NULL;                              \
           }                                                            \
                                                                        \
           if (_pH.nAllocSize <= 0)                                     \
           {                                                            \
              _pH.nWritePos = -1;                                       \
              _pH.nReadPos = -1;                                        \
              _pH.nListEnd = -1;                                        \
              _pH.nSizeOfList = 0;                                      \
              _pP->bPopulated = OMX_FALSE;                              \
           }                                                            \
           break;                                                       \
        }                                                               \
    }                                                                   \
    pthread_mutex_unlock(&_pH.mutex);

#define ListFreeBufferExt(_pH, _pB, _pP, prevAllocSize, _pHB)           \
    pthread_mutex_lock(&_pH.mutex);                                     \
    for (OMX_S32 _nIndex = 0; _nIndex <= _pH.nListEnd; _nIndex++)       \
    {                                                                   \
        if (_pHB[_nIndex] == _pB)                                       \
        {                                                               \
            if (_pHB[_nIndex])                                          \
            {                                                           \
                if (prevAllocSize==_pH.nAllocSize)                      \
                {                                                       \
                    _pH.nAllocSize--;                                   \
                    free(_pB);                                          \
                }                                                       \
                _pH.pBufHdr[_nIndex] = NULL;                            \
            }                                                           \
            if (_pH.nAllocSize <= 0)                                    \
            {                                                           \
                _pH.nWritePos = (OMX_U32)-1;                            \
                _pH.nReadPos = (OMX_U32)-1;                             \
                _pH.nListEnd = (OMX_U32)-1;                             \
                _pH.nSizeOfList = 0;                                    \
                _pP->bPopulated = OMX_FALSE;                            \
            }                                                           \
            break;                                                      \
        }                                                               \
    }                                                                   \
    pthread_mutex_unlock(&_pH.mutex);

/*
 * Frees the memory allocated for BufferList entries.
 * This is called in case the client crashes suddenly before freeing all the component buffers.
 * The nAllocSize parameter is
 *   checked (for nonzero value) to see if there are still elements on the list.
 * If yes, then the remaining elements are freed.
 */
#define ListFreeAllBuffers(_pH, _nIndex)                             \
    pthread_mutex_lock(&_pH.mutex);                                  \
    for (_nIndex = 0; _nIndex <= _pH.nListEnd; _nIndex++){           \
        if (_pH.pBufHdr[_nIndex]){                                   \
           _pH.nAllocSize--;                                         \
           if (_pH.pBufHdr[_nIndex]->pBuffer)                        \
              _pH.pBufHdr[_nIndex]->pBuffer = NULL;                  \
           OMX_BUFFERHEADERTYPE *bufhdr = (OMX_BUFFERHEADERTYPE *)_pH.pBufHdr[_nIndex]; \
           free(bufhdr);                                             \
           _pH.pBufHdr[_nIndex] = NULL;                              \
           if (_pH.nAllocSize <= 0){                                 \
              _pH.nWritePos = -1;                                    \
              _pH.nReadPos = -1;                                     \
              _pH.nListEnd = -1;                                     \
              _pH.nSizeOfList = 0;                                   \
              }                                                      \
           }                                                         \
        }                                                            \
    pthread_mutex_unlock(&_pH.mutex);



/*
 * Loads the parameters of the buffer header.
 * When the list has nBufferCountActual elements allocated
 * then the bPopulated value of port definition is set to true.
 */
#define LoadBufferHeader(_pList, _pBufHdr, _pAppPrivate, _nSizeBytes, _nPortIndex,    \
                                                            _ppBufHdr, _pPortDef)     \
    pthread_mutex_lock(&_pList.mutex);                                                \
    _pBufHdr->nAllocLen = _nSizeBytes;                                                \
    _pBufHdr->pAppPrivate = _pAppPrivate;                                             \
    if (_pList.eDir == OMX_DirInput){                                                 \
       _pBufHdr->nInputPortIndex = _nPortIndex;                                       \
       _pBufHdr->nOutputPortIndex = OMX_NOPORT;                                       \
       }                                                                              \
    else{                                                                             \
       _pBufHdr->nInputPortIndex = OMX_NOPORT;                                        \
       _pBufHdr->nOutputPortIndex = _nPortIndex;                                      \
       }                                                                              \
    _ppBufHdr = _pBufHdr;                                                             \
    if (_pList.nListEnd == ((OMX_S32)(_pPortDef->nBufferCountActual) - 1))            \
       _pPortDef->bPopulated = OMX_TRUE;                                              \
    pthread_mutex_unlock(&_pList.mutex);

#define CopyBufferHeaderList(_pList, _pPortDef, _pBufHdr)                            \
    pthread_mutex_lock(&_pList.mutex);                                               \
    if (_pPortDef->bPopulated == OMX_TRUE) {                                         \
        OMX_S32 i;                                                                   \
        for (i = 0; i <= _pList.nListEnd; i++) {                                     \
            _pBufHdr[i] = _pList.pBufHdr[i];                                         \
        }                                                                            \
    }                                                                                \
    pthread_mutex_unlock(&_pList.mutex);

