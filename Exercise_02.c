#include <xc.h>
#include <stdint.h>

#pragma config FOSC = XT, WDTE = OFF, PWRTE = OFF, BOREN = ON, LVP = OFF
#define _XTAL_FREQ 4000000

#define NUM_SENSORS 2 // Using AN0 and AN1

volatile uint8_t t0_ticks = 0;
volatile uint8_t start_sequence = 0;
volatile uint8_t channel_done = 0;
volatile uint8_t current_idx = 0;
volatile uint16_t sensor_readings[NUM_SENSORS];

void __interrupt() ISR_Handler(void) {
    // Timer0 Interrupt: Creates the 1-second interval
    if (INTCONbits.TMR0IF) {
        if (++t0_ticks >= 50) {
            start_sequence = 1;
            t0_ticks = 0;
        }
        TMR0 = 178;
        INTCONbits.TMR0IF = 0;
    }

    // ADC Interrupt: Captured automatically when conversion ends
    if (PIR1bits.ADIF) {
        sensor_readings[current_idx] = (uint16_t)((ADRESH << 8) | ADRESL);
        channel_done = 1;
        PIR1bits.ADIF = 0;
    }
}

void main(void) {
    TRISA = 0x03; // RA0 and RA1 as Analog Inputs
    TRISD = 0x00; // PORTD for output LEDs
    PORTD = 0x00;

    // ADC: Right Justified, Fosc/8, Turn On
    ADCON1 = 0x80;
    ADCON0 = 0x41;

    // Timer0: 1:256 Prescaler
    OPTION_REG = 0x07;
    TMR0 = 178;

    // Interrupt Enablers
    PIE1bits.ADIE = 1;     // ADC Interrupt
    INTCONbits.TMR0IE = 1; // Timer0 Interrupt
    INTCONbits.PEIE = 1;   // Peripheral Interrupt
    INTCONbits.GIE = 1;    // Global Interrupt

    while(1) {
        // Step 1: Initiate the sequence every 1 second
        if (start_sequence) {
            start_sequence = 0;
            current_idx = 0;
            ADCON0 = (ADCON0 & 0xC7) | (current_idx << 3); // Switch to AN0
            __delay_us(20);                                // Wait for Tacq
            ADCON0bits.GO = 1;                             // Start conversion
        }

        // Step 2: Handle channel switching after each interrupt
        if (channel_done) {
            channel_done = 0;
            current_idx++;

            if (current_idx < NUM_SENSORS) {
                ADCON0 = (ADCON0 & 0xC7) | (current_idx << 3); // Switch to AN1
                __delay_us(20);                                // Wait for Tacq
                ADCON0bits.GO = 1;
            } else {
                // Step 3: All sensors updated; run application logic
                PORTDbits.RD0 = (sensor_readings[0] >= (uint16_t)(41 * 2.046f + 0.5f));
                // LED on if S1 >= 41C
                PORTDbits.RD1 = (sensor_readings[1] >= (uint16_t)(21 * 2.046f + 0.5f));
                // LED on if S2 >= 21C
            }
        }
    }
}