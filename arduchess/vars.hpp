#pragma once

#define CH2K_ARDUINO
#include "ch2k/ch2k.hpp"

enum {
  STATE_TITLE_START,
  STATE_TITLE,
  STATE_NEW_GAME_START,
  STATE_NEW_GAME,
  //STATE_OPTIONS_START,
  //STATE_OPTIONS,
  STATE_HUMAN_START,
  STATE_HUMAN,           // select a piece
  STATE_HUMAN_MOVE,      // select destination
  STATE_HUMAN_PROMOTION, // choose promotion type
  STATE_AI_START,
  STATE_AI,
  STATE_ANIM,
  STATE_GAME_PAUSED_START,
  STATE_GAME_PAUSED,
  STATE_GAME_OVER,
} state;

// half-screen data
static uint8_t buf[64*64/8];

// ch2k engine object
static ch2k::game g;

static uint8_t ta, tb, tc;
static uint8_t nframe;

// algebraic notation move history
static constexpr uint8_t const MOVEHIST_SIZE = 6;
static uint8_t movehist[MOVEHIST_SIZE][2][7];
static uint8_t move_counter;

// board cache
static ch2k::piece b[8][8];
// whether board is rotated
static bool rotated;
// whose turn is it?
static ch2k::piece_color turn;
static uint8_t game_status;

// ai data
static uint8_t aihappy[2];
static uint8_t ailevel[2]; // 0: human, 1-3: ai level
static uint8_t aicontempt[2];

// cursor position
uint8_t cx, cy;

static uint8_t buttons, buttons_prev;
