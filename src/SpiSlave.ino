#include <SPI.h>
#include <helper.h>

char rx_buf[512];
char tx_buf[512];

volatile byte pos;
volatile int byteCount=0;
volatile boolean process_it;
volatile boolean enabled;
volatile COMM_MODE currentMode;
volatile DMX_COMMAND currentCommand;

// Flag for marking Debug Mode. in case of debug we send debug data to serial port else we use it for dmx
boolean DebugFlag=true;

void setup(void)
{
    Serial.begin(115200); // debugging
    Serial.println("Setup Slave  Mode");

    pinMode(MISO, OUTPUT);
    //pinMode(MOSI,INPUT);
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
    Serial.println("Interrupt attached");
}

void loop(void)
{
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
            case COMMAND:
                
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
            case SEND_DATA:
                currentMode=IDLE;
                break;
            case RECEIVE_DATA:
                currentMode=IDLE;
                break;
            default:
                break;
        }
        // process_it            
        debugOutput("Slave receive: ");
        debugOutput(rx_buf);

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
        sprintf(tmp,"Buffer has space byteCount = %i, char = %i",byteCount,(int)c);
        debugOutput(tmp);
        if(byteCount==-1)
        {
            /*
            //We receive the first byte so we have to interpret them as mode
            switch ((COMM_MODE)c)
            {
                case COMM_MODE::SEND_DATA:
                    currentMode=COMM_MODE::SEND_DATA;
                    break;
                case COMM_MODE::RECEIVE_DATA:
                    currentMode=COMM_MODE::RECEIVE_DATA;
                    break;
                default:
                    currentMode=COMM_MODE::COMMAND;
                    break;
            } */
            // we receive the first byte after enable so we interprete them as command
            
            switch ((DMX_COMMAND)c) 
            {
                case DMX_COMMAND::SET_SENDER_MODE:
                    initDmxSender();
                    break;
                case DMX_COMMAND::SET_RECEIVER_MODE:
                    initDmxReceiver();
                    break;
                case DMX_COMMAND::SET_DEBUG_MODE:
                    toogleDebug();
                    break;
                case DMX_COMMAND::GET_BUFFER:
                    sendBuffer();
                    break;
                default:
                    initDmxSender();
                    break;
            }
            pos=0;
            byteCount=0;
            sprintf(tmp,"Command received: %i",(int)c);
            debugOutput(tmp);
        }
        else
        {
            //Collect the bytes into the Buffer for later processing or send the buffer
            if(currentMode!= COMM_MODE::SEND_DATA)
            {
                rx_buf[byteCount] = c;
                debugOutput(".");
            }
            else
            {
                SPI.transfer(tx_buf[byteCount]);
            }
            byteCount++;
        }
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
        Serial.println(data);
    }
}

void debugOutput(char * data)
{
    if(DebugFlag)
    {
        Serial.println(data);
    }
}

void debugOutput(const unsigned char * data)
{
    if(DebugFlag)
    {
        Serial.println((char *)data);
    }
}

void debugOutput(unsigned char * data)
{
    if(DebugFlag)
    {
        Serial.println((char *)data);
    }
}

void initDmxSender()
{
    currentMode=COMMAND;
    currentCommand=SET_SENDER_MODE;
    debugOutput("Set_Sender_Mode_Cmd..");
    process_it=true;
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