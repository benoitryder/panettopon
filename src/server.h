#ifndef SERVER_H_
#define SERVER_H_

#include <map>
#include <boost/ptr_container/ptr_map.hpp>
#include "instance.h"
#include "netplay.h"
#include "game.h"


class Config;


/// Instance for hosted games.
class ServerInstance: public GameInstance, netplay::ServerObserver
{
  static const std::string CONF_SECTION;

 public:
  ServerInstance(boost::asio::io_service &io_service);
  virtual ~ServerInstance();

  /// Set configuration values from a config file.
  void loadConf(const Config &cfg);

  /// Start server on a given port.
  void startServer(int port);

  /// Create and return a new local player.
  Player *newLocalPlayer();

  /** @name Observer interface. */
  //@{
  virtual void onPeerConnect(netplay::PeerSocket *peer);
  virtual void onPeerDisconnect(netplay::PeerSocket *peer);
  virtual void onPeerPacket(netplay::PeerSocket *peer, const netplay::Packet &pkt);
  //@}

 private:
  /// Return next player ID to use.
  PlId nextPlayerId();

  /** @brief Initialize a new player.
   *
   * If peer is \e NULL, a local player is created.
   */
  Player *newPlayer(netplay::PeerSocket *peer);
  /// Remove a player.
  void removePlayer(PlId plid);

  /** @name Packet processing.
   *
   * Methods called from onPeerPacket().
   * netplay::Callback exceptions are thrown on error.
   */
  //@{

  /** @brief Retrieve and check a player from an ID and peer.
   *
   * If player is not found or is not associated to the peer, it is an error.
   */
  Player *checkPeerPlayer(PlId plid, const netplay::PeerSocket *peer);

  void processPacketInput(netplay::PeerSocket *peer, const netplay::Input &pkt_input);
  void processPacketGarbage(netplay::PeerSocket *peer, const netplay::Garbage &pkt_gb);
  void processPacketPlayer(netplay::PeerSocket *peer, const netplay::Player &pkt_pl);

  //@}

  /// Change state, reset ready flags if needed, send packets.
  void setState(State state);

  void prepareMatch();
  void startMatch();
  void stopMatch();

  /// Step one player's field, process garbages, send Input packets.
  void stepField(Player *pl, KeyState keys);

  typedef boost::ptr_map<PlId, Player> PlayerContainer;
  typedef std::map<PlId, netplay::PeerSocket *> PeerContainer;

  netplay::ServerSocket socket_;
  PlayerContainer players_;
  PeerContainer peers_;
  ServerConf conf_;
  PlId current_plid_;
};



//TODO below: obsolete


/** @brief Match used by server.
 * 
 * Manage garbage creation and match related packets.
 */
class ServerMatch: public Match
{
 public:
  ServerMatch();
  virtual ~ServerMatch() {}

  virtual void start();
  virtual void stop();

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

  /// Store garbages of active chains.
  typedef std::map<Field *, Garbage *> GbChainMap;
  GbChainMap gbs_chain_;

  /// Store last garbage field for each field.
  typedef std::map<Field *, FieldContainer::iterator> GbTargetMap;
  GbTargetMap targets_chain_;
  GbTargetMap targets_combo_;

  GbId current_gbid_;
};


#endif
