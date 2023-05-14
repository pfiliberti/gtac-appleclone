# GTAC-2 (Apple II clone) motherboard

## GTAC-2 ROM card connector

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

## References

- [Apple II ROM disassemply](https://6502disassembly.com/a2-rom/)
- [MEWA-48 Apple II clone](http://john.ccac.rwth-aachen.de:8000/patrick/MEWA48.htm)
- [Staff C1 (Apple II clone)](https://www.oldcomputr.com/staff-c1-apple-ii-clone/)
- [Apple II clone](https://www.vintagecomputergarage.com/home-1/2016/2/17/catch-of-the-day-3ck7y)
- It looks like a variation of the GTAC-2 clone motherboard. It only had 7 slots, with slot 4 removed and used by the Z80 and extra 16K of RAM.

## Missing components

  Location    Part
  A1          14.31818MHz crystal
+ A2          7400N
+ C11         74LS04
+ C14         74LS32
+ D2          74LS32
+ D3          74LS00
  D6          74LS20
+ D7          74LS08
+ D8          74LS08 -> 54LS08
+ E7          74LS32 -> 54LS32
+ F5          74LS373
+ F12         74LS138
+ H1          74LS08
+ H10         74LS245
+ H11         556 TIMER
  H13         556 TIMER
+ K13         LM741


