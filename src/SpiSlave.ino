#include <DMXSerial.h>
#include <SPI.h>
#include "helper.h"
#include <SoftwareSerial.h>

// #define WITH_BUFFER

#if WITH_BUFFER
char rx_buf[512];
char tx_buf[512];
#endif

volatile int byteCount=0;
char tmp[100];

volatile boolean process_it;
// holds the state of SPI Communication
volatile boolean enabled;
// hold the last set DMX Mode
volatile COMM_MODE currentMode;
// the debugFlag is used to enable debug output on the software serial line, can be changed by receive a DebugMode Flag
volatile boolean debugFlag=false;
// instance DMXSerial
DMXSerialClass MYDMX;
// instance SoftwareSerial for debugging, is only used if debugFlag set to true.
SoftwareSerial debug(5,6); //RX, TX Pin

// setup iss called one time during startup
void setup(void)
{
    debug.begin(115200); // debugging
    debug.println("Setup Slave  Mode");
    pinMode(MISO, OUTPUT);
    pinMode(SS, INPUT_PULLUP);
    //turn SPI in Slave-Mode
    SPCR |= bit(SPE);
    //get Ready for Interrupt
    enabled = false;
    currentMode=IDLE;
    process_it = false;
    ResetRxBuffer();
    ResetTxBuffer();
    //turn on Interrupt
    SPI.attachInterrupt();
    debug.println("Interrupt attached");
}

// main loop
void loop(void)
{
    // check the SPI State
    handleSSPin();
    if (process_it)
    {
        switch (currentMode)
        {
            case SEND_DATA:
                // the transfer of bytes is done in the SPI ISR
                // if the cycle over then reset mode to IDLE
                // maybe we run into time problems with ISR 
                if(!enabled)
                {
                    currentMode=IDLE;
                    debugOutput("SendData processed.");
                }
                break;
            case RECEIVE_DATA:
                // the transfer will be handled in thes SPI ISR to avoid additional buffers
                // if the cycle over then reset Mode to IDLE
                if(!enabled)
                {
                    currentMode=IDLE;
                    debugOutput("ReceiveData processed.");
                }
                break;
            default:
                // if the cycle over then reset Mode to IDLE
                if(!enabled)
                {
                    currentMode=IDLE;
                }
                break;
        }
        // reset the process it flag
        process_it = false;
        // reset the byteCount
        byteCount=-1;
    }
}

// SPI interrupt service routine ISR
ISR(SPI_STC_vect)
{
    byte c = SPDR; //grab byte from SPI Register
    #ifndef WITH_BUFFER
    // write and read directly to and from DMXSerial MYDMX
    // is the SPI just enabled we expect the bytecounter is -1 and the 1 byte will set the mode
    if(byteCount==-1)
    {
        switch (c)
        {
            case COMM_MODE::SEND_DATA:
                initDmxSender();
                break;
            case COMM_MODE::RECEIVE_DATA:
                initDmxReceiver();
                break;
            case COMM_MODE::DEBUG:
                toogleDebug();
                break;
            default:
                // do nothing
                break;
        }
    }
    else
    {
        switch (currentMode)
        {
            case COMM_MODE::SEND_DATA:
                MYDMX.write(byteCount,c);
                debugOutputWrite(byteCount,c);
                break;
            case COMM_MODE::RECEIVE_DATA:
                c=MYDMX.read(byteCount);
                SPDR=c;
                debugOutputRead(byteCount,c);
                break;
            default:
                // do nothing
                break;
        }
        debugOutput(tmp);
    }
    byteCount=byteCount+1;
    // prevent overrun. A DMX universe can only handle 512 channels
    if(byteCount>511)
    {
        debugOutput("buffer > 511 reset them.");
        byteCount=0;
    }

    #else
    //add to buffer if room
    if (byteCount < (int)(sizeof(rx_buf)))
    {
        //sprintf(tmp,"Buffer has space byteCount = %i, char = %i",byteCount,(int)c);
        //debugOutput(tmp);
        if(byteCount==-1)
        {
            debugOutput("bytecount is -1, expecting command");
            //We receive the first byte so we have to interpret them as mode
            switch ((COMM_MODE)c)
            {
                case COMM_MODE::SEND_DATA:
                    if(debugFlag)
                    {
                        currentMode=COMM_MODE::SEND_DATA;
                        debugOutput("Set Send_Mode");
                    }
                    else
                    {
                        initDmxSender();
                    }
                    break;
                case COMM_MODE::RECEIVE_DATA:
                    debugOutput("Set Receive_Mode");
                    currentMode=COMM_MODE::RECEIVE_DATA;
                    break;
                default:
                    debugOutput("Set Command_Mode");
                    currentMode=COMM_MODE::COMMAND;
                    break;
            } 
            // we receive the first byte after enable so we interprete them as command
            
            //pos=0;
            //byteCount=0;
            sprintf(tmp,"Command received: %i",(int)c);
            debugOutput(tmp);
        }
        else
        {
            //Collect the bytes into the Buffer for later processing or send the buffer
            switch(currentMode)
            {
                case COMM_MODE::SEND_DATA:
                    if(!debugFlag)
                    {
                        MYDMX.write(byteCount,c);
                    }
                    else
                    {
                        rx_buf[byteCount] = c;
                        debugOutput(".");
                    }
                    break;
                case COMM_MODE::RECEIVE_DATA:
                    SPI.transfer(tx_buf[byteCount]);
                    debugOutput(",");
                    break;
                default:
                    debugOutput("Byte received, no mode set.");
                    break;   
            }
        }
        byteCount++;
    }
    else
    {
        sprintf(tmp,"Buffer overrun byteCount = %i, sizeof(buff) = %i",byteCount,sizeof(rx_buf));
        debugOutput(tmp);
    }

    #endif
}

