#ifndef CLIENT_H_
#define CLIENT_H_

/** @file
 * @brief Client API.
 */

#include <boost/ptr_container/ptr_map.hpp>
#include "monotone_timer.hpp"
#include "netplay.pb.h"
#include "netplay.h"
#include "player.h"
#include "game.h"

class Client;
class ClientInterface;


/** @brief Match used by client.
 */
class ClientMatch: public Match
{
 public:
  ClientMatch(Client &client): client_(client) {}
  virtual ~ClientMatch() {}

  /// Process an Input packet.
  bool processInput(Field *fld, const netplay::Input &np_input);
  /// Process a Garbage packet.
  bool processGarbage(const netplay::Garbage &np_garbage);
  bool stepField(Field *fld, KeyState keys);

 private:
  Client &client_;
};


class Client: public netplay::PacketSocket
{
 protected:
  typedef boost::ptr_map<PlId, Player> PlayerContainer;

  enum State {
    STATE_NONE = 0,  ///< not started
    STATE_ROOM,
    STATE_INIT,
    STATE_READY,
    STATE_GAME,
  };

 public:

  Client(ClientInterface &intf, boost::asio::io_service &io_service);
  virtual ~Client() {}

  /** @brief Connect to a server.
   *
   * Timeout is given in microseconds, -1 to wait indefinitely.
   */
  void connect(const char *host, int port, int tout);

  /// Close connection to the server.
  void disconnect();

  /// Send a chat message.
  void sendChat(const std::string &txt);
  /// Tell the server we are ready.
  void sendReady();

  const ServerConf &conf() const { return conf_; }

  /// Return the client's player.
  Player *player() const { return player_; }

  /// Retrieve a player by ID.
  Player *player(PlId plid)
  {
    PlayerContainer::iterator it = players_.find(plid);
    return it == players_.end() ? NULL : it->second;
  }
  const Player *player(PlId plid) const
  {
    PlayerContainer::const_iterator it = players_.find(plid);
    return it == players_.end() ? NULL : it->second;
  }

 protected:
  virtual void onError(const std::string &msg, const boost::system::error_code &ec);
  virtual bool onPacketReceived(const netplay::Packet &pkt);
  void onTimeout(const boost::system::error_code &ec);
  void onConnect(const boost::system::error_code &ec);
  void onInputTick(const boost::system::error_code &ec);

 private:
  State state_;
  ClientMatch match_;
  Player *player_;
  ServerConf conf_;
  ClientInterface &intf_;
  KeyState next_input_;
  boost::asio::io_service &io_service_;
  boost::posix_time::ptime tick_clock_;
  boost::asio::monotone_timer timer_; ///< timer for timeouts
 protected:
  PlayerContainer players_;
};

#endif
