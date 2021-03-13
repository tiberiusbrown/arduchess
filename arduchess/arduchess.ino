#include <SPI.h>
#include <EEPROM.h>
#include <Arduboy2.h>
#include <Arduino.h>

#include "vars.hpp"
#include "title.hpp"
#include "font.hpp"
#include "graphics.hpp"
#include "save.hpp"

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
  uint8_t ci = turn.binary_index();
  game_status = g.check_status();
  if(game_status > ch2k::game::STATUS_CHECK)
    state = STATE_GAME_OVER;
  else if(ailevel[ci] != 0)
  {
    if(ailevel[!ci] == 0)
      rotated = turn.is_white();
    g.max_depth_ = 8;
    g.max_nodes_ = uint16_t(1) << (ailevel[ci]*2 + 7);
    g.contempt_ = aicontempt[ci];
    state = STATE_AI_START;
  }
  else
  {
    rotated = turn.is_black();
    state = STATE_HUMAN_START;
  }
}

static void update_game_state_after_move()
{
  if(ailevel[turn.binary_index()] != 0)
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

static void poll_buttons()
{
    uint8_t b = Arduboy2Core::buttonsState();
    
    uint8_t chg = 0;
    uint8_t diff = b ^ buttons;
    
    for(uint8_t n = 0, m = 1; m != 0; m <<= 1, ++n)
    {
        if(diff & m)
        {
            if(++button_debounce[n] >= DEBOUNCE_NUM)
            {
                button_debounce[n] = 0;
                chg |= m;
            }
        }
        else
            button_debounce[n] = 0;
    }
    
    buttons_prev = buttons;
    buttons ^= chg;
}

static uint8_t just_pressed(uint8_t b)
{
    return (buttons & ~buttons_prev & b) ? 1 : 0;
}

static void setup_for_game()
{
    cx = 4 * 8;
    cy = 6 * 8;
    aihappy[0] = aihappy[1] = 2;
    ply = 0;
    for(uint8_t i = 0; i < sizeof(movehist); ++i)
        *((uint8_t*)movehist + i) = SF_NULL;
    undohist_num = 0;
    rotated = false;
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
  
    init_saves();

    nframe = 0;
    state = STATE_TITLE_START;

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
    uint8_t i = 0;
    uint8_t t[7]; // TODO: set t to appropriate movehist pointer
    ch2k::piece& p = b[mv.fr().row()][mv.fr().col()];
    if(!p.type().is_pawn())
        t[i++] = pgm_read_byte(&SAN_PIECE_CHARS[p.type().x - 3]);
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
    if(p.type().is_pawn() && cap)
        need_file = true;
    if(need_file)
        t[i++] = SF_a + mv.fr().col();
    if(need_rank)
        t[i++] = SF_8 - mv.fr().row();
    if(cap)
        t[i++] = SF_CAP;
    t[i++] = SF_a + mv.to().col();
    t[i++] = SF_8 - mv.to().row();
    if(mv.is_promotion())
        t[i++] = pgm_read_byte(&SAN_PIECE_CHARS[mv.promotion_piece_type().x - 3]);
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
    ++ply;
    
    ta = mv.fr().x;
    tb = mv.to().x;
    p = ch2k::piece::NOTHING;
    
    for(uint8_t i = UNDOHIST_SIZE-1; i > 0; --i)
        undohist[i] = undohist[i - 1];
    for(uint8_t i = SANHIST_SIZE-1; i > 0; --i)
        sanhist[i] = sanhist[i - 1];
    
    {
        auto ci = g.square_to_index(mv.to());
        undohist[0].cap = ci.is_nothing() ? 0 : g.index_to_piece(ci).x;
        undohist[0].flags = g.stack_base()->flags;
        undohist[0].half_move = g.stack_base()->half_move;
        sanhist[0] = SF_NULL;
        if(!p.type().is_pawn()) sanhist[0] = t[0];
        if(cap) sanhist[0] |= SANFLAG_CAP;
        if(need_rank) sanhist[0] |= SANFLAG_RANK;
        if(need_file) sanhist[0] |= SANFLAG_FILE;
    }
    if(++undohist_num > UNDOHIST_SIZE)
        undohist_num = UNDOHIST_SIZE;
    
    g.execute_move(mv);
    
    n = g.check_status();
    if(n == ch2k::game::STATUS_MATED)
    {
        t[i++] = SF_MATE;
        sanhist[0] |= SANFLAG_MATE;
    }
    else if(n == ch2k::game::STATUS_CHECK || g.in_check)
    {
        t[i++] = SF_CHECK;
        sanhist[0] |= SANFLAG_CHECK;
    }
    if(i < 7) t[i] = SF_NULL;
    for(uint8_t j = 0; j < 7; ++j)
    {
        for(i = 0; i < MOVEHIST_SIZE * 2 - 1; ++i)
            movehist[i][j] = movehist[i + 1][j];
            movehist[i][j] = movehist[i + 1][j];
        movehist[MOVEHIST_SIZE * 2 - 1][j] = t[j];
    }
      
    nframe = 0;
    state = STATE_ANIM;
}

void undo_move()
{
    if(ply == 0 || undohist_num == 0) return;

    ch2k::move m = g.get_rep_move(0);
    ch2k::piece_index icap;
    ch2k::square b = m.to();
    if(m.is_castle())
    {
        // set captured piece to rook
        if(b.col() < 4) // queenside
            icap = g.square_to_index(b + ch2k::square_delta::EAST);
        else            // kingside
            icap = g.square_to_index(b + ch2k::square_delta::WEST);
    }
    else
    {
        // add captured piece to piece lists
        ch2k::piece pcap { undohist[0].cap };
        if(!m.is_promotion() && m.is_en_passant())
        {
            b = ch2k::square::from_rowcol(m.fr().row(), b.col());
            pcap = ch2k::piece::pawn(g.c_);
        }
        ch2k::piece_index t = g.square_to_index(b);
        icap = g.add_piece(pcap, b);
        g.index_to_square(icap) = ch2k::square::NOWHERE;
        g.square_to_index(b) = t;
    }
    g.unexecute_move(m, icap, undohist[0].flags, undohist[0].half_move);
        
    --ply;
    --undohist_num;
    for(uint8_t j = 0; j < UNDOHIST_SIZE - 1; ++j)
        undohist[j] = undohist[j + 1];
    
    for(uint8_t j = 0; j < 7; ++j)
    {
        for(uint8_t i = MOVEHIST_SIZE * 2 - 1; i > 0; --i)
        {
            movehist[i][j] = movehist[i - 1][j];
            movehist[i][j] = movehist[i - 1][j];
        }
    }
    
    if(ply >= MOVEHIST_SIZE * 2)
    {
        get_san_from_hist(
            movehist[0],
            g.get_rep_move(MOVEHIST_SIZE * 2 - 1),
            sanhist[MOVEHIST_SIZE * 2 - 1]);
    }
    for(uint8_t j = 0; j < SANHIST_SIZE - 1; ++j)
        sanhist[j] = sanhist[j + 1];

    update_board_cache();
    update_game_state();
}

void loop() {
    // if watchdog timer is enabled
    if (WDTCSR & _BV(WDE))
    {
      // disable ints and set magic key
      cli();
      *(volatile uint8_t*)0x800 = 0x77;
      *(volatile uint8_t*)0x801 = 0x77;
      for(;;);
    }
    
    poll_buttons();
    
    switch(state)
    {
    case STATE_TITLE:
        if(just_pressed(LEFT_BUTTON))
        {
            if(--ta >= NUM_MSGS_TITLE) ta = NUM_MSGS_TITLE - 1;
        }
        else if(just_pressed(RIGHT_BUTTON))
        {
            if(++ta >= NUM_MSGS_TITLE) ta = 0;
        }
        else if(just_pressed(A_BUTTON))
        {
            if(ta == 0)
            {
                g.new_game();
                //g.load_fen("8/2P5/8/8/8/8/k6K/8 w - -"); // test promotion
                update_board_cache();
                state = STATE_NEW_GAME_START;
            }
            else if(ta == 1)
            {
                ta = 0, tc = 1;
                state = STATE_SAVE_START;
            }
            //else if(ta == 2)
            //    state = STATE_HELP_START;
        }
        break;
    //case STATE_HELP:
    //    if(just_pressed(A_BUTTON) || just_pressed(B_BUTTON))
    //        state = STATE_TITLE_START;
    //    break;
    case STATE_NEW_GAME:
        if(just_pressed(LEFT_BUTTON))
        {
            uint8_t& t = ta == 0 ? tb : tc;
            if(--t >= NUM_MSGS_PLAYER) t = NUM_MSGS_PLAYER - 1;
        }
        else if(just_pressed(RIGHT_BUTTON))
        {
            uint8_t& t = ta == 0 ? tb : tc;
            if(++t >= NUM_MSGS_PLAYER) t = 0;
        }
        else if(just_pressed(UP_BUTTON))
        {
            if(--ta >= 2) ta = 1;
        }
        else if(just_pressed(DOWN_BUTTON))
        {
            if(++ta >= 2) ta = 0;
        }
        else if(just_pressed(A_BUTTON))
        {
            ailevel[0] = tb;
            ailevel[1] = tc;
            aicontempt[0] = 0;
            aicontempt[1] = 0;
            setup_for_game();
            update_game_state();
        }
        else if(just_pressed(B_BUTTON))
            state = STATE_TITLE_START;
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
        else if(just_pressed(B_BUTTON))
        {
            state = STATE_HUMAN_MOVE;
        }
        break;
    case STATE_HUMAN:
    case STATE_HUMAN_MOVE:
        if(just_pressed(B_BUTTON))
        {
            state = STATE_GAME_PAUSED_START;
            break;
        }
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
            uint8_t cyi = cy >> 3, cxi = cx >> 3;
            if(rotated)
            {
                cyi = 7 - cyi;
                cxi = 7 - cxi;
            }
            ch2k::square s = ch2k::square::from_rowcol(cyi, cxi);
            if(state == STATE_HUMAN)
            {
                if(b[cyi][cxi].color() == turn)
                {
                    state = STATE_HUMAN_MOVE;
                    ta = s.x;
                }
            }
            else
            {
                if(b[cyi][cxi].color() == turn)
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
    case STATE_GAME_OVER:
        if(just_pressed(B_BUTTON))
            state = STATE_GAME_PAUSED_START;
        break;
    case STATE_GAME_PAUSED:
        if(just_pressed(B_BUTTON))
            update_game_state();
        else if(just_pressed(UP_BUTTON))
        {
            if(--ta >= NUM_MSGS_PAUSE) ta = NUM_MSGS_PAUSE - 1;
        }
        else if(just_pressed(DOWN_BUTTON))
        {
            if(++ta >= NUM_MSGS_PAUSE) ta = 0;
        }
        else if(just_pressed(A_BUTTON))
        {
            switch(ta)
            {
                case 0: update_game_state(); break;
                case 1: undo_move(); break;
                case 2: ta = tc = 0; state = STATE_SAVE_START; break;
                case 3: state = STATE_TITLE_START; break;
                default: break;
            }
        }
        break;
    case STATE_SAVE:
        if(just_pressed(B_BUTTON))
        {
            update_board_cache();
            state = tc ? STATE_TITLE_START : STATE_GAME_PAUSED_START;
        }
        else if(just_pressed(UP_BUTTON))
        {
            if(--ta > NUM_SAVE_FILES) ta = NUM_SAVE_FILES;
            state = STATE_SAVE_START;
        }
        else if(just_pressed(DOWN_BUTTON))
        {
            if(++ta > NUM_SAVE_FILES) ta = 0;
            state = STATE_SAVE_START;
        }
        else if(just_pressed(A_BUTTON))
        {
            bool valid = save_is_valid(ta - 1);
            if(ta == 0)
            {
                update_board_cache();
                state = tc ? STATE_TITLE_START : STATE_GAME_PAUSED_START;
            }
            else if(tc)
            {
                // this is load game
                if(valid)
                {
                    setup_for_game();
                    load_game(ta - 1);
                    update_board_cache();
                    update_game_state();
                }
            }
            else if(valid)
            {
                tb = 1;
                state = STATE_SAVE_OVERWRITE;
            }
            else
            {
                save_game(ta - 1);
                update_board_cache();
                update_game_state();
            }
        }
        break;
    case STATE_SAVE_OVERWRITE:
        if(just_pressed(UP_BUTTON) || just_pressed(DOWN_BUTTON))
            tb = !tb;
        else if(just_pressed(B_BUTTON))
            state = STATE_SAVE;
        else if(just_pressed(A_BUTTON))
        {
            if(tb == 0)
            {
                save_game(ta - 1);
                update_board_cache();
                update_game_state();
            }
            else
                state = STATE_SAVE;
        }
    default: break;
    }
    
    // save battery yay
    Arduboy2Core::idle();
}

ISR(TIMER3_COMPA_vect)
{
    switch(state)
    {
    case STATE_TITLE_START:
        render_title_part(TITLE_IMG + 0);
        paint_left_half(false);
        ta = 0;
        state = STATE_TITLE;
    case STATE_TITLE:
        render_title_part(TITLE_IMG + 64);
        render_title_text();
        paint_right_half(true);
        break;
    //case STATE_HELP_START:
    //    render_help_left();
    //    paint_left_half(true);
    //    render_help_right();
    //    paint_right_half(true);
    //    state = STATE_HELP;
    //    break;
    case STATE_NEW_GAME_START:
        render_board();
        paint_left_half(true);
        ta = 0;
        tb = 0;
        tc = 2;
        state = STATE_NEW_GAME;
    case STATE_NEW_GAME:
        render_new_game_menu();
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
        {
            uint8_t r = (ta >> 4) * 8;
            uint8_t c = (ta & 7) * 8;
            if(rotated) r = 56 - r, c = 56 - c;
            render_cursor(r, c);
        }
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
    case STATE_GAME_PAUSED_START:
        render_board();
        paint_left_half(true);
        ta = 0;
        state = STATE_GAME_PAUSED;
    case STATE_GAME_PAUSED:
        render_game_paused();
        paint_right_half(true);
        break;
    case STATE_SAVE_START:
        render_save_game_board();
        paint_left_half(true);
        state = STATE_SAVE;
    case STATE_SAVE:
        render_save_game();
        paint_right_half(true);
        break;
    case STATE_SAVE_OVERWRITE:
        render_save_game_overwrite();
        paint_right_half(true);
        break;
    case STATE_GAME_OVER:
        render_game_info();
        paint_right_half(true);
        break;
    default: break;
    }
    
    ++nframe;
}
