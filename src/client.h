#ifndef CLIENT_H_
#define CLIENT_H_

#include "instance.h"
#include "netplay.h"


/// Instance for remote games.
class ClientInstance: public GameInstance,
    public netplay::ClientSocket::Observer
{
 public:
  ClientInstance(boost::asio::io_service &io_service);
  virtual ~ClientInstance();

  /** @brief Connect to a server.
   *
   * Timeout is given in microseconds, -1 to wait indefinitely.
   */
  void connect(const char *host, int port, int tout);

  /// Close connection to the server.
  void disconnect();

#if 0
  /// Send a chat message.
  void sendChat(const std::string &txt);
  /// Tell the server we are ready.
  void sendReady();
#endif

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

  void onInputTick(const boost::system::error_code &ec);

  void stopMatch();

  netplay::ClientSocket socket_;
  KeyState next_input_;
  boost::posix_time::ptime tick_clock_;
  boost::asio::monotone_timer timer_; ///< timer for game ticks
};

#endif
