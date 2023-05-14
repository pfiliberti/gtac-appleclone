/*
 * ps2interface.c
 *
 *  This program is the interface code for AVR with a PS2 keyboard.
 *  It implements a PS2 keyboard interface and an Apple II parallel interface.
 *
 * Apple II                AVR
 * +------+               +-----+
 * |      |               |     |
 * | DATA +--< PA0..6 ]---+     |
 * |      |               |     |
 * |      |               |     +---> PS2 keyboard
 * |      |               |     |
 * | ^STB +---< PA7 ]-----+     |
 * |      |               |     |
 * +------+               +-----+
 *
 *
 * ATtiny84 AVR IO
 *
 * | Function   | AVR    | Pin     | I/O               |
 * |------------|--------|---------|-------------------|
 * | PS2 clock  | PB0    | 2       | In/out w/ pull up |
 * | PS2 data   | PB1    | 3       | In/out w/ pull up |
 * | Strobe     | PA7    | 24      | Out               |
 * | 7-bit code | PA0..6 | 8..13,7 | Out               |
 *
 * Port A bit assignment
 *
 *  b7 b6 b5 b4 b3 b2 b1 b0
 *  |  |  |  |  |  |  |  |
 *  |  |  |  |  |  |  |  +--- 'o' \
 *  |  |  |  |  |  |  +------ 'o' |
 *  |  |  |  |  |  +--------- 'o' |
 *  |  |  |  |  +------------ 'o'  Scan code data output
 *  |  |  |  +--------------- 'o' |
 *  |  |  +------------------ 'o' |
 *  |  +--------------------- 'o' /
 *  +------------------------ 'o' Strobe (^STB)
 *
 * Port B bit assignment
 *
 *              b3 b2 b1 b0
 *              |  |  |  |
 *              |  |  |  +--- 'i' PS2 clock
 *              |  |  +------ 'i' PS2 data
 *              |  +--------- 'i'
 *              +------------ 'i'
 *
 * Note: all references to data sheet are for ATtiny84 8006K–AVR–10/10
 *
 * TODO:
 * 1) Keyboard error handling and recovery.
 * 2) Apple II 'REPT' ket not implemented.
 *
 */

#include    <stdint.h>
#include    <stdlib.h>

#include    <avr/io.h>
#include    <avr/interrupt.h>
#include    <avr/wdt.h>
#include    <util/delay.h>

// IO ports B, C, and D initialization
#define     PA_DDR_INIT     0xff        // Port data direction
#define     PA_PUP_INIT     0x00        // Port input pin pull-up
#define     PA_INIT         0b10000000  // Port initial values

#define     PB_DDR_INIT     0x00
#define     PB_PUP_INIT     0x03
#define     PB_INIT         0x00

// Pin change interrupt setting
#define     GIMSK_INIT      0b00100000  // Enable pin change sensing on PCINT8..11
#define     PCMSK1_INIT     0x01        // Enable pin change interrupt on PB0 -> PCINT8

// PS2 control line masks
#define     PS2_CLOCK       0x01
#define     PS2_DATA        0x02

// Apple II strobe
#define     APPLE_STB       0b10000000

// Buffers
#define     PS2_BUFF_SIZE   32

