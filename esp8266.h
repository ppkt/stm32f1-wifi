#ifndef __ESP8266_H__
#define __ESP8266_H__
#include <stm32f10x.h>
#include "usart.h"
#include <stdbool.h>

typedef enum {
    AT_CWLAP, // Get AP list
    AT_CIFSR, // Get IP address
    AT_CWJAP,  // Connected AP info
    AT_CWQAP, // Disconnect from AP
    AT_CIPSERVER, // TCP Server
    AT, // Check module presence
    AT_RST, // Restarts device
    ATE0, // Disable echo
    ATE1 // Enable echo
} Operation;

typedef enum {
    SET_EXECUTE,
    INQUIRY
} Type;

typedef enum {
    STATUS_NOT_WORKING = 0,
    STATUS_WORKING,
    STATUS_PROBLEM
} Status;

//void esp8266_send_command(Type type, Operation operation);
//void esp8266_parse_line();
//void esp8266_wait_for_answer();
bool esp8266_check_presence(volatile u8 *line_ready);
void esp8266_reset(volatile u8 *line_ready);
void esp8266_set_echo(bool new_state, volatile u8 *line_ready);


#endif
