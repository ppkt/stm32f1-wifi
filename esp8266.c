#include "esp8266.h"
#include <string.h>
#include <stdio.h>

Operation op;
Type t;
ESP8266_Status s = STATUS_NOT_WORKING;

const char ok_answer[] = "OK\r\n";
//const size_t ok_answer_length = strlen(ok_answer);
const char ready_answer[] = "ready\r\n";
//const size_t ready_answer_length = strlen(ready_answer);
const char error_answer[] = "ERROR\r\n";
//const size_t error_answer_length = strlen(error_answer);

const char at_inquiry[] = "AT\r\n";
const char at_rst[] = "AT+RST\r\n";
const char ate0[] = "ATE0\r\n";
const char ate1[] = "ATE1\r\n";
const char at_cifsr_t[] = "AT+CIFSR\r\n";
const char at_cwlap[] = "AT+CWLAP\r\n"; // List of Access Points
const char at_cwjap_i[] = "AT+CWJAP?\r\n"; // Conncted AP
const char at_close[] = "AT+CIPCLOSE\r\n"; // Close connetion
char *at_cwjap_se;
char *at_cwmode_se;
char *at_cipmux_se;
char *at_cipstart_se;
char *at_cipsend_se;
char *at_data_buffer;

u8* l_ready = NULL;
char* g_buffer = NULL;

Access_Point ap[20];
u8 ap_index = 0;

Protocol current_protocol = TCP;

char* connected_ap;

void esp8266_init(u8* line_ready) {
	l_ready = line_ready;
	g_buffer = usart_get_string();

    // Reset and set mode 3
//    esp8266_reset(&line_ready);
//    esp8266_set_mode(3);

    esp8266_check_presence();

    // disable echo
    esp8266_set_echo(false);
    esp8266_wait_for_answer();

    // enable echo
//    esp8266_set_echo(true, &line_ready);
//    esp8266_wait_for_answer(&line_ready);

    esp8266_check_presence();
    esp8266_wait_for_answer();

    // get information (ESSID) about AP
//  esp8266_get_connected_ap();
//  esp8266_debug_print_connected_ap();

    // check IP address (should work)
//    esp8266_get_ip_addresses();
//    esp8266_debug_print_ip_address();

    // get list of Access Points
//    esp8266_get_list_of_aps();
//    esp8266_debug_print_list_of_aps();

//    esp8266_check_presence(&line_ready);

    // Setup module to work in single connection mode
    esp8266_connection_mode(0);

    esp8266_check_presence();
    esp8266_wait_for_answer();
}

u8 parse_ok(void) {
    char *pos = strstr(g_buffer, ok_answer);
    if (pos == 0) {
        return 1; // No match
    }

    memset(g_buffer, 0, received_string_length);
    usart_clear_string();

    return 0;
}

u8 parse_error(void) {
    char *pos = strstr(g_buffer, error_answer);
    if (pos == 0) {
        return 1; // No match
    }
    memset(g_buffer, 0 , received_string_length);
    usart_clear_string();
    return 0;
}

u8 parse_ready(void) {
    char *pos = strstr(g_buffer, ready_answer);
    if (pos == 0) {
        return 1; // No match
    }

    memset(g_buffer, 0, received_string_length);
    usart_clear_string();

    return 0;
}

// string - incoming buffer
u8 parse_AT(void) {
    u8 ret = parse_ok();
    return ret;
}

