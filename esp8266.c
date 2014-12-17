#include "esp8266.h"
#include <string.h>

Operation op;
Type t;
Status s = STATUS_NOT_WORKING;

const char ok_answer[] = "OK\r\n";
const size_t ok_answer_length = strlen(ok_answer);
const char ready_answer[] = "ready\r\n";
const size_t ready_answer_length = strlen(ready_answer);

const char at_inquiry[] = "AT\r\n";
const char at_rst[] = "AT+RST\r\n";
const char ate0[] = "ATE0\r\n";
const char ate1[] = "ATE1\r\n";
const char at_cifsr_t[] = "AT+CIFSR=?\r\n";
// List of Access Points
const char at_cwlap[] = "AT+CWLAP\r\n";

volatile u8* l_ready = NULL;
volatile char* g_buffer = NULL;

Access_Point ap[20];
u8 ap_index = 0;

void esp8266_init(volatile u8* line_ready) {
	l_ready = line_ready;
	g_buffer = usart_get_string();
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
    case AT_CWLAP:
    	usart1_print(at_cwlap);
    	break;
    case AT_CIPSERVER:
    case AT_CWJAP:
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
    memmove(buffer, pos + ok_answer_length, new_length);
    usart_clear_string();

    return 0;
}

u8 parse_ready(volatile char *buffer) {
    volatile char *pos = strstr(buffer, ready_answer);
    if (pos == 0) {
        return 1; // No match
    }

    u8 new_length = strlen(pos) + ready_answer_length;
    memmove(buffer, pos + ready_answer_length, new_length);
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

typedef enum {
	cwlap_encryption = 0,
	cwlap_essid = 1,
	cwlap_signal = 2,
	cwlap_mac = 3,
	cwlap_channel = 4
} cwlap_state;

u8 parse_AT_CWLAP() {
	static const char* cwlap = "+CWLAP:";
    volatile char *p = strstr(g_buffer, cwlap);
    if (p == NULL) {
    	return;
    }

	const u8 tmp_buffer_len = 40;
	char tmp_buffer[tmp_buffer_len];

	memset(tmp_buffer, '\0', tmp_buffer_len);

    p += strlen(cwlap) + 1; //+CWLAP:(
    u8 index = 0;
    cwlap_state current_state = 0;

    Access_Point current_ap;
    while(*p != ')') {
    	if (*p == ',') { // each section is separated by comma
    		switch (current_state) {
    		case cwlap_encryption:
    			current_ap.encryption = atoi(tmp_buffer);
    			break;
    		case cwlap_essid:
    			current_ap.essid = malloc(index);
    			strncpy(current_ap.essid, tmp_buffer, index);
    			break;
    		case cwlap_signal:
    			current_ap.signal = atoi(tmp_buffer);
    			break;
    		case cwlap_mac:
    			current_ap.mac = malloc(index);
    			strncpy(current_ap.mac, tmp_buffer, index);
    			break;
    		case cwlap_channel:
				current_ap.channel = atoi(tmp_buffer);
				break;
    		}

    		current_state = (current_state + 1) % 5;
    		memset(tmp_buffer, '\0', tmp_buffer_len);
    		index = 0;
    	} else {
			switch (current_state) {
			case cwlap_essid:
			case cwlap_mac:
				if (*p != '"') {
					tmp_buffer[index] = *p;
				} else {
					--index; // ignore " and keep index of tmp_buffer
				}
				break;
			default:
				tmp_buffer[index] = *p;
				break;
			}

			++index;
    	}
    	++p;
    }
    ap[ap_index++] = current_ap;
    u8 new_length = p - g_buffer;
    memmove(g_buffer, p, new_length);
    usart_clear_string();
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
    case AT_CWLAP:
    	parse_AT_CWLAP();
    	if (parse_ok(string) != 0) {
			usart2_print("Q"); // error
		} else {
			usart2_print("W"); // success
			s = STATUS_NOT_WORKING;
		}
    	break;
    case AT_CIPSERVER:
    case AT_CWJAP:
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

char** esp8266_get_list_of_aps() {
	esp8266_send_command(TYPE_SET_EXECUTE, AT_CWLAP);
	esp8266_wait_for_answer(l_ready);
}