// Host to Keyboard commands
#define     PS2_HK_LEDS     0xED    // Set Status Indicators, next byte LED bitmask
#define     PS2_HK_ECHO     0xEE    // Echo
#define     PS2_HK_INVALID  0xEF    // Invalid Command
#define     PS2_HK_ALTCODE  0xF0    // Select Alternate Scan Codes, next byte Scan code set
#define     PS2_HK_INVALID2 0xF1    // Invalid Command
#define     PS2_HK_TMDELAY  0xF3    // Set Typematic Rate/Delay, next byte Encoded rate/delay
#define     PS2_HK_ENABLE   0xF4    // Enable
#define     PS2_HK_DISABLE  0xF5    // Default Disable
#define     PS2_HK_DEFAULT  0xF6    // Set Default
#define     PS2_HK_SET1     0xF7    // Set All Keys - Typematic
#define     PS2_HK_SET2     0xF8    // Set All Keys - Make/Break
#define     PS2_HK_SET3     0xF8    // Set All Keys - Make
#define     PS2_HK_SET4     0xFA    // Set All Keys - Typematic/Make/Break
#define     PS2_HK_SET5     0xFB    // Set All Key Type - Typematic, next byte Scan code
#define     PS2_HK_SET6     0xFC    // Set All Key Type - Make/Break, next byte Scan code
#define     PS2_HK_SET7     0xFD    // Set All Key Type - Make, next byte Scan code
#define     PS2_HK_RESEND   0xFE    // Resend
#define     PS2_HK_RESET    0xFF    // Reset

#define     PS2_HK_SCRLOCK  1       // Scroll lock - mask 1 on/0 off
#define     PS2_HK_NUMLOCK  2       // Num lock   - mask 1 on/0 off
#define     PS2_HK_CAPSLOCK 4       // Caps lock  - mask 1 on/0 off

#define     PS2_HK_TYPEMAT  0b01111111  // 1Sec delay, 2Hz repetition

// Keyboard to Host commands
#define     PS2_KH_ERR23    0x00    // Key Detection Error/Overrun (Code Sets 2 and 3)
#define     PS2_KH_BATOK    0xAA    // BAT Completion Code
#define     PS2_KH_ERR      0xFC    // BAT Failure Code
#define     PS2_KH_ECHO     0xEE    // Echo
#define     PS2_KH_BREAK    0xF0    // Break (key-up)
#define     PS2_KH_ACK      0xFA    // Acknowledge (ACK)
#define     PS2_KH_RESEND   0xFE    // Resend
#define     PS2_KH_ERR1     0xFF    // Key Detection Error/Overrun (Code Set 1)

// Apple II keyboard
#define     KBNA            0b00000000
#define     CTRL            0b00000001
#define     SHFT            0b00000010

/****************************************************************************
  Types
****************************************************************************/
typedef enum
{
    PS2_IDLE,
    PS2_DATA_BITS,
    PS2_PARITY,
    PS2_STOP,
    PS2_RX_ERR_START,
    PS2_RX_ERR_OVERRUN,
    PS2_RX_ERR_PARITY,
    PS2_RX_ERR_STOP
} ps2_state_t;

/****************************************************************************
  Function prototypes
****************************************************************************/
void    reset(void) __attribute__((naked)) __attribute__((section(".init3")));
void    ioinit(void);

int     ps2_send(uint8_t);
int     ps2_recv_x(void);       // Blocking
int     ps2_recv(void);         // Non-blocking

void    kbd_test_led(void);
int     kdb_led_ctrl(uint8_t);
int     kbd_code_set(int);
int     kbd_typematic_set(uint8_t);

void    apple_kbd_write(int code, uint8_t shift_ctrl_flags);
void    apple_kbd_stb(void);

/****************************************************************************
  Globals
****************************************************************************/
// Circular buffer holding PS2 scan codes
uint8_t      ps2_scan_codes[PS2_BUFF_SIZE];
int          ps2_buffer_out = 0;
volatile int ps2_buffer_in = 0;
volatile int ps2_scan_code_count = 0;

// Variable maintaining state of bit stream from PS2
volatile ps2_state_t ps2_rx_state = PS2_IDLE;
volatile uint8_t  ps2_rx_data_byte = 0;
volatile int      ps2_rx_bit_count = 0;
volatile int      ps2_rx_parity = 0;

// PS2 keyboard status
volatile uint8_t    kbd_lock_keys = 0;

// Shift status and scan code translation tables
uint8_t shift_ctrl_state = KBNA;

