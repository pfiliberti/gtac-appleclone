# Apple II clone motherboard revival

## ROM expansion card

Schematic of ROM expansion. GTAC clones had the ROM in an expansion card plugged into a special ROM slot that replaced the normal Apple II Slot-0. The ROM slot has a slightly different pin out than an expansion slot, specifically with chip enable lines decoded on the motherboard. The ROM card in the schematics was positioned to fit in any *regular* slot and provides its own chip enable decode logic. This was done simply to save the need for building two separate cards, one for ROM and another for any expansion projects.

Schematics are for KiCAD version 6.0.2+dfsg-1

### GTAC-2 ROM card connector

Top view

```

LINE #27 ALSO CONNECTS TO 2716/32 SOCKET P.18 ^OE
LINE #24 ALSO CONNECTS TO 2716/32 SOCKET P.20 ^OE

                       GND  |o 26  25 o|  +5V
(DMA IN) $F000 - $FFFF ^CE  |o 27  24 o|  OPTIONAL PULLDOWN NOT INSTALLED (DMA OUT)
(INT IN) $D000 - $EFFF ^CE  |o 28  23 o|  n.c.                            (INT OUT)
                      ^NMI  |o 29  22 o|  ^DMA
                      ^IRQ  |o 30  21 o|  RDY
                    ^RESET  |o 31  20 o|  ^IOSTB $C800 - CFFF
                      ^INH  |o 32  19 o|  n.c                             (SYNC)
                      -12v  |o 33  18 o|  R/w
                       -5v  |o 34  17 o|  AD15
                     COLOR  |o 35  16 o|  AD14
                        7M  |o 36  15 o|  AD13
                        Q3  |o 37  14 o|  AD12
                       ph1  |o 38  13 o|  AD11
                     USER1  |o 39  12 o|  AD10
                       ph0  |o 40  11 o|  AD9
(^DEVSEL)             n.c.  |o 41  10 o|  AD8
                        D7  |o 42   9 o|  AD7
                        D6  |o 43   8 o|  AD6
                        D5  |o 44   7 o|  AD5
                        D4  |o 45   6 o|  AD4
                        D3  |o 46   5 o|  AD3
                        D2  |o 47   4 o|  AD2
                        D1  |o 48   3 o|  AD1
                        D0  |o 49   2 o|  AD0
                      +12v  |o 50   1 o|  n.c.                             (^IOSEL)

```

### References

- [Apple II ROM disassemply](https://6502disassembly.com/a2-rom/)
- [MEWA-48 Apple II clone](http://john.ccac.rwth-aachen.de:8000/patrick/MEWA48.htm)
- [Staff C1 (Apple II clone)](https://www.oldcomputr.com/staff-c1-apple-ii-clone/)
- [Apple II clone](https://www.vintagecomputergarage.com/home-1/2016/2/17/catch-of-the-day-3ck7y)

## PS2 keyboard to Apple II parallel

Interface card with AVR ATtiny84 for a PS2 keyboard. The software implements a PS2 keyboard interface and an Apple II parallel interface. The AVR connects directly into the Apple II keyboard connector instead of the computer's keyboard. The AVR to Apple II interface is a parallel interface with one strobe signal.

