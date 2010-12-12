#ifndef GAME_H_
#define GAME_H_

/** @file
 * @brief Game field, blocks and game mechanics.
 */

#include <stdint.h>
#include <map>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/ptr_container/ptr_list.hpp>
#include <boost/ptr_container/ptr_deque.hpp>
#include "netplay.pb.h"
#include "util.h"

class Field;
class Match;
class Player;


/// Field configuration.
struct FieldConf
{
  uint16_t swap_tk;      ///< swap duration
  uint16_t raise_tk;     ///< auto raise period, 0 if disabled
  uint16_t raise_steps;  ///< number of raise steps per line (must be not null)
  /** @name Slope and y-intercept for computing stop frames.
   */
  //@{
  uint16_t stop_combo_0;
  uint16_t stop_combo_k;
  uint16_t stop_chain_0;
  uint16_t stop_chain_k;
  //@}
  uint16_t lost_tk;      ///< respite from death (at least 1)
  uint16_t gb_wait_tk;   ///< garbage waiting time
  uint16_t flash_tk;     ///< flashing duration
  uint16_t levitate_tk;  ///< levitating duration
  uint16_t pop_tk;       ///< popping period (per block)
  uint16_t pop0_tk;      ///< first pop delay
  uint16_t transform_tk; ///< transform state duration (color blocks)

  uint8_t color_nb;      ///< number of available colors

  /// Random color picking mode for raised lines.
  enum RaiseAdjacent {
    ADJACENT_NEVER = 1,
    ADJACENT_ALWAYS = 2,
    ADJACENT_ALTERNATE = 3,
  };
  /// Allow or not identical adjacent color blocks in raised lines.
  RaiseAdjacent raise_adjacent;

  /// Check field validity.
  bool isValid() const;
};

/** @brief Generic macro for field configuration fields.
 *
 * Parameters: field name.
 *
 * @warning \e expr is not applied to the \e raise_adjacent field.
 */
#define FIELD_CONF_APPLY(expr) { \
  expr(swap_tk); \
  expr(raise_tk); \
  expr(raise_steps); \
  expr(stop_combo_0); \
  expr(stop_combo_k); \
  expr(stop_chain_0); \
  expr(stop_chain_k); \
  expr(lost_tk); \
  expr(gb_wait_tk); \
  expr(flash_tk); \
  expr(levitate_tk); \
  expr(pop_tk); \
  expr(pop0_tk); \
  expr(transform_tk); \
  expr(color_nb); \
}


/// Garbage.
struct Garbage
{
  enum Type {
    TYPE_NONE = 0,
    TYPE_COMBO,
    TYPE_CHAIN,
    TYPE_SPECIAL, ///< for !-blocks
  };

  GbId gbid;
  Field *from;     ///< field who created it
  Field *to;       ///< targetted field

  Type type;
  FieldPos pos;    ///< position on the field, if significant
  FieldPos size;   ///< width and height
  /** @brief Tick of the next state change, or 0.
   * @note Only used by the server, for dropping timing.
   */
  Tick ntick;
};


/// Color block attributes.
struct BkColor {
  enum State {
    REST = 1,    ///< default state
    FALL,        ///< falling
    LAID,        ///< on ground after fall
    LEVITATE,    ///< will fall
    FLASH,       ///< matched
    MUTATE,      ///< will pop
    CLEARED,     ///< popped, will return to \e NONE type
    TRANSFORMED, ///< transformed from garbage
  } state;
  uint8_t color;
};

/// Garbage block attributes.
struct BkGarbage {
  enum State {
    REST = 1,    ///< default state
    FALL,        ///< falling
    FLASH,       ///< matched
    MUTATE,      ///< will be transformed
    TRANSFORMED, ///< transformed from garbage to garbage
  } state;
  Garbage *garbage;
};

/// Field block.
struct Block
{
  enum Type {
    NONE    = 0, ///< no block
    COLOR,
    GARBAGE,
  };

  Block(): type(NONE), swapped(false), chaining(false), ntick(0) {}
  bool isNone() const { return type == NONE; }
  bool isColor() const { return type == COLOR; }
  bool isGarbage() const { return type == GARBAGE; }
  bool isState(BkColor::State v) const {
    return type == COLOR && bk_color.state == v;
  }
  bool isState(BkGarbage::State v) const {
    return type == GARBAGE && bk_garbage.state == v;
  }


  Type type;

