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

/*
 * TUNEABLE DECLARATIONS
 */

 /*
  * Common Definitions
  */
#define OMX_NOPORT              0xFFFFFFFE
#define OMX_TIMEOUT             500            // Timeout value in milisecond
#define OMX_MAX_TIMEOUTS        500            // Count of Maximum number of times the component can time out
#define BUFF_SIGNATURE          0xBCBEEFBC

/*
 * In our OMX implementation we do not fill in the data in the output buffer.
 * We have some sort of "MetaData" in the output buffer. This metadata is then used
 * in order to render the frame. Hence the size of the output buffer is actually constant
 * and does not depend on the video window size. To save memory, we will keep the size of
 * output video buffer as 100 bytes.
 */
#define OUT_VIDEO_BUFF_WIDTH		10
#define OUT_VIDEO_BUFF_HEIGHT		10

/*
 * Definitions For Video Component
 */
#define NUM_IN_BUFFERS          4                // Input Buffers Count
#define INPUT_BUFFER_SIZE       (3072 * 1024)     // 128 KB Of Input Buffer
#define NUM_OUT_BUFFERS         4                // Output Buffers
#define MAX_NUM_OUT_BUFFERS     32               // Max of output buffers

#define NUM_IN_DESCRIPTORS_PER_IO 2
#define NUM_IN_DESCRIPTORS      (NUM_IN_BUFFERS * NUM_IN_DESCRIPTORS_PER_IO * 2)
#define PES_VIDEO_PID           0xE0
#define MAX_NUM_TIMESTAMPS      32              // Max number of entries in timestamp lookup



/*
 *  Definitions For Audio Component
 */
#define NUM_IN_AUDIO_BUFFERS            4               // Input Buffers Count
#define INPUT_AUDIO_BUFFER_SIZE         (16 * 1024)     // 4 KB Of Input Buffer
#define NUM_OUT_AUDIO_BUFFERS           4               // Output Buffers
#define OUTPUT_AUDIO_BUFFER_SIZE        (8 * 1024)     // 8 KB Of Output Buffer

#define NUM_IN_AUDIO_DESCRIPTORS        (NUM_IN_AUDIO_BUFFERS * 2)
#define PES_AUDIO_PID                   0xC0

#ifdef BCM_OMX_SUPPORT_ENCODER
/*
 * Definitions For Video Encoder Component
 */
#define VIDEO_ENCODER_NUM_IN_BUFFERS          4                // Input Buffers Count
#define VIDEO_ENCODER_OUTPUT_BUFFER_SIZE       (3072 * 1024)     // 128 KB Of Input Buffer
#define VIDEO_ENCODER_NUM_OUT_BUFFERS         8                // Output Buffers
#define VIDEO_ENCODER_MAX_NUM_OUT_BUFFERS     32               // Max of output buffers
#define VIDEO_ENCODER_DEFAULT_FRAMERATE         983040          // 15 Frames/sec in Q16 format

#endif
