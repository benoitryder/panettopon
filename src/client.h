#ifndef CLIENT_H_
#define CLIENT_H_

#include "instance.h"
#include "netplay.h"


/// Instance for remote games.
class ClientInstance: public GameInstance,
    public netplay::ClientSocket::Observer
{
 public:
  struct Observer: GameInstance::Observer
  {
    /// Called on server notification.
    virtual void onNotification(Severity sev, const std::string &msg) = 0;
    /// Called on server disconnection.
    virtual void onServerDisconnect() = 0;
  };

  ClientInstance(Observer &obs, boost::asio::io_service &io_service);
  virtual ~ClientInstance();

  /** @brief Connect to a server.
   *
   * Timeout is given in microseconds, -1 to wait indefinitely.
   * Once the method returned the client still have to wait for server
   * configuration (or to be rejected).
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
  virtual void onServerDisconnect();
  //@}

 protected:
  GameInstance::Observer &observer() const { return observer_; }

 private:
  Observer &observer_;

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
