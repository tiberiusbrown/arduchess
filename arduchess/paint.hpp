#pragma once

static void paint_half(uint8_t const* b, bool clear)
{
  uint16_t count;

  asm volatile (
    "   ldi   %A[count], %[len_lsb]               \n\t" //for (len = WIDTH * HEIGHT / 8)
    "   ldi   %B[count], %[len_msb]               \n\t"
    "1: ld    __tmp_reg__, %a[ptr]      ;2        \n\t" //tmp = *(image)
    "   out   %[spdr], __tmp_reg__      ;1        \n\t" //SPDR = tmp
    "   cpse  %[clear], __zero_reg__    ;1/2      \n\t" //if (clear) tmp = 0;
    "   mov   __tmp_reg__, __zero_reg__ ;1        \n\t"
    "2: sbiw  %A[count], 1              ;2        \n\t" //len --
    "   sbrc  %A[count], 0              ;1/2      \n\t" //loop twice for cheap delay
    "   rjmp  2b                        ;2        \n\t"
    "   st    %a[ptr]+, __tmp_reg__     ;2        \n\t" //*(image++) = tmp
    "   brne  1b                        ;1/2 :18  \n\t" //len > 0
    "   in    __tmp_reg__, %[spsr]                \n\t" //read SPSR to clear SPIF
    : [ptr]     "+&e" (b),
      [count]   "=&w" (count)
    : [spdr]    "I"   (_SFR_IO_ADDR(SPDR)),
      [spsr]    "I"   (_SFR_IO_ADDR(SPSR)),
      [len_msb] "M"   (64 * (64 / 8 * 2) >> 8),   // 8: pixels per byte
      [len_lsb] "M"   (64 * (64 / 8 * 2) & 0xFF), // 2: for delay loop multiplier
      [clear]   "r"   (clear)
  );
}

static void paint_left_half(bool clear)
{
  Arduboy2Core::LCDCommandMode();
  Arduboy2Core::SPItransfer(0x21);
  Arduboy2Core::SPItransfer(0);
  Arduboy2Core::SPItransfer(63);
  Arduboy2Core::LCDDataMode();
  
  paint_half(buf, clear);
}

static void paint_right_half(bool clear)
{
  Arduboy2Core::LCDCommandMode();
  Arduboy2Core::SPItransfer(0x21);
  Arduboy2Core::SPItransfer(64);
  Arduboy2Core::SPItransfer(127);
  Arduboy2Core::LCDDataMode();
  
  paint_half(buf, clear);
}
