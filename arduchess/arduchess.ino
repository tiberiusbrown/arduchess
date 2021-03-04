#include <SPI.h>
#include <EEPROM.h>
#include <Arduboy2.h>
#include <Arduino.h>

#define CH2K_ARDUINO
#include "ch2k/ch2k.hpp"

// half-screen data
static uint8_t buf[64*64/8];

#include "title.hpp"
#include "small_font.hpp"
#include "paint.hpp"

static ch2k::game g;

enum {
  STATE_TITLE,
  STATE_HUMAN_START,
  STATE_HUMAN,           // select a piece
  STATE_HUMAN_MOVE,      // select destination
  STATE_HUMAN_PROMOTION, // choose promotion type
  STATE_AI_START,
  STATE_AI,
  STATE_ANIM,
  STATE_GAME_OVER,
} state;

static uint8_t ta, tb, tc;
static uint8_t nframe;

// algebraic notation move history
static constexpr uint8_t const MOVEHIST_SIZE = 6;
static uint8_t movehist[MOVEHIST_SIZE][2][7];
static uint8_t move_counter;

// board cache
static ch2k::piece b[8][8];
// whose turn is it?
static ch2k::piece_color turn;
static uint8_t game_status;
static void update_game_state_after_move();

// bitmask of whether each player is ai
static uint8_t ai;

// ai data
static uint8_t aihappy[2];
static uint8_t ailevel[2];

static uint8_t const MSG_MATE[] PROGMEM =
{ SF_C, SF_H, SF_E, SF_C, SF_K, SF_M, SF_A, SF_T, SF_E };
static uint8_t const MSG_DRAW[] PROGMEM =
{ SF_D, SF_R, SF_A, SF_W, SF_COLON };
static uint8_t const MSG_50M[] PROGMEM =
{ SF_5, SF_0, SF_SPACE, SF_M, SF_O, SF_V, SF_E, SF_S };
static uint8_t const MSG_REP[] PROGMEM =
{ SF_R, SF_E, SF_P, SF_E, SF_T, SF_I, SF_T, SF_I, SF_O, SF_N };
static uint8_t const MSG_STALE[] PROGMEM =
{ SF_S, SF_T, SF_A, SF_L, SF_E, SF_M, SF_A, SF_T, SF_E };
static uint8_t const MSG_MAT[] PROGMEM =
{ SF_M, SF_A, SF_T, SF_E, SF_R, SF_I, SF_A, SF_L };
static uint8_t const MSG_DEPTH[] PROGMEM =
{ SF_D, SF_E, SF_P, SF_T, SF_H, SF_COLON };
static uint8_t const MSG_HUMAN[] PROGMEM =
{ SF_H, SF_U, SF_M, SF_A, SF_N };
static uint8_t const MSG_WHITE[] PROGMEM =
{ SF_W, SF_H, SF_I, SF_T, SF_E };
static uint8_t const MSG_BLACK[] PROGMEM =
{ SF_B, SF_L, SF_A, SF_C, SF_K };

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

static void update_board_cache()
{
  for(uint8_t r = 0; r < 8; ++r)
    for(uint8_t c = 0; c < 8; ++c)
    {
      ch2k::piece_index i = g.square_to_index(
        ch2k::square::from_rowcol(r, c));
      b[r][c] = i.is_nothing() ? ch2k::piece::NOTHING : g.index_to_piece(i);
    }
}

static void update_game_state()
{
  turn = g.c_;
  game_status = g.check_status();
  if(game_status > ch2k::game::STATUS_CHECK)
    state = STATE_GAME_OVER;
  else if(turn.x & ai)
  {
    g.max_depth_ = 8;
    g.max_nodes_ = uint16_t(1) << (ailevel[turn.binary_index()]*2 + 9);
    state = STATE_AI_START;
  }
  else
    state = STATE_HUMAN_START;
}

static void update_game_state_after_move()
{
  if(turn.x & ai)
  {
    u8 happy = 2;
    if(g.score_ < -850) happy = 0;
    else if(g.score_ < -250) happy = 1;
    else if(g.score_ > +850) happy = 4;
    else if(g.score_ > +250) happy = 3;
    aihappy[turn.binary_index()] = happy;
  }
  update_game_state();
}

// cursor position
uint8_t cx, cy;

static uint8_t buttons, buttons_prev;
static void poll_buttons()
{
  buttons_prev = buttons;
  buttons = Arduboy2Core::buttonsState();
}

static uint8_t just_pressed(uint8_t b)
{
  return (buttons & ~buttons_prev & b) ? 1 : 0;
}

