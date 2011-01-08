#ifndef SERVER_H_
#define SERVER_H_

#include <map>
#include "instance.h"
#include "netplay.h"
#include "game.h"


class Config;


/// Instance for hosted games.
class ServerInstance: public GameInstance,
    public netplay::ServerSocket::Observer,
    public GarbageDistributor::Observer
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
  Player *newLocalPlayer(const std::string &nick);

  /** @name ServerSocket::Observer interface. */
  //@{
  virtual void onPeerConnect(netplay::PeerSocket *peer);
  virtual void onPeerDisconnect(netplay::PeerSocket *peer);
  virtual void onPeerPacket(netplay::PeerSocket *peer, const netplay::Packet &pkt);
  //@}

  /** @name GarbageDistributor::Observer interface. */
  //@{
  virtual void onGarbageAdd(const Garbage *gb, unsigned int pos);
  virtual void onGarbageUpdateSize(const Garbage *gb);
  virtual void onGarbageDrop(const Garbage *gb);
  //@}

 private:
  /// Return next player ID to use.
  PlId nextPlayerId();

  /** @brief Initialize a new player.
   *
   * If peer is \e NULL, a local player is created.
   */
  Player *newPlayer(netplay::PeerSocket *peer, const std::string &nick);
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

  /// Step a remote player field, process garbages, send Input packets.
  void stepRemoteField(Player *pl, KeyState keys);

  //@}

  /// Change state, reset ready flags if needed, send packets.
  void setState(State state);

  void prepareMatch();
  void startMatch();
  void stopMatch();

  typedef std::map<PlId, netplay::PeerSocket *> PeerContainer;

  netplay::ServerSocket socket_;
  GarbageDistributor gb_distributor_;
  PeerContainer peers_;
  PlId current_plid_;
};


#endif