uint8_t scan_code_xlate[4][58] =
{
    /* Normal codes
     */
    {   
        KBNA,                                                       // 0
        0x9b, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, // 10
        0xb0, 0xad, 0xba, KBNA, KBNA, 0xd1, 0xd7, 0xc5, 0xd2, 0xd4, // 20
        0xd9, 0xd5, 0xc9, 0xcf, 0xd0, KBNA, KBNA, 0x8d, KBNA, 0xc1, // 30
        0xd3, 0xc4, 0xc6, 0xc7, 0xc8, 0xca, 0xcb, 0xcc, 0xbb, KBNA, // 40
        KBNA, KBNA, KBNA, 0xda, 0xd8, 0xc3, 0xd6, 0xc2, 0xce, 0xcd, // 50
        0xac, 0xae, 0xaf, KBNA, 0x88, 0x95, 0xa0                    // 57
    },

    /* Ctrl codes
     */
    {
        KBNA,                                                       // 0
        0x9b, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, // 10
        0xb0, 0xad, 0xba, KBNA, KBNA, 0x91, 0x97, 0x85, 0x92, 0x94, // 20
        0x99, 0x95, 0x89, 0x8f, 0x90, KBNA, KBNA, 0x8d, KBNA, 0x81, // 30
        0x93, 0x84, 0x86, 0x87, 0x88, 0x8a, 0x8b, 0x8c, 0xbb, KBNA, // 40
        KBNA, KBNA, KBNA, 0x9a, 0x98, 0x83, 0x96, 0x82, 0x8e, 0x8d, // 50
        0xac, 0xae, 0xaf, KBNA, 0x88, 0x95, 0xa0                    // 57
    },

    /* Shift codes
     */
    {
        KBNA,                                                       // 0
        0x9b, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, // 10
        0xb0, 0xbd, 0xaa, KBNA, KBNA, 0xd1, 0xd7, 0xc5, 0xd2, 0xd4, // 20
        0xd9, 0xd5, 0xc9, 0xcf, 0xc0, KBNA, KBNA, 0x8d, KBNA, 0xc1, // 30
        0xd3, 0xc4, 0xc6, 0xc7, 0xc8, 0xca, 0xcb, 0xcc, 0xab, KBNA, // 40
        KBNA, KBNA, KBNA, 0xda, 0xd8, 0xc3, 0xd6, 0xc2, 0xde, 0xdd, // 50
        0xbc, 0xbe, 0xbf, KBNA, 0x88, 0x95, 0xa0                    // 57
    },

    /* Shift + Ctrl codes
     */
    {
        KBNA,                                                       // 0
        0x9b, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, // 10
        0xb0, 0xbd, 0xaa, KBNA, KBNA, 0x91, 0x97, 0x85, 0x92, 0x94, // 20
        0x99, 0x95, 0x89, 0x8f, 0x80, KBNA, KBNA, 0x8d, KBNA, 0x81, // 30
        0x93, 0x84, 0x86, 0x87, 0x88, 0x8a, 0x8b, 0x8c, 0xab, KBNA, // 40
        KBNA, KBNA, KBNA, 0x9a, 0x98, 0x83, 0x96, 0x82, 0x9e, 0x94, // 50
        0xbc, 0xbe, 0xbf, KBNA, 0x88, 0x95, 0xa0                    // 57
    }
};

/* ----------------------------------------------------------------------------
 * main() control functions
 *
 */
