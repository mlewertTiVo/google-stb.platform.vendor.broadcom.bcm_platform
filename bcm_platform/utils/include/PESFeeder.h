#ifndef _PES_CONVERSION_CLASS_
#define _PES_CONVERSION_CLASS_

#include <utils/Log.h>
#include <utils/threads.h> // Mutex Class operations
#include "BcmDebug.h"

/*Required Nexus Files....*/

#include "nexus_platform.h"
#include "nexus_platform_client.h"
#include "nexus_core_utils.h"
#include "nexus_playpump.h"
#include "bioatom.h"
#include "bmedia_util.h"
#include "MiscIFace.h"
#include "LinkedList.h"
#include "TuneableDefines.h"


//#define DEBUG_PES_DATA  

using namespace android;
#define PES_HEADER_SIZE             128
#define PES_EOS_BUFFER_SIZE         1024
#define CODEC_CONFIG_BUFFER_SIZE    1024
#define PES_BUFFER_SIZE(_BuffSz_)   (PES_HEADER_SIZE + _BuffSz_ + (4 * 1024) + CODEC_CONFIG_BUFFER_SIZE)

typedef void (*BUFFER_DONE_CALLBACK) (unsigned int, unsigned int,unsigned int);

typedef struct _DONE_CONTEXT_
{   
    unsigned int            Param1;
    unsigned int            Param2;
    unsigned int            Param3;
    BUFFER_DONE_CALLBACK    pFnDoneCallBack;
}DONE_CONTEXT;

typedef struct _NEXUS_INPUT_CONTEXT_
{
    //
    // We Combine and copy the PES Data In to Another Buffer
    // And Fire that buffer using NEXUS DESCRIPTOR.
    //
    unsigned char   *PESData;
    size_t          SzPESBuffer;        // Total Size Of the PES Buffer We Allocated
    size_t          SzValidPESData;     // Size Of Valid Data In PES Buffer.
    NEXUS_PlaypumpScatterGatherDescriptor NxDesc[1];  
    DONE_CONTEXT    DoneContext;
    LIST_ENTRY      ListEntry;
}NEXUS_INPUT_CONTEXT, *PNEXUS_INPUT_CONTEXT;

class DataSender
{
public:
    virtual size_t ProcessESData(unsigned int pts, 
                                unsigned char *pDataBuffer, 
                                size_t SzDataBuff,
                                unsigned char *pOutData,
                                size_t SzOutData)=0;

    virtual bool SendPESDataToHardware(PNEXUS_INPUT_CONTEXT)=0;
    virtual void *AllocatePESBuffer(size_t)=0;
    virtual void FreePESBuffer(void *)=0;
    virtual ~DataSender(){return;}
private:

};

//Forward Declarations
class CfgMgr;
class ConfigDataMgr;
class ConfigDataMgrWithSend;
class ConfigDataMgrFactory;

class PESFeeder : public DecoderEventsListener, 
                  public DataSender
{
public :
    PESFeeder(char const *ClientName,
              unsigned int vPID, 
              unsigned int NumDescriptors, 
              NEXUS_TransportType NxTransType,
              NEXUS_VideoCodec vidCdc=NEXUS_VideoCodec_eNone);

    ~PESFeeder();

    size_t ProcessESData(unsigned int pts, 
                         unsigned char *pDataBuffer, 
                         size_t SzDataBuff,
                         unsigned char *pOutData,
                         size_t SzOutData);

    size_t  ProcessESData(unsigned int pts,
                          unsigned char *pHeaderToInsert,
                          size_t SzHdrBuff,
                          unsigned char *pDataBuffer, 
                          size_t SzDataBuff,
                          unsigned char *pOutData,
                          size_t SzOutData);

    size_t CopyPESData(void *pBuffer, size_t bufferSz);
    bool SendPESDataToHardware(PNEXUS_INPUT_CONTEXT);
    void XFerDoneCallBack();

    // This one in unmanaged Buffer Allocation
    void * AllocatePESBuffer(size_t SzPESBuffer);
    void FreePESBuffer(void *);

    bool StartDecoder(StartDecoderIFace *pStartDecoIface);
    bool RegisterFeederEventsListener(FeederEventsListener *pInEvtLisnr);
    bool NotifyEOS(unsigned int);

    bool Flush();

    //Implement the DecoderEventsListener
    void FlushStarted();
    void FlushDone();
    void EOSDelivered();

    bool SaveCodecConfigData(void *,size_t SzData);
private :
    //Scope limited class
    class Pauser
    {
    public:
        Pauser(NEXUS_PlaypumpHandle);
        ~Pauser();
    private:
        NEXUS_PlaypumpHandle NxPPHandle;
        Pauser (Pauser& CpyFromMe);
        void operator=(Pauser& Rhs);
    };
    
    //Event listeners 
    FeederEventsListener            *FdrEvtLsnr;

    /*Instance Variables*/
    unsigned int                    NexusPID;
    batom_factory_t                 AtomFactory;
    batom_accum_t                   AccumulatorObject;
    batom_cursor                    CursorToAccum;
    NEXUS_PlaypumpHandle            NxPlayPumpHandle;
    NEXUS_PidChannelHandle          NxVidPidChHandle;


    unsigned char                   *pPESEosBuffer;
    

    //Size of Valid Codec Config Data. MAX is CODEC_CONFIG_BUFFER_SIZE
    bool                            SendCfgDataOnNextInput;
    CfgMgr                          *pCfgDataMgr;
    NEXUS_VideoCodec                vidCodec;    
    
    //For Internal Data Transfer
    PNEXUS_INPUT_CONTEXT            pInterNotifyCnxt;

    //Usage
    //Mutex::Autolock lock(nexSurf->mFreeListLock);
    unsigned int                    CntDesc;    //Number Of Descriptors We have Requestd  
    Mutex                           mListLock;
   
    LIST_ENTRY                      ActiveQ;  // Fired Desc Wait Here i.e We Can Fire More Than One  
    unsigned int                    FiredCnt; // Check Later If This Is Needed!!  

    //Flush Statistics
    unsigned int                    FlushCnt;
    unsigned char                   ClientIDString[128];
#ifdef DEBUG_PES_DATA
    DumpData                        *DataBeforFlush;
    DumpData                        *DataAfterFlush;
#endif 

private :
    
    size_t InitiatePESHeader(unsigned int PtsUs, 
                           size_t SzDataBuff,
                           unsigned char *pHeaderBuff,
                           size_t SzHdrBuff);
    
    bool PrepareEOSData();  
    void DiscardConfigData();
    void StopPlayPump();
    void StartPlayPump();
    bool IsPlayPumpStarted();

private :
    /*Operations Hidden from outside world*/
    PESFeeder (PESFeeder& CpyFromMe);
    void operator=(PESFeeder& Rhs);
    
};

