#ifndef  __MISC_INTERFACES__
#define __MISC_INTERFACES__

/**
 * @brief : This interface is used to notify the Object 
 * that is interested in getting the notification when the 
 * decoder has given out the last frame marked with EOS Flag. 
 *  
 * @function:  OutputEOSDelivered 
 * Notifies the listener that the Ouput EOS is delivered. 
 * @param ClientFlags: The flags that the client wants to 
 * be processed when EOS is received. 
 *  
 * @function: RegisterEOSDoneCallBack Registers the callback 
 * When the EOS processing is complete. 
 * 
 **/

class DecoderEventsListener
{
public:
    virtual void EOSDelivered()=0;
    virtual void FlushStarted()=0;
    virtual void FlushDone()=0;
    virtual ~DecoderEventsListener(){return;}
};


/**
 * @brief  : This Interface is used to talk to the decoder 
 * for starting it using some parameter and Registering Any 
 * type pof listeners with it 
 * 
 */
class StartDecoderIFace
{
public:
    virtual bool StartDecoder(unsigned int)=0;
    virtual ~StartDecoderIFace(){return;}
};

/**
 * @brief : This interface is used to notify the Object 
 * which is interested to get the EOS notification. 
 *  
 * @function:  InputEOSReceived 
 * Notifies the listener that the Input EOS is received. 
 * @param unsigned int ClientFlags: The flags that the client 
 * wants to be processed when EOS is received. 
 *  
 * 
 */

class FeederEventsListener
{
public:
    virtual void InputEOSReceived(unsigned int)=0;
    virtual ~FeederEventsListener(){return;}
};

#endif

