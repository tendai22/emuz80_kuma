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

extern const unsigned char rom[]; // Equivalent to ROM, see end of this file
unsigned char ram[RAM_SIZE]; // Equivalent to RAM

/*
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
*/

#define dff_reset() do {G3POL = 1; G3POL = 0;} while(0);
#define db_setin() (TRISC = 0xff)
#define db_setout() (TRISC = 0x00)

#define TOGGLE() (LATA^=(1<<7))

#if 0
// Never called, logically
void __interrupt(irq(default),base(8)) Default_ISR(){}

// Called at Z80 MREQ falling edge (PIC18F47Q43 issues WAIT)
void __interrupt(irq(CLC1),base(8)) CLC_ISR(){
   static union {
        unsigned int mem; // 16 bits Address
        struct {
            unsigned char l; // Address low 8 bits
            unsigned char h; // Address high 8 bits
        };
    } address;
    
    CLC1IF = 0; // Clear interrupt flag
    
    address.h = PORTD; // Read address high
    address.l = PORTB; // Read address low
    
    if(!RA5) { // Z80 memory read cycle (RD active)
        db_setout(); // Set data bus as output
        if(address.mem < ROM_SIZE){ // ROM area
            LATC = rom[address.mem]; // Out ROM data
        } else if((address.mem >= RAM_TOP) && (address.mem < (RAM_TOP + RAM_SIZE))){ // RAM area
            LATC = ram[address.mem - RAM_TOP]; // RAM data
        } else if(address.mem == UART_CREG){ // UART control register
            LATC = PIR9; // U3 flag
        } else if(address.mem == UART_DREG){ // UART data register
            LATC = U3RXB; // U3 RX buffer
        } else { // Out of memory
            LATC = 0xff; // Invalid data
        }
    } else { // Z80 memory write cycle (RD inactive)
        if((address.mem >= RAM_TOP) && (address.mem < (RAM_TOP + RAM_SIZE))){ // RAM area
            ram[address.mem - RAM_TOP] = PORTC; // Write into RAM
        } else if(address.mem == UART_DREG) { // UART data register
            U3TXB = PORTC; // Write into U3 TX buffer
        }
    }
    dff_reset(); // WAIT inactive
}
//  Called at Z80 MREQ rising edge
void __interrupt(irq(INT0),base(8)) INT0_ISR(){
    INT0IF = 0; // Clear interrupt flag
    db_setin(); // Set data bus as input
}
#endif

