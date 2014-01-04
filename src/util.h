#ifndef UTIL_H_
#define UTIL_H_

/** @file
 * @brief Common utilities.
 */

#include <stdint.h>


/// Default port for PnP servers.
#define DEFAULT_PNP_PORT   2426

/// Game tick count.
typedef uint32_t Tick;

/// Player ID (not null).
typedef uint32_t PlId;

/** @brief Field ID (not null).
 *
 * All field IDs of a match are contiguous.
 */
typedef uint32_t FldId;

/// Garbage ID (not null).
typedef uint32_t GbId;


/** @name Field size.
 *
 * Due to implementation, values are limited to 127.
 */
//@{
#define FIELD_WIDTH    6
#define FIELD_HEIGHT  12
//@}

/** @brief Position on the field.
 *
 * y=1 is the bottom line, 0 is the next raising line.
 */
struct FieldPos
{
  FieldPos(): x(-1), y(-1) {}
  FieldPos(int8_t x, int8_t y): x(x), y(y) {}
  int8_t x, y;
};


/// Input keys values.
enum GameKey {
  GAME_KEY_NONE  = 0,
  GAME_KEY_UP    = 0x1,
  GAME_KEY_DOWN  = 0x2,
  GAME_KEY_LEFT  = 0x4,
  GAME_KEY_RIGHT = 0x8,
  GAME_KEY_MOVE  = 0xf, // move mask
  GAME_KEY_SWAP  = 0x10,
  GAME_KEY_RAISE = 0x20,
};
/// Key state, a bitset of GameKey values.
typedef int KeyState;


#endif
