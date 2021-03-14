#pragma once

#include "vars.hpp"
#include "font.hpp"
#include "save.hpp"

// piece image data: 7 wide x 8 tall
static uint8_t const PIECE_IMGS[][7] PROGMEM =
{
  { 0x00, 0x68, 0x7c, 0x7c, 0x7c, 0x68, 0x00 }, // white pawn
  { 0x00, 0x00, 0x14, 0x00, 0x14, 0x00, 0x00 }, // checkerboard
  { 0x18, 0x1c, 0x4a, 0x7e, 0x7e, 0x7c, 0x70 }, // white knight
  { 0x20, 0x4c, 0x7e, 0x37, 0x7e, 0x4c, 0x20 }, // white bishop
  { 0x46, 0x7c, 0x7e, 0x7c, 0x7e, 0x7c, 0x46 }, // white rook
  { 0x43, 0x64, 0x7d, 0x7e, 0x7d, 0x64, 0x43 }, // white queen
  { 0x44, 0x68, 0x7a, 0x7f, 0x7a, 0x68, 0x44 }, // white king
  { 0x00, 0x68, 0x54, 0x44, 0x54, 0x68, 0x00 }, // black pawn
  { 0x18, 0x14, 0x4a, 0x72, 0x42, 0x4c, 0x70 }, // black knight
  { 0x20, 0x4c, 0x72, 0x21, 0x72, 0x4c, 0x20 }, // black bishop
  { 0x46, 0x7c, 0x46, 0x44, 0x46, 0x7c, 0x46 }, // black rook
  { 0x43, 0x64, 0x5d, 0x46, 0x5d, 0x64, 0x43 }, // black queen
  { 0x44, 0x68, 0x5a, 0x4f, 0x5a, 0x68, 0x44 }, // black king  
};

static uint8_t const SMOOTHERSTEP[32] PROGMEM =
{
  0, 0, 0, 0, 1, 2, 3, 5, 7, 10, 12, 16, 19, 22, 26, 30, 34,
  38, 42, 45, 48, 52, 54, 57, 59, 61, 62, 63, 64, 64, 64, 64
};

const uint8_t* get_piece_img(ch2k::piece pc)
{
  if(pc.color().is_white())
    return PIECE_IMGS[pc.type().x - 1];
  else
    return PIECE_IMGS[pc.type().x + 5];
}

static void draw_pixel(uint8_t y, uint8_t x)
{
  uint8_t m = 1 << (y & 7);
  buf[(uint16_t(y & 0xf8) << 3) + x] |= m;
}

static void draw_hline(uint8_t y, uint8_t x0, uint8_t x1)
{
  uint8_t m = 1 << (y & 7);
  uint8_t* p = buf + (uint16_t(y & 0xf8) << 3) + x0; 
  while(x0++ <= x1)
    *p++ |= m;
}

static void draw_vline(uint8_t y0, uint8_t y1, uint8_t x)
{
  uint8_t* p = buf + ((y0 & 0xf8) << 3) + x;
  *p |= (0xff << (y0 & 7));
  p += 64;
  y0 &= 0xf8;
  y0 += 8;
  while((y0 += 8) <= y1 + 1)
  {
    *p = 0xff;
    p += 64;
  }
  *p |= ((1 << ((y1 + 1) & 7)) - 1);
}

static void draw_piece_aligned(uint8_t const* i, uint8_t* p)
{
  *p++ = pgm_read_byte(i++);
  *p++ = pgm_read_byte(i++);
  *p++ = pgm_read_byte(i++);
  *p++ = pgm_read_byte(i++);
  *p++ = pgm_read_byte(i++);
  *p++ = pgm_read_byte(i++);
  *p   = pgm_read_byte(i);
}

static void draw_piece_unaligned(
  uint8_t const* i,
  uint8_t* p,
  uint8_t y,
  uint8_t x)
{
  if(!(y & 0x7))
    draw_piece_aligned(i, p + (uint16_t(y) << 3) + x);
  else
  {
    p += uint16_t(y & 0xf8) << 3;
    p += x;
    uint8_t f0 = y & 0x7;
    uint8_t f1 = 8-f0;
    uint8_t m0 = 0xfe << f0;
    uint8_t m1 = 0xff >> f1;
    for(uint8_t n = 0; n < 7; ++n)
    {
      uint8_t ip = pgm_read_byte(i++);
      uint8_t ip0 = ip << f0;
      uint8_t ip1 = ip >> f1;
      p[0] = (p[0] & ~m0) | ip0;
      p[64] = (p[64] & ~m1) | ip1;
      ++p;
    }
  }
}

