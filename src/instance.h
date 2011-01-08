#ifndef INSTANCE_H_
#define INSTANCE_H_

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
  Player(PlId plid);
  virtual ~Player();

  PlId plid() const { return plid_; }
  const std::string &nick() const { return nick_; }
  void setNick(const std::string &v) { nick_ = v; }
  bool ready() const { return ready_; }
  void setReady(bool v) { ready_ = v; }
  const Field *field() const { return field_; }
  Field *field() { return field_; }
  void setField(Field *fld) { field_ = fld; }

 private:
  PlId plid_;   ///< Player ID
  std::string nick_;
  bool ready_;  ///< ready for server state change
  Field *field_;
};



/// Observer for instance events.
struct InstanceObserver
{
  virtual void onChat(const Player *pl, const std::string &msg) = 0;
  virtual void onPlayerJoined(const Player *pl) = 0;
  /// Called before player's nick change.
  virtual void onPlayerChangeNick(const Player *pl, const std::string &nick) = 0;
  virtual void onPlayerReady(const Player *pl) = 0;
  virtual void onPlayerQuit(const Player *pl) = 0;
  virtual void onMatchInit(const Match *m) = 0;
  virtual void onMatchReady(const Match *m) = 0;
  virtual void onMatchStart(const Match *m) = 0;
  virtual void onMatchEnd(const Match *m) = 0;
  virtual void onFieldStep(const Player *pl) = 0;
  virtual void onFieldLost(const Player *pl) = 0;
};


/// Manage a game instance.
class GameInstance
{
 protected:
  enum State {
    STATE_NONE = 0,  ///< not started
    STATE_LOBBY,
    STATE_INIT,
    STATE_READY,
    STATE_GAME,
  };

 public:
  GameInstance();
  virtual ~GameInstance();

 protected:
  Match match_;
  State state_;
};


#endif
