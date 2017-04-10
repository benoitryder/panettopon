#ifndef GAME_H_
#define GAME_H_

/** @file
 * @brief Game field, blocks and game mechanics.
 */

#include <stdint.h>
#include <memory>
#include <map>
#include <vector>
#include <deque>
#include <list>
#include "netplay.pb.h"
#include "util.h"

class Field;
class Match;
class IniFile;


/// Field configuration.
struct FieldConf
{
  /// Configuration name, must be unique among others
  std::string name;

  uint16_t swap_tk;      ///< swap duration
  /// Auto raise speeds (values may be 0)
  std::vector<uint16_t> raise_speeds;
  uint16_t manual_raise_speed;  ///< manual raise speed
  /// Ticks of raise speed changes (must be increasing)
  std::vector<uint16_t> raise_speed_changes;
  uint16_t raise_steps;  ///< number of raise steps per line (must be not null)
  /** @name Slope and y-intercept for computing stop frames.
   */
  //@{
  uint16_t stop_combo_0;
  uint16_t stop_combo_k;
  uint16_t stop_chain_0;
  uint16_t stop_chain_k;
  //@}
  uint16_t lost_tk;      ///< respite from death
  uint16_t gb_hang_tk;   ///< garbage hanging time
  uint16_t flash_tk;     ///< flashing duration
  uint16_t levitate_tk;  ///< levitating duration
  uint16_t pop_tk;       ///< popping period (per block)
  uint16_t pop0_tk;      ///< first pop delay
  uint16_t transform_tk; ///< transform state duration (color blocks)

  uint16_t color_nb;      ///< number of available colors

  /// Random color picking mode for raised lines.
  enum class RaiseAdjacent {
    NEVER = 1,
    ALWAYS = 2,
    ALTERNATE = 3,
  };
  /// Allow or not identical adjacent color blocks in raised lines.
  RaiseAdjacent raise_adjacent;

  /// Check field validity.
  bool isValid() const;
  /** @brief Set configuration from a packet
   * @note Configuration validity is checked.
   */
  void fromPacket(const netplay::FieldConf& pkt);
  /// Set configuration to a packet
  void toPacket(netplay::FieldConf& pkt) const;

  /** @brief Set configuration from a configuration file
   * @note Configuration validity is checked.
   */
  void fromIniFile(const IniFile& cfg, const std::string& section);
};

/** @brief Generic macro for field configuration fields.
 *
 * Parameters: field name, INI property name.
 *
 * @warning \e expr is not applied to all fields, only to "simple" ones.
 */
#define FIELD_CONF_APPLY(expr) { \
  expr(swap_tk, SwapTicks); \
  expr(manual_raise_speed, ManualRaiseSpeed); \
  expr(stop_combo_0, StopCombo0); \
  expr(stop_combo_k, StopComboStep); \
  expr(stop_chain_0, StopChain0); \
  expr(stop_chain_k, StopChainStep); \
  expr(lost_tk, LostTicks); \
  expr(gb_hang_tk, GbHangTicks); \
  expr(flash_tk, FlashTicks); \
  expr(levitate_tk, LevitateTicks); \
  expr(pop_tk, PopTicks); \
  expr(pop0_tk, Pop0Ticks); \
  expr(transform_tk, TransformTicks); \
  expr(color_nb, ColorNb); \
}


/// Garbage.
struct Garbage
{
  enum class Type {
    NONE = 0,
    COMBO,
    CHAIN,
    SPECIAL, ///< for !-blocks
  };

  GbId gbid;
  Field* from;     ///< field who created it
  Field* to;       ///< targetted field

  Type type;
  FieldPos pos;    ///< position on the field, if significant
  FieldPos size;   ///< width and height
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
  Garbage* garbage;  ///< never null
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

  /// Information on combo that matched the block
  struct ComboInfo {
    unsigned int chain;  ///< Chain value of the combo
    unsigned int pos;  ///< Block pop position in combo
    /** @brief Number of blocks of the same type in combo
     *
     * Value is used to compute the tick for the following ending state (NONE
     * for popped color blocks, TRANSFORMED for garbages).
     *
     * Since color blocks does not wait for garbage blocks to pop, they do not
     * share the same value.
     */
    unsigned int group_end;
  };
  ComboInfo combo_info;
};


/// Game field.
class Field
{
 public:
  typedef Block::ComboInfo ComboInfo;

  /// Raise progress value at which raise occurs
  static constexpr uint32_t RAISE_PROGRESS_MAX = 65536;

  /// Information on the last step.
  struct StepInfo {
    unsigned int combo = 0;  ///< Combo count (0 if no match)
    unsigned int chain = 1;  ///< Chain count (default: 1)
    bool raised = false;  ///< Field lifted up
    bool swap = false;  ///< Start a swap
    bool move = false;  ///< Cursor moved
    /// Block state changes
    struct {
      unsigned int laid = 0;  ///< Blocks that fall to the ground
      /// Chain and combo position of popped blocks and mutated garbages
      std::vector<ComboInfo> popped;
    } blocks;
  };
  typedef std::deque<std::unique_ptr<Garbage>> GarbageList;