int main(void)
{
    int     scan_code;

    // Initialize IO devices
    ioinit();

    // Wait enough time for keyboard to complete self test
    _delay_ms(1000);

    // Light LEDs in succession
    kbd_test_led();

    // Set typematic delay and rate
    kbd_typematic_set(PS2_HK_TYPEMAT);

    // Change code set to "1" so code set translation does not needs to take place on the AVR
    kbd_code_set(1);

    // Caps lock on as power indicator
    kdb_led_ctrl(PS2_HK_CAPSLOCK);

    // Start watch-dog timer here after.
    wdt_enable(WDTO_500MS);
    sei();

    // loop forever
    while ( 1 )
    {
        /* Reset the watch-dog here. May not be neede but left this code
         * here just in case Apple hangs etc.
         */
        wdt_reset();

        scan_code = ps2_recv();

        /* Only pass make and break codes for keys in range 1 to 83
         * Handle 'E0' modifier for keypad by removing the 'E0' which
         * will effectively reduce any keyboard to one that is equivalent to an 83 key keyboard.
         * Discard PrtScrn E0,2A,E0,37 and E0,B7,E0,AA.
         * dISCARD E1 sequence of Pause/Break.
         */
        if  ( scan_code != -1 )
        {
            /* Handle 'E1' scan code case for Pause/Break key
             */
            if ( scan_code == 0xe1 )
            {
                scan_code = ps2_recv_x();

                /* Get the next (3rd) byte and discard sequence
                 */
                if ( scan_code == 0x1d )
                {
                    ps2_recv_x();
                    continue;
                }
                else if ( scan_code == 0x9d )
                {
                    ps2_recv_x();
                    continue;
                }
            }

            /* Handle 'E0' scan code cases
             */
            if ( scan_code == 0xe0 )
            {
                scan_code = ps2_recv_x();

                /* Right Ctrl same as Left Ctrl, retain both 'make' and 'break'
                 */
                if ( scan_code == 0x1d ||
                     scan_code == 0x9d    )
                {
                    // Do nothing.
                }
                /* Left and right arrow keys captured
                 * are remapped.
                 */
                else if ( scan_code == 0x4b )
                {
                    scan_code = 55;
                }
                else if ( scan_code == 0x4d )
                {
                    scan_code = 56;
                }
                /* Discard all the rest including break codes
                 */
                else
                {
                    continue;
                }
            }

            /* Detect state of Shift and Ctrl keys
             */
            if ( scan_code == 0x1d )
            {
                shift_ctrl_state |= CTRL;   // Ctrl 'make'
                continue;
            }
            else if ( scan_code == 0x9d )
            {
                shift_ctrl_state &= ~CTRL;  // Ctrl 'break'
                continue;
            }
            else if ( scan_code == 0x2a ||
                      scan_code == 0x36 )
            {
                shift_ctrl_state |= SHFT;   // Shift 'make'
                continue;
            }
            else if ( scan_code == 0xaa ||
                      scan_code == 0xb6 )
            {
                shift_ctrl_state &= ~SHFT;  // Shift 'break'
                continue;
            }

            /* Now it is safe to ignore unwanted scan codes
             * including all 'Break' codes
             */
            switch ( scan_code )
            {
                case 14:            // BS
                case 15:            // TAB
                case 26:            // [
                case 27:            // ]
                case 40:            // '
                case 41:            // `
                case 43:            // forward-slash
                case 58 ... 255:    // Locks and Fn keys etc. TODO
                    continue;
            }

            /* Send code to Apple II
             */
            if ( scan_code > 0 )
                apple_kbd_write(scan_code, shift_ctrl_state);
        }
    }

    return 0;
}

/* ----------------------------------------------------------------------------
 * reset()
 *
 *  Clear SREG_I on hardware reset.
 *  source: http://electronics.stackexchange.com/questions/117288/watchdog-timer-issue-avr-atmega324pa
 */
void reset(void)
{
     cli();
    // Note that for newer devices (any AVR that has the option to also
    // generate WDT interrupts), the watchdog timer remains active even
    // after a system reset (except a power-on condition), using the fastest
    // prescaler value (approximately 15 ms). It is therefore required
    // to turn off the watchdog early during program startup.
    MCUSR = 0; // clear reset flags
    wdt_disable();
}

/* ----------------------------------------------------------------------------
 * ioinit()
 *
 *  initialize IO interfaces
 *  timer and data rates calculated based on 4MHz internal clock
 *
 */
