#pragma once

enum
{
  SF_NULL,
  
  // small letters for moves
  SF_N, SF_B, SF_R, SF_Q, SF_K,
  SF_a, SF_b, SF_c, SF_d, SF_e, SF_f, SF_g, SF_h,
  SF_0, SF_1, SF_2, SF_3, SF_4, SF_5, SF_6, SF_7, SF_8, SF_9,
  SF_DASH, SF_CAP, SF_CHECK, SF_MATE,
  
  // ai reactions
  SF_BIGFROWN, SF_FROWN, SF_FACE, SF_SMILE, SF_BIGSMILE,

  // other letters for spelling game status
  SF_C, SF_H, SF_E, SF_M, SF_A, SF_T, SF_D, SF_W, SF_S, SF_L,
  SF_P, SF_I, SF_O, SF_V, SF_U, SF_COLON, SF_SPACE,
};

static uint8_t const SMALL_FONT[] PROGMEM =
{
  4, 0x1f, 0x02, 0x04, 0x1f, // N
  3, 0x1f, 0x15, 0x0a, // B
  3, 0x1f, 0x05, 0x1a, // R
  3, 0x0e, 0x19, 0x1e, // Q
  3, 0x1f, 0x04, 0x1b, // K
  3, 0x08, 0x14, 0x1c, // a
  3, 0x1f, 0x14, 0x08, // b
  3, 0x0c, 0x12, 0x12, // c
  3, 0x08, 0x14, 0x1f, // d
  3, 0x0c, 0x1a, 0x14, // e
  2, 0x1e, 0x05,       // f
  3, 0x12, 0x15, 0x0f, // g
  3, 0x1f, 0x04, 0x18, // h
  3, 0x0e, 0x11, 0x0e, // 0
  3, 0x12, 0x1f, 0x10, // 1
  3, 0x19, 0x15, 0x12, // 2
  3, 0x11, 0x15, 0x0a, // 3
  3, 0x07, 0x04, 0x1f, // 4
  3, 0x17, 0x15, 0x09, // 5
  3, 0x0e, 0x15, 0x08, // 6
  3, 0x01, 0x19, 0x07, // 7
  3, 0x0a, 0x15, 0x0a, // 8
  3, 0x02, 0x15, 0x0e, // 9
  2, 0x04, 0x04,       // dash
  3, 0x14, 0x08, 0x14, // cap
  3, 0x04, 0x0e, 0x04, // check
  5, 0x0a, 0x1f, 0x0a, 0x1f, 0x0a, // mate

  5, 0x19, 0x0d, 0x0c, 0x0d, 0x19, // big frown
  5, 0x11, 0x09, 0x08, 0x09, 0x11, // frown
  5, 0x01, 0x11, 0x10, 0x11, 0x09, // face
  5, 0x09, 0x11, 0x10, 0x11, 0x09, // smile
  5, 0x0d, 0x19, 0x18, 0x19, 0x0d, // big smile

  3, 0x0e, 0x11, 0x11, // C
  3, 0x1f, 0x04, 0x1f, // H
  3, 0x1f, 0x15, 0x11, // E
  5, 0x1f, 0x02, 0x04, 0x02, 0x1f, // M
  3, 0x1e, 0x05, 0x1e, // A
  3, 0x01, 0x1f, 0x01, // T
  3, 0x1f, 0x11, 0x0e, // D
  5, 0x0f, 0x10, 0x0e, 0x10, 0x0f, // W
  3, 0x12, 0x15, 0x09, // S
  3, 0x1f, 0x10, 0x10, // L
  3, 0x1f, 0x05, 0x02, // P
  3, 0x11, 0x1f, 0x11, // I
  4, 0x0e, 0x11, 0x11, 0x0e, // O
  3, 0x07, 0x18, 0x07, // V
  4, 0x0f, 0x10, 0x10, 0x0f, // U
  1, 0x0a, // colon
  2, 0x00, 0x00, // space
};

static uint8_t small_char_width(uint8_t c)
{
  uint8_t const* p = SMALL_FONT;
  uint8_t r;
  while(c-- > 0)
  {
    r = pgm_read_byte(p);
    p += r + 1;
  }
  return r;
}

static uint8_t small_text_width(uint8_t const* t, uint8_t n)
{
  uint8_t c = *t++;
  if(c == SF_NULL) return 0;
  uint8_t r = small_char_width(c);
  while(--n > 0)
  {
    c = *t++;
    if(c == SF_NULL) return r;
    r += small_char_width(c) + 1;
  }
  return r;
}

static const uint8_t* small_char_img(char c)
{
  uint8_t const* p = SMALL_FONT;
  while(--c > 0)
  {
    uint8_t r = pgm_read_byte(p);
    p += r + 1;
  }
  return p;
}

static void draw_small_text(uint8_t const* t, uint8_t n, uint8_t y, uint8_t x)
{
  uint8_t* p = buf;
  p += uint16_t(y & 0xf8) << 3;
  p += x;
  uint8_t f0 = y & 0x7;
  uint8_t f1 = 8-f0;
  uint8_t m0 = 0x1f << f0;
  uint8_t m1 = 0x1f >> f1;
  while(n-- > 0)
  {
    uint8_t c = *t++;
    if(c == SF_NULL) return;
    uint8_t const* i = small_char_img(c);
    uint8_t w = pgm_read_byte(i++);
    for(uint8_t n = 0; n < w; ++n)
    {
      uint8_t ip = pgm_read_byte(i++);
      uint8_t ip0 = ip << f0;
      uint8_t ip1 = ip >> f1;
      p[0] = (p[0] & ~m0) | ip0;
      p[64] = (p[64] & ~m1) | ip1;
      ++p;
    }
    ++p;
  }
}

static void draw_small_text_prog(uint8_t const* t, uint8_t n, uint8_t y, uint8_t x)
{
  constexpr uint8_t const N = 16;
  uint8_t b[N];
  if(n > N) n = N;
  for(uint8_t i = 0; i < n; ++i)
    b[i] = pgm_read_byte(&t[i]);
  draw_small_text(b, n, y, x);
}
