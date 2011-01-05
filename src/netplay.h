#ifndef NETPLAY_H_
#define NETPLAY_H_

/** @file
 * @brief Netplay protocol handling.
 *
 * Messages are serialized using protocol buffers and prefixed by their size
 * (16-bit, network order).
 * See netplay.proto for message structure and meaning.
 */

#include <stdint.h>
#include <string>
#include <queue>
#include <stdexcept>
#include <boost/asio/ip/tcp.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include "monotone_timer.hpp"


namespace netplay {

class Packet;
class PeerSocket;
class ServerSocket;


/// Exception to be raised by callbacks.
class CallbackError: public std::runtime_error {};


/// Handle packet read/write operations.
class PacketSocket
{
  friend class PeerSocket;
  friend class ServerSocket;
 public:
  PacketSocket(boost::asio::io_service &io_service);
  virtual ~PacketSocket();

 protected:
  /** @brief Send and process an error.
   *
   * On error, the next read/write operation is not prepared.
   */
  virtual void processError(const std::string &msg, const boost::system::error_code &ec) = 0;
  void processError(const std::string &msg) { boost::system::error_code ec; processError(msg, ec); }
  /// Process an incoming packet.
  virtual void processPacket(const Packet &pkt) = 0;

 public:
  static std::string serializePacket(const Packet &pkt);

  void readNext();
  void writeNext();
  void writePacket(const Packet &pkt)
  {
    return this->writeRaw(serializePacket(pkt));
  }
  void writeRaw(const std::string &s);

  /// Close the socket.
  virtual void close();

  /** @brief Do pending write operations and close.
   *
   * Further read packets will be ignored.
   */
  void closeAfterWrites();

 private:
  void onReadSize(const boost::system::error_code &ec);
  void onReadData(const boost::system::error_code &ec);
  void onWrite(const boost::system::error_code &ec);

 protected:
  boost::asio::ip::tcp::socket socket_;
  uint16_t pkt_size_max_;

  boost::asio::io_service &io_service() { return socket_.get_io_service(); }

 private:
  bool delayed_close_;    ///< closeAfterWrites() has been called
  std::queue<std::string> write_queue_;
  /** @name Attributes for packet reading. */
  //@{
  char read_size_buf_[2]; ///< buffer for the size of the next read packet
  uint16_t read_size_;    ///< size of the next packet
  char *read_buf_;        ///< buffer for the read packet
  size_t read_buf_size_;  ///< allocated size of read_buf_
  //@}
};



/// Observer for server events.
struct ServerObserver
{
  /// Method called on client connection.
  virtual void onPeerConnect(PeerSocket *peer) = 0;
  /// Method called after a peer disconnection.
  virtual void onPeerDisconnect(PeerSocket *peer) = 0;
  /// Method called on input packet on a peer.
  virtual void onPeerPacket(PeerSocket *peer, const netplay::Packet &pkt);
};


/// Socket for server peers.
class PeerSocket: public PacketSocket
{
 public:
  PeerSocket(ServerSocket &server);
  virtual ~PeerSocket() {}
  boost::asio::ip::tcp::endpoint &peer() { return peer_; }

  /// Close the socket and remove the peer from the server.
  virtual void close();

 protected:
  virtual void processError(const std::string &msg, const boost::system::error_code &ec);
  virtual void processPacket(const netplay::Packet &pkt);
 private:
  ServerSocket &server_;
  boost::asio::ip::tcp::endpoint peer_;
  bool has_error_; ///< Avoid multiple processError() calls.
};


/// Socket for server.
class ServerSocket: public PacketSocket
{
  friend class PeerSocket;
 public:
  ServerSocket(boost::asio::io_service &io_service, ServerObserver &obs);
  virtual ~ServerSocket();

  /// Start server on a given port.
  void start(int port);
  /// Return true if the server has been started.
  bool started() const { return started_; }

  /** @brief Remove the peer asynchronously.
   *
   * Removal is delayed using io_service_.post() to make sure the peer is not
   * deleted from a call to one of its method.
   */
  void removePeer(PeerSocket *peer);

 private:
  void acceptNext();
  void onAccept(const boost::system::error_code &ec);
  void doRemovePeer(PeerSocket *peer);

  boost::asio::ip::tcp::acceptor acceptor_;
  bool started_;
  ServerObserver &observer_;

  typedef boost::ptr_vector<PeerSocket> PeerSocketContainer;
  /** @brief Sockets of connected clients.
   *
   * Peers are pushed back. Thus a peer being accepted is always at the back.
   */
  PeerSocketContainer peers_;
};


/// Socket for client.
class ClientSocket: public PacketSocket
{
 public:
  ClientSocket(boost::asio::io_service &io_service);
  virtual ~ClientSocket();

  /** @brief Connect to a server.
   *
   * Timeout is given in microseconds, -1 to wait indefinitely.
   */
  void connect(const char *host, int port, int tout);
  /// Close connection to the server.
  void disconnect();
  /// Return true if the client is connected.
  bool connected() const { return connected_; }

 protected:
  virtual void processError(const std::string &msg, const boost::system::error_code &ec);
  virtual void processPacket(const netplay::Packet &pkt);
  void onTimeout(const boost::system::error_code &ec);
  void onConnect(const boost::system::error_code &ec);

 private:
  boost::asio::monotone_timer timer_; ///< for timeouts
  bool connected_;
};


}

#endif
