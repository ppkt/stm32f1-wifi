#include "esp8266.h"
#include <string.h>

Operation op;
Type t;
Status s = STATUS_NOT_WORKING;

const char ok_answer[] = "\r\nOK\r\n";
const size_t ok_answer_length = strlen(ok_answer);
const char ready_answer[] = "\r\nready\r\n";
const size_t ready_answer_length = strlen(ready_answer);

const char at_inquiry[] = "AT\r\n";
const char at_rst[] = "AT+RST\r\n";
const char ate0[] = "ATE0\r\n";
const char ate1[] = "ATE1\r\n";
const char at_cifsr_t[] = "AT+CIFSR=?\r\n";

volatile u8* l_ready = NULL;

void esp8266_init(volatile u8* line_ready) {
	l_ready = line_ready;
}

void esp8266_send_command(Type type, Operation operation) {
    if (s != STATUS_NOT_WORKING) {
        // Operation in progress
        // TODO: Add message to queue?
        return;
    }

    op = operation;
    t = type;
    s = STATUS_WORKING;
    switch (operation) {
    case AT:
        usart1_print(at_inquiry);
        break;
    case AT_RST:
        usart1_print(at_rst);
    	break;
    case ATE0:
    	usart1_print(ate0);
    	break;
    case ATE1:
    	usart1_print(ate1);
    	break;
    case AT_CIFSR:
    	if (t == TYPE_TEST) {
    		usart1_print(at_cifsr_t);
    	}
    	break;
    case AT_CIPSERVER:
    case AT_CWJAP:
    case AT_CWLAP:
    case AT_CWQAP:
    	break;
    }
}



u8 parse_ok(volatile char *buffer) {
    volatile char *pos = strstr(buffer, ok_answer);
    if (pos == 0) {
        return 1; // No match
    }

    u8 new_length = strlen(pos) + ok_answer_length;
    buffer = (volatile char*) memmove(buffer, pos + ok_answer_length, new_length);
    usart_clear_string();

    return 0;
}

u8 parse_ready(volatile char *buffer) {
    volatile char *pos = strstr(buffer, ready_answer);
    if (pos == 0) {
        return 1; // No match
    }

    u8 new_length = strlen(pos) + ready_answer_length;
    buffer = (volatile char*) memmove(buffer, pos + ready_answer_length, new_length);
    usart_clear_string();

    return 0;
}

// string - incoming buffer
u8 parse_AT(volatile char *buffer) {
	return parse_ok(buffer);
}

u8 parse_AT_RST(volatile char *buffer) {
	static int state = 0;

	if (state == 0) {
		if (parse_ok(buffer) == 0) {
			state = 1;
		}
	} else if (state == 1) {
		if (parse_ready(buffer) == 0) {
			state = 2;
		}
	}
	return state;
}

volatile Status esp8266_status() {
	return s;
}

void esp8266_parse_line() {
    if (s != STATUS_WORKING) {
        // Nothing to do
        return;
    }
    volatile char* string = usart_get_string();

    switch (op) {
    case AT:
        if (parse_AT(string) != 0) {
            usart2_print("-"); // error
        } else {
            usart2_print("+"); // success
            s = STATUS_NOT_WORKING;
        }
        break;
    case AT_RST:
        if (parse_AT_RST(string) == 0) {
            usart2_print("0"); // waiting
        } else if (parse_AT_RST(string) == 1) {
            usart2_print("1"); // reboot initialized
        } else if (parse_AT_RST(string) == 2) {
        	usart2_print("2"); // success
            s = STATUS_NOT_WORKING;
        }
        break;
    case ATE0:
    case ATE1:
        if (parse_ok(string) != 0) {
            usart2_print("q"); // error
        } else {
            usart2_print("w"); // success
            s = STATUS_NOT_WORKING;
        }
    	break;
    case AT_CIFSR:
        if (parse_AT(string) != 0) {
            usart2_print("-"); // error
        } else {
            usart2_print("+"); // success
            s = STATUS_NOT_WORKING;
        }
    	break;
    case AT_CIPSERVER:
    case AT_CWJAP:
    case AT_CWLAP:
    case AT_CWQAP:
    	break;
    }
}

void esp8266_wait_for_answer(volatile u8 *line_ready) {
	while (s != STATUS_NOT_WORKING) {
		__WFI();
		if (*line_ready) {
			esp8266_parse_line();
			*line_ready = 0;
		}
	}
}

bool esp8266_check_presence(volatile u8 *line_ready) {
    esp8266_send_command(TYPE_INQUIRY, AT);
	esp8266_wait_for_answer(line_ready);
	return true;
}


void esp8266_reset(volatile u8 *line_ready) {
	esp8266_send_command(TYPE_SET_EXECUTE, AT_RST);
	esp8266_wait_for_answer(line_ready);
}

void esp8266_set_echo(bool new_state, volatile u8 *line_ready) {
	if (new_state == true) {
		esp8266_send_command(TYPE_SET_EXECUTE, ATE1);
	} else {
		esp8266_send_command(TYPE_SET_EXECUTE, ATE0);
	}
}

char* esp8266_get_ip_addresses() {
	esp8266_send_command(TYPE_TEST, AT_CIFSR);
	esp8266_wait_for_answer(l_ready);
	esp8266_wait_for_answer(l_ready);
	return "";
}
