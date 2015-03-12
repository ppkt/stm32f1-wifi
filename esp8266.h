#ifndef __ESP8266_H__
#define __ESP8266_H__
#include <stm32f10x.h>
#include "usart.h"
#include <stdbool.h>
#include <stdlib.h>

typedef enum {
    AT_CWLAP, // Get AP list
    AT_CIFSR, // Get IP address
    AT_CWJAP,  // Connected AP info
    AT_CWQAP, // Disconnect from AP
    AT_CIPSERVER, // TCP Server
    AT, // Check module presence
    AT_RST, // Restarts device
    ATE0, // Disable echo
    ATE1, // Enable echo
    AT_CWMODE, // Wifi mode (1 = Static, 2 = AP, 3 = both)
    AT_CIPMUX, // 0 for single connection mode, 1 for multiple connection
    AT_CIPSTART, // establish connetion with remote host
    AT_CIPSEND, // begin sending data
    AT_SEND_DATA, // send data
    AT_CLOSE // close connection
} Operation;

typedef enum {
    TYPE_SET_EXECUTE,
    TYPE_INQUIRY,
    TYPE_TEST
} Type;

typedef enum {
    STATUS_NOT_WORKING = 0,
    STATUS_WORKING,
    STATUS_PROBLEM
} ESP8266_Status;

typedef enum {
	ENCRYPTION_NONE = 0,
	ENCRYPTION_WEP,
	ENCRYPTION_WPA_PSK,
	ENCRYPTION_WPA_PSK2,
	ENCRYPTION_WPA2_PSK
} Encryption;

typedef struct {
	Encryption encryption;
	char* essid;
	short int signal;
	char* mac;
	unsigned short int channel;
} Access_Point;

typedef enum {
    TCP = 0,
    UDP
} Protocol;

void esp8266_init(u8* line_ready);
void esp8266_wait_for_answer(void);
bool esp8266_check_presence(void);
void esp8266_reset(void);
void esp8266_set_echo(bool new_state);
void esp8266_set_mode(u8 mode);
char* esp8266_get_ip_addresses(void);
void esp8266_get_connected_ap(void);
void esp8266_get_list_of_aps(void);
void esp8266_join_ap(char* login, char* password);
void esp8266_connection_mode(u8 mode);
void esp8266_send_data(char* ip_address, u16 port, Protocol protocol, char* data, u8 length);
void esp8266_close_connection(void);

void esp8266_debug_print_connected_ap(void);
void esp8266_debug_print_list_of_aps(void);
void esp8266_debug_print_ip_address(void);

#endif
