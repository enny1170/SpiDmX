#include <DMXSerial.h>
#include <SPI.h>
#include "helper.h"
#include <SoftwareSerial.h>

//#define WITH_BUFFER
#define SPI_ENABLE_INTERRUPT
#ifdef SPI_ENABLE_INTERRUPT
    #define SPI_INTERRUPT_PIN 3
#endif
#define DMX_MODE_Pin 4
#define DEBUG_RX_PIN 5
#define DEBUG_TX_PIN 6

#ifdef WITH_BUFFER
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

// instance SoftwareSerial for debugging, is only used if debugFlag set to true.
SoftwareSerial debug(5,6); //RX, TX Pin

// setup iss called one time during startup
void setup(void)
{
    DMXSerial.init(DMXController);
    debug.begin(115200); // debugging
    debug.println("Setup Slave  Mode");
    pinMode(MISO, OUTPUT);
    pinMode(SS, INPUT_PULLUP);
    pinMode(DMX_MODE_Pin,INPUT_PULLUP);
    #ifdef SPI_ENABLE_INTERRUPT
        pinMode(SPI_INTERRUPT_PIN,INPUT_PULLUP);
    #endif
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
    #ifdef SPI_ENABLE_INTERRUPT
        if(SPI_INTERRUPT_PIN==2)
            attachInterrupt(INT0,Spi_Enable_Interrupt_ISR,CHANGE);
        else
            attachInterrupt(INT1,Spi_Enable_Interrupt_ISR,CHANGE);
    #endif
    debug.println("Interrupt attached");
}

// main loop
void loop(void)
{
    #ifndef SPI_ENABLE_INTERRUPT
        // check the SPI State
        handleSSPin();
    #endif
    // check ModePin State
    handleModePin();
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
                    //currentMode=IDLE;
                    debugOutput("SendData processed.");
                }
                break;
            case RECEIVE_DATA:
                // the transfer will be handled in thes SPI ISR to avoid additional buffers
                // if the cycle over then reset Mode to IDLE
                if(!enabled)
                {
                    //currentMode=IDLE;
                    debugOutput("ReceiveData processed.");
                }
                break;
            default:
                // if the cycle over then reset Mode to IDLE
                if(!enabled)
                {
                    //currentMode=IDLE;
                }
                break;
        }
        // reset the process it flag
        process_it = false;
        // reset the byteCount
        byteCount=0;
    }
}

// SPI EnableInterrupt service routine
// this will called by SPI_ENABLE_INTERRUPT is defined and the SPI_INTERRUPT_PIN is connected to SS
void Spi_Enable_Interrupt_ISR(void)
{
    handleSSPin();
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
/*         switch ((COMM_MODE)c)
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
        } */
    }
    else
    {
        // Write gotten byte to DMX
        DMXSerial.write(byteCount+1,c);
        // Read Next byte from DMX
        SPDR=DMXSerial.read(byteCount+2);
        debug.print((int)c);
        debug.print(" - ");
        debug.println(byteCount);
/*         switch (currentMode)
        {
            case COMM_MODE::SEND_DATA:
                debug.print("Send ");
                debug.print((int)c);
                debug.print(" ");
                DMXSerial.write(byteCount+1,c);
                //write byte for the next cycle
                if(byteCount+2>511)
                {
                    SPDR=0x00;
                }
                else
                {
                    SPDR=DMXSerial.read(byteCount+2);
                }
                //debugOutputWrite(byteCount,c);
                break;
            case COMM_MODE::RECEIVE_DATA:
                c=DMXSerial.read(byteCount);
                debug.print("Receive ");
                debug.print((int)c);
                debug.print(" ");
                //write byte for the next cycle
                if(byteCount+2>511)
                {
                    SPDR=0x00;
                }
                else
                {
                    SPDR=DMXSerial.read(byteCount+2);
                }
                //SPDR=c;
                //debugOutputRead(byteCount,c);
                break;
            case COMM_MODE::IDLE:

            default:
                // do nothing
                debug.print("default with Mode ");
                debug.print((int)currentMode);
                debug.print(" ");
                break;
        }
        //debugOutput(tmp);
 */
    }

    //debug.println((int)c);
    byteCount=byteCount+1;
    // prevent overrun. A DMX universe can only handle 512 channels
    if(byteCount>512)
    {
        debugOutput("buffer > 512 reset them.");
        byteCount=0;
    }

    #else
    //add to buffer if room
    if (byteCount < 512)
    {
        //sprintf(tmp,"Buffer has space byteCount = %i, char = %i",byteCount,(int)c);
        //debugOutput(tmp);
        if(byteCount==-1)
        {
            //debugOutput("bytecount is -1, expecting command");
            //We receive the first byte so we have to interpret them as mode
            switch ((COMM_MODE)c)
            {
                case COMM_MODE::SEND_DATA:
                    initDmxSender();
                    break;
                case COMM_MODE::RECEIVE_DATA:
                    initDmxReceiver();
                    break;
                default:
                    toogleDebug();
                    break;
            } 
        }
        else
        {
            //Collect the bytes into the Buffer for later processing or send the buffer
            switch(currentMode)
            {
                case COMM_MODE::SEND_DATA:
                        rx_buf[byteCount] = c;
                    break;
                case COMM_MODE::RECEIVE_DATA:
                    SPDR=tx_buf[byteCount];
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
    DMXSerial.init(DMXController);
    debugOutput("Set_Sender_Mode_Cmd..");
}

void initDmxReceiver()
{
    currentMode=COMM_MODE::RECEIVE_DATA;
    DMXSerial.init(DMXReceiver);
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
            // now we expect to get 512 bytes from Master
            byteCount=0;
            // reset the process Flag
            process_it=false;
            // store the first value in SPDR
            SPDR=DMXSerial.read(1);
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

void handleModePin(void)
{

    if(digitalRead(DMX_MODE_Pin)==HIGH)
    {
        // init receiver Mode is Pin High and another Mode is set
        if(!currentMode==RECEIVE_DATA)
        {
            initDmxReceiver();
        }
    }
    else
    {
        // init sender Mode is Pin Low and aonother Mode is set
        if(!currentMode==SEND_DATA)
        {
            initDmxSender();
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
        char tmpx[100];
        sprintf(tmpx,"Channel (%i) set to value [%i]",channel,(int)c);
        debug.println(tmpx);
        //debug.print("Write Channel: ");
        //debug.print(channel);
        //debug.print(" Value: ");
        //debug.println((int)c);
    }
}

void debugOutputRead(int channel,uint8_t c)
{
    if(debugFlag)
    {
        char tmpy[100];

        sprintf(tmpy,"Channel (%i) get value [%i]",channel,(int)c);
        debug.println(tmpy);
    }
}
