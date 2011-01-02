#ifndef SERVER_H_
#define SERVER_H_

/** @file
 * @brief Server API.
 */

#include <boost/ptr_container/ptr_map.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include "netplay.h"
#include "netplay.pb.h"
#include "player.h"
#include "game.h"

class Config;
class Server;
class ServerInterface;


/// Player used by the server.
class ServerPlayer: public Player, public netplay::PacketSocket
{
 public:
  ServerPlayer(Server *server, PlId plid);
  virtual ~ServerPlayer() {}

  boost::asio::ip::tcp::endpoint &peer() { return peer_; }

 protected:
  virtual void onError(const std::string &msg, const boost::system::error_code &ec);
  virtual bool onPacketReceived(const netplay::Packet &pkt);

 private:
  Server *server_;
  boost::asio::ip::tcp::endpoint peer_;
  bool has_error_; ///< Avoid multiple onError() calls.
};


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
  bool processInput(ServerPlayer *pl, const netplay::Input &np_input);

  /// Process a Garbage packet.
  bool processGarbage(ServerPlayer *pl, const netplay::Garbage &np_garbage);

 protected:
  /// Step one field, process garbages, send Input packets.
  bool stepField(ServerPlayer *pl, KeyState keys);

  /** @brief Distribute new garbages sent by a field.
   *
   * This method uses combo/chain data of the last step.
   */
  void distributeGarbages(ServerPlayer *pl);

  /// Drop the next garbage, if needed.
  void dropGarbages(ServerPlayer *pl);

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
class Server
{
 protected:
  typedef boost::ptr_map<PlId, ServerPlayer> PlayerContainer;

  static const std::string CONF_SECTION;

  enum State {
    STATE_NONE = 0,  ///< not started
    STATE_ROOM,
    STATE_INIT,
    STATE_READY,
    STATE_GAME,
  };

 public:

  Server(ServerInterface &intf, boost::asio::io_service &io_service);
  ~Server();

  /// Start server on a given port.
  void start(int port);

  /// Return true if the server has been started.
  bool isStarted() const { return state_ != STATE_NONE; }

  /// Set configuration values from a config file.
  void loadConf(const Config &cfg);

  /** @brief Schedule a player removing.
   * 
   * The player is moved to the \e removed_players_ array and will be deleted
   * in a next step, when closed.
   * This method can be safely called from a \e ServerPlayer handler.
   */
  void removePlayerAfterWrites(ServerPlayer *pl);

  /// Send an error to the player and remove it.
  void removePlayerWithError(ServerPlayer *pl, const std::string &msg);

  /** @brief Remove a player.
   *
   * Same as removePlayerAfterWrites() then force closing.
   * write operations).
   */
  void removePlayer(ServerPlayer *pl);

  /** @brief Free the given player.
   *
   * This method is passed to io_service_.post() to make sure that the player
   * is not deleted from one of its method.
   */
  void freePlayerHandler(ServerPlayer *pl);

  /** @brief Process a packet.
   *
   * A valid packet must have at least one field set, from those a client is
   * allowed to send. Packet's \e plid (if any) must match player's.
   *
   * @return \e false if packet is invalid.
   */
  bool processPacket(ServerPlayer *pl, const netplay::Packet &pkt);

  boost::asio::io_service &io_service() { return io_service_; }
  const ServerConf &conf() const { return conf_; }

  /// Send a packet to all players.
  void broadcastPacket(const netplay::Packet &pkt);

 private:

  /** @brief Initialize the accepted player.
   *
   * Add the player to \e players_, send init packets to him and tell others
   * about him.
   * Set the read handler.
   */
  void initAcceptedPlayer();

  /// Change state, reset ready flags if needed, send packet.
  void setState(State state);

  void prepareMatch();
  void startMatch();

  void acceptNext();
  void onAccept(const boost::system::error_code &ec);

  /// Return next player ID to use.
  PlId nextPlayerId();

  State state_;
  ServerMatch match_;
  PlayerContainer players_;
  ServerConf conf_;
  /// Currently accepted player.
  ServerPlayer *accepted_player_;
  /// Array of removed player, waiting for deletion when closed.
  boost::ptr_vector<ServerPlayer> removed_players_;
  ServerInterface &intf_;
  boost::asio::io_service &io_service_;
  boost::asio::ip::tcp::acceptor acceptor_;
  PlId current_plid_;
};


#endif