  Field(FldId fldid, const FieldConf& conf, uint32_t seed);
  ~Field();

  FldId fldid() const { return fldid_; }
  Tick tick() const { return tick_; }
  bool lost() const { return lost_; }
  int32_t seed() const { return seed_; }
  unsigned int chain() const { return chain_; }
  const FieldPos& cursor() const { return cursor_; }
  /// Return true if swap is active.
  bool isSwapping() const { return swap_dt_ != 0; }
  const FieldPos& swapPos() const { return swap_; }
  unsigned int swapDelay() const { return swap_dt_; }
  unsigned int rank() const { return rank_; }
  uint32_t raiseProgress() const { return raise_progress_; }

  void enableSwap(bool v) { enable_swap_ = v; }
  void enableRaise(bool v) { enable_raise_ = v; }

  const StepInfo& stepInfo() const { return step_info_; }
  const FieldConf& conf() const { return conf_; }
  const Block& block(uint8_t x, uint8_t y) const {
    assert( x < FIELD_WIDTH );
    assert( y <= FIELD_HEIGHT );
    return grid_[x][y];
  }
  const Block& block(const FieldPos& pos) const { return this->block(pos.x, pos.y); }

  /// Return hanging garbage at given position.
  const Garbage& hangingGarbage(size_t pos) const { return *gbs_hang_[pos]; }
  size_t hangingGarbageCount() const { return gbs_hang_.size(); }
  const GarbageList& waitingGarbages() const { return gbs_wait_; }

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

  /// Move a hanging garbage to wait list.
  void waitGarbageDrop(const Garbage& gb);
  /** @brief Drop the next waiting garbage.
   *
   * Garbage will fall on the field as soon as possible.
   */
  void dropNextGarbage();

  /** @brief Insert a hanging garbage at a given position.
   *
   * If \e pos is -1, push at the end.
   * The garbage will be owned by the field.
   */
  void insertHangingGarbage(std::unique_ptr<Garbage> gb, unsigned int pos);

  /** @brief Remove a given hanging garbage.
   *
   * The garbage is released and returned.
   */
  std::unique_ptr<Garbage> removeHangingGarbage(const Garbage& gb);

  /** @brief Fill field with random blocks.
   *
   * \e n lines are filled with blocks. The next line is updated.
   *
   * @note The field is assumed to be empty. Upper lines are not cleared.
   */
  void fillRandom(int n);

  /// Flag as lost.
  void abort();
  void setRank(unsigned int rank)
  {
    assert( rank > 0 );
    rank_ = rank;
  }

  /// Fill a packet message with grid content.
  void setGridContentToPacket(google::protobuf::RepeatedPtrField<netplay::PktPlayerField_Block>& grid);
  /** @brief Set grid content from a packet.
   * @return \e false on invalid packet data.
   */
  bool setGridContentFromPacket(const google::protobuf::RepeatedPtrField<netplay::PktPlayerField_Block>& grid);

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

  /// Set state for all blocks of a garbage.
  void setGarbageState(const Garbage& gb, BkGarbage::State st);

  /// Handle garbage block fall.
  void fallGarbage(Garbage& gb);

  /** @brief Match garbage and its neighbors, recursively.
   *
   * If given block is not a resting garbage, do nothing.
   * 
   * Garbage size and position are updated here.
   *
   * Return the number of matched garbage blocks.
   */
  unsigned int matchGarbage(const Block& bk);

  /** @brief Transform a garbage block to a color block.
   *
   * Drawn color is different from right and below blocks (if any), except
   * every 5 blocks where it IS the color of the below block (skipping empty
   * blocks if needed).
   *
   * Invalid random colors are skipped and a new draw is made.
   *
   * Block is removed if it has only one line.
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

  /// Field index, different for each field of a match.
  FldId fldid_;
  /// Cursor position (left block).
  FieldPos cursor_;
  /// Current swap (left block), if swapping
  FieldPos swap_;
  /// Tick count before end of swap, 0 if disabled
  unsigned int swap_dt_;

  unsigned int chain_;  ///< Current chain value, or 1
  Tick tick_;           ///< Current frame (don't change after losing)
  int32_t seed_;        ///< Current random seed

  bool lost_;           ///< True if field lost
  Tick lost_dt_;        ///< Time with screen full before losing, or 0
  unsigned int rank_;   ///< Rank (1 is 1st), 0 if didn't lost yet

  bool enable_swap_;  ///< Enable swapping
  bool enable_raise_;  ///< Enable raising the field

  /** @brief Field content.
   *
   * Y-pos 1 is the bottom line, 0 is the next raising line.
   */
  Block grid_[FIELD_WIDTH][FIELD_HEIGHT+1];

