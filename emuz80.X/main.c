/*
  PIC18F47Q43 ROM RAM and UART emulation firmware

  Target: EMUZ80 - The computer with only Z80 and PIC18F47Q43
  Compiler: MPLAB XC8 v2.36
  Written by Tetsuya Suzuki
*/

// CONFIG1
#pragma config FEXTOSC = OFF    // External Oscillator Selection (Oscillator not enabled)
#pragma config RSTOSC = HFINTOSC_64MHZ// Reset Oscillator Selection (HFINTOSC with HFFRQ = 64 MHz and CDIV = 1:1)

// CONFIG2
#pragma config CLKOUTEN = OFF   // Clock out Enable bit (CLKOUT function is disabled)
#pragma config PR1WAY = ON      // PRLOCKED One-Way Set Enable bit (PRLOCKED bit can be cleared and set only once)
#pragma config CSWEN = ON       // Clock Switch Enable bit (Writing to NOSC and NDIV is allowed)
#pragma config FCMEN = ON       // Fail-Safe Clock Monitor Enable bit (Fail-Safe Clock Monitor enabled)

// CONFIG3
#pragma config MCLRE = EXTMCLR  // MCLR Enable bit (If LVP = 0, MCLR pin is MCLR; If LVP = 1, RE3 pin function is MCLR )
#pragma config PWRTS = PWRT_OFF // Power-up timer selection bits (PWRT is disabled)
#pragma config MVECEN = ON      // Multi-vector enable bit (Multi-vector enabled, Vector table used for interrupts)
#pragma config IVT1WAY = ON     // IVTLOCK bit One-way set enable bit (IVTLOCKED bit can be cleared and set only once)
#pragma config LPBOREN = OFF    // Low Power BOR Enable bit (Low-Power BOR disabled)
#pragma config BOREN = SBORDIS  // Brown-out Reset Enable bits (Brown-out Reset enabled , SBOREN bit is ignored)

// CONFIG4
#pragma config BORV = VBOR_1P9  // Brown-out Reset Voltage Selection bits (Brown-out Reset Voltage (VBOR) set to 1.9V)
#pragma config ZCD = OFF        // ZCD Disable bit (ZCD module is disabled. ZCD can be enabled by setting the ZCDSEN bit of ZCDCON)
#pragma config PPS1WAY = OFF    // PPSLOCK bit One-Way Set Enable bit (PPSLOCKED bit can be set and cleared repeatedly (subject to the unlock sequence))
#pragma config STVREN = ON      // Stack Full/Underflow Reset Enable bit (Stack full/underflow will cause Reset)
#pragma config LVP = ON         // Low Voltage Programming Enable bit (Low voltage programming enabled. MCLR/VPP pin function is MCLR. MCLRE configuration bit is ignored)
#pragma config XINST = OFF      // Extended Instruction Set Enable bit (Extended Instruction Set and Indexed Addressing Mode disabled)

// CONFIG5
#pragma config WDTCPS = WDTCPS_31// WDT Period selection bits (Divider ratio 1:65536; software control of WDTPS)
#pragma config WDTE = OFF       // WDT operating mode (WDT Disabled; SWDTEN is ignored)

// CONFIG6
#pragma config WDTCWS = WDTCWS_7// WDT Window Select bits (window always open (100%); software control; keyed access not required)
#pragma config WDTCCS = SC      // WDT input clock selector (Software Control)

// CONFIG7
#pragma config BBSIZE = BBSIZE_512// Boot Block Size selection bits (Boot Block size is 512 words)
#pragma config BBEN = OFF       // Boot Block enable bit (Boot block disabled)
#pragma config SAFEN = OFF      // Storage Area Flash enable bit (SAF disabled)
#pragma config DEBUG = OFF      // Background Debugger (Background Debugger disabled)

// CONFIG8
#pragma config WRTB = OFF       // Boot Block Write Protection bit (Boot Block not Write protected)
#pragma config WRTC = OFF       // Configuration Register Write Protection bit (Configuration registers not Write protected)
#pragma config WRTD = OFF       // Data EEPROM Write Protection bit (Data EEPROM not Write protected)
#pragma config WRTSAF = OFF     // SAF Write protection bit (SAF not Write Protected)
#pragma config WRTAPP = OFF     // Application Block write protection bit (Application Block not write protected)

// CONFIG10
#pragma config CP = OFF         // PFM and Data EEPROM Code Protection bit (PFM and Data EEPROM code protection disabled)

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.