void setup() {
  Arduboy2Base::boot();
  poll_buttons();
  if(buttons & UP_BUTTON)
  {
    Arduboy2Core::sendLCDCommand(OLED_ALL_PIXELS_ON);
    Arduboy2Core::digitalWriteRGB(RGB_ON, RGB_ON, RGB_ON);
    Arduboy2Core::digitalWriteRGB(RED_LED, RGB_ON);
    for(;;);
  }

  //g.load_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -");
  g.new_game();

  nframe = 0;
  cx = 4 * 8;
  cy = 6 * 8;
  state = STATE_TITLE;
  ai = ch2k::piece_color::B_;
  aihappy[0] = aihappy[1] = 2;
  ailevel[0] = ailevel[1] = 1;

  move_counter = 0;
  for(uint8_t i = 0; i < MOVEHIST_SIZE; ++i)
    movehist[i][0][0] = movehist[i][1][0] = SF_NULL;

  update_board_cache();

  TCCR3A = 0;
  TCCR3B = _BV(WGM32) | _BV(CS32); // CTC mode, prescaler /256
  OCR3A = 1000; // ~62 Hz
  bitWrite(TIMSK3, OCIE3A, 1);
}

ch2k::move valid_move(uint8_t a, uint8_t b)
{
  uint8_t n = g.gen_moves(&g.mvs_[0]);
  while(n-- > 0)
  {
    if(g.mvs_[n].fr().x == a && g.mvs_[n].to().x == b)
      return g.mvs_[n];
  }
  return ch2k::move::NO_MOVE;
}

static void send_move(ch2k::move mv)
{
  static uint8_t const PIECE_CHARS[] PROGMEM =
  {
    SF_N, SF_B, SF_R, SF_Q, SF_K
  };
  uint8_t i = 0;
  uint8_t t[7];
  ch2k::piece& p = b[mv.fr().row()][mv.fr().col()];
  if(!p.type().is_pawn())
    t[i++] = pgm_read_byte(&PIECE_CHARS[p.type().x - 3]);
  uint8_t n = g.gen_moves(&g.mvs_[0]);
  bool need_rank = false, need_file = false;
  for(uint8_t j = 0; j < n; ++j)
  {
    ch2k::move m = g.mvs_[j];
    if(m.to() != mv.to()) continue;
    if(m.fr() == mv.fr()) continue;
    if(g.square_to_piece(m.fr()) != p) continue;
    if(m.fr().row() == mv.fr().row()) need_file = true;
    if(m.fr().col() == mv.fr().col()) need_rank = true;
  }
  bool cap = !g.square_to_index(mv.to()).is_nothing() ||
    (!mv.is_promotion() && mv.is_en_passant());
  if(need_file || (p.type().is_pawn() && cap))
    t[i++] = SF_a + mv.fr().col();
  if(need_rank)
    t[i++] = SF_8 - mv.fr().row();
  if(cap)
    t[i++] = SF_CAP;
  t[i++] = SF_a + mv.to().col();
  t[i++] = SF_8 - mv.to().row();
  if(mv.is_promotion())
    t[i++] = pgm_read_byte(&PIECE_CHARS[mv.promotion_piece_type().x - 3]);
  if(mv.is_castle())
  {
    i = 0;
    t[i++] = SF_0;
    t[i++] = SF_DASH;
    t[i++] = SF_0;
    if(mv.to().col() < mv.fr().col())
    {
      t[i++] = SF_DASH;
      t[i++] = SF_0;
    }
  }
  if(g.c_.is_white())
    ++move_counter;

  ta = mv.fr().x;
  tb = mv.to().x;
  p = ch2k::piece::NOTHING;
  g.execute_move(mv);
  nframe = 0;
  state = STATE_ANIM;

  n = g.check_status();
  if(n == ch2k::game::STATUS_MATED)
    t[i++] = SF_MATE;
  else if(n == ch2k::game::STATUS_CHECK || g.in_check)
    t[i++] = SF_CHECK;
  if(i < 7) t[i] = SF_NULL;
  for(uint8_t j = 0; j < 7; ++j)
  {
    if(g.c_.is_black())
    {
      for(i = 0; i < MOVEHIST_SIZE - 1; ++i)
      {
        movehist[i][0][j] = movehist[i + 1][0][j];
        movehist[i][1][j] = movehist[i + 1][1][j];
      }
    }
    movehist[MOVEHIST_SIZE - 1][g.c_.is_white() ? 1 : 0][j] = t[j];
  }
  if(g.c_.is_black())
    movehist[MOVEHIST_SIZE - 1][1][0] = SF_NULL;
}

