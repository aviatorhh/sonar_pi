#include "settings.h"

#include "UART.h"
#include "FIFO.h"
#include "spi.h"
#include "tuss4470.h"

#include <Arduino.h>
#include <avr/interrupt.h>

// Pin configuration
const int IO1 = 8;
const int IO2 = 9;
const int O3 = 3;
const int O4 = 2;
const int analogIn = A0;

#define RES_8BIT  0

volatile uint8_t c_in;
volatile bool rx_avail;

uint16_t depth_index;
int16_t temp_scaled;
uint16_t vDrv_scaled;
volatile uint8_t checksum;

float temperature = 0.0f;
int vDrv = 0;

#define START_BYTE 0xaa

volatile uint8_t pulseCount = 0;
uint16_t sampleIndex = 0;
volatile uint16_t sampleIndex2 = 0;

volatile bool detectedDepth = false;  // Condition flag
volatile uint16_t depthDetectSample = 0;

#define RX_BUFFER_SIZE 16

volatile fifo_t rx_buffer;  // needed for future serial input reading

#ifdef ADC_8BIT_RESOLUTION
volatile uint8_t sample;
#else
volatile uint16_t sample;
#endif
volatile bool captured;

// state of the measuring process
enum Mode {
    IDLE,
    START,
    RUNNING,
    END
};

// Begin immediately
enum Mode mode = START;

//#define SPEED_OF_SOUND 1440  // default sound speed meters/second in water
#define SPEED_OF_SOUND 343  // default sound speed meters/second in air

// Number of initial samples to ignore after sending the transducer pulse
// These ignored samples represent the "blind zone" where the transducer is still ringing
// It is calculated to account for different depths. Only tested on 40kHz!!
#define BLINDZONE_SAMPLE_END NUM_SAMPLES / MAX_DEPTH * .8

// Timer period in HZ. Divided by two because we need to take into account the way down and up
#define T2_HZ ((SPEED_OF_SOUND / MAX_DEPTH) * NUM_SAMPLES) / 2
#define T2_PRESCALER 1
#define _OCR2A (F_CPU / (T2_PRESCALER * T2_HZ)) - 1 // Value for the CTC setting

#if _OCR2A > 0xff
#undef T2_PRESCALER
#define T2_PRESCALER 8L  // Timer prescaler
#undef _OCR2A
#define _OCR2A (F_CPU / (T2_PRESCALER * T2_HZ)) - 1
#if _OCR2A > 0xff
#undef T2_PRESCALER
#define T2_PRESCALER 32L  // Timer prescaler
#undef _OCR2A
#define _OCR2A (F_CPU / (T2_PRESCALER * T2_HZ)) - 1
#if _OCR2A > 0xff
#undef T2_PRESCALER
#define T2_PRESCALER 64L  // Timer prescaler
#undef _OCR2A
#define _OCR2A (F_CPU / (T2_PRESCALER * T2_HZ)) - 1
#if _OCR2A > 0xff
#undef T2_PRESCALER
#define T2_PRESCALER 128L  // Timer prescaler
#undef _OCR2A
#define _OCR2A (F_CPU / (T2_PRESCALER * T2_HZ)) - 1
#if _OCR2A > 0xff
#undef T2_PRESCALER
#define T2_PRESCALER 256L  // Timer prescaler
#undef _OCR2A
#define _OCR2A (F_CPU / (T2_PRESCALER * T2_HZ)) - 1
#if _OCR2A > 0xff
#undef T2_PRESCALER
#define T2_PRESCALER 1024L  // Timer prescaler
#undef _OCR2A
#define _OCR2A (F_CPU / (T2_PRESCALER * T2_HZ)) - 1
#if _OCR2A > 0xff
#error HZ too slow for timer 2
#endif
#endif
#endif
#endif
#endif
#endif
#endif

#ifdef USE_DEPTH_OVERRIDE
uint8_t max;    // Holds the max value when iterating over the values
#endif

void startTransducerBurst();
void stopTransducerBurst();
void setupTimer2();
void stopTimer2();
void tuss4470Write(uint8_t addr, uint8_t data);
uint8_t tuss4470Parity(uint8_t* spi16Val);
unsigned int BitShiftCombine(unsigned char x_high, unsigned char x_low);
uint8_t parity16(uint8_t val);
#ifndef USE_DEPTH_OVERRIDE
void handleInterrupt();
#endif