  /** @brief Specific block data.
   *
   * A \e union is used because inheritance does not allow static inheritance.
   * Moreover, this avoid cast issues.
   */
  union {
    BkColor bk_color;
    BkGarbage bk_garbage;
  };

  bool swapped;  ///< being swapped (not for garbages)
  bool chaining; ///< block is part of a chain

  /// Tick of the next state change, or 0.
  Tick ntick;
  /** @brief Position in flashing group.
   *
   * For flashing blocks: block of the same group that will pop before the whole
   * group disappear.
   *
   * For garbages/processed color block: block of the same group to process
   * before the whole group starts to fall.
   */
  unsigned int group_pos;

};


/// Game field.
class Field
{
 public:
  /// Information on the last step.
  struct StepInfo {
    StepInfo();  /// Create a new instance with default fresh values.
    unsigned int combo; ///< Combo count (0 if no match).
    unsigned int chain; ///< Chain count (default: 1).
    bool raised;  ///< Field lifted up.
  };

  Field(Player *pl, uint32_t seed);
  ~Field();

  const Player *player() const { return player_; }
  Tick tick() const { return tick_; }
  bool lost() const { return lost_; }
  int32_t seed() const { return seed_; }
  unsigned int chain() const { return chain_; }
  const FieldPos &cursor() const { return cursor_; }
  /// Return true if swap is active.
  bool isSwapping() const { return swap_dt_ != 0; }
  const FieldPos &swapPos() const { return swap_; }
  unsigned int swapDelay() const { return swap_dt_; }
  unsigned int rank() const { return rank_; }
  unsigned int raiseStep() const { return raise_step_; }

  const StepInfo &stepInfo() const { return step_info_; }
  const FieldConf &conf() const { return conf_; }
  void setConf(const FieldConf &conf) { conf_ = conf; }
  const Block &block(uint8_t x, uint8_t y) const {
    assert( x < FIELD_WIDTH );
    assert( y <= FIELD_HEIGHT );
    return grid_[x][y];
  }
  const Block &block(const FieldPos &pos) const { return this->block(pos.x, pos.y); }

  /// Return waiting garbage at given position.
  const Garbage &waitingGarbage(size_t pos) const { return gbs_wait_[pos]; }
  size_t waitingGarbageCount() const { return gbs_wait_.size(); }

  /** @brief Init for match.
   *
   * This should be called after setting configuration.
   */
  void initMatch();

  /** @brief Advance of one frame.
   *
   * @todo Stop count decrease even while swapping.
   * @todo Pause raising on block fall (screen shakes).
   */
  void step(KeyState keys);

  /** @brief Drop the next waiting garbage.
   *
   * Garbage will fall on the field as soon as possible.
   */
  void dropNextGarbage();

  /** @brief Insert a waiting garbage at a given position.
   *
   * If \e pos is -1, push at the end.
   * The garbage will be owned by the field.
   */
  void insertWaitingGarbage(Garbage *gb, int pos);

  /** @brief Remove a given waiting garbage.
   *
   * The garbage is released but not destroyed.
   */
  void removeWaitingGarbage(Garbage *gb);

  /** @brief Fill field with random blocks.
   *
   * \e n lines are filled with blocks. The next line is updated.
   *
   * @note The field is assumed to be empty. Upper lines are not cleared.
   */
  void fillRandom(int n);

  /// Detach from player, flag as lost.
  void abort();
  void setRank(unsigned int rank)
  {
    assert( rank > 0 );
    rank_ = rank;
  }

  /// Fill a packet message with grid content.
  void setGridContentToPacket(google::protobuf::RepeatedPtrField<netplay::Field_Block> *grid);
  /** @brief Set grid content from a packet.
   * @return \e false on invalid packet data.
   */
  bool setGridContentFromPacket(const google::protobuf::RepeatedPtrField<netplay::Field_Block> &grid);

 private:

  /// Raise (lift up) the field of one line.
  void raise();

  /** @brief Set a random color for raising.
   *
   * Drawn color is ensured to be different from above, and 2d right block.
   *
   * This method can be used to generate new fields by looping \e y in
   * descendant order and \e x from left to right.
   *
   * @note The block state is modified.
   *
   * @todo Support all PdP random color picking.
   */
  void setRaiseColor(int x, int y=0);

  /// Set auto-raise mode.
  inline void resetAutoRaise()
  {
    raise_dt_ = conf_.raise_tk / conf_.raise_steps;
  }