// main routine
void main(void) {
    // System initialize
    OSCFRQ = 0x08; // 64MHz internal OSC

    // Address bus A15-A8 pin
    ANSELD = 0x00; // Disable analog function
    WPUD = 0xff; // Week pull up
    TRISD = 0xff; // Set as input

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

    // INT output pin
    ANSELE2 = 0; // Disable analog function
    LATE2 = 1; // No interrupt request
    TRISE2 = 0; // Set as output

    // IOREQ input pin
    ANSELA0 = 0; // Disable analog function
    WPUA0 = 1; // Week pull up
    TRISA0 = 1; // Set as input

    // RD(RA5)  input pin
    ANSELA5 = 0; // Disable analog function
    WPUA5 = 1; // Week pull up
    TRISA5 = 1; // Set as intput

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
    
    // MREQ(RA1) CLC input pin
    ANSELA1 = 0; // Disable analog function
    WPUA1 = 1; // Week pull up
    TRISA1 = 1; // Set as input
    CLCIN0PPS = 0x1; //RA1->CLC1:CLCIN0;

    // RFSH(RA2) CLC input pin
    ANSELA2 = 0; // Disable analog function
    WPUA2 = 1; // Week pull up
    TRISA2 = 1; // Set as input
    CLCIN1PPS = 0x2; //RA2->CLC1:CLCIN1;

    // WAIT(RA4) CLC output pin
    ANSELA4 = 0; // Disable analog function
    LATA4 = 1; // Default level
    TRISA4 = 0; // Set as output
    RA4PPS = 0x01;  //RA4->CLC1:CLC1;

    // CLC logic configuration
    CLCSELECT = 0x0; // Use only 1 CLC instance  
    CLCnPOL = 0x82; // LCG2POL inverted, LCPOL inverted

    // CLC data inputs select
    CLCnSEL0 = 0; // D-FF CLK assign CLCIN0PPS(RA0)
    CLCnSEL1 = 1; // D-FF D assign CLCIN1PPS(RA2) 
    CLCnSEL2 = 127; // D-FF S assign none
    CLCnSEL3 = 127; // D-FF R assign none

    // CLCn gates logic select
    CLCnGLS0 = 0x1; // Connect LCG1D1N 
    CLCnGLS1 = 0x4; // Connect LCG2D2N
    CLCnGLS2 = 0x0; // Connect none
    CLCnGLS3 = 0x0; // Connect none

    CLCDATA = 0x0; // Clear all CLC outs
    CLCnCON = 0x8c; // Select D-FF, falling edge inturrupt
#if 0
    // Vectored Interrupt Controller setting sequence
    INTCON0bits.IPEN = 1; // Interrupt priority enable

    IVTLOCK = 0x55;
    IVTLOCK = 0xAA;
    IVTLOCKbits.IVTLOCKED = 0x00; // Unlock IVT

    IVTBASEU = 0;
    IVTBASEH = 0;
    IVTBASEL = 8; // Default VIT base address

    IVTLOCK = 0x55;
    IVTLOCK = 0xAA;
    IVTLOCKbits.IVTLOCKED = 0x01; // Lock IVT

    // CLC VI enable
    CLC1IF = 0; // Clear the CLC interrupt flag
    CLC1IE = 1; // Enabling CLC1 interrupt

    // INT0 VI enable
    INT0PPS = 0x1; //RA1->INTERRUPT MANAGER:INT0;
    INT0EDG = 1; // Rising edge
    INT0IE = 1;
#endif
    
//#define Z80_CLK 2500000UL // Z80 clock frequency
#define Z80_CLK 2500000UL // Z80 clock frequency

    // Z80 clock(RA3) by NCO FDC mode
    RA3PPS = 0x3f; // RA3 asign NCO1
    ANSELA3 = 0; // Disable analog function
    TRISA3 = 0; // PWM output pin
    NCO1INCU = (unsigned char)((Z80_CLK*2/61/65536) & 0xff);
    NCO1INCH = (unsigned char)((Z80_CLK*2/61/256) & 0xff);
    NCO1INCL = (unsigned char)((Z80_CLK*2/61) & 0xff);
    NCO1CLK = 0x00; // Clock source Fosc
    NCO1PFM = 0;  // FDC mode
    NCO1OUT = 1;  // NCO output enable
    NCO1EN = 1;   // NCO enable

//#define TEST_PIN A7
#if defined(TEST_PIN)
    // RA7 as TEST pin ()
    TRISA7 = 0;     // A7 output
    LATA7 = 0;      // pin value is 0
    RA7PPS = 0;     // PPS as LATA7
#else
#undef TOGGLE
#define TOGGLE()
#endif //defined(TEST_PIN)
    // Z80 start
    // wait for at least four clocks, before starting Z80
    for (int i = 5; i-- > 0; )
        __asm volatile("nop");
//    GIE = 1; // Global interrupt enable
    LATE1 = 1; // Release reset
    db_setin();

//    while(1); // All things come to those who wait

    while(1) {
        static union {
            unsigned int mem; // 16 bits Address
            struct {
                unsigned char l; // Address low 8 bits
                unsigned char h; // Address high 8 bits
            };
        } address;
        unsigned short mask;
        // infinit nop
        while(RA4);  // WAIT for /WAIT falling down
        TOGGLE();
        // now wait start
        address.h = PORTD; // Read address high
        address.l = PORTB; // Read address low
        if(!RA5) { // Z80 memory read cycle (RD active)
            db_setout(); // Set data bus as output
            mask = address.mem & 0xf000;
            if (!(mask & 0xe000)) {
                LATC = rom[address.mem];
            } else if (mask == 0x8000) {
                LATC = ram[address.mem - RAM_TOP];
            } else if (address.mem == UART_CREG) {
                LATC = PIR9;    // U3 flag
            } else if (address.mem == UART_DREG) {
                LATC = U3RXB;   // U3 RX buffer
            } else {
                LATC = 0xff;
            }
        } else {    // Z80 memory write cycle (RD inactive, WR active)
            while(RE0); // wait for /WR becomes low
            TOGGLE();
            mask = address.mem & 0xf000;
            if (mask == 0x8000) {
                ram[address.mem - RAM_TOP] = PORTC;
            } else if (address.mem == UART_CREG) {
                LATC = PIR9;    // U3 flag
            } else if (address.mem == UART_DREG) {
                U3TXB = PORTC;   // Write into U3 TX buffer
            }
            TOGGLE();
        }
        dff_reset(); // WAIT inactive
        while(!RA1); // wait for MREQ negate
        db_setin();
        TOGGLE();
    }
}