void setup() {
    cli();  // we want to do our setup in peace

    // Init the serial communication with 500kbaud
    const uint16_t ubrr = (F_CPU / (115200 * 8)) - 1;
    UART0_Init(ubrr);
    rx_buffer = fifo_create(RX_BUFFER_SIZE, sizeof(char));

    // Init the serial peripheral interface. See spi.h for setup.
    spi_init();

    // Configure GPIOs
    pinMode(IO1, OUTPUT);
    digitalWrite(IO1, HIGH);
    pinMode(IO2, OUTPUT);
    pinMode(O4, INPUT_PULLUP);

#ifndef USE_DEPTH_OVERRIDE    
    attachInterrupt(digitalPinToInterrupt(O4), handleInterrupt, RISING);
#endif
    // Initialize TUSS4470 with specific configurations
    // check TUSS4470 datasheet for more settings!
    tuss4470Write(BPF_CONFIG_1, FILTER_FREQUENCY_REGISTER);  // Set BPF center frequency
    tuss4470Write(VDRV_CTRL, 0xF);                        // Enable VDRV (not Hi-Z)
    tuss4470Write(BURST_PULSE, 0x0F);                       // Set burst pulses to 16
#ifndef USE_DEPTH_OVERRIDE    
    tuss4470Write(ECHO_INT_CONFIG, THRESHOLD_VALUE);            // enable threshold detection on OUT_4
#endif
    tuss4470Write(DEV_CTRL_2, 0x01);                       // Set LNA gain (0x00 = 15V/V, 0x01 = 10V/V, 0x02 = 20V/V, 0x03 = 12.5V/V)

    // Set up ADC
    ADCSRA = _BV(ADEN); // Enable ADC

    // set divider for sampling rate
#if ADC_DIVISION_FACTOR == 2
    ADCSRA &= ~(_BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0));
#elif ADC_DIVISION_FACTOR == 4
    ADCSRA |= _BV(ADPS1);
#elif ADC_DIVISION_FACTOR == 8
    ADCSRA |= _BV(ADPS1) | _BV(ADPS0);
#elif ADC_DIVISION_FACTOR == 16
    ADCSRA |= _BV(ADPS2);
#elif ADC_DIVISION_FACTOR == 32
    ADCSRA |= _BV(ADPS2) | _BV(ADPS0);
#elif ADC_DIVISION_FACTOR == 64
    ADCSRA |= _BV(ADPS2) | _BV(ADPS1);
#elif ADC_DIVISION_FACTOR == 128
    ADCSRA |= _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);
#else
#error You need to define a ADC division factor (ADC_DIVISION_FACTOR) of either 2, 4, 8, 16, 32, 64, 128
#endif


    ADMUX = _BV(REFS0);  // Reference voltage: AVcc
#ifdef ADC_8BIT_RESOLUTION
    ADMUX |= _BV(ADLAR);  // left aligned
#endif
    // Input channel: ADC0 (default)
    ADCSRB = 0;              // Free-running mode
    ADCSRA |= _BV(ADATE);  // Enable auto-trigger (free-running)
    ADCSRA |= _BV(ADSC);   // Start conversion

    // Enable UART reception interrupt
    UCSR0B |= _BV(RXCIE0);

    sei();  // who let the dogs out?!
}

void loop() {

    if (mode == START) {
        captured = false;
        checksum = 0;
        sampleIndex = 0;
        sampleIndex2 = 0;
#ifdef USE_DEPTH_OVERRIDE        
        max = 0;
#endif        

        depth_index = depthDetectSample;
//        vDrv_scaled = sample_timer;
        checksum ^= (uint8_t)(depth_index & 0xFF);
        checksum ^= (uint8_t)(depth_index >> 8);
        checksum ^= (uint8_t)(temp_scaled & 0xFF);
        checksum ^= (uint8_t)(temp_scaled >> 8);
        checksum ^= (uint8_t)(vDrv_scaled & 0xFF);
        checksum ^= (uint8_t)(vDrv_scaled >> 8);
#ifdef ADC_8BIT_RESOLUTION
        UART0_Transmit(START_BYTE | _BV(RES_8BIT));
#else
        UART0_Transmit(START_BYTE);
#endif
        UART0_Transmit16_t(depth_index);
        UART0_Transmit16_t(temp_scaled);
        UART0_Transmit16_t(vDrv_scaled);

        // Trigger time-of-flight measurement
        tuss4470Write(TOF_CONFIG, 0x01);

        setupTimer2();
        TIMSK2 |= _BV(OCIE2A);      
        // enable timer2 interrupt for ADC capture
        startTransducerBurst();
        mode = RUNNING;
        delay(1);   // a hack to account for a startup delay. Needs to be corrected somehow.
    } else if (mode == RUNNING) {

        if (captured) {
            
         
            sampleIndex++;


// #ifdef ADC_8BIT_RESOLUTION
//             const uint8_t v = sample;
//             checksum ^= v;
//             UART0_Transmit(v);
// #else
//             const uint16_t v = sample;
//             checksum ^= v & 0xff;
//             checksum ^= (v >> 8) & 0xff;
//             UART0_Transmit16_t(v);
// #endif
            
#ifdef USE_DEPTH_OVERRIDE
            int overrideSample = 0; // gets the sample index of the max value

            if (sampleIndex2 > BLINDZONE_SAMPLE_END) {
                if (sample > max) {
                    max = sample;
                    overrideSample = sampleIndex2;
                }
            }
            if (overrideSample > 0) {
                depthDetectSample = overrideSample;
            }
            if (sampleIndex2 >= BLINDZONE_SAMPLE_END) {
                detectedDepth = false;
            }

            
            
#endif
            // if (sampleIndex == num_samples) {
            //     mode = END;
            // } else {
                            captured = false;
                
            // }
        }

    } else if (mode == END) {
        // stop ADC captures
        stopTimer2();
        // Stop time-of-flight measurement
        tuss4470Write(TOF_CONFIG, 0x00);

//         const uint16_t overdub = num_samples - sampleIndex;

//         for(uint16_t i = 0; i < overdub; i++) {
            
        

// #ifdef ADC_8BIT_RESOLUTION
//             //const uint8_t v = sample;
//             checksum ^= sample;
//             UART0_Transmit(sample);
// #else
//             //const uint16_t v = sample;
//             checksum ^= sample & 0xff;
//             checksum ^= (sample >> 8) & 0xff;
//             UART0_Transmit16_t(sample);
// #endif
//         }
        // Send out the checksum now
        UART0_Transmit(checksum);

        mode = START;
    }

    // RX on UART? Does nothing for now
    if (!fifo_is_empty(rx_buffer)) {
        char c;
        fifo_get(rx_buffer, &c);
    }
}

