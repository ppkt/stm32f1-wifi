#include <stm32f10x.h>
#include <stm32f10x_tim.h>
#include <stm32f10x_rcc.h>
#include <misc.h>
#include <stdio.h>

#include "usart.h"
#include "esp8266.h"
#include "hd44780-i2c.h"
#include "ds18b20.h"

// Warning!
// TIM2 and TIM3 are in use!

u8 line_ready = 0;
char buffer[40];
char packet[128];

char* ip_address = "192.168.0.16";
//char* ip_address = "192.168.0.13"; // rpi

void update_display(simple_float *temperatures, u8 size) {
    hd44780_cmd(0x01);
    u8 len;
    char hd_display[32];
    memset(hd_display, 0, 32);

    if (size >= 1 && temperatures[0].is_valid) {
        sprintf(buffer, "%d.%03u ", temperatures[0].integer, temperatures[0].fractional);
        len = strlen(buffer);
        strcat(hd_display, buffer);
        memset(hd_display + len, ' ', 8 - len);
    }

    if (size >= 2 && temperatures[1].is_valid) {
        sprintf(buffer, "%d.%03u", temperatures[1].integer, temperatures[1].fractional);
        len = strlen(buffer);
        strcat(hd_display, buffer);
    }
    hd44780_print(hd_display);
    memset(hd_display, 0, 32);
    hd44780_go_to_line(1);

    if (size >= 3 && temperatures[2].is_valid) {
        sprintf(buffer, "%d.%03u ", temperatures[2].integer, temperatures[2].fractional);
        len = strlen(buffer);
        strcat(hd_display, buffer);
        memset(hd_display + len, ' ', 8 - len);
    }

    if (size >= 4 && temperatures[3].is_valid) {
        sprintf(buffer, "%d.%03u", temperatures[3].integer, temperatures[3].fractional);
        len = strlen(buffer);
        strcat(hd_display, buffer);
    }
    hd44780_print(hd_display);
}

void get_sensors(char packet[128], ds18b20_devices devices) {
    memset(packet, 0, 128);
    u8 i, j;

    strcat(packet, "1"); // Type of packet (1 - sensors on wire)
    sprintf(buffer, "Found %u devices on 1-Wire", devices.size);
    usart2_print(buffer);
    for (i = 0; i < devices.size; ++i) {
        sprintf(buffer, "Device %u:\r\n", i);
        usart2_print(buffer);
        for (j = 0; j < 8; ++j) {
            memset(buffer, 0, 10);
            sprintf(buffer, "%02X", devices.devices[i].address[j]);
            usart2_print(buffer);
            strcat(packet, buffer);
        }

        if (i + 1 < devices.size) {
            strcat(packet, " ");
        }

        usart2_print("\r\n");
    }
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

    // Initialize WIFI module
    esp8266_init(&line_ready);

    // Initialize 1-Wire bus and thermal sensors conneted to bus
    usart2_print("Initializing thermal sensor\r\n");
    ds18b20_init(GPIOB, GPIO_Pin_9, TIM3);
//    ds18b20_set_precission(0);
    ds18b20_devices devices;
    devices = ds18b20_get_devices(false);
    u8 i;

    get_sensors(packet, devices);

    esp8266_send_data(ip_address, 5000, UDP, packet, strlen(packet));
    esp8266_wait_for_answer();
    // Required in case of UDP, if removed - all packets will be send on port 6000
    esp8266_close_connection();

    usart2_print("Done\r\n");

//	char greeting[] = "Hello, world!";
//	esp8266_send_data(ip_address, 5000, UDP, greeting, strlen(greeting));
//    esp8266_wait_for_answer();
//    esp8266_close_connection();

    int num = 0;

    usart2_print("System ready\r\n");
    while(1)
    {
        if (++num % 10 == 0) {
            devices = ds18b20_get_devices(true);
            get_sensors(packet, devices);
            esp8266_send_data(ip_address, 5000, UDP, packet, strlen(packet));
        }

        usart2_print("i");
        ds18b20_convert_temperature_all();
        usart2_print("I");
        ds18b20_wait_for_conversion();
        usart2_print("j");
        simple_float *temperatures = ds18b20_read_temperature_all();
        usart2_print("J");

        memset(packet, 0, 128);

        strcat(packet, "2"); // Type of packet (2 - sensors data)
        for (i = 0; i < devices.size; ++i) {
            if (!temperatures[i].is_valid) {
                continue;
            }

            sprintf(buffer, "%u=%d.%03u", i, temperatures[i].integer, temperatures[i].fractional);
            strcat(packet, buffer);

            if (i + 1 < devices.size) {
                strcat(packet, " ");
            }
        }

        update_display(temperatures, devices.size);

        usart2_print(packet);

        free(temperatures);
        esp8266_send_data(ip_address, 5000, UDP, packet, strlen(packet));
        esp8266_wait_for_answer();
//        esp8266_close_connection();
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

