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
  /** @brief Error callback.
   *
   * On error, the next read/write operation is not prepared.
   */
  virtual void onError(const std::string &msg, const boost::system::error_code &ec) = 0;
  void onError(const std::string &msg) { boost::system::error_code ec; onError(msg, ec); }

  /** @brief Received packet callback.
   * 
   * If \e false is returned, \e onError() will be called.
   */
  virtual bool onPacketReceived(const Packet &pkt) = 0;

 public:
  static std::string serializePacket(const Packet &pkt);

  void readNext();
  void writeNext();
  void writePacket(const Packet &pkt)
  {
    return this->writeRaw(serializePacket(pkt));
  }
  void writeRaw(const std::string &s);

  /** @brief Do pending write operations and close.
   *
   * Further read packets will be ignored.
   */
  void closeAfterWrites();

  inline bool isClosed() const { return !socket_.is_open(); }

 private:
  void onReadSize(const boost::system::error_code &ec);
  void onReadData(const boost::system::error_code &ec);
  void onWrite(const boost::system::error_code &ec);

 protected:
  boost::asio::ip::tcp::socket socket_;
  uint16_t pkt_size_max_;

 private:
  bool delayed_close_;    ///< closeAfterPendingWrites() has been called
  std::queue<std::string> write_queue_;
  /** @name Attributes for packet reading. */
  //@{
  char read_size_buf_[2]; ///< buffer for the size of the next read packet
  uint16_t read_size_;    ///< size of the next packet
  char *read_buf_;        ///< buffer for the read packet
  size_t read_buf_size_;  ///< allocated size of read_buf_
  //@}
};


/// Socket for server peers.
class PeerSocket: public PacketSocket
{
 public:
  PeerSocket(ServerSocket &server);
  virtual ~PeerSocket() {}
  boost::asio::ip::tcp::endpoint &peer() { return peer_; }
 protected:
  virtual void onError(const std::string &msg, const boost::system::error_code &ec);
  virtual bool onPacketReceived(const netplay::Packet &pkt);
 private:
  ServerSocket &server_;
  boost::asio::ip::tcp::endpoint peer_;
  bool has_error_; ///< Avoid multiple onError() calls.
};


class ServerSocket: public PacketSocket
{
 public:
  ServerSocket(boost::asio::io_service &io_service);
  virtual ~ServerSocket();

  /// Start server on a given port.
  void start(int port);

  /// Return true if the server has been started.
  bool isStarted() const { return started_; }

  /// Method called on client connection.
  virtual void onPeerConnect(PeerSocket *peer) = 0;

 private:
  void acceptNext();
  void onAccept(const boost::system::error_code &ec);

  boost::asio::ip::tcp::acceptor acceptor_;
  bool started_;

  typedef boost::ptr_vector<PeerSocket> PeerSocketContainer;
  /** @brief Sockets of connected clients.
   *
   * Peers are pushed back. Thus a peer being accepted is always at the back.
   */
  PeerSocketContainer peers_;
};



}

#endif
