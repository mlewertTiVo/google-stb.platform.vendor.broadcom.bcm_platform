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

#define NUM_IN_DESCRIPTORS      (NUM_IN_BUFFERS * 2)
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
#endif

