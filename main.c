#include <stm32f10x.h>
#include <misc.h>
#include <stdio.h>

#include "usart.h"
#include "esp8266.h"

volatile u8 line_ready = 0;

int main(void)
{
    USART1_Init();
    USART2_Init();

    usart2_print("Hello\r\n");

    esp8266_init(&line_ready);

//    // Reset and set mode 3
//    esp8266_reset(&line_ready);
//    esp8266_set_mode(3);

    // disable echo
    esp8266_set_echo(false, &line_ready);
    esp8266_wait_for_answer(&line_ready);

//    // enable echo
//    esp8266_set_echo(true, &line_ready);
//    esp8266_wait_for_answer(&line_ready);

	esp8266_check_presence(&line_ready);


	// get information (ESSID) about AP
//	esp8266_get_connected_ap();
//	esp8266_debug_print_connected_ap();

    // check IP address (should work)
//	esp8266_get_ip_addresses();
//    esp8266_debug_print_ip_address();

	// get list of Access Points
//	esp8266_get_list_of_aps();
//    esp8266_debug_print_list_of_aps();

//	esp8266_check_presence(&line_ready);

	// Setup module to work in single connection mode
	esp8266_connection_mode(0);

	esp8266_check_presence(&line_ready);
    esp8266_wait_for_answer(&line_ready);

	char greeting[] = "Hello, world!";
	esp8266_send_data("192.168.0.16", 5000, TCP, greeting, strlen(greeting) - 1);
    esp8266_wait_for_answer(&line_ready);

    int i = 0;
    while(1)
    {
        for (i = 0; i < 10000000; ++i) {}
        esp8266_send_data("192.168.0.16", 5000, TCP, greeting, strlen(greeting) - 1);
        esp8266_wait_for_answer(&line_ready);
    }
}


void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        char c = USART_ReceiveData(USART1);

        usart_string_append(c);
        if (c == '\n') {
            line_ready = 1;
        } else if (c == ' ') {
            if (usart_get_previous_char() == '>') {
                line_ready = 1;
            }
        }
    }
}
