#ifndef CLIENT_H_
#define CLIENT_H_

#include <memory>
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
    virtual void onNotification(Severity sev, const std::string& msg) = 0;
    /// Called on server connection or connection error
    virtual void onServerConnect(bool success) = 0;
    /// Called on server disconnection.
    virtual void onServerDisconnect() = 0;
  };

  ClientInstance(Observer& obs, boost::asio::io_service& io_service);
  virtual ~ClientInstance();

  /** @brief Connect to a server.
   *
   * Timeout is given in milliseconds, -1 to wait indefinitely.
   * Once the method returned the client still have to wait for server
   * configuration (or to be rejected).
   */
  void connect(const char* host, int port, int tout);

  /// Close connection to the server.
  void disconnect();

  typedef std::function<void(Player*, const std::string&)> NewPlayerCallback;
  /** @brief Create a new local player
   *
   * On success, the callback is called with the created player.
   * On error, the player is null and the second parameter contains the error message.
   */
  void newLocalPlayer(const std::string& nick, NewPlayerCallback cb);

  /** @name Local player operations. */
  //@{
  virtual void playerSetNick(Player& pl, const std::string& nick);
  virtual void playerSetFieldConf(Player& pl, const FieldConf& conf);
  virtual void playerSetState(Player& pl, Player::State state);
  virtual void playerSendChat(Player& pl, const std::string& msg);
  virtual void playerStep(Player& pl, KeyState keys);
  //@}

  /** @name ClientSocket::Observer interface. */
  //@{
  virtual void onServerConnect(bool success);
  virtual void onServerDisconnect();
  virtual void onServerEvent(const netplay::ServerEvent& event);
  //@}

 protected:
  GameInstance::Observer& observer() const { return observer_; }

 private:
  Observer& observer_;

  /** @name Packet processing.
   *
   * Methods called from onClientPacket().
   * netplay::CallbackError exceptions are thrown on error.
   */
  //@{
  void processPktInput(const netplay::PktInput& pkt);
  void processPktNewGarbage(const netplay::PktNewGarbage& pkt);
  void processPktUpdateGarbage(const netplay::PktUpdateGarbage& pkt);
  void processPktGarbageState(const netplay::PktGarbageState& pkt);
  void processPktChat(const netplay::PktChat& pkt);
  void processPktNotification(const netplay::PktNotification& pkt);
  void processPktServerConf(const netplay::PktServerConf& pkt);
  void processPktServerState(const netplay::PktServerState& pkt);
  void processPktPlayerConf(const netplay::PktPlayerConf& pkt);
  void processPktPlayerState(const netplay::PktPlayerState& pkt);
  void processPktPlayerRank(const netplay::PktPlayerRank& pkt);
  void processPktPlayerField(const netplay::PktPlayerField& pkt);
  //@}

  /** @brief Create and register new player from its conf
   * @note Observer method is not called.
   */
  Player& createNewPlayer(const netplay::PktPlayerConf& pkt, bool local);
  void processNewPlayerResponse(const netplay::ServerResponse& response, NewPlayerCallback cb);

  void stopMatch();

  std::shared_ptr<netplay::ClientSocket> socket_;
};

#endif
