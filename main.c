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
    esp8266_init(&line_ready);

    esp8266_reset(&line_ready);
    esp8266_wait_for_answer(&line_ready);

    // disable echo
    esp8266_set_echo(false, &line_ready);
    esp8266_wait_for_answer(&line_ready);

    // enable echo
    esp8266_set_echo(true, &line_ready);
    esp8266_wait_for_answer(&line_ready);

	esp8266_check_presence(&line_ready);

	// check IP address (should work)
	esp8266_get_ip_addresses();

	esp8266_check_presence(&line_ready);
    int i = 0;
    while(1)
    {
        for (i = 0; i < 10000000; ++i) {}
    }
}


void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        char c = USART_ReceiveData(USART1);

        usart_string_append(c);
        if (c == '\n') {
            line_ready = 1;
        }
    }
}