static void render_board()
{
  // pieces and checkerboard
  uint8_t* p = buf;
  for(uint8_t r = 0; r < 8; ++r)
  {
    for(uint8_t c = 0; c < 8; ++c)
    {
      uint8_t rt = rotated ? 7 - r : r;
      uint8_t ct = rotated ? 7 - c : c;
      ch2k::piece pc = b[rt][ct];
      if(!pc.is_nothing())
        draw_piece_aligned(get_piece_img(pc), p);
      else if(!((r + c) & 1))
        draw_piece_aligned(PIECE_IMGS[1], p);
      p += 8;
    }
  }
}

// render animated move
static void render_anim()
{
  uint8_t x = nframe & 0x1f;
  uint8_t y;
  uint8_t ax = (ta & 7) * 8, bx = (tb & 7) * 8;
  uint8_t ay = (ta >> 4) * 8, by = (tb >> 4) * 8;
  if(rotated)
  {
    ax = 56 - ax, bx = 56 - bx;
    ay = 56 - ay, by = 56 - by;
  }
  x = pgm_read_byte(&SMOOTHERSTEP[x]);
  y = uint8_t((uint16_t(x) * (by - ay) + 32) >> 6) + ay;
  x = uint8_t((uint16_t(x) * (bx - ax) + 32) >> 6) + ax;
  //ch2k::piece p = g.square_to_piece(ch2k::square{tb});
  //if(g.stack().lastmove.is_promotion())
  //  p = g.c_.is_white() ? ch2k::piece::BP : ch2k::piece::WP;
  draw_piece_unaligned(
    get_piece_img(ch2k::piece{ tc }),
    buf, y, x);
}

static void render_cursor(uint8_t y, uint8_t x)
{
  // draw cursor
  int16_t n = ((int16_t)y << 3) + x;
  if(y >= 8)
  {
    if(x >= 8)
      buf[n-64-1] |= 0x80;
    for(uint8_t i = 0; i < 7; ++i)
      buf[n-64+i] |= 0x80;
    if(cx < 56)
      buf[n-64+7] |= 0x80;
  }
  if(x >= 8)
    buf[n-1] |= 0x7f;
  if(x < 56)
    buf[n+7] |= 0x7f;
  if(y < 56)
  {
    if(x >= 8)
      buf[n-1] |= 0x80;
    for(uint8_t i = 0; i < 7; ++i)
      buf[n+i] |= 0x80;
    if(x < 56)
      buf[n+7] |= 0x80;
  }
}

static void render_title_text()
{
  uint8_t const* msg = (uint8_t const*)pgm_read_word(&MSGS_TITLE[ta]);
  uint8_t w = small_text_width_prog(msg, 255);
  draw_small_text_prog(msg, 255, 20, 20 - (w / 2));
}

static void draw_pretty_box(uint8_t y1, uint8_t x1, uint8_t y2, uint8_t x2)
{
    draw_hline(y1, x1 + 1, x2 - 1);
    draw_hline(y2, x1 + 1, x2 - 1);
    draw_vline(y1 + 1, y2 - 1, x1);
    draw_vline(y1 + 1, y2 - 1, x2);
    draw_pixel(y1 + 1, x1 + 2);
    draw_pixel(y1 + 1, x2 - 2);
    draw_pixel(y2 - 1, x1 + 2);
    draw_pixel(y2 - 1, x2 - 2);
    draw_pixel(y1 + 2, x1 + 1);
    draw_pixel(y1 + 2, x2 - 1);
    draw_pixel(y2 - 2, x1 + 1);
    draw_pixel(y2 - 2, x2 - 1);
}

static void render_game_info_side(ch2k::piece_color c, uint8_t yo)
{
  constexpr uint8_t giw = 63;
  constexpr uint8_t gih = 13;

  // draw boxes
  draw_pretty_box(yo, 0, yo + gih - 1, giw - 1);

  // draw current turn indicator
  if(state != STATE_GAME_OVER && turn == c)
  {
    uint8_t y = yo + 2;
    for(uint8_t i = 3; i < giw-3; i += 2)
    {
      draw_pixel(y, i);
      draw_pixel(y+gih-5, i);
    }
    for(uint8_t i = 1; i < 8; i += 2)
    {
      draw_pixel(y+i, 2);
      draw_pixel(y+i, giw-3);
    }
  }

  // draw ai face and info
  uint8_t ci = c.binary_index();
  if(ailevel[ci] != 0)
  {
    uint8_t t = SF_BIGFROWN + aihappy[ci];
    draw_small_text(&t, 1, yo+4, 4);
    uint8_t a[3];
    a[0] = SF_L; a[1] = SF_V; a[2] = SF_1 - 1 + ailevel[ci];
    draw_small_text(a, 3, yo+4, 11);
  }
  else
  {
    draw_small_text_prog(MSG_HUMAN, 5, yo+4, 4);
  }
  if(!(turn == c && (ailevel[ci] != 0) && state == STATE_AI_START))
    draw_small_text_prog(c.is_white() ? MSG_WHITE : MSG_BLACK, 5, yo+4, 35);
}

