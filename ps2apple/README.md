# PS2 keyboard interface for Apple II

This program is the interface code for AVR with a PS2 keyboard. It implements a PS2 keyboard interface and an Apple II parallel interface. The AVR connects directly into the Apple II keyboard connector instead of the computer's keyboard. The code configures the PS2 keyboard, accepts scan codes, and converts the AT scan codes to Apple II keyboard codes. The AVR to Apple II interface is a parallel interface with one strobe signal.

## Resources

- [wiki](https://en.wikipedia.org/wiki/PS/2_port)
- [AVR interface to PS2](http://www.electronics-base.com/projects/complete-projects/108-avr-ps2-keyboard-key-readout)
- [PS2 protocol](http://www.burtonsys.com/ps2_chapweske.htm)
- [PS2 keyboard command protocol](https://wiki.osdev.org/PS/2_Keyboard)
- W. Gayler - The Apple II Circuit Description
- Apple II Reference Manual

## Interface

### Connectivity

The general connectivity is as follows:

```

  Apple II                AVR
 +------+               +-----+
 |      |               |     |
 | DATA +--< PA0..6 ]---+     |
 |      |               |     |
 |      |               |     +---> PS2 keyboard
 |      |               |     |
 | ^STB +---< PA7 ]-----+     |
 |      |               |     |
 +------+               +-----+

```

### ATtiny84 AVR IO

| Function   | AVR    | Pin     | I/O               |
|------------|--------|---------|-------------------|
| PS2 clock  | PB0    | 2       | In/out w/ pull up |
| PS2 data   | PB1    | 3       | In/out w/ pull up |
| Strobe     | PA7    | 24      | Out               |
| 7-bit code | PA0..6 | 8..13,7 | Out               |

## Scan code processing

- Only pass make and break codes for keys in range 1 to 83
- Handle 'E0' modifier for keypad by removing the 'E0' which will effectively reduce any keyboard to one that is equivalent to an 83 key keyboard.
- Discard PrtScrn E0,2A,E0,37 and E0,B7,E0,AA; my PC does not support print screen.
- Convert E1 sequence of Pause/Break to scan code 54h/84
