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
    usart1_print("ATE0\r\n");
    int i = 0;
    while(1)
    {
        esp8266_send_command(INQUIRY, AT);
        for (i = 0; i < 10000000; ++i) {
//            __WFI();
            if (line_ready) {
                esp8266_parse_line();
                line_ready = 0;
            }
        }
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
