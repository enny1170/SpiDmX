#ifndef HELPER_H
#define HELPER_H

enum COMM_MODE
{
    SEND_DATA=0x01,
    RECEIVE_DATA=0x00,
    COMMAND=0x02,
    IDLE=0x03,
    DEBUG=0x04
} ;

#endif