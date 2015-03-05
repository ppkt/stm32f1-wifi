#ifndef __USART_H__
#define __USART_H__
#include <stm32f10x.h>
#include "stm32f10x_gpio.h"
#include <stm32f10x_usart.h>
#include <stm32f10x_rcc.h>
#include <misc.h>
#include <string.h>

#define received_string_length 200

void USART1_IRQHandler(void);

void USART1_Init();
void USART2_Init();

void usart1_print(const char* c);
void usart2_print(char* c);

void PrintChar(char c);
void PrintCharPc(char c);

void usart_string_append(char c);
char usart_get_previous_char();
void usart_clear_string();

char* usart_get_string();
u8 usart_get_string_length();

#endif