void ioinit(void)
{
    // Reconfigure system clock scaler to 8MHz
    CLKPR = 0x80;   // change clock scaler (sec 8.12.2 p.37)
    CLKPR = 0x00;

    // initialize general IO PB and PD pins
    DDRB  = ~PB_DDR_INIT;
    PORTB = PB_INIT;
    DDRB  = PB_DDR_INIT;
    PORTB = PB_INIT | PB_PUP_INIT;

    DDRA  = PA_DDR_INIT;
    PORTA = PA_INIT | PA_PUP_INIT;

    // pin change interrupt setting
    GIMSK = GIMSK_INIT;
    PCMSK1 = PCMSK1_INIT;
}

/* ----------------------------------------------------------------------------
 * ps2_send_cmd()
 *
 *  Send a command to the PS2 keyboard
 *  1)   Bring the Clock line low for at least 100 microseconds.
 *  2)   Bring the Data line low.
 *  3)   Release the Clock line.
 *
 *  4)   Set/reset the Data line to send the first data bit
 *  5)   Wait for the device to bring Clock low.
 *  6)   Repeat steps 5-7 for the other seven data bits and the parity bit
 *  7)   Wait for the device to bring Clock high.
 *
 *  8)   Release the Data line.
 *  9)   Wait for the device to bring Data low.
 *  10)  Wait for the device to bring Clock  low.
 *  11)  Wait for the device to release Data and Clock
 *
 *  param: command byte
 *  return: -1 transmit error, 0 ok
 */
int ps2_send(uint8_t byte)
{
    int     ps2_tx_parity = 1;
    int     ps2_tx_bit_count = 0;
    uint8_t ps2_data_bit;
    int     result;

    // disable interrupts and reset receiver state so receive ISR does not run
    cli();
    ps2_rx_state = PS2_IDLE;
    ps2_rx_data_byte = 0;
    ps2_rx_bit_count = 0;
    ps2_rx_parity = 0;

    // follow byte send steps
    DDRB |= PS2_CLOCK;
    PORTB &= ~PS2_CLOCK;
    _delay_us(100);

    DDRB |= PS2_DATA;
    PORTB &= ~PS2_DATA;

    DDRB &= ~PS2_CLOCK;
    PORTB |= PS2_CLOCK;

    while ( ps2_tx_bit_count < 10 )
    {
        // this will repeat 8 bits of data, one parity bit, and one stop bit for transmission
        if ( ps2_tx_bit_count < 8 )
        {
            ps2_data_bit = byte & 0x01;
            ps2_tx_parity += ps2_data_bit;
        }
        else if ( ps2_tx_bit_count == 8 )
        {
            ps2_data_bit = (uint8_t)ps2_tx_parity & 0x01;
        }
        else
        {
            ps2_data_bit = 1;
        }

        do {} while ( (PINB & PS2_CLOCK) );

        if ( ps2_data_bit )
            PORTB |= PS2_DATA;
        else
            PORTB &= ~PS2_DATA;

        do {} while ( !(PINB & PS2_CLOCK) );

        ps2_tx_bit_count++;
        byte = byte >> 1;
    }

    // restore data line to receive mode
    DDRB &= ~PS2_DATA;
    PORTB |= PS2_DATA;

    // check here for ACK pulse and line to idle
    do {} while ( (PINB & PS2_CLOCK) );
    result = -1 * (int)(PINB & PS2_DATA);

    // wait for clock to go high before enabling interrupts
    do {} while ( !(PINB & PS2_CLOCK) );
    sei();

    // allow keyboard to recover before exiting,
    // so that another ps2_send() is spaced in time from this call.
    _delay_ms(20);

    return result;
}

/* ----------------------------------------------------------------------------
 * ps2_recv_x()
 *
 *  Get a byte from the PS2 input buffer and block until byte is available
 *
 *  param:  none
 *  return: data byte value
 *
 */
int ps2_recv_x(void)
{
    int     result;

    do
    {
        result = ps2_recv();
    } while ( result == -1 );

    return result;
}

