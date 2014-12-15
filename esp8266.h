#ifndef __ESP8266_H__
#define __ESP8266_H__
#include <stm32f10x.h>
#include "usart.h"

typedef enum {
    AT_CWLAP, // Get AP list
    AT_CIFSR, // Get IP address
    AT_CWJAP,  // Connected AP info
    AT_CWQAP, // Disconnect from AP
    AT_CIPSERVER, // TCP Server
    AT // Check module presence
} Operation;

typedef enum {
    SET_EXECUTE,
    INQUIRY
} Type;

typedef enum {
    STATUS_WORKING,
    STATUS_NOT_WORKING,
    STATUS_PROBLEM
} Status;

void esp8266_send_command(Type type, Operation operation);
void esp8266_parse_line();

#endif