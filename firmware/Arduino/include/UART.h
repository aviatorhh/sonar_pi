#ifndef UART_H_
#define UART_H_

#include <stdint.h>

extern "C" {

void UART0_Init(uint16_t baud);
void UART0_Transmit(unsigned char data);
unsigned char UART0_Receive();
void UART0_Print(char* str);
void UART0_Println(char* str);
void UART0_Flush();
}

#define UART0_out(txt)  strcpy(c_buf, txt); \
                        UART0_Println(c_buf)

#define UART0_out_P(str)    const static char txt[] PROGMEM = str; \
                            strcpy_P(c_buf, txt); \
                            UART0_Println(c_buf)

#define UART0_Transmit16_t(v) UART0_Transmit((uint8_t)(v & 0xff)); \
        UART0_Transmit((uint8_t)(v >> 8));

#endif