/* ----------------------------------------------------------------------------
 * ps2_recv()
 *
 *  Get a byte from the PS2 input buffer
 *
 *  param:  none
 *  return: -1 if buffer empty, otherwise data byte value
 *
 */
int ps2_recv(void)
{
    int     result = -1;

    if ( ps2_scan_code_count > 0 )
    {
        result = (int)ps2_scan_codes[ps2_buffer_out];
        ps2_scan_code_count--;
        ps2_buffer_out++;
        if ( ps2_buffer_out == PS2_BUFF_SIZE )
            ps2_buffer_out = 0;
    }

    return result;
}

/* ----------------------------------------------------------------------------
 * ps2_test_led()
 *
 *  Simple light test for keyboard LEDs.
 *  The function discards the keyboard's response;
 *  if something is wrong LEDs want light up in succession.
 *
 *  param:  none
 *  return: none
 */
void kbd_test_led(void)
{
    kdb_led_ctrl(PS2_HK_SCRLOCK);

    _delay_ms(200);

    kdb_led_ctrl(0);
    kdb_led_ctrl(PS2_HK_CAPSLOCK);

    _delay_ms(200);

    kdb_led_ctrl(0);
    kdb_led_ctrl(PS2_HK_NUMLOCK);

    _delay_ms(200);

    kdb_led_ctrl(0);
    kdb_led_ctrl(PS2_HK_CAPSLOCK);

    _delay_ms(200);

    kdb_led_ctrl(0);
    kdb_led_ctrl(PS2_HK_SCRLOCK);

    _delay_ms(200);

    kdb_led_ctrl(0);
}

/* ----------------------------------------------------------------------------
 * kdb_led_ctrl()
 *
 *  Function for setting LED state to 'on' or 'off'
 *
 *  param:  LED bits, b0=Scroll lock b1=Num lock b2=Caps Lock
 *  return: none
 */
int kdb_led_ctrl(uint8_t state)
{
    int temp_scan_code;

    state &= 0x07;

    ps2_send(PS2_HK_LEDS);
    temp_scan_code = ps2_recv_x();

    if ( temp_scan_code == PS2_KH_ACK )
    {
        ps2_send(state);
        temp_scan_code = ps2_recv_x();
    }

    return temp_scan_code;
}

/* ----------------------------------------------------------------------------
 * kbd_code_set()
 *
 *  The function requests the keyboard to change its scan code set,
 *  Legal values are 1, 2 or 3.
 *
 *  param:  scan code set identifier
 *  return: PS2_KH_ACK no errors, other keyboard response if error
 */
int kbd_code_set(int set)
{
    int temp_scan_code;

    if ( set < 1 || set > 3 )
        return PS2_KH_RESEND;

    ps2_send(PS2_HK_ALTCODE);
    temp_scan_code = ps2_recv_x();

    if ( temp_scan_code == PS2_KH_ACK )
    {
        ps2_send((uint8_t)set);
        temp_scan_code = ps2_recv_x();
    }

    return temp_scan_code;
}

/* ----------------------------------------------------------------------------
 * kbd_typematic_set()
 *
 *  The function sets the keyboard typematic rate and delay.
 *
 *  Bit/s   Meaning
 *  ....... ...................................................
 *  0 to 4  Repeat rate (00000b = 30 Hz, ..., 11111b = 2 Hz)
 *  5 to 6  Delay before keys repeat (00b = 250 ms, 01b = 500 ms, 10b = 750 ms, 11b = 1000 ms)
 *     7    Must be zero
 *
 *  param:  typematic rate and delay
 *  return: PS2_KH_ACK no errors, other keyboard response if error
 */
int kbd_typematic_set(uint8_t configuration)
{
    int temp_scan_code;

    configuration &= 0x7f;

    ps2_send(PS2_HK_TMDELAY);
    temp_scan_code = ps2_recv_x();

    if ( temp_scan_code == PS2_KH_ACK )
    {
        ps2_send(configuration);
        temp_scan_code = ps2_recv_x();
    }

    return temp_scan_code;
}