static void render_game_info()
{
    render_game_info_side(ch2k::piece_color::W, rotated ? 0 : 50);
    render_game_info_side(ch2k::piece_color::B, rotated ? 50 : 0);
    
    uint8_t y = 14 + 6 * 5;
    uint16_t move_counter = (ply + 1) / 2;
    if(move_counter < MOVEHIST_SIZE)
        y -= (MOVEHIST_SIZE - (uint8_t)move_counter) * 6;
    else if(state == STATE_GAME_OVER)
        y -= 6;
    uint8_t t[8];
    for(uint8_t i = 0; y >= 14; ++i)
    {
        if(i >= ply) break;
        uint8_t x, x2;
        get_san_from_hist(t, g.get_rep_move(i), sanhist[i]);
        if(((i ^ ply) & 1) != 0)
        {
            x = (small_text_width(t, 7) + 0) >> 1;
            if(x > 12) x = 12;
                x = 26 - x;
            draw_small_text(t, 7, y, x);
            y -= 6;
        }
        else
        {
            x = small_text_width(t, 7);
            x2 = 52 - ((x + 1) >> 1);
            if(x2 + x > 64)
                x2 = 64 - x;
            draw_small_text(t, 7, y, x2);
        }
    }
    if(state == STATE_GAME_OVER)
    {
        if(game_status == ch2k::game::STATUS_MATED)
            draw_small_text_prog(MSG_MATE, 9, 44, 0);
        else if(game_status >= ch2k::game::STATUS_DRAW_50MOVE)
        {
            draw_small_text_prog(MSG_DRAW, 5, 44, 0);
            switch(game_status)
            {
            case ch2k::game::STATUS_DRAW_50MOVE:
                draw_small_text_prog(MSG_50M, 8, 44, 22); break;
            case ch2k::game::STATUS_DRAW_REPETITION:
                draw_small_text_prog(MSG_REP, 10, 44, 22); break;
            case ch2k::game::STATUS_DRAW_MATERIAL:
                draw_small_text_prog(MSG_MAT, 8, 44, 22); break;
            case ch2k::game::STATUS_DRAW_STALEMATE:
                draw_small_text_prog(MSG_STALE, 9, 44, 22); break;
            default: break;
            }
        }
    }
    if(ply == 0) return;
    uint8_t n[3];
    {
        n[2] = SF_0 + move_counter % 10;
        uint16_t mc2 = move_counter / 10;
        n[1] = SF_0 + mc2 % 10;
        n[0] = SF_0 + mc2 / 10;
    }
    y = 14 + 6 * 5;
    if(move_counter < MOVEHIST_SIZE)
        y -= (MOVEHIST_SIZE - (uint8_t)move_counter) * 6;
    else if(state == STATE_GAME_OVER)
        y -= 6;
    for(uint8_t i = 0; y >= 14; ++i)
    {
        t[0] = n[0], t[1] = n[1], t[2] = n[2];
        if(t[0] == SF_0)
        {
            t[0] = SF_SPACE3;
            if(t[1] == SF_0) t[1] = SF_SPACE3;
        }
        draw_small_text(t, 3, y, 0);
        if(n[2] == SF_0)
        {
            n[2] = SF_9;
            if(n[1] == SF_0)
            {
                n[1] = SF_9;
                if(n[0] == SF_0) n[0] = SF_9;
                else --n[0];
            }
            else --n[1];
        }
        else --n[2];
        y -= 6;
    }
}

static void render_ai_progress()
{
  constexpr uint8_t xoff = 25;
  constexpr uint8_t h = 3;
  uint8_t level = ailevel[turn.binary_index()]*2 + 2;
  uint8_t progress = uint8_t(g.nodes_ >> level);
  uint8_t offset = turn.is_white() ? 55 : 5;
  for(uint8_t i = 0; i < h; ++i)
    draw_hline(i + offset, xoff, xoff + progress);
  draw_hline(offset - 1, xoff, xoff + 32);
  draw_hline(offset + h, xoff, xoff + 32);
  draw_vline(offset, offset + h-1, xoff - 1);
  draw_vline(offset, offset + h-1, xoff + 32 + 1);
}

