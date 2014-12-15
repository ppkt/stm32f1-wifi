#include "esp8266.h"
#include <string.h>

Operation op;
Type t;
Status s = STATUS_NOT_WORKING;

const char ok_answer[] = "OK\r\n";
const size_t ok_answer_length = strlen(ok_answer);

const char at_inquiry[] = "AT\r\n";

void esp8266_send_command(Type type, Operation operation) {
    if (s != STATUS_NOT_WORKING) {
        // Operation in progress
        // TODO: Add message to queue?
        return;
    }

    switch (operation) {
    case AT:
        op = AT;
        t = INQUIRY;
        usart1_print(at_inquiry);
        s = STATUS_WORKING;
    }
}


// string - incoming buffer
u8 parse_AT(volatile char *buffer) {
    volatile char *pos = strstr(buffer, ok_answer);
    if (pos == 0) {
        return 1; // No match
    }

    u8 new_length = strlen(pos) + ok_answer_length;
    buffer = (volatile char*) memmove(buffer, pos, new_length);
    usart_clear_string();

    return 0;
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
            s = STATUS_PROBLEM;
        } else {
            usart2_print("+"); // success
            s = STATUS_NOT_WORKING;
        }
    }
}