#include <xc.h>
#include <stdio.h>

#include "config.h"

#define _XTAL_FREQ 64000000UL

extern const unsigned char rom[]; // Equivalent to ROM, see the file "rom.c'
unsigned char ram[RAM_SIZE]; // Equivalent to RAM


// UART3 Transmit
void putch(char c) {
    while(!U3TXIF); // Wait or Tx interrupt flag set
    U3TXB = c; // Write data
}

// UART3 Recive
char getch(void) {
    while(!U3RXIF); // Wait for Rx interrupt flag set
    return U3RXB; // Read data
}

#define dff_reset() do {G3POL = 1; G3POL = 0;} while(0);
#define db_setin() (TRISC = 0xff)
#define db_setout() (TRISC = 0x00)

#define TOGGLE() (LATD^=(1<<5))

// main routine
void main(void) {
    // System initialize
    OSCFRQ = 0x08; // 64MHz internal OSC

    // Address bus A12-A8 pin
    ANSELD = 0x00; // Disable analog function
    WPUD = 0x1f; // Week pull up
    TRISD = 0x1f; // Set as input
    
    // Address bus A15-A13 pins
    ANSELA = 0x00;
    WPUA = 0x07;
    TRISA = 0x07;

    // Address bus A7-A0 pin
    ANSELB = 0x00; // Disable analog function
    WPUB = 0xff; // Week pull up
    TRISB = 0xff; // Set as input

    // Data bus D7-D0 pin
    ANSELC = 0x00; // Disable analog function
    WPUC = 0xff; // Week pull up
    TRISC = 0xff; // Set as input(default)

    // WR input pin
    ANSELE0 = 0; // Disable analog function
    WPUE0 = 1; // Week pull up
    TRISE0 = 1; // Set as input

    // RESET output pin
    ANSELE1 = 0; // Disable analog function
    LATE1 = 0; // Reset
    TRISE1 = 0; // Set as output

    // /CSRAM output pin
    ANSELE2 = 0; // Disable analog function
    LATE2 = 1; // Disable SRAM
    TRISE2 = 0; // Set as output

    // IOREQ input pin
    ANSELA0 = 0; // Disable analog function
    WPUA0 = 1; // Week pull up
    TRISA0 = 1; // Set as input

    // RD(RA3)  input pin
    ANSELA3 = 0; // Disable analog function
    WPUA3 = 1; // Week pull up
    TRISA3 = 1; // Set as intput

    // UART3 initialize
    U3BRG = 416; // 9600bps @ 64MHz
    U3RXEN = 1; // Receiver enable
    U3TXEN = 1; // Transmitter enable

    // UART3 Receiver
    ANSELA7 = 0; // Disable analog function
    TRISA7 = 1; // RX set as input
    U3RXPPS = 0x07; //RA7->UART3:RX3;

    // UART3 Transmitter
    ANSELA6 = 0; // Disable analog function
    LATA6 = 1; // Default level
    TRISA6 = 0; // TX set as output
    RA6PPS = 0x26;  //RA6->UART3:TX3;

    U3ON = 1; // Serial port enable
    
    // MREQ(RA5) CLC input pin
    ANSELA5 = 0; // Disable analog function
    WPUA5 = 1; // Week pull up
    TRISA5 = 1; // Set as input
    CLCIN0PPS = 0x5; //RA5->CLC1:CLCIN0;

    // RFSH(RD6) CLC input pin
    ANSELD6 = 0; // Disable analog function
    WPUD6 = 1; // Week pull up
    TRISD6 = 1; // Set as input
    CLCIN2PPS = 0x1E; //RD6->CLC1:CLCIN2;

    // WAIT(RA4) CLC output pin
    ANSELA4 = 0; // Disable analog function
    LATA4 = 1; // Default level
    TRISA4 = 0; // Set as output
    RA4PPS = 0x01;  //RA4->CLC1:CLC1;

    // CLC logic configuration
    CLCSELECT = 0x0; // Use only 1 CLC instance  
    CLCnPOL = 0x82; // LCG2POL inverted, LCPOL inverted

    // CLC data inputs select
    CLCnSEL0 = 0; // D1S D-FF CLK assign CLCIN0PPS(RA0)
    CLCnSEL1 = 2; // D2S D-FF D assign CLCIN2PPS(RD6) 
    CLCnSEL2 = 127; // D-FF S assign none
    CLCnSEL3 = 127; // D-FF R assign none

    // CLCn gates logic select
    CLCnGLS0 = 0x1; // Connect LCG1D1N 
    CLCnGLS1 = 0x4; // Connect LCG2D2N
    CLCnGLS2 = 0x0; // Connect none
    CLCnGLS3 = 0x0; // Connect none

    CLCDATA = 0x0; // Clear all CLC outs
    CLCnCON = 0x8c; // Select D-FF, falling edge inturrupt
    
//#define Z80_CLK 2500000UL // Z80 clock frequency

    // Z80 clock(RD7) by NCO FDC mode
    RD7PPS = 0x3f; // RD7 asign NCO1
    ANSELD7 = 0; // Disable analog function
    TRISD7 = 0; // PWM output pin
    NCO1INCU = (unsigned char)((Z80_CLK*2/61/65536) & 0xff);
    NCO1INCH = (unsigned char)((Z80_CLK*2/61/256) & 0xff);
    NCO1INCL = (unsigned char)((Z80_CLK*2/61) & 0xff);
    NCO1CLK = 0x00; // Clock source Fosc
    NCO1PFM = 0;  // FDC mode
    NCO1OUT = 1;  // NCO output enable
    NCO1EN = 1;   // NCO enable

#define TEST_PIN D5
#if defined(TEST_PIN)
    // RD5 as TEST pin ()
//    printf("hello, world\r\n");
    TRISD5 = 0;     // D5 output
    LATD5 = 0;      // pin value is 0
    RD5PPS = 0;     // PPS as LATD5
#if 0
    while (1) {
        TOGGLE();
        __delay_ms(500);
        TOGGLE();
        __delay_ms(500);
    }
#endif 
#else
#undef TOGGLE
#define TOGGLE()
#endif //defined(TEST_PIN)
    // Z80 start
    // wait for at least four clocks, before starting Z80
    for (int i = 5; i-- > 0; )
        __asm volatile("nop");  // count 5 is rough estimation.
    LATE1 = 1; // Release reset
    db_setin(); // Databus Input mode

    while(1) {
        static union {
            unsigned int mem; // 16 bits Address
            struct {
                unsigned char l; // Address low 8 bits
                unsigned char h; // Address high 8 bits
            };
        } address;
        unsigned short mask;
        static unsigned char c;
        // infinit nop
        while(1) {
            while (RA4);    // wait for /WAIT == L
            TOGGLE();
            c = PORTA & 0x1f;
            if (c == 0) {
                // READ_ROM
                address.h = PORTD & 0x1f; // Read address high
                address.l = PORTB; // Read address low
                db_setout(); // Set data bus as output
                LATC = c = rom[address.mem];
                dff_reset(); // WAIT inactive
                while(!RA5); // wait for MREQ negate
                db_setin();
                TOGGLE();
                continue;
            } else if (c == 4) {
                // READ_RAM
                address.h = PORTD & 0xf; // Read address high
                address.l = PORTB; // Read address low
                db_setout(); // Set data bus as output
                LATC = ram[address.mem];
                dff_reset(); // WAIT inactive
                while(!RA5); // wait for MREQ negate
                db_setin();
                TOGGLE();
                continue;
            } else if (c == 7) {
                // READ UART
                db_setout();
                if (PORTB == (UART_CREG & 0xff)) {
                    LATC = c = PIR9;    // U3 flag
                } else if (PORTB == (UART_DREG & 0xff)) {
                    LATC = c = U3RXB;   // U3 RX buffer
                }
                //printf("[%04x,%02X]", PORTC, c);
                dff_reset(); // WAIT inactive
                while(!RA5); // wait for MREQ negate
                db_setin();
                TOGGLE();
                continue;
            } else if (c == 12) {
                // WRITE_RAM
                address.h = PORTD & 0xf; // Read address high
                address.l = PORTB; // Read address low
                while(RE0);     // wait for WR Low
                ram[address.mem] = PORTC;
                dff_reset(); // WAIT inactive
                TOGGLE();
                continue;
            } else if (c == 15) {
                // WRITE_UART
                while(RE0);     // WAIT for WR Low
                c = PORTB;
                if (c == (UART_CREG & 0xff)) {
                    PIR9 = PORTC;    // U3 flag
                } else if (c == (UART_DREG & 0xff)) {
                    U3TXB = PORTC;   // Write into U3 TX buffer
                }
                dff_reset(); // WAIT inactive
                TOGGLE();
                continue;
            } else {
                dff_reset(); // WAIT inactive
                TOGGLE();
                continue;
            }
        }
    }
}


