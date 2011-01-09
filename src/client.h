#ifndef CLIENT_H_
#define CLIENT_H_

#include "instance.h"
#include "netplay.h"


/// Instance for remote games.
class ClientInstance: public GameInstance,
    public netplay::ClientSocket::Observer
{
 public:
  ClientInstance(GameInstance::Observer &obs, boost::asio::io_service &io_service);
  virtual ~ClientInstance();

  /** @brief Connect to a server.
   *
   * Timeout is given in microseconds, -1 to wait indefinitely.
   */
  void connect(const char *host, int port, int tout);

  /// Close connection to the server.
  void disconnect();

  /** @brief Create a new local player.
   *
   * The player will not be actually created until the server replies. Events
   * should be observed to known when player creation is completed.
   */
  void newLocalPlayer(const std::string &nick);

  /** @name Local player operations. */
  //@{
  virtual void playerSetNick(Player *pl, const std::string &nick);
  virtual void playerSetReady(Player *pl, bool ready);
  virtual void playerSendChat(Player *pl, const std::string &msg);
  virtual void playerStep(Player *pl, KeyState keys);
  virtual void playerQuit(Player *pl);
  //@}

  /** @name ClientSocket::Observer interface. */
  //@{
  virtual void onClientPacket(const netplay::Packet &pkt);
  //@}

 private:
  /** @name Packet processing.
   *
   * Methods called from onClientPacket().
   * netplay::Callback exceptions are thrown on error.
   */
  //@{
  void processPacketInput(const netplay::Input &pkt_input);
  void processPacketGarbage(const netplay::Garbage &pkt_gb);
  void processPacketField(const netplay::Field &pkt_fld);
  void processPacketPlayer(const netplay::Player &pkt_pl);
  void processPacketServer(const netplay::Server &pkt_server);
  //@}

  void stopMatch();

  netplay::ClientSocket socket_;
};

#endif
