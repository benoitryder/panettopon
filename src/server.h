#ifndef SERVER_H_
#define SERVER_H_

/** @file
 * @brief Server API.
 */

#include <boost/ptr_container/ptr_map.hpp>
#include "netplay.h"
#include "player.h"
#include "game.h"

class Config;
class Server;
class ServerInterface;


/** @brief Match used by server.
 * 
 * Manage garbage creation and match related packets.
 */
class ServerMatch: public Match
{
 public:
  ServerMatch(Server &server);
  virtual ~ServerMatch() {}

  virtual void start();
  virtual void stop();

  /// Process an Input packet.
  bool processInput(Player *pl, const netplay::Input &np_input);

  /// Process a Garbage packet.
  bool processGarbage(Player *pl, const netplay::Garbage &np_garbage);

 protected:
  /// Step one field, process garbages, send Input packets.
  bool stepField(Player *pl, KeyState keys);

  /** @brief Distribute new garbages sent by a field.
   *
   * This method uses combo/chain data of the last step.
   */
  void distributeGarbages(Player *pl);

  /// Drop the next garbage, if needed.
  void dropGarbages(Player *pl);

  /// Add and send a new garbage, update chain garbage if needed.
  void addGarbage(Field *from, Field *to, Garbage::Type type, int size);
  /// Increase chain garbage of field and send it.
  void increaseChainGarbage(Field *from);

 private:

  Server &server_;

  /// Store garbages of active chains.
  typedef std::map<Field *, Garbage *> GbChainMap;
  GbChainMap gbs_chain_;

  /// Store last garbage field for each field.
  typedef std::map<Field *, FieldContainer::iterator> GbTargetMap;
  GbTargetMap targets_chain_;
  GbTargetMap targets_combo_;

  GbId current_gbid_;
};


/// Game server
class Server: public netplay::ServerObserver
{
 protected:
  typedef boost::ptr_map<PlId, Player> PlayerContainer;

  static const std::string CONF_SECTION;

  enum State {
    STATE_NONE = 0,  ///< not started
    STATE_LOBBY,
    STATE_INIT,
    STATE_READY,
    STATE_GAME,
  };

 public:

  Server(ServerInterface &intf, boost::asio::io_service &io_service);
  ~Server();

  /// Start server on a given port.
  void start(int port);

  /// Set configuration values from a config file.
  void loadConf(const Config &cfg);

  /** @name Observer interface. */
  //@{
  virtual void onPeerConnect(netplay::PeerSocket *peer);
  virtual void onPeerDisconnect(netplay::PeerSocket *peer);
  virtual void onPeerPacket(netplay::PeerSocket *peer, const netplay::Packet &pkt);
  //@}

  const ServerConf &conf() const { return conf_; }

  /// Send a packet to all players.
  void broadcastPacket(const netplay::Packet &pkt);

 private:

  /** @brief Retrieve a player from a peer.
   * @return The Player, \e NULL if not found.
   */
  Player *peer2player(netplay::PeerSocket *peer) const;

  /// Change state, reset ready flags if needed, send packet.
  void setState(State state);

  void prepareMatch();
  void startMatch();

  /// Return next player ID to use.
  PlId nextPlayerId();

  netplay::ServerSocket socket_;
  State state_;
  ServerMatch match_;
  PlayerContainer players_;
  ServerConf conf_;
  ServerInterface &intf_;
  PlId current_plid_;
};


#endif
