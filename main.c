#include <stm32f10x.h>
#include <misc.h>
#include <stdio.h>

#include "usart.h"
#include "esp8266.h"
#include "hd44780-i2c.h"

volatile u8 line_ready = 0;

void NVIC_Configuration(void)
{

    /* 1 bit for pre-emption priority, 3 bits for subpriority */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

    NVIC_SetPriority(I2C1_EV_IRQn, 0x00);
    NVIC_EnableIRQ(I2C1_EV_IRQn);

    NVIC_SetPriority(I2C1_ER_IRQn, 0x01);
    NVIC_EnableIRQ(I2C1_ER_IRQn);


    NVIC_SetPriority(I2C2_EV_IRQn, 0x00);
    NVIC_EnableIRQ(I2C2_EV_IRQn);

    NVIC_SetPriority(I2C2_ER_IRQn, 0x01);
    NVIC_EnableIRQ(I2C2_ER_IRQn);

}

int main(void)
{
    // Initialize USART to WIFI module
    USART1_Init();
    // Initialize USART to PC
    USART2_Init();

    // Setup NVIC
//    NVIC_Configuration();
    // Initialize I2C bus and i2c-hd44780 module
    I2C_LowLevel_Init(I2C1);
    hd44780_init(TIM2);
    hd44780_print("Hello");

    // Setup SysTick
    usart2_print("Setting Core clock\r\n");
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    if (SysTick_Config(SystemCoreClock / 1000))  while (1);
    usart2_print("Done\r\n");

    // Initialize 1-Wire bus and thermal sensors conneted to bus
    usart2_print("Initializing thermal sensor\r\n");
    ds18b20_init(GPIOB, GPIO_Pin_9, TIM3);
    usart2_print("Done\r\n");

    usart2_print("Hello\r\n");

    // Initialize WIFI module
    esp8266_init(&line_ready);

	char greeting[] = "Hello, world!";
	esp8266_send_data("192.168.0.16", 5000, TCP, greeting, strlen(greeting));
    esp8266_wait_for_answer();

    int i = 0;
    int num = 0;
    char buf[20];
    usart2_print("System ready\r\n");
    while(1)
    {
        sprintf(buf, " %d ", num++);
        hd44780_cmd(0x01);
        hd44780_print(buf);

        usart2_print("i");
        ds18b20_read_temperature_all();
        usart2_print("I");
        ds18b20_wait_for_conversion();
        usart2_print("j");
//        usart2_print("%d---\r\n", ds18b20_get_precission());
        ds18b20_convert_temperature_all();
        usart2_print("J");

        esp8266_send_data("192.168.0.16", 5000, TCP, buf, strlen(buf));
        esp8266_wait_for_answer();
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
        if (line_ready == 1)
            usart2_print("*");
    }
}

void SysTick_Handler(void)
{
  delay_decrement();
}

