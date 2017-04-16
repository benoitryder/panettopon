#ifndef SERVER_H_
#define SERVER_H_

#include <map>
#include <memory>
#include "instance.h"
#include "netplay.h"
#include "game.h"


class IniFile;


/// Instance for hosted games.
class ServerInstance: public GameInstance,
    public netplay::ServerSocket::Observer,
    public GarbageDistributor::Observer
{
  static const std::string CONF_SECTION;

 public:
  typedef GameInstance::Observer Observer;

  ServerInstance(GameInstance::Observer& obs, boost::asio::io_service& io_service);
  virtual ~ServerInstance();

  /// Set configuration values from a config file.
  void loadConf(const IniFile& cfg);

  /// Start server on a given port.
  void startServer(int port);
  /// Stop the server.
  void stopServer();

  /// Create and return a new local player.
  Player& newLocalPlayer(const std::string& nick);

  /** @name Local player operations. */
  //@{
  virtual void playerSetNick(Player& pl, const std::string& nick);
  virtual void playerSetFieldConf(Player& pl, const FieldConf& conf);
  virtual void playerSetState(Player& pl, Player::State state);
  virtual void playerSendChat(Player& pl, const std::string& msg);
  virtual void playerStep(Player& pl, KeyState keys);
  //@}

  /** @name ServerSocket::Observer interface. */
  //@{
  virtual void onPeerConnect(netplay::PeerSocket& peer);
  virtual void onPeerDisconnect(netplay::PeerSocket& peer);
  virtual void onPeerClientEvent(netplay::PeerSocket& peer, const netplay::ClientEvent& event);
  virtual void onPeerClientCommand(netplay::PeerSocket& peer, const netplay::ClientCommand& command);
  //@}

  /** @name GarbageDistributor::Observer interface. */
  //@{
  virtual void onGarbageAdd(const Garbage& gb, unsigned int pos);
  virtual void onGarbageUpdateSize(const Garbage& gb);
  virtual void onGarbageDrop(const Garbage& gb);
  //@}

 protected:
  GameInstance::Observer& observer() const { return observer_; }

 private:
  Observer& observer_;

  /// Return next player ID to use.
  PlId nextPlayerId();

  /** @brief Initialize a new player.
   *
   * If peer is \e NULL, a local player is created.
   */
  Player& newPlayer(netplay::PeerSocket* peer, const std::string& nick);
  /** @brief Remove a player
   *
   * If player is playing, its field is aborting and match is updated.
   *
   * @note \a pl will not exist anymore after call.
   */
  void removePlayer(Player& pl);

  /** @name Packet processing.
   *
   * Methods called from onPeerPacket().
   * netplay::CallbackError are thrown on error.
   */
  //@{

  /** @brief Retrieve and check a player from an ID and peer.
   *
   * If player is not found or is not associated to the peer, it is an error.
   */
  Player& checkPeerPlayer(PlId plid, const netplay::PeerSocket& peer);

  void processPktInput(netplay::PeerSocket& peer, const netplay::PktInput& pkt);
  void processPktGarbageState(netplay::PeerSocket& peer, const netplay::PktGarbageState& pkt);
  void processPktChat(netplay::PeerSocket& peer, const netplay::PktChat& pkt);
  std::unique_ptr<netplay::PktPlayerConf> processPktPlayerJoin(netplay::PeerSocket& peer, const netplay::PktPlayerJoin& pkt);
  void processPktPlayerConf(netplay::PeerSocket& peer, const netplay::PktPlayerConf& pkt);
  void processPktPlayerState(netplay::PeerSocket& peer, const netplay::PktPlayerState& pkt);
  //@}

  /// Check if all players are ready and take actions
  void checkAllPlayersReady();

  /// Step a player field, process garbages, send Input packets.
  virtual void doStepPlayer(Player& pl, KeyState keys);

  /// Update fields ranks, stop the match if needed
  void updateRanks();

  /// Change state, reset ready flags if needed, send packets.
  void setState(State state);

  void prepareMatch();
  void startMatch();
  void stopMatch();

  typedef std::map<PlId, netplay::PeerSocket*> PeerContainer;

  std::shared_ptr<netplay::ServerSocket> socket_;
  GarbageDistributor gb_distributor_;
  PeerContainer peers_;
  PlId current_plid_;
};


#endif
