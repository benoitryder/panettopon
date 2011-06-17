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

  /// Maximum size of packets (without size indicator).
  uint16_t pkt_size_max;

  uint32_t pl_nb_max; ///< Maximum number of players.
  uint32_t tk_usec; ///< Game tick/frame/step period.

  /** @brief Size of the sliding frame window.
   *
   * Client should not sent frames until all information on the n-th previous
   * frame have been received. Invalid frames will be dropped by the server.
   *
   * Value must be lower than the \e gb_wait_tk field configuration value.
   */
  uint32_t tk_lag_max;
};

/** @brief Generic macro for server configuration fields.
 *
 * Parameters are: field name, <tt>.ini</tt> name and type suffix (for Config
 * \e get methods).
 */
#define SERVER_CONF_APPLY(expr) { \
  expr(pkt_size_max, PacketSizeMax, Int); \
  expr(pl_nb_max,    PlayerNumber,  Int); \
  expr(tk_usec,      TickPeriod,    Int); \
  expr(tk_lag_max,   LagTicksLimit, Int); \
}


/** @brief Player.
 *
 * A player is a client connected to the server.
 * It may not be associated to a field.
 */
class Player
{
 public:
  Player(PlId plid, bool local);
  virtual ~Player();

  PlId plid() const { return plid_; }
  bool local() const { return local_; }
  const std::string &nick() const { return nick_; }
  void setNick(const std::string &v) { nick_ = v; }
  bool ready() const { return ready_; }
  void setReady(bool v) { ready_ = v; }
  const Field *field() const { return field_; }
  Field *field() { return field_; }
  void setField(Field *fld) { field_ = fld; }

 private:
  PlId plid_;   ///< Player ID
  bool local_;  ///< \e true for local players
  std::string nick_;
  bool ready_;  ///< ready for server state change
  Field *field_;
};



/// Manage a game instance.
class GameInstance
{
 public:
  enum State {
    STATE_NONE = 0,  ///< not started
    STATE_LOBBY,
    STATE_INIT,
    STATE_READY,
    STATE_GAME,
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
    virtual void onChat(Player *pl, const std::string &msg) = 0;
    /// Called on new player (even local).
    virtual void onPlayerJoined(Player *pl) = 0;
    /// Called before player's nick change.
    virtual void onPlayerChangeNick(Player *pl, const std::string &nick) = 0;
    /// Called after player's ready state change.
    virtual void onPlayerReady(Player *pl) = 0;
    /// Called when a player quit.
    virtual void onPlayerQuit(Player *pl) = 0;
    /// Called on state update.
    virtual void onStateChange(State state) = 0;
    /// Called after a player field step.
    virtual void onPlayerStep(Player *pl) = 0;
  };

  typedef boost::ptr_map<PlId, Player> PlayerContainer;

  GameInstance();
  virtual ~GameInstance();

  const PlayerContainer &players() const { return players_; }
  PlayerContainer &players() { return players_; }
  const Match &match() const { return match_; }
  State state() const { return state_; }
  const ServerConf &conf() const { return conf_; }

  /** @name Local player operations.
   *
   * Modify the player and send packets, if needed.
   */
  //@{
  virtual void playerSetNick(Player *pl, const std::string &nick) = 0;
  virtual void playerSetReady(Player *pl, bool ready) = 0;
  virtual void playerSendChat(Player *pl, const std::string &msg) = 0;
  virtual void playerStep(Player *pl, KeyState keys) = 0;
  virtual void playerQuit(Player *pl) = 0;
  //@}

  /// Return the player with a given PlId or \e NULL.
  Player *player(PlId plid);
  /// Return the player associated to a given field, or \e NULL.
  Player *player(const Field *fld);

 protected:
  /// Step a player field, update match tick.
  virtual void doStepPlayer(Player *pl, KeyState keys);
  /// Like doStepPlayer() but throw netplay::CallbackError.
  void stepRemotePlayer(Player *pl, KeyState keys);

  /**@ brief Observer accessor.
   *
   * Virtual to allow subclassed observers with additional callbacks.
   */
  virtual Observer &observer() const = 0;

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
    virtual KeyState getNextInput(Player *pl) = 0;
  };

  GameInputScheduler(GameInstance &instance, InputProvider &input_, boost::asio::io_service &io_service);
  ~GameInputScheduler();

  GameInstance &instance() const { return instance_; }

  void start();
  void stop();

 private:
  GameInstance &instance_;
  InputProvider &input_;
  void onInputTick(const boost::system::error_code &ec);

  typedef std::vector<Player *> PlayerContainer;
  PlayerContainer players_;  ///< Local players still playing.
  boost::posix_time::ptime tick_clock_;
  boost::asio::monotone_timer timer_;
};


#endif