  StepInfo step_info_;     ///< Last step information.
  const FieldConf& conf_;  ///< Configuration.

  /// Key state, or-ed GameKeyState values.
  KeyState key_state_;
  /// Repetition count of the key state.
  unsigned int key_repeat_;

  /// Current raising progress
  uint32_t raise_progress_;
  /// Current raising speed index
  unsigned int raise_speed_index_;
  /// True if manual raise is active
  bool manual_raise_;
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

  /// Garbages before they are dropped (first to be dropped at front).
  GarbageList gbs_hang_;
  /// Queue of garbages waiting to be dropped.
  GarbageList gbs_wait_;
  /// Queue of dropped garbages, waiting to fall.
  GarbageList gbs_drop_;
  /// Dropped garbages, on field.
  std::list<std::unique_ptr<Garbage>> gbs_field_; // could be a set
};


/** @brief Manage interactions between fields.
 *
 * A match can be either started or stopped. Initial state is being stopped.
 * Fields cannot be added or removed when started.
 */
class Match
{
 public:
  typedef std::vector<std::unique_ptr<Field>> FieldContainer;
  typedef std::map<GbId, Garbage*> GarbageMap;

  Match();
  ~Match() {}

  bool started() const { return started_; }
  const FieldContainer& fields() const { return fields_; }

  /// Start the match, init fields.
  void start();
  /// Stop the match, preserve fields and end state
  void stop();
  /// Clear match fields and other state information
  void clear();
  /// Create and return a new field.
  Field& addField(const FieldConf& conf, uint32_t seed);

  /** @brief Return the match tick.
   *
   * Match tick is the lowest tick of still playing fields. If all fields have
   * lost, it is the tick at which game stopped (max tick value).
   */
  Tick tick() const { return tick_; }
  /** @brief Update match tick.
   *
   * Should be called after stepping a field.
   */
  void updateTick();

  /** @brief Rank fields.
   *
   * Updated fields are pushed to \e ranked.
   *
   * @return \e true if all fields have been ranked (end of match),
   * \e false otherwise.
   */
  bool updateRanks(std::vector<const Field*>& ranked);

  const GarbageMap& hangingGarbages() const { return gbs_hang_; }
  const GarbageMap& waitingGarbages() const { return gbs_wait_; }

  /** @brief Add a new (hanging) garbage.
   *
   * The garbage is added to the \e to field, which must not be \e NULL.
   */
  void addGarbage(std::unique_ptr<Garbage> gb, unsigned int pos);
  /// Move a hanging garbage to wait list.
  void waitGarbageDrop(const Garbage& gb);

 protected:
  FieldContainer fields_;

  /// Map of all hanging garbages.
  GarbageMap gbs_hang_;
  /// Map of all waiting garbages
  GarbageMap gbs_wait_;

  bool started_;
  Tick tick_;
};



/** @brief Create and distribute garbages to fields.
 *
 * This class decides how new garbages are distributed among players on
 * chain/combo. It is responsible for using unique garbage IDs.
 *
 * It is intended to be used by a server.
 */
class GarbageDistributor
{
  typedef Match::FieldContainer FieldContainer;

 public:
  /// Observer for distributed garbage
  struct Observer
  {
    /// Called on new garbage.
    virtual void onGarbageAdd(const Garbage& gb, unsigned int pos) = 0;
    /// Called on garbage size change.
    virtual void onGarbageUpdateSize(const Garbage& gb) = 0;
    /// Called on garbage (scheduled) drop.
    virtual void onGarbageDrop(const Garbage& gb) = 0;
  };

  GarbageDistributor(Match& match, Observer& obs);
  ~GarbageDistributor() {}

  /// Clear state for a new match.
  void reset();

  /** @brief Update and distribute garbages after a field step.
   *
   * This method uses combo/chain data of the last step.
   * The match is updated too.
   */
  void updateGarbages(Field& fld);

 private:
  /** @brief Create, add and return a new (hanging) garbage.
   *
   * The garbage is added to the match.
   * The \e to field must not be \e NULL.
   */
  void newGarbage(Field* from, Field* to, Garbage::Type type, int size);

  /// Return next garbage ID to use.
  GbId nextGarbageId();

  Match& match_;
  Observer& observer_;

  /// Store garbages of active chains.
  typedef std::map<Field*, Garbage*> GbChainMap;
  GbChainMap gbs_chain_;

  /// Store last garbage field for each field.
  typedef std::map<Field*, FieldContainer::const_iterator> GbTargetMap;
  GbTargetMap targets_chain_;
  GbTargetMap targets_combo_;

  /// Store drop tick of hanging garbages.
  typedef std::map<const Garbage*, Tick> GbDropTickMap;
  GbDropTickMap drop_ticks_;

  GbId current_gbid_;
};


#endif