static void render_promotion_menu()
{
  uint8_t* p;
  p = buf + 64*2 + 12;
  for(uint8_t i = 0; i <= 38; ++i) *p++ &= 0x0f;
  p = buf + 64*3 + 12;
  for(uint8_t i = 0; i <= 38; ++i) *p++ = 0;
  p = buf + 64*4 + 12;
  for(uint8_t i = 0; i <= 38; ++i) *p++ &= 0xf8;
  draw_pretty_box(20, 12, 34, 50);
  uint8_t po = g.c_.is_white() ? 0 : 6;
  draw_piece_aligned(PIECE_IMGS[po + 2], buf + 64 * 3 + 16);
  draw_piece_aligned(PIECE_IMGS[po + 3], buf + 64 * 3 + 24);
  draw_piece_aligned(PIECE_IMGS[po + 4], buf + 64 * 3 + 32);
  draw_piece_aligned(PIECE_IMGS[po + 5], buf + 64 * 3 + 40);
}

static void render_new_game_menu()
{
  draw_pretty_box(0, 0, 14, 62);
  draw_small_text_prog(MSG_T_NEW, 8, 5, 15);
  draw_small_text_prog(MSG_WHITE, 5, 18, 3);
  draw_small_text_prog(MSG_BLACK, 5, 26, 3);
  draw_small_text_prog(
    (uint8_t const*)pgm_read_word(&MSGS_PLAYER[tb]), 255, 18, 34);
  draw_small_text_prog(
    (uint8_t const*)pgm_read_word(&MSGS_PLAYER[tc]), 255, 26, 34);
  uint8_t* b = &buf[ta * 64 + 64*2];
  for(uint8_t i = 33; i < 61; ++i)
    b[i] ^= 0xfe;
}

static void render_game_paused()
{
  draw_pretty_box(0, 0, 14, 62);
  if(game_status > ch2k::game::STATUS_CHECK)
    draw_small_text_prog(MSG_GAME_OVER, 9, 5, 12);
  else
    draw_small_text_prog(MSG_GAME_PAUSED, 11, 5, 9);
  static constexpr uint8_t const yo = 18;
  static constexpr uint8_t const xo = 11;
  for(uint8_t n = 0, y = yo; n < NUM_MSGS_PAUSE; ++n, y += 8)
  {
      uint8_t const* t = (uint8_t const*)pgm_read_word(&MSGS_PAUSE[n]);
      draw_small_text_prog(t, 255, y, xo);
  }
  uint8_t* b = &buf[ta * 64 + 64*2];
  for(uint8_t i = xo - 1; i < xo + 42; ++i)
    b[i] ^= 0xfe;
}

static void render_save_game_title()
{
    draw_pretty_box(0, 0, 14, 62);
    draw_small_text_prog(tc ? MSG_T_LOAD : MSG_P_SAVE, 9, 5, 12);
}

static void render_save_game()
{
    render_save_game_title();
    static constexpr uint8_t const yo = 18;
    static constexpr uint8_t const xo = 20;
    uint8_t check = SF_CHECKMARK;
    draw_small_text_prog(MSG_P_BACK, 4, yo, xo);
    for(uint8_t n = 0, y = yo + 8; n < NUM_SAVE_FILES; ++n, y += 8)
    {
        draw_small_text_prog(MSG_P_SAVE, 4, y, xo);
        uint8_t c = SF_1 + n;
        draw_small_text(&c, 1, y, xo + 18);
        if(save_is_valid(n))
            draw_small_text(&check, 1, y, xo - 6);
    }
    uint8_t* b = &buf[ta * 64 + 64*2];
    for(uint8_t i = xo - 1; i < xo + 22; ++i)
        b[i] ^= 0xfe;
}

static void render_save_game_board()
{
    // render save file board
    bool valid = ta > 0 && save_is_valid(ta - 1);
    if(valid)
    {
        update_board_cache_from_save(ta - 1);
    }
    else
    {
        for(uint8_t i = 0; i < 64; ++i)
            ((ch2k::piece*)b)[i] = ch2k::piece::NOTHING;
    }
    render_board();
}

static void render_save_game_overwrite()
{
    render_save_game_title();
    static constexpr uint8_t const yo = 18;
    static constexpr uint8_t const xo = 12;
    draw_small_text_prog(MSG_OVERWRITE_SAVE, 15, yo, 1);
    draw_small_text_prog(MSG_OVERWRITE_SAVE, 9, yo + 8, xo);
    draw_small_text_prog(MSG_CANCEL, 6, yo + 16, xo);
    uint8_t* b = &buf[tb * 64 + 64*3];
    for(uint8_t i = xo - 1; i < xo + 39; ++i)
        b[i] ^= 0xfe;
}

static void clear_buf()
{
  for(auto& i : buf) i = 0;
}

static void paint_half(uint8_t const* b, bool clear)
{
  uint16_t count;
  // this code is adapted from the Arduboy2 library
  // which is licensed under BSD-3
  // the only modification is to adjust the loop count
  // because ArduChess buffers 64x64 half-screens to save RAM
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