u8 parse_AT_RST(void) {
    static int state = 0;

    if (state == 0) {
        if (parse_ok() == 0) {
            state = 1;
        }
    } else if (state == 1) {
        if (parse_ready() == 0) {
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

void parse_AT_CWLAP(void) {
    // Response line should start with this string
    static const char* cwlap = "+CWLAP:";
    char *p = strstr(g_buffer, cwlap);
    if (p == NULL) {
        return;
    }

    // buffer for 40 chars
    const u8 tmp_buffer_len = 40;
    char tmp_buffer[tmp_buffer_len];

    memset(tmp_buffer, '\0', tmp_buffer_len);

    p += strlen(cwlap) + 1; //+CWLAP:(
    u8 index = 0;
    cwlap_state current_state = 0;

    Access_Point current_ap;
    while(*p != ')') {
        if (*p == ',') { // each section is separated by comma
            index++;
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

void parse_AT_CWJAP(void) {
    // Response is in format: +CWJAP:"AccessPointName"\r\nOK\r\n
    static const char* cwjap = "+CWJAP:";
    char *p = strstr(g_buffer, cwjap);
    if (p == NULL) {
        return;
    }

    p += strlen(cwjap) + 1; //+CWJAP:"

    // Find next " char
    char *q = strstr(p, "\"");
    if (q == NULL) {
        return;
    }

    // Text between " is name of current AP
    u8 diff = q - p;
    connected_ap = calloc(diff + 1, sizeof(char));
    strncpy(connected_ap, p, diff);

    // Move rest of string
    memmove(g_buffer, q, q - g_buffer);
    usart_clear_string();
}


u8 parse_AT_CWMODE(void) {
    char *pos = strstr(g_buffer, ok_answer);
    if (pos == 0) {
        // No match
        char *no_change = "no change";
        pos = strstr(g_buffer, no_change);
    }

    if (pos == 0) {
        return 1;
    }

    memset(g_buffer, 0, received_string_length);
    usart_clear_string();

    return 0;
}


u8 parse_AT_CIPSTART(void) {
    static u8 state = 0; // Two states, waiting for "OK" and waiting for "Linked"
    // TODO: add error support

    if (state == 0) {
        // We can receive either OK or "link is builded"
        if (parse_ok() == 0) { // ok found
            state = 1;
        } else {

            static const char already_connect[] = "ALREAY CONNECT\r\n"; // srsly? connection was established earlier ie. before restart

            char *p1 = strstr(g_buffer, already_connect);
            if (p1 != 0) {
                state = 2;
            }
        }

    } else if (state == 1) {
        static const char linked[] = "Linked\r\n";

        char *p1 = strstr(g_buffer, linked);
        if (p1 != 0) {
            state = 2; // Matched
        }

    }

    switch (state) {
    case 0:
        usart2_print("0");
        break;
    case 1:
        usart2_print("1");
        break;
    case 2:
        usart2_print("2");
        break;
    }

    if (state == 2) {
        memset(g_buffer, 0, received_string_length);
        usart_clear_string();

        state = 0;
        return 2;
    }
    return state;
}

u8 parse_AT_CIPMUX(void) {
    if (parse_ok() != 0) {
        // no "ok", check is link is established already

        char builded[] = "link is builded\r\n";

        char *p1 = strstr(g_buffer, builded);

        if (p1 == 0) {
            return 1; // No match
        } // else: "link is builded" is present, proceed
    }

    memset(g_buffer, 0, received_string_length);
    usart_clear_string();
    return 0;
}

u8 parse_AT_CIPSEND() {
    static const char* prompt = "> ";

    char *pos = strstr(g_buffer, prompt);
    if (pos == NULL) {
        return 1;
    }

    memset(g_buffer, 0, received_string_length);
    usart_clear_string();

    return 0;
}

u8 parse_AT_SEND_DATA() {
    static const char* unlink = "Unlink\r\n";
    static const char* send_ok = "SEND OK\r\n";

    static u8 state = 0;
    if (state == 0) {
        char *pos = strstr(g_buffer, send_ok);
        if (pos == NULL) {
            if (parse_error() != 0) {
                state = 0;
            } else {
                state = 1;
            }
        }

        memset(g_buffer, 0, received_string_length);
        usart_clear_string();
        state = 1;
        return state;
    }
    else if (state == 1) {
        if (parse_ok() != 0) {
            return state;
        }
        state = 2;
    } else if (state == 2) {
        char *pos = strstr(g_buffer, unlink);
        if (pos == NULL) {
            return state;
        }
        memset(g_buffer, 0, received_string_length);
        usart_clear_string();
        state = 3;
    }

    if (state == 2) {
        state = 0;
        return 2;
    }
    else if (state == 3) {
        state = 0;
        return 3;
    }
    return state;
}

void esp8266_send_command(Type type, Operation operation) {
    if (s != STATUS_NOT_WORKING) {
        // Operation in progress
        // TODO: Add message to queue?
        while (1) {}
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
        break;
    case AT_CWJAP:
        if (t == TYPE_INQUIRY) {
            usart1_print(at_cwjap_i);
        } else if (t == TYPE_SET_EXECUTE) {
            usart1_print(at_cwjap_se);
        }

        break;
    case AT_CWQAP:
    	break;
    case AT_CWMODE:
        if (t == TYPE_SET_EXECUTE) {
            usart1_print(at_cwmode_se);
        }
        break;
    case AT_CIPMUX:
        if (t == TYPE_SET_EXECUTE) {
            usart1_print(at_cipmux_se);
        }
        break;
    case AT_CIPSTART:
        if (t == TYPE_SET_EXECUTE) {
            usart1_print(at_cipstart_se);
        }
        break;
    case AT_CIPSEND:
        if (t == TYPE_SET_EXECUTE) {
            usart1_print(at_cipsend_se);
        }
        break;
    case AT_SEND_DATA:
        usart1_print(at_data_buffer);
        break;
    case AT_CLOSE:
        usart1_print(at_close);
        break;
    }
}

void esp8266_parse_line(void) {
    if (s != STATUS_WORKING) {
        // Nothing to do
        return;
    }
    u8 val;

    switch (op) {
    case AT:
        if (parse_AT() != 0) {
            usart2_print("-"); // error
        } else {
            usart2_print("+"); // success
            s = STATUS_NOT_WORKING;
        }
        break;
    case AT_RST:
        val = parse_AT_RST();
        if (val == 0) {
            usart2_print("0"); // waiting
        } else if (val == 1) {
            usart2_print("1"); // reboot initialized
        } else if (val == 2) {
            usart2_print("2"); // success
            s = STATUS_NOT_WORKING;
        }
        break;
    case ATE0:
    case ATE1:
        if (parse_ok() != 0) {
            usart2_print("q"); // error
        } else {
            usart2_print("Q"); // success
            s = STATUS_NOT_WORKING;
        }
        break;
    case AT_CIFSR:
        if (parse_AT() != 0) {
            usart2_print("e"); // error
        } else {
            usart2_print("E"); // success
            s = STATUS_NOT_WORKING;
        }
        break;
    case AT_CWLAP: // get list of access points
        parse_AT_CWLAP();
        if (parse_ok() != 0) {
            usart2_print("w"); // error
        } else {
            usart2_print("W"); // success
            s = STATUS_NOT_WORKING;
        }
        break;
    case AT_CIPSERVER:
        break;
    case AT_CWJAP: // get connected ap
        parse_AT_CWJAP();
        if (parse_ok() != 0) {
            usart2_print("o"); // error
        } else {
            usart2_print("O"); // success
            s = STATUS_NOT_WORKING;
        }
        break;
    case AT_CWQAP:
        break;
    case AT_CWMODE:
        if (parse_AT_CWMODE() != 0) {
            usart2_print("b"); // error
        } else {
            usart2_print("B"); // success
            s = STATUS_NOT_WORKING;
        }
        break;
    case AT_CIPMUX:
        if (parse_AT_CIPMUX() != 0) {
            usart2_print("x"); // error
        } else {
            usart2_print("X"); // success
            s = STATUS_NOT_WORKING;
        }
        break;
    case AT_CIPSTART:
        val = parse_AT_CIPSTART();
        if (val == 0) {
            usart2_print("m1"); // waiting
        } else if (val == 1) {
            usart2_print("m2"); // OK received
            if (current_protocol == UDP) {
                s = STATUS_NOT_WORKING; // UDP is stateless
            }
        } else if (val == 2) {
            usart2_print("M"); // Linked received
            s = STATUS_NOT_WORKING;
        }
        break;
    case AT_CIPSEND:
        if (parse_AT_CIPSEND() != 0) {
            usart2_print("u"); // error
        } else {
            usart2_print("U"); // success
            s = STATUS_NOT_WORKING;
        }
        break;
    case AT_SEND_DATA:
        val = parse_AT_SEND_DATA();
        if (val == 0) {
            usart2_print("y1"); // waiting for end of transmission
        } else if (val == 1) {
            usart2_print("y2"); // sent successfully
        } else if (val == 2) {
            usart2_print("y3"); // echo received
            s = STATUS_NOT_WORKING;
        } else {
            usart2_print("Y"); // unlinked
            s = STATUS_NOT_WORKING;
        }
        break;
    case AT_CLOSE:
        if (parse_ok() != 0) {
            usart2_print("h"); // error
        } else {
            usart2_print("H"); // success
            s = STATUS_NOT_WORKING;
        }
        break;
    }
}

void esp8266_wait_for_answer(void) {
    u16 timeout = 1000;
    while (s != STATUS_NOT_WORKING) {
        if (*l_ready) {
            esp8266_parse_line();
            *l_ready = 0;
        } else {
            if (timeout-- == 0) {
                usart2_print("\r\nTIMEOUT\r\n");
                s = STATUS_NOT_WORKING;
            }
        }
        __WFI();
    }
}

ESP8266_Status esp8266_status(void) {
	return s;
}

bool esp8266_check_presence(void) {
    esp8266_send_command(TYPE_INQUIRY, AT);
	esp8266_wait_for_answer();
	return true;
}


void esp8266_reset(void) {
	esp8266_send_command(TYPE_SET_EXECUTE, AT_RST);
	esp8266_wait_for_answer();
}

void esp8266_set_echo(bool new_state) {
	if (new_state == true) {
		esp8266_send_command(TYPE_SET_EXECUTE, ATE1);
	} else {
		esp8266_send_command(TYPE_SET_EXECUTE, ATE0);
	}
}

char* esp8266_get_ip_addresses() {
	esp8266_send_command(TYPE_TEST, AT_CIFSR);
	esp8266_wait_for_answer();
	esp8266_wait_for_answer();
	return "";
}

void esp8266_get_list_of_aps() {
	esp8266_send_command(TYPE_SET_EXECUTE, AT_CWLAP);
	esp8266_wait_for_answer();
}

void esp8266_get_connected_ap() {
    esp8266_send_command(TYPE_INQUIRY, AT_CWJAP);
    esp8266_wait_for_answer();
}

void esp8266_join_ap(char* ap, char* password) {
    // prepare credentials:
    char buffer[70];
    sprintf(buffer, "AT+CWJAP=\"%s\",\"%s\"\r\n", ap, password);
    at_cwjap_se = calloc(strlen(buffer), sizeof(char));
    strcpy(at_cwjap_se, buffer);

    esp8266_send_command(TYPE_SET_EXECUTE, AT_CWJAP);
    esp8266_wait_for_answer();

    free(at_cwjap_se);
}

void esp8266_set_mode(u8 mode) {
    if ((mode > 3) || mode == 0) {
        return; // Only 1, 2, 3
    }
    at_cwmode_se = calloc(16, sizeof(char));
    sprintf(at_cwmode_se, "AT+CWMODE=%d\r\n", mode);
    esp8266_send_command(TYPE_SET_EXECUTE, AT_CWMODE);

    free(at_cwmode_se);
}

void esp8266_connection_mode(u8 mode) {
    if ((mode != 1) && (mode != 0)) {
        return; // Only 0 or 1
    }
    at_cipmux_se = calloc(16, sizeof(char));
    sprintf(at_cipmux_se, "AT+CIPMUX=%d\r\n", mode);
    esp8266_send_command(TYPE_SET_EXECUTE, AT_CIPMUX);
    esp8266_wait_for_answer();

    free(at_cipmux_se);
}

void esp8266_send_data(char* ip_address, u16 port, Protocol protocol, char* data, u8 length) {
    // Establish connection with remote host
    char buffer[256];
    char *proto;
    proto = calloc(4, sizeof(char));
    if (protocol == TCP) {
        strcpy(proto, "TCP");
    } else if (protocol == UDP) {
        strcpy(proto, "UDP");
    }
    current_protocol = protocol;
    sprintf(buffer, "AT+CIPSTART=\"%s\",\"%s\",%u\r\n", proto, ip_address, port);
    at_cipstart_se = calloc(strlen(buffer) + 1, sizeof(char));
    strcpy(at_cipstart_se, buffer);

    esp8266_send_command(TYPE_SET_EXECUTE, AT_CIPSTART);
    esp8266_wait_for_answer();

    // Send data
    sprintf(buffer, "AT+CIPSEND=%u\r\n", length);
    at_cipsend_se = calloc(strlen(buffer) + 1, sizeof(char));
    strcpy(at_cipsend_se, buffer);

    esp8266_send_command(TYPE_SET_EXECUTE, AT_CIPSEND);
    esp8266_wait_for_answer();

    at_data_buffer = calloc(length + 1, sizeof(u8));
    strncpy(at_data_buffer, data, length);

    usart2_print("+");

    esp8266_send_command(TYPE_SET_EXECUTE, AT_SEND_DATA);
    esp8266_wait_for_answer();

    free(proto);
    free(at_cipsend_se);
    free(at_cipstart_se);
    free(at_data_buffer);
}

void esp8266_close_connection(void) {
    esp8266_send_command(TYPE_SET_EXECUTE, AT_CLOSE);
    esp8266_wait_for_answer();
}

char debug_buffer[70];
void esp8266_debug_print_list_of_aps() {
    u8 i = 0;
    Access_Point tmp;
    for (i = 0; i < ap_index; ++i) {
        tmp = ap[i];
        sprintf(debug_buffer, "Access Point %d\r\n", i);
        usart2_print(debug_buffer);

        sprintf(debug_buffer, "ESSID: %s\r\n", tmp.essid);
        usart2_print(debug_buffer);

        sprintf(debug_buffer, "MAC: %s\r\n", tmp.mac);
        usart2_print(debug_buffer);

        sprintf(debug_buffer, "Signal: %d\r\n", tmp.signal);
        usart2_print(debug_buffer);

        usart2_print("\r\n");
    }
}

void esp8266_debug_print_connected_ap() {
    sprintf(debug_buffer, "Current AP: %s\r\n", connected_ap);
    usart2_print(debug_buffer);
}

void esp8266_debug_print_ip_address() {

}
