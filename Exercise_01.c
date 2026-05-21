#include <xc.h>
#include <stdint.h>

// Configuration Bits: Standard XT Oscillator at 4MHz
#pragma config FOSC = XT, WDTE = OFF, PWRTE = OFF, BOREN = ON, LVP = OFF

#define _XTAL_FREQ 4000000

// --- GLOBAL VOLATILE VARIABLES ---
// Marked volatile because they are modified within the ISR context
volatile uint8_t t0_counter = 0;    // Software counter to scale 20ms to 1s
volatile uint8_t time_to_sample = 0; // Flag: High when 1 second has passed
volatile uint8_t data_ready = 0;     // Flag: High when ADC hardware is finished
volatile uint16_t raw_temp = 0;      // Stores the 10-bit ADC result

// --- INTERRUPT SERVICE ROUTINE ---
void __interrupt() ISR(void) {

    // 1. Timer0 Interrupt: The 1-Second "Metronome"
    if (INTCONbits.TMR0IF) {
        t0_counter++;
        if (t0_counter >= 50) {      // 20ms * 50 = 1000ms = 1 second
            time_to_sample = 1;     // Raise the flag for the main loop
            t0_counter = 0;          // Reset software counter
        }
        TMR0 = 178;                  // Reload for next 20ms cycle
        INTCONbits.TMR0IF = 0;       // Clear Timer0 Overflow Flag
    }

    // 2. ADC Interrupt: Fired after Conversion Time (Tad)
    if (PIR1bits.ADIF) {
        // Capture 10-bit result (Right Justified)
        raw_temp = (uint16_t)((ADRESH << 8) | ADRESL);
        data_ready = 1;              // Signal main loop that data is available
        PIR1bits.ADIF = 0;           // Clear ADC hardware flag
    }
}

void main(void) {
    // Port Setup
    TRISA = 0x01;  // RA0 (AN0) as Input
    TRISD = 0x00;  // PORTD as Output for Indicator LED
    PORTD = 0x00;

    // ADC Initialization
    ADCON1 = 0x80; // Right Justified, AN0-AN7 Analog
    ADCON0 = 0x41; // Clock Fosc/8, Channel 0 (AN0), ADC ON

    // Timer0 Setup (Fosc/4, 1:256 Prescaler)
    OPTION_REG = 0x07;
    TMR0 = 178;

    // Interrupt Control
    INTCONbits.TMR0IE = 1; // Enable Timer0 Overflow Interrupt
    PIE1bits.ADIE = 1;     // Enable ADC Completion Interrupt
    INTCONbits.PEIE = 1;   // Enable Peripheral Interrupts
    INTCONbits.GIE = 1;    // Enable Global Interrupts

    // Degree celcius to adc value convertion
    uint8_t threshhold_temp = 40; // 40 degree celcius
    // Convert to ADC counts (10mV/°C and 4.88mV per count)
    uint16_t threshhold_adc = (uint16_t)(threshhold_temp * 2.046f + 0.5f);
    // Nearest integer (round, not truncate)
    // Example: 102.3 + 0.5 = 102.8, then (uint16_t) = 102
    //          102.7 + 0.5 = 103.2, then (uint16_t) = 103 ✔

    while(1) {
        // Logic Block 1: The Sampling Trigger
        if (time_to_sample) {
            time_to_sample = 0;      // Clear flag immediately

            // Physical Step: Wait for Acquisition Time (Tacq)
            // This allows the internal capacitor to charge to sensor voltage.
            __delay_us(20);

            ADCON0bits.GO = 1;       // Start conversion (Non-blocking)
        }

        // Logic Block 2: The Data Processor
        // This only runs when the hardware interrupt delivered the data.
        if (data_ready) {
            data_ready = 0;          // Clear flag

            // Threshold Logic (Example: ~0.5V threshold)
            // LM35 outputs 10mV/°C, so 0.5V corresponds to 50°C,
            // which is in ADC counts (1.5V / 4.88mV per count).
            if (raw_temp >= threshhold_adc) {
                PORTDbits.RD0 = 1;
            } else {
                PORTDbits.RD0 = 0;
            }
        }

        // Note: The CPU is free here to perform other tasks while
        // waiting for the next 1-second trigger!
    }
}