void loop() {
  // if watchdog timer is enabled
  if (WDTCSR & _BV(WDE))
  {
    // disable ints and set magic key
    cli();
    *(volatile uint16_t *)0x800 = 0x7777;
    for(;;);
  }
  
  poll_buttons();

  switch(state)
  {
  case STATE_TITLE:
    if(just_pressed(A_BUTTON))
      update_game_state();
    break;
  case STATE_HUMAN_PROMOTION:
    if(just_pressed(LEFT_BUTTON))
      nframe = 0, --tc;
    if(just_pressed(RIGHT_BUTTON))
      nframe = 0, ++tc;
    tc &= 3;
    if(just_pressed(A_BUTTON))
    {
      tc += 3;
      ch2k::move mv;
      mv.clear_set_fr(ch2k::square{ta});
      mv.clear_set_to(ch2k::square{tb});
      mv.set_promotion();
      mv.set_promotion_piece_type(ch2k::piece_type{tc});
      send_move(mv);
    }
    break;
  case STATE_HUMAN:
  case STATE_HUMAN_MOVE:
    if(just_pressed(UP_BUTTON))
      nframe = 0, cy -= 8;
    if(just_pressed(DOWN_BUTTON))
      nframe = 0, cy += 8;
    if(just_pressed(LEFT_BUTTON))
      nframe = 0, cx -= 8;
    if(just_pressed(RIGHT_BUTTON))
      nframe = 0, cx += 8;
    cy &= 56;
    cx &= 56;
    if(just_pressed(A_BUTTON))
    {
      if(state == STATE_HUMAN)
      {
        ch2k::square s = ch2k::square::from_rowcol(cy >> 3, cx >> 3);
        if(g.square_to_index(s).is_of_color(g.c_))
        {
          state = STATE_HUMAN_MOVE;
          ta = s.x;
        }
      }
      else
      {
        ch2k::square s = ch2k::square::from_rowcol(cy >> 3, cx >> 3);
        if(g.square_to_index(s).is_of_color(g.c_))
        {
          ta = s.x;
        }
        else
        {
          tb = s.x;
          ch2k::move mv = valid_move(ta, tb);
          if(mv.is_promotion())
            tc = 3, state = STATE_HUMAN_PROMOTION;
          else if(mv != ch2k::move::NO_MOVE)
            send_move(mv);
        }
      }
    }
    break;
  case STATE_AI:
    g.iterative_deepening();
    if(g.best_ != ch2k::move::NO_MOVE)
      send_move(g.best_);
    else
      state = STATE_GAME_OVER;
    break;
  default: break;
  }

  // save battery yay
  Arduboy2Core::idle();
}

const uint8_t* get_piece_img(ch2k::piece pc)
{
  if(pc.color().is_white())
    return PIECE_IMGS[pc.type().x - 1];
  else
    return PIECE_IMGS[pc.type().x + 5];
}

