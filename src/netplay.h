#ifndef NETPLAY_H_
#define NETPLAY_H_

/** @file
 * @brief Netplay protocol handling and sockets.
 *
 * Messages are serialized using protocol buffers and prefixed by their size
 * (16-bit, network order).
 * See netplay.proto for message structure and meaning.
 */

#include <stdint.h>
#include <string>
#include <memory>
#include <vector>
#include <queue>
#include <stdexcept>
#include <boost/asio/ip/tcp.hpp>
#include "monotone_timer.hpp"


namespace netplay {

class Packet;
class ServerEvent;
class ClientEvent;
class ClientCommand;
class ServerResponse;
class PeerSocket;
class ServerSocket;

enum class ResponseResult {
  OK = 0,
  ERROR = 1,
};

/// Callback for ClientCommand
typedef std::function<void(const ServerResponse&)> CommandCallback;

/// Exception to be raised on netplay fatal error
class CallbackError: public std::runtime_error
{
 public:
  CallbackError(const std::string& s): std::runtime_error(s) {}
};

/// Exception to be raised for a error ServerResponse
class CommandError: public std::runtime_error
{
 public:
  CommandError(const std::string& s): std::runtime_error(s) {}
};


/// Base socket for both server and clients.
class BaseSocket: public std::enable_shared_from_this<BaseSocket>
{
 protected:
  /// Maximum packet size (without size indicator)
  static const uint32_t pkt_size_max;
 public:
  BaseSocket(boost::asio::io_service& io_service);
  virtual ~BaseSocket();

  /** @brief Close the socket.
   *
   * Closing a socket already closed or being closed has no effect.
   */
  virtual void close();

  boost::asio::io_service& io_service() { return socket_.get_io_service(); }

 protected:
  boost::asio::ip::tcp::socket socket_;
};

/// Handle packet read/write operations.
class PacketSocket: public BaseSocket
{
 public:
  PacketSocket(boost::asio::io_service& io_service);
  virtual ~PacketSocket();

 protected:
  /** @brief Send and process a netplay fatal error
   *
   * On error, the next read/write operation is not prepared.
   */
  virtual void processError(const std::string& msg, const boost::system::error_code& ec) = 0;
  void processError(const std::string& msg) { boost::system::error_code ec; processError(msg, ec); }
  /// Process an incoming packet.
  virtual void processPacket(const Packet& pkt) = 0;

  static std::string serializePacket(const Packet& pkt);

 public:
  void readNext();
 protected:
  void writeNext();
  void writeRaw(const std::string& s);
  void writePacket(const Packet& pkt)
  {
    return this->writeRaw(serializePacket(pkt));
  }

  /** @brief Do pending write operations and close.
   *
   * Further read packets will be ignored.
   */
  void closeAfterWrites();

 private:
  void onReadSize(const boost::system::error_code& ec);
  void onReadData(const boost::system::error_code& ec);
  void onWrite(const boost::system::error_code& ec);

 private:
  bool delayed_close_;    ///< closeAfterWrites() has been called
  std::queue<std::string> write_queue_;
  /** @name Attributes for packet reading. */
  //@{
  uint32_t read_size_;    ///< size of the next packet
  char read_size_buf_[sizeof(read_size_)]; ///< buffer for the size of the next read packet
  char* read_buf_;        ///< buffer for the read packet
  size_t read_buf_size_;  ///< allocated size of read_buf_
  //@}
};



/// Socket for server peers.
class PeerSocket: public PacketSocket
{
  friend class ServerSocket;
 public:
  PeerSocket(ServerSocket& server);
  virtual ~PeerSocket() {}
  boost::asio::ip::tcp::endpoint& peer() { return peer_; }

  /// Close the socket and remove the peer from the server.
  virtual void close();
  /// Send a ServerEvent to the peer
  void sendServerEvent(std::unique_ptr<ServerEvent> event);
  /// Send a ServerResponse to the peer
  void sendServerResponse(std::unique_ptr<ServerResponse> response);
  /// Send an error notification and close the socket.
  void sendError(const std::string& msg) { PacketSocket::processError(msg); }

 protected:
  virtual void processError(const std::string& msg, const boost::system::error_code& ec) final;
  virtual void processPacket(const Packet& pkt) final;
 private:
  ServerSocket* server_;
  boost::asio::ip::tcp::endpoint peer_;
  bool has_error_; ///< Avoid multiple processError() calls.
};


/// Socket for server.
class ServerSocket: public std::enable_shared_from_this<ServerSocket>
{
  friend class PeerSocket;
 public:
  struct Observer
  {
    /// Called on client connection.
    virtual void onPeerConnect(PeerSocket& peer) = 0;
    /// Called after a peer disconnection.
    virtual void onPeerDisconnect(PeerSocket& peer) = 0;
    /// Called on ClientEvent packet from a peer
    virtual void onPeerClientEvent(PeerSocket& peer, const ClientEvent& event) = 0;
    /// Called on ClientCommand packet from a peer, must call peer.sendServerResponse()
    virtual void onPeerClientCommand(PeerSocket& peer, const ClientCommand& command) = 0;
  };

  ServerSocket(Observer& obs, boost::asio::io_service& io_service);
  ~ServerSocket();

  /// Start server on a given port.
  void start(int port);
  /// Return true if the server has been started.
  bool started() const { return started_; }
  /// Close the server, if not already closed.
  void close();
  boost::asio::io_service& io_service() { return acceptor_.get_io_service(); }

  /// Send a ServerEvent to all peers, excepting \e except.
  void broadcastEvent(std::unique_ptr<ServerEvent> event, const PeerSocket* except=nullptr);

 private:
  void acceptNext();
  void onAccept(const boost::system::error_code& ec);

  boost::asio::ip::tcp::acceptor acceptor_;
  bool started_;
  Observer& observer_;

  typedef std::vector<std::shared_ptr<PeerSocket>> PeerSocketContainer;
  /// Sockets of connected accepted clients.
  PeerSocketContainer peers_;
  std::shared_ptr<PeerSocket> peer_accept_; ///< currently accepted peer
};


/// Socket for client.
class ClientSocket: public PacketSocket
{
 public:
  struct Observer
  {
    /// Called on server connection or connection error
    virtual void onServerConnect(bool success) = 0;
    /// Called on server disconnection.
    virtual void onServerDisconnect() = 0;
    /// Called on ServerEvent packet from a peer
    virtual void onServerEvent(const ServerEvent& event) = 0;
  };

  ClientSocket(Observer& obs, boost::asio::io_service& io_service);
  virtual ~ClientSocket();

  /** @brief Connect to a server.
   *
   * Timeout is given in milliseconds, -1 to wait indefinitely.
   */
  void connect(const char* host, int port, int tout);
  /// Return true if the client is connected.
  bool connected() const { return connected_; }

  /// Close the socket.
  virtual void close();

  /// Send a ClientEvent to the server
  void sendClientEvent(std::unique_ptr<ClientEvent> event);
  /// Send a ClientCommand to the server
  void sendClientCommand(std::unique_ptr<ClientCommand> command, CommandCallback cb);

 protected:
  virtual void processError(const std::string& msg, const boost::system::error_code& ec) final;
  virtual void processPacket(const Packet& pkt) final;
 private:
  void onTimeout(const boost::system::error_code& ec);
  void onConnect(const boost::system::error_code& ec);

  Observer& observer_;
  std::queue<CommandCallback> command_callbacks_;  ///< callbacks for command responses
  boost::asio::monotone_timer timer_; ///< for timeouts
  bool connected_;
};


}

#endif
