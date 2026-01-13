#ifndef SPI_H_
#define SPI_H_

#include <stdint.h>

#define CS_PIN PB2
#define DDR_CS DDRB
#define PORT_CS PORTB

#define DDR_SPI DDRB

#define CS_LOW PORT_CS &= ~_BV(CS_PIN)
#define CS_HIGH PORT_CS |= _BV(CS_PIN)


#define SPI_FOSC_32 // TUSS4470 has a 500kHz bus

#ifdef __cplusplus
extern "C" {
#endif
    void spi_init(void);
    uint8_t spi_master_transmit(uint8_t data);
#ifdef __cplusplus
}
#endif
#endif