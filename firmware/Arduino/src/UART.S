#include <avr/io.h>

// https://ww1.microchip.com/downloads/en/appnotes/doc42055.pdf

.global UART0_Init
.global UART0_Transmit
.global UART0_Receive
.global UART0_Print
.global UART0_Print_P
.global UART0_Println
.global UART0_Println_P
.global UART0_Flush



UART0_Init:
    ; Set baud rate 
    sts UBRR0H, r25
    sts UBRR0L, r24
    ; double speed
    ldi r18, _BV(U2X0) 
    sts UCSR0A, r18 
    ; Enable receiver and transmitter 
    ldi r18, _BV(RXEN0)|_BV(TXEN0) 
    sts UCSR0B, r18 
    ; 8N1
    ldi r18, _BV(UCSZ01)|_BV(UCSZ00)
    sts UCSR0C, r18
    ret

UART0_Receive:
    lds r16, UDR0
    ret

UART0_Transmit:
    sts UDR0, r24
    rcall UART0_Flush
    ret

UART0_Print:
    ; get address from array into Z
    movw ZL, r24
_loop:
    ld r24, Z+
    tst r24
    breq _end
    rcall UART0_Transmit
    rjmp _loop
_end:
    ret

UART0_Print_P:
    ; get address from array into Z
    movw ZL, r24
_loop1:
    lpm r24, Z+ ; from progmem
    tst r24
    breq _end1
    rcall UART0_Transmit
    rjmp _loop1
_end1:
    ret

UART0_Flush:
    lds r18, UCSR0A
    sbrs r18, UDRE0
    rjmp UART0_Flush
    ret

UART0_Println:
    rcall UART0_Print
    ldi r24, '\r'
    rcall UART0_Transmit
    ldi r24, '\n'
    rcall UART0_Transmit
    ret

UART0_Println_P:
    rcall UART0_Print_P
    ldi r24, '\r'
    rcall UART0_Transmit
    ldi r24, '\n'
    rcall UART0_Transmit
    ret