  /// Set state for all blocks of a garbage.
  void setGarbageState(const Garbage *gb, BkGarbage::State st);

  /// Handle garbage block fall.
  void fallGarbage(Garbage *gb);

  /** @brief Match garbage and its neighbors, recursively.
   *
   * If given block is not a falling garbage, do nothing.
   * 
   * Garbage size and position are updated here.
   * Block is removed if it has only one line.
   */
  void matchGarbage(Block *bk, bool chained);

  /** @brief Transform a garbage block to a color block.
   *
   * Drawn color is different from right and below blocks (if any), except
   * every 5 blocks where it IS the color of the below block (skipping empty
   * blocks if needed).
   *
   * Invalid random colors are skipped and a new draw is made.
   *
   * @note The block state is modified.
   */
  void transformGarbage(int x, int y);


  /** @brief Reentrant random number generator.
   *
   * Don't rely on the libc's \e rand_r() to ensure a unique implementation among
   * all servers and clients.
   *
   * Implementation follows the exemple given by POSIX.1-2001.
   * Max generated number is 32767.
   *
   */
  uint32_t rand();

  Player *player_; // not const only set/reset field_ on it
  /// Cursor position (left block).
  FieldPos cursor_;
  /// Current swap (left block), if swapping
  FieldPos swap_;
  /// Tick count before end of swap, 0 if disabled
  unsigned int swap_dt_;

  unsigned int chain_;  ///< Current chain value, or 1
  Tick tick_;           ///< Current frame (don't change after losing)
  int32_t seed_;        ///< Current random seed

  bool lost_;           ///< True if player lost
  Tick lost_dt_;        ///< Time with screen full before losing, or 0
  unsigned int rank_;   ///< Rank (1 is 1st), 0 if didn't lost yet

  /** @brief Field content.
   *
   * Y-pos 1 is the bottom line, 0 is the next raising line.
   */
  Block grid_[FIELD_WIDTH][FIELD_HEIGHT+1];

  StepInfo step_info_;     ///< Last step information.
  FieldConf conf_;  ///< Configuration.

  /// Key state, or-ed GameKeyState values.
  KeyState key_state_;
  /// Repetition count of the key state.
  unsigned int key_repeat_;

  /// Remaining raise step before next line.
  unsigned int raise_step_;
  /// Tick before next raise, -1 for manual raise.
  int raise_dt_;
  /// Remaining stop ticks.
  unsigned int stop_dt_;
  /// Transformed block counter, for transforming garbages.
  unsigned int transformed_nb_;
  /// Number of lines which have been raised.
  unsigned int raised_lines_;

  /// Drop positions for combo garbages.
  uint8_t gb_drop_pos_[FIELD_WIDTH+1];

  typedef boost::ptr_deque<Garbage> GarbageList;
  /// Garbages before they are dropped (first to be dropped at front).
  GarbageList gbs_wait_;
  /// Queue of dropped garbages, waiting to fall.
  GarbageList gbs_drop_;
  /// Dropped garbages, on field.
  boost::ptr_list<Garbage>  gbs_field_; // could be a set
};


/** @brief Manage interactions between fields.
 *
 * A match can be either started or stopped. Initial state is being stopped.
 * Fields cannot be added or removed when started.
 */
class Match
{
 public:
  typedef boost::ptr_vector<Field> FieldContainer;

  Match(): started_(false), tick_(0) {}
  virtual ~Match() {}

  bool started() const { return started_; }
  const FieldContainer &fields() const { return fields_; }

  /// Start the match, init fields.
  virtual void start();
  /// Stop the match, remove fields.
  virtual void stop();
  /// Add a field and return it.
  Field *addField(Player *pl, uint32_t seed);
  /** @brief Make a field abort.
   *
   * The field is detached from its player.
   * However it is not destroyed not ranked.
   */
  void removeField(Field *fld);

  /** @brief Return the match tick.
   *
   * Match tick is the lowest tick of still playing fields. If all fields have
   * lost, it is the tick at which game stopped (max tick value).
   */
  Tick tick() const { return tick_; }

 protected:
  void updateTick();

  FieldContainer fields_;

  /// Map of all waiting garbages.
  typedef std::map<GbId, Garbage *> GbWaitingMap;
  GbWaitingMap gbs_wait_;

 private:
  bool started_;
  Tick tick_;

};

#endif