/**
 * This timer runs for ADC value capture
 */
void setupTimer2() {

    // CTC
    TCCR2A = _BV(WGM21);
    // Prescaler
#if T2_PRESCALER == 1
    TCCR2B = _BV(CS20);
#elif T2_PRESCALER == 8
    TCCR2B = _BV(CS21);
#elif T2_PRESCALER == 32
    TCCR2B = _BV(CS20) | _BV(CS21);
#elif T2_PRESCALER == 64
    TCCR2B = _BV(CS22);
#elif T2_PRESCALER == 128
    TCCR2B = _BV(CS20) | _BV(CS22);
#elif T2_PRESCALER == 256
    TCCR2B = _BV(CS21) | _BV(CS22);
#elif T2_PRESCALER == 1024L
    TCCR2B = _BV(CS20) | _BV(CS21) | _BV(CS22);
#else
#error No prescaler for timer2 defined
#endif
    TCNT2 = 0;
    OCR2A = _OCR2A;

}

void stopTimer2() {
    TIMSK2 &= ~_BV(OCIE2A);
    TCCR2B &= ~(_BV(CS20) | _BV(CS21) | _BV(CS22));
}

ISR(TIMER1_COMPA_vect) {
    pulseCount++;
    if (pulseCount == 32) {
        stopTransducerBurst();
        pulseCount = 0;  // Reset counter for next cycle
    }
}

ISR(TIMER2_COMPA_vect) {
#ifdef ADC_8BIT_RESOLUTION
    sample = ADCH;
#else
    sample = ADC;
#endif
    captured = true;
    sampleIndex2++;
#ifdef ADC_8BIT_RESOLUTION
            const uint8_t v = sample;
            checksum ^= v;
            UART0_Transmit(v);
#else
            const uint16_t v = sample;
            checksum ^= v & 0xff;
            checksum ^= (v >> 8) & 0xff;
            UART0_Transmit16_t(v);
#endif
if (sampleIndex2 == num_samples) {
                mode = END;
            } 
}


ISR(USART_RX_vect) {
    char data = UART0_Receive();
    fifo_add(rx_buffer, &data);
}

void startTransducerBurst() {
    TCCR1A = _BV(COM1A0);             // Toggle OC1A (pin 9) on Compare Match
    TCCR1B = _BV(WGM12) | _BV(CS10);  // CTC mode, no prescaler

    OCR1A = DRIVE_FREQUENCY_TIMER_DIVIDER;

    TIMSK1 = _BV(OCIE1A);  // Enable Timer1 Compare Match A interrupt
}

void stopTransducerBurst() {
    TCCR1B = 0;  // Stop Timer1 by clearing clock select bits
    TIMSK1 &= ~_BV(OCIE1A);  // Disable Timer1 interrupt
}

void tuss4470Write(uint8_t addr, uint8_t data) {
    uint8_t buf[2];
    buf[0] = (addr & 0x3F) << 1;  // Set write bit and address
    buf[1] = data;
    buf[0] |= tuss4470Parity(buf);

    CS_LOW;
    spi_master_transmit(buf[0]);
    spi_master_transmit(buf[1]);
    CS_HIGH;
}

uint8_t tuss4470Parity(uint8_t* spi16Val) {
    return parity16(BitShiftCombine(spi16Val[0], spi16Val[1]));
}

unsigned int BitShiftCombine(unsigned char x_high, unsigned char x_low) {
    return (x_high << 8) | x_low;  // Combine high and low uint8_ts
}

uint8_t parity16(uint8_t val) {
    uint8_t ones = 0;
    for (uint8_t i = 0; i < 16; i++) {
        if ((val >> i) & 1) {
            ones++;
        }
    }
    return (ones + 1) % 2;  // Odd parity calculation
}

#ifndef USE_DEPTH_OVERRIDE
/**
 * Signal from the TUSS4470 for dpeth measurement
 */
void handleInterrupt() {
    if (!detectedDepth) {
        depthDetectSample = sampleIndex;
        detectedDepth = true;
    }
}
#endif
