#include <DMXSerial.h>
#include <SPI.h>
#include <helper.h>
#include <SoftwareSerial.h>

char rx_buf[512];
char tx_buf[512];

volatile byte pos;
volatile int byteCount=0;
volatile boolean process_it;
volatile boolean enabled;
volatile COMM_MODE currentMode;
volatile DMX_COMMAND currentCommand;
DMXSerialClass MYDMX;
SoftwareSerial debug(5,6); //RX, TX Pin

// Flag for marking Debug Mode. in case of debug we send debug data to serial port else we use it for dmx
boolean DebugFlag=true;

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
    currentMode=RECEIVE_DATA;
    process_it = false;
    ResetRxBuffer();
    ResetTxBuffer();
    //turn on Interrupt
    SPI.attachInterrupt();
    debug.println("Interrupt attached");
}

void loop(void)
{
    char tmp[100];
    //Serial.println("loop");
    if (digitalRead(SS) == LOW)
    {
        if (!enabled)
        {
            //last state was not enabled so we just enable
            enabled = true;
            //pinMode(MISO,OUTPUT);
            //pinMode(MOSI,INPUT);
            if(currentMode==IDLE)
            {
                //expecting a command
                pos = -1;
                byteCount=-1;
            }
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
            //pinMode(MISO,INPUT);
            //pinMode(MOSI,OUTPUT);
            process_it=true;
            debugOutput("Disable SPI");
        }
    }

    if (process_it)
    {
        
        switch (currentMode)
        {
/*             case COMMAND:
                
                switch (currentCommand)
                {
                    case SET_SENDER_MODE:
                        currentMode=SEND_DATA;
                        ResetRxBuffer();
                        byteCount=0;
                        break;
                    case SET_RECEIVER_MODE:
                        currentMode=RECEIVE_DATA;
                        ResetTxBuffer();
                        byteCount=0;
                        break;
                    case SET_DEBUG_MODE:
                        break;
                    default:
                        currentMode=IDLE;
                        break;
                }
                break;
 */
            case SEND_DATA:
                currentMode=IDLE;
                sprintf(tmp,"Buffer has received %i bytes",byteCount);
                debugOutput(tmp);            
                debugOutput("Slave receive: ");
                debugOutput(rx_buf);
                break;
            case RECEIVE_DATA:
                debugOutput("Sending out tx_buffer ");
                for(int i=0;i<512;i++)
                {
                    rx_buf[i]=SPI.transfer((unsigned char)tx_buf[i]);
                }
                //currentMode=IDLE;
                break;
            default:
                break;
        }
        // process_it

        process_it = false;
        ResetRxBuffer();    
    }
}

// SPI interrupt routine
ISR(SPI_STC_vect)
{
    byte c = SPDR; //grab byte from SPI Register
    char tmp[100];
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
                    if(DebugFlag)
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
                    if(!DebugFlag)
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
}

void ResetRxBuffer(void)
{
    //fill Array ith 0x00
    memcpy(rx_buf,(char *) '0',sizeof(rx_buf));            
    pos=-1;
    byteCount=-1;
}

void ResetTxBuffer(void)
{
    //fill Array ith 0x00
    memcpy(tx_buf,(char *) '0',sizeof(tx_buf));            
    pos=-1;
    byteCount=-1;
}

void debugOutput(const char * data)
{
    if(DebugFlag)
    {
        debug.println(data);
    }
}

void debugOutput(char * data)
{
    if(DebugFlag)
    {
        debug.println(data);
    }
}

void debugOutput(const unsigned char * data)
{
    if(DebugFlag)
    {
        debug.println((char *)data);
    }
}

void debugOutput(unsigned char * data)
{
    if(DebugFlag)
    {
        debug.println((char *)data);
    }
}

void initDmxSender()
{
    if(!currentMode==COMM_MODE::SEND_DATA)
    {
        if(!DebugFlag)
        {
            MYDMX.init(DMXController);
        }
        else
        {
            debugOutput("Set_Sender_Mode_Cmd..");
        }
        currentMode=COMM_MODE::SEND_DATA;
    }
}

void initDmxReceiver()
{
    currentMode=COMMAND;
    currentCommand=SET_RECEIVER_MODE;
    debugOutput("Set_Receiver_Mode_Cmd..");
    process_it=true;
}

void toogleDebug()
{
    if(DebugFlag)
    {
        DebugFlag=false;
    }
    else
    {
        DebugFlag=true;
    }
    debugOutput("Toggle_Debug_Flag_Cmd..");
}

void sendBuffer()
{

}