#ifndef INSTANCE_H_
#define INSTANCE_H_

#include <boost/asio/io_service.hpp>
#include <boost/ptr_container/ptr_map.hpp>
#include "monotone_timer.hpp"
#include "game.h"

// undef Windows conflicting macro
#undef SEVERITY_ERROR


/** @brief Server configuration values.
 */
struct ServerConf
{
  ServerConf() { this->toDefault(); }
  /// Initialize with default values.
  void toDefault();

  uint32_t pl_nb_max; ///< Maximum number of players.
  uint32_t tk_usec; ///< Game tick/frame/step period.

  /** @brief Size of the sliding frame window.
   *
   * Client should not sent frames until all information on the n-th previous
   * frame have been received. Invalid frames will be dropped by the server.
   *
   * Value must be lower than the \e gb_hang_tk field configuration value.
   */
  uint32_t tk_lag_max;

  /// Field configurations indexed by their name
  std::map<std::string, FieldConf> field_confs;
};

/** @brief Generic macro for server configuration fields.
 *
 * Parameters are: field name, <tt>.ini</tt> name and type suffix (for IniFile
 * \e get methods).
 */
#define SERVER_CONF_APPLY(expr) { \
  expr(pl_nb_max,    PlayerNumber ); \
  expr(tk_usec,      TickPeriod   ); \
  expr(tk_lag_max,   LagTicksLimit); \
}


/** @brief Player.
 *
 * A player is a client connected to the server.
 * It may not be associated to a field.
 */
class Player
{
 public:
  /** @brief Player states
   * @note Values match protobuf ones.
   */
  enum class State {
    NONE = 0,  ///< not started
    QUIT = 1,  ///< left os is leaving the server
    LOBBY,  ///< in the lobby
    LOBBY_READY,  ///< in the lobby, ready to play
    GAME_INIT,  ///< initializing game
    GAME_READY,  ///< game initialization complete
    GAME,  ///< in game
  };

  Player(PlId plid, bool local);
  virtual ~Player();

  PlId plid() const { return plid_; }
  bool local() const { return local_; }
  const std::string& nick() const { return nick_; }
  void setNick(const std::string& v) { nick_ = v; }
  State state() const { return state_; }
  void setState(State v) { state_ = v; }
  const FieldConf& fieldConf() const { return field_conf_; }
  const std::string& fieldConfName() const { return field_conf_name_; }
  void setFieldConf(const FieldConf& conf, const std::string& name);
  const Field* field() const { return field_; }
  Field* field() { return field_; }
  FldId fldid() const { return field_ ? field_->fldid() : 0; }
  void setField(Field* fld) { field_ = fld; }

 private:
  PlId plid_;   ///< Player ID
  bool local_;  ///< \e true for local players
  std::string nick_;
  State state_;  ///< player actual state
  FieldConf field_conf_;
  std::string field_conf_name_;
  Field* field_;
};



/// Manage a game instance.
class GameInstance
{
 public:
  enum class State {
    NONE = 0,  ///< not started
    LOBBY,
    GAME_INIT,
    GAME_READY,
    GAME,
  };

  enum Severity {
    SEVERITY_MESSAGE = 1,
    SEVERITY_NOTICE,
    SEVERITY_WARNING,
    SEVERITY_ERROR,
  };

  struct Observer
  {
    /// Called on chat message.
    virtual void onChat(Player* pl, const std::string& msg) = 0;
    /// Called on new player (even local).
    virtual void onPlayerJoined(Player* pl) = 0;
    /// Called after player's nick change.
    virtual void onPlayerChangeNick(Player* pl, const std::string& nick) = 0;
    /// Called after state update
    virtual void onPlayerStateChange(Player* pl) = 0;
    /// Called after player's field configuration change.
    virtual void onPlayerChangeFieldConf(Player* pl) = 0;
    /// Called on state update.
    virtual void onStateChange() = 0;
    /// Called after a player field step.
    virtual void onPlayerStep(Player* pl) = 0;
  };

  typedef boost::ptr_map<PlId, Player> PlayerContainer;

  GameInstance();
  virtual ~GameInstance();

  const PlayerContainer& players() const { return players_; }
  PlayerContainer& players() { return players_; }
  const Match& match() const { return match_; }
  State state() const { return state_; }
  const ServerConf& conf() const { return conf_; }

  /** @name Local player operations.
   *
   * Modify the player and send packets, if needed.
   */
  //@{
  virtual void playerSetNick(Player* pl, const std::string& nick) = 0;
  virtual void playerSetFieldConf(Player* pl, const FieldConf& conf, const std::string& name) = 0;
  virtual void playerSetState(Player* pl, Player::State state) = 0;
  virtual void playerSendChat(Player* pl, const std::string& msg) = 0;
  virtual void playerStep(Player* pl, KeyState keys) = 0;
  //@}

  /// Return the player with a given PlId or \e NULL.
  Player* player(PlId plid);
  /// Return the player associated to a given field, or \e NULL.
  Player* player(const Field* fld);

 protected:
  /// Step a player field, update match tick.
  virtual void doStepPlayer(Player* pl, KeyState keys);
  /// Like doStepPlayer() but throw netplay::CallbackError.
  void stepRemotePlayer(Player* pl, KeyState keys);

  /**@ brief Observer accessor.
   *
   * Virtual to allow subclassed observers with additional callbacks.
   */
  virtual Observer& observer() const = 0;

  PlayerContainer players_;
  Match match_;
  State state_;
  ServerConf conf_;
};


/** @brief Input tick scheduler.
 *
 * When running, send appropriate calls to GameInstance::playerStep().
 * Local players are assumed to not change when the game is running.
 */
class GameInputScheduler
{
 public:
  struct InputProvider {
    /// Return next input for a given player.
    virtual KeyState getNextInput(Player* pl) = 0;
  };

  GameInputScheduler(GameInstance& instance, InputProvider& input_, boost::asio::io_service& io_service);
  ~GameInputScheduler();

  GameInstance& instance() const { return instance_; }

  void start();
  void stop();

 private:
  GameInstance& instance_;
  InputProvider& input_;
  void onInputTick(const boost::system::error_code& ec);

  typedef std::vector<Player*> PlayerContainer;
  PlayerContainer players_;  ///< Local players still playing.
  boost::posix_time::ptime tick_clock_;
  boost::asio::monotone_timer timer_;
};


#endif