static void render_board()
{
  // pieces and checkerboard
  uint8_t* p = buf;
  for(uint8_t r = 0; r < 8; ++r)
  {
    for(uint8_t c = 0; c < 8; ++c)
    {
      ch2k::piece pc = b[r][c];
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
  x = pgm_read_byte(&SMOOTHERSTEP[x]);
  y = uint8_t((uint16_t(x) * (by - ay) + 32) >> 6) + ay;
  x = uint8_t((uint16_t(x) * (bx - ax) + 32) >> 6) + ax;
  ch2k::piece p = g.square_to_piece(ch2k::square{tb});
  if(g.stack().lastmove.is_promotion())
    p = g.c_.is_white() ? ch2k::piece::BP : ch2k::piece::WP;
  draw_piece_unaligned(
    get_piece_img(p),
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
  /*
  uint8_t t[16];
  t[0] = SF_N;
  t[1] = SF_b;
  t[2] = SF_CAP;
  t[3] = SF_c;
  t[4] = SF_5;
  t[5] = SF_MATE;
  t[6] = SF_NULL;
  draw_small_text(t, 15, 80);
  */
}

static void render_game_info_side(ch2k::piece_color c, uint8_t yo)
{
  constexpr uint8_t giw = 63;
  constexpr uint8_t gih = 13;

  // draw boxes
  draw_hline(yo+0, 1, giw-2);
  draw_hline(yo+gih-1, 1, giw-2);
  draw_vline(yo+1, yo+gih-2, 0);
  draw_vline(yo+1, yo+gih-2, giw-1);
  draw_pixel(yo+1, 2);
  draw_pixel(yo+1, giw-3);
  draw_pixel(yo+2, 1);
  draw_pixel(yo+2, giw-2);
  draw_pixel(yo+gih-3, 1);
  draw_pixel(yo+gih-3, giw-2);
  draw_pixel(yo+gih-2, 2);
  draw_pixel(yo+gih-2, giw-3);

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
  if(ai & c.x)
  {
    uint8_t ci = c.binary_index();
    uint8_t t = SF_BIGFROWN + aihappy[ci];
    draw_small_text(&t, 1, yo+4, 4);
    uint8_t a[3];
    a[0] = SF_L; a[1] = SF_V; a[2] = SF_1 + ailevel[ci];
    draw_small_text(a, 3, yo+4, 11);
  }
  else
  {
    draw_small_text_prog(MSG_HUMAN, 5, yo+4, 4);
  }
  if(!((ai & c.x) && state == STATE_AI_START))
    draw_small_text_prog(c.is_white() ? MSG_WHITE : MSG_BLACK, 5, yo+4, 35);
}

static void render_game_info()
{
  render_game_info_side(ch2k::piece_color::W, 50);
  render_game_info_side(ch2k::piece_color::B, 0);

  uint8_t y = 14 + 6 * 5;
  if(move_counter < MOVEHIST_SIZE)
    y -= (MOVEHIST_SIZE - move_counter) * 6;
  else if(state == STATE_GAME_OVER)
    y -= 6;
  for(uint8_t i = MOVEHIST_SIZE - 1; y >= 14; --i)
  {
    uint8_t x, x2;
    x = (small_text_width(movehist[i][0], 7) + 0) >> 1;
    if(x > 14) x = 14;
    x = 24 - x;
    draw_small_text(movehist[i][0], 7, y, x);
    x = small_text_width(movehist[i][1], 7);
    x2 = 50 - ((x + 1) >> 1);
    if(x2 + x > 64)
      x2 = 64 - x;
    draw_small_text(movehist[i][1], 7, y, x2);
    y -= 6;
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
  uint8_t n[2];
  if(move_counter == 0) return;
  n[1] = SF_0 + move_counter % 10;
  n[0] = SF_0 + move_counter / 10;
  y = 14 + 6 * 5;
  if(move_counter < MOVEHIST_SIZE)
    y -= (MOVEHIST_SIZE - move_counter) * 6;
  else if(state == STATE_GAME_OVER)
    y -= 6;
  for(uint8_t i = 0; y >= 14; ++i)
  {
    if(n[0] == SF_0)
    {
      if(n[1] == SF_0) break;
      draw_small_text(n+1, 1, y, 4);
    }
    else
      draw_small_text(n, 2, y, 0);
    if(n[1] == SF_0)
    {
      n[1] = SF_9;
      if(n[0] == SF_0) n[0] = SF_9;
      else --n[0];
    }
    else --n[1];
    y -= 6;
  }
}

static void render_ai_progress()
{
  constexpr uint8_t xoff = 25;
  constexpr uint8_t h = 3;
  uint8_t level = ailevel[turn.binary_index()]*2 + 4;
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
  draw_hline(20, 13, 13 + 36);
  draw_hline(34, 13, 13 + 36);
  draw_vline(21, 33, 12);
  draw_vline(21, 33, 12 + 38);
  uint8_t po = g.c_.is_white() ? 0 : 6;
  draw_piece_aligned(PIECE_IMGS[po + 2], buf + 64 * 3 + 16);
  draw_piece_aligned(PIECE_IMGS[po + 3], buf + 64 * 3 + 24);
  draw_piece_aligned(PIECE_IMGS[po + 4], buf + 64 * 3 + 32);
  draw_piece_aligned(PIECE_IMGS[po + 5], buf + 64 * 3 + 40);
}

static void clear_buf()
{
  for(auto& i : buf) i = 0;
}

ISR(TIMER3_COMPA_vect)
{
  switch(state)
  {
  case STATE_TITLE:
    render_title_part(TITLE_IMG + 0);
    paint_left_half(false);
    render_title_part(TITLE_IMG + 64);
    render_title_text();
    paint_right_half(true);
    break;
  case STATE_AI_START:
    render_board();
    paint_left_half(true);
    render_game_info();
    paint_right_half(false);
    state = STATE_AI;
    break;
  case STATE_AI:
    render_ai_progress();
    paint_right_half(false);
    break;
  case STATE_HUMAN_START:
    render_game_info();
    paint_right_half(true);
    state = STATE_HUMAN;
    // fallthrough
  case STATE_HUMAN:
  case STATE_HUMAN_MOVE:
    render_board();
    if(state == STATE_HUMAN_MOVE)
      render_cursor((ta >> 4) * 8, (ta & 7) * 8);
    if(!(nframe & 0x08))
      render_cursor(cy, cx);
    paint_left_half(true);
    break;
  case STATE_HUMAN_PROMOTION:
    render_board();
    render_promotion_menu();
    if(!(nframe & 0x08))
      render_cursor(24, tc * 8 + 16);
    paint_left_half(true);
    break;
  case STATE_ANIM:
    if(nframe == 0)
    {
      clear_buf();
      render_game_info();
      paint_right_half(true);
    }
    if(nframe == 31)
      update_board_cache();
    render_board();
    if(nframe < 31)
      render_anim();
    paint_left_half(true);
    if(nframe == 31)
      update_game_state_after_move();
    break;
  case STATE_GAME_OVER:
    render_game_info();
    paint_right_half(true);
    break;
  default: break;
  }
  
  ++nframe;
}