//Default Configuration Data Manager Class
class CfgMgr
{
public:
    CfgMgr(){return;};
    virtual ~CfgMgr(){return;};
    virtual void DiscardConfigData()
    {
        ALOGD("%s: Ignored ",__PRETTY_FUNCTION__);
        return;
    };

    virtual bool SaveConfigData(void *pData, size_t szData)=0;
    virtual void SendConfigDataToHW()
    {
        ALOGD("%s: Ignored ",__PRETTY_FUNCTION__);
        return;
    };

    virtual bool AccumulateConfigData(batom_accum_t)
    {
        ALOGD("%s: Ignored ",__PRETTY_FUNCTION__);
        return true;
    };

    virtual size_t GetConfigDataSz()=0;
private:
    /*Operations Hidden from outside world*/
    CfgMgr (CfgMgr& CpyFromMe);
    void operator=(CfgMgr& Rhs);

};

class ConfigDataMgr : public CfgMgr
{
public:
    ConfigDataMgr();
    ~ConfigDataMgr();
    bool SaveConfigData(void *, size_t );
    bool AccumulateConfigData(batom_accum_t);
    size_t GetConfigDataSz();
    void DiscardConfigData();

private:
    //Size of Valid Codec Config Data. MAX is CODEC_CONFIG_BUFFER_SIZE
    size_t          SzCodecConfigData;
    unsigned char   *pCodecConfigData;

private:
    /*Operations Hidden from outside world*/
    ConfigDataMgr (CfgMgr& CpyFromMe);
    void operator=(ConfigDataMgr& Rhs);
};

static 
void ConfigDataSentAsPES(unsigned int Param1, 
                         unsigned int Param2, 
                         unsigned int Param3);

class ConfigDataMgrWithSend : public CfgMgr
{
public:
    ConfigDataMgrWithSend(DataSender&,unsigned int);
    ~ConfigDataMgrWithSend(); 
    bool SaveConfigData(void *, size_t);
    void SendConfigDataToHW();
    void DiscardConfigData();
    size_t GetConfigDataSz();

private:
    //Size of Valid Codec Config Data. MAX is CODEC_CONFIG_BUFFER_SIZE
    size_t          SzCodecConfigData;
    unsigned char   *pCodecConfigData;

    //PES converted Codec Config Data
    const unsigned int    allocedSzPESBuffer;

    size_t          SzPESCodecConfigData;
    unsigned char   *pPESCodecConfigData;

    //The sender interface
    DataSender&     Sender;

    unsigned int    ptsToUseForPES;
    NEXUS_INPUT_CONTEXT   XferContext;
    BKNI_EventHandle      XferDoneEvt;  
    void XferDoneNotification();


    friend void ConfigDataSentAsPES(unsigned int Param1, 
                                    unsigned int Param2, 
                                    unsigned int Param3);

private:
    /*Operations Hidden from outside world*/
    ConfigDataMgrWithSend (CfgMgr& CpyFromMe);
    void operator=(ConfigDataMgrWithSend& Rhs);
};

class ConfigDataMgrFactory
{
public:
    static CfgMgr* CreateConfigDataMgr(NEXUS_VideoCodec,
                                       DataSender&);

private:
    ConfigDataMgrFactory(){return;};
};

static
CfgMgr*
CreateCfgDataMgr(NEXUS_VideoCodec vidCodec, DataSender& Sender);

#endif