/* ----------------------------------------------------------------------------
 * apple_kbd_write()
 *
 *  Write keyboard code and pulse the strobe line.
 *
 *  param:  Keyboard code, Shift and Ctrl key flags
 *  return: none
 *
 */
void apple_kbd_write(int code, uint8_t shift_ctrl_flags)
{
    uint8_t byte;

    byte = scan_code_xlate[shift_ctrl_flags][code];
    if ( byte > 0x80 )
        PORTA = byte;

    _delay_ms(8);       // Not really needed.
    apple_kbd_stb();
}

/* ----------------------------------------------------------------------------
 * apple_kbd_stb()
 *
 *  Pulse the Apple II strobe line (active low).
 *
 *  param:  none
 *  return: none
 *
 */
void apple_kbd_stb(void)
{
    PORTA &= ~APPLE_STB;
    _delay_us(2);
    PORTA |= APPLE_STB;
}

/* ----------------------------------------------------------------------------
 * This ISR will trigger when PB0 changes state.
 * PB0 is connected to the PS2 keyboard clock line, and PB1 to the data line.
 * ISR will check PB0 state and determine if it is '0' or '1',
 * as well as track clock counts and input bits from PB1.
 * Once input byte is assembled is will be added to a circular buffer.
 *
 */
ISR(PCINT1_vect)
{
    uint8_t         ps2_data_bit;

    if ( (PINB & PS2_CLOCK) == 0 )
    {
        ps2_data_bit = (PINB & PS2_DATA) >> 1;

        switch ( ps2_rx_state )
        {
            /* do nothing if an error was already signaled
             * let the main loop handle the error
             */
            case PS2_RX_ERR_START:
            case PS2_RX_ERR_OVERRUN:
            case PS2_RX_ERR_PARITY:
            case PS2_RX_ERR_STOP:
                break;

            /* if in idle, then check for valid start bit
             */
            case PS2_IDLE:
                if ( ps2_data_bit == 0 )
                {
                    ps2_rx_data_byte = 0;
                    ps2_rx_bit_count = 0;
                    ps2_rx_parity = 0;
                    ps2_rx_state = PS2_DATA_BITS;
                }
                else
                    ps2_rx_state = PS2_RX_ERR_START;
                break;

            /* accumulate eight bits of data LSB first
             */
            case PS2_DATA_BITS:
                ps2_rx_parity += ps2_data_bit;
                ps2_data_bit = ps2_data_bit << ps2_rx_bit_count;
                ps2_rx_data_byte += ps2_data_bit;
                ps2_rx_bit_count++;
                if ( ps2_rx_bit_count == 8 )
                    ps2_rx_state = PS2_PARITY;
                break;

            /* evaluate the parity and signal error if it is wrong
             */
            case PS2_PARITY:
                if ( ((ps2_rx_parity + ps2_data_bit) & 1) )
                    ps2_rx_state = PS2_STOP;
                else
                    ps2_rx_state = PS2_RX_ERR_PARITY;
                break;

            /* check for valid stop bit
             */
            case PS2_STOP:
                if ( ps2_data_bit == 1 )
                {
                    if ( ps2_scan_code_count < PS2_BUFF_SIZE )
                    {
                        ps2_scan_codes[ps2_buffer_in] = ps2_rx_data_byte;
                        ps2_scan_code_count++;
                        ps2_buffer_in++;
                        if ( ps2_buffer_in == PS2_BUFF_SIZE )
                            ps2_buffer_in = 0;
                        ps2_rx_state = PS2_IDLE;
                    }
                    else
                        ps2_rx_state = PS2_RX_ERR_OVERRUN;
                }
                else
                    ps2_rx_state = PS2_RX_ERR_STOP;
                break;
        }
    }
}