void ResetRxBuffer(void)
{
    //fill Array ith 0x00
    #ifdef WITH_BUFFER
    memcpy(rx_buf,(char *) '0',sizeof(rx_buf));            
    #endif
    byteCount=-1;
}

void ResetTxBuffer(void)
{
    //fill Array ith 0x00
    #ifdef WITH_BUFFER
    memcpy(tx_buf,(char *) '0',sizeof(tx_buf));            
    #endif
    byteCount=-1;
}


void initDmxSender()
{
    currentMode=COMM_MODE::SEND_DATA;
    MYDMX.init(DMXController);
    debugOutput("Set_Sender_Mode_Cmd..");
}

void initDmxReceiver()
{
    currentMode=COMM_MODE::RECEIVE_DATA;
    MYDMX.init(DMXReceiver);
    debugOutput("Set_Receiver_Mode_Cmd..");
}

void toogleDebug()
{
    if(debugFlag)
    {
        debugFlag=false;
    }
    else
    {
        debugFlag=true;
    }
    debugOutput("Toggle_Debug_Flag_Cmd..");
}


// check this Slave is activated by the Master
// each Communication Cycle will be started by the Master setting the SS-Pin to LOW 
// and closed by set SS-Pin to High. This triggers also the processing in the main loop
void handleSSPin()
{
    if (digitalRead(SS) == LOW)
    {
        if (!enabled)
        {
            // last state was not enabled so we just enable
            enabled = true;
            // now we expect to get 513 bytes from Master, the first one will be interpreted as command
            // expecting a command
            byteCount=-1;
            // reset the process Flag
            process_it=false;
            debugOutput("Enable SPI");
        }
    }
    else
    {
        if (enabled)
        {
            //last state was enabled so we disable
            enabled = false;
            process_it = true;
            debugOutput("Disable SPI");
        }
    }
}

// some Helper Methods
void debugOutput(const char * data)
{
    if(debugFlag)
    {
        debug.println(data);
    }
}

void debugOutput(char * data)
{
    if(debugFlag)
    {
        debug.println(data);
    }
}

void debugOutput(const unsigned char * data)
{
    if(debugFlag)
    {
        debug.println((char *)data);
    }
}

void debugOutput(unsigned char * data)
{
    if(debugFlag)
    {
        debug.println((char *)data);
    }
}

void debugOutputWrite(int channel,uint8_t c)
{
    if(debugFlag)
    {
        sprintf(tmp,"Channel (%i) set to value [%i]",channel,(int)c);
        debug.println(tmp);
    }
}

void debugOutputRead(int channel,uint8_t c)
{
    if(debugFlag)
    {
        sprintf(tmp,"Channel (%i) get value [%i]",channel,(int)c);
        debug.println(tmp);
    }
}
