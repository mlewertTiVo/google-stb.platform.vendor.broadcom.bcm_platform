#ifndef _ERROR_STATUS_HEADER_
#define _ERROR_STATUS_HEADER_

typedef enum _ErrorStatus__
{
    ErrStsSuccess=0,
    ErrStsInvalidParam,
    ErrStsClientCreateFail,
    ErrStsDecoAcquireFail,
    ErrStsConnectResourcesFail,
    ErrStsNexusGetDecoStsFailed,
    ErrStsNexusGetDecodedFrFail,
    ErrStsNexusReturnedErr,
    ErrStsDecoNotStarted,
    ErrStsDecodeListEmpty,
    ErrStsDecodeListFull,
    ErrStsStartDecoFailed,
    ErrStsRetFramesNoFrProcessed,
    ErrStsMisalignedFramesDropped,
    ErrStsOutOfResource,
    ErrStsInvalidBuffState,
    ErrStsListCorrupted,
#ifdef BCM_OMX_SUPPORT_ENCODER
    ErrStsEncoAcquireFail,
    ErrStsNexusGetEncoStsFailed,
    ErrStsNexusGetEncodedFrFail,
    ErrStsEncoNotStarted,
    ErrStsEncodeListEmpty,
    ErrStsEncodeListFull,
    ErrStsStartEncoFailed,
    ErrStSTCChannelFailed,
    ErrStsNexusSurfaceFailed
#endif    
}ErrorStatus;


#endif
