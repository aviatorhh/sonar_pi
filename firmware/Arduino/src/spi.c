#include "spi.h"

#include <avr/io.h>

#if defined(__AVR_ATmega328P__)
#define DD_MOSI PB3
#define DD_SCK PB5
#elif defined(__AVR_ATmega1284P__)
#define DD_MOSI PB5
#define DD_SCK PB7
#elif defined(__AVR_ATmega32U4__)
#define DD_MOSI PB2
#define DD_SCK PB1
#endif

#ifndef CS_PIN
#error No CS_PIN defined
#endif

#ifndef DDR_CS
#error No DDR_CS defined
#endif

#ifndef PORT_CS
#error No PORT_CS defined
#endif

void spi_init() {
	// make these pins an output
    DDR_SPI |= _BV(DD_MOSI) | _BV(DD_SCK);
    DDR_CS |= _BV(CS_PIN);

    //DDRB &= ~_BV(PB4);
    //PORTB |= _BV(PB4);

    // initial CS pin state
    CS_HIGH;

    // SPI mode enable, set as master, mode 1
    SPCR = _BV(SPE) | _BV(MSTR) | _BV(CPHA);

#if defined(SPI_FOSC_2)
    SPCR |= _BV(SPI2X);  // fosc/2
#elif defined(SPI_FOSC_4)
#elif defined(SPI_FOSC_8)
    SPSR |= _BV(SPI2X);
    SPCR |= _BV(SPR0);  // fosc/8
#elif defined(SPI_FOSC_16)
    SPCR |= _BV(SPR0);  // fosc/16
#elif defined(SPI_FOSC_32)
    SPSR |= _BV(SPI2X);
    SPCR |= _BV(SPR1);  // fosc/32
#elif defined(SPI_FOSC_128)
    SPCR |= _BV(SPR1) | _BV(SPR0);  // fosc/128
#else
    SPCR |= _BV(SPR1);  // fosc/64
#endif	
}

uint8_t spi_master_transmit(uint8_t data) {
    SPDR = data;

    while (!(SPSR & _BV(SPIF))) { };

    return SPDR;
}