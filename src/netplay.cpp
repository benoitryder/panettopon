#include <cstring>
#include <boost/bind.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/placeholders.hpp>
#include "netplay.h"
#include "netplay.pb.h"
#include "log.h"


namespace asio = boost::asio;
using namespace asio::ip;


namespace netplay {


BaseSocket::BaseSocket(asio::io_service &io_service):
    socket_(io_service),
    pkt_size_max_(netplay::Server::Conf::default_instance().pkt_size_max())
{
}

BaseSocket::~BaseSocket()
{
}

void BaseSocket::close()
{
  assert( socket_.is_open() );
  socket_.close();
}


PacketSocket::PacketSocket(asio::io_service &io_service):
    BaseSocket(io_service),
    delayed_close_(false), read_size_(0), read_buf_(NULL), read_buf_size_(0)
{
}

PacketSocket::~PacketSocket()
{
  delete[] read_buf_;
}

void PacketSocket::closeAfterWrites()
{
  assert( socket_.is_open() );
  delayed_close_ = true;
  if( write_queue_.empty() ) {
    this->close();
  }
}

void PacketSocket::onReadSize(const boost::system::error_code &ec)
{
  if( delayed_close_ || ec == asio::error::operation_aborted )
    return;
  if( !ec ) {
    uint16_t n_size;
    ::memcpy(&n_size, read_size_buf_, sizeof(n_size));
    read_size_ = asio::detail::socket_ops::network_to_host_short(n_size);
    if( read_size_ > pkt_size_max_ ) {
      this->processError("packet is too large");
    } else if( read_size_ == 0 ) {
      this->readNext(); // ignore null size
    } else {
      // resize buffer if needed
      // note: size is not reduced if pkt_size_max_ is reduced
      if( read_size_ > read_buf_size_ ) {
        assert( read_buf_size_ != 0 || read_buf_ == NULL );
        delete[] read_buf_;
        read_buf_ = new char[pkt_size_max_];
        read_buf_size_ = pkt_size_max_;
      }
      asio::async_read(
          socket_, asio::buffer(read_buf_, read_size_),
          boost::bind(&PacketSocket::onReadData, this, asio::placeholders::error));
    }
  } else {
    this->processError("read error", ec);
  }
}

void PacketSocket::onReadData(const boost::system::error_code &ec)
{
  if( delayed_close_ || ec == asio::error::operation_aborted )
    return;
  if( !ec ) {
    netplay::Packet pkt;
    if( !pkt.ParsePartialFromArray(read_buf_, read_size_) ) {
      this->processError("invalid packet");
    } else if( !pkt.IsInitialized() ) {
      LOG("missing required fields:\n%s", pkt.DebugString().c_str());
      std::string msg("missing required fields: ");
      msg += pkt.InitializationErrorString();
      this->processError(msg);
    } else {
      try {
        this->processPacket(pkt);
        this->readNext();
      } catch(const CallbackError &e) {
        LOG("packet processing failed:\n%s", pkt.DebugString().c_str());
        this->processError(std::string("packet processing failed: ")+e.what());
      }
    }
  } else {
    this->processError("read error", ec);
  }
}

std::string PacketSocket::serializePacket(const Packet &pkt)
{
  // prepare buffer
  uint16_t pkt_size = pkt.ByteSize();
  const size_t buf_size = sizeof(pkt_size) + pkt_size;
  char buf[buf_size];

  // write packet size
  uint16_t n_pkt_size = asio::detail::socket_ops::host_to_network_short(pkt_size);
  ::memcpy(buf, &n_pkt_size, sizeof(n_pkt_size));

  // serialize message
  if( !pkt.SerializeToArray(buf+sizeof(pkt_size), pkt_size) ) {
    throw std::runtime_error("packet serialization failed");
  }

  return std::string(buf, sizeof(buf));
}

void PacketSocket::readNext()
{
  asio::async_read(
      socket_, asio::buffer(read_size_buf_, sizeof(read_size_buf_)),
      boost::bind(&PacketSocket::onReadSize, this, asio::placeholders::error));
}

void PacketSocket::onWrite(const boost::system::error_code &ec)
{
  write_queue_.pop();
  if( ec == asio::error::operation_aborted )
    return;
  if( !ec ) {
    if( ! write_queue_.empty() ) {
      this->writeNext();
    } else if( delayed_close_ ) {
      if( socket_.is_open() ) {
        this->close();
      }
    }
  } else {
    this->processError("write error", ec);
  }
}

void PacketSocket::writeNext()
{
  asio::async_write(
      socket_, asio::buffer(write_queue_.front()),
      boost::bind(&PacketSocket::onWrite, this, asio::placeholders::error));
}

void PacketSocket::writeRaw(const std::string &s)
{
  assert( s.size() > 0 && s.size() <= pkt_size_max_ );
  bool process_next = write_queue_.empty();
  write_queue_.push(s);
  if( process_next ) {
    this->writeNext();
  }
}


PeerSocket::PeerSocket(ServerSocket &server):
    PacketSocket(server_.io_service()),
    server_(server), has_error_(false)
{
  pkt_size_max_ = server.pkt_size_max_;
}

void PeerSocket::processError(const std::string &msg, const boost::system::error_code &ec)
{
  if( has_error_ ) {
    return;
  }
  has_error_ = true;

  if( ec ) {
    LOG("PeerSocket[%p]: %s: %s", this, msg.c_str(), ec.message().c_str());
  } else {
    LOG("PeerSocket[%p]: %s", this, msg.c_str());
  }

  // send notification
  netplay::Packet pkt;
  netplay::Notification *notif = pkt.mutable_notification();
  notif->set_txt(msg);
  notif->set_severity(netplay::Notification::ERROR);
  this->writePacket(pkt);

  this->closeAfterWrites();
}

void PeerSocket::processPacket(const netplay::Packet &pkt)
{
  server_.observer_.onPeerPacket(this, pkt);
}

void PeerSocket::close()
{
  PacketSocket::close();
  server_.removePeer(this);
}


ServerSocket::ServerSocket(ServerObserver &obs, asio::io_service &io_service):
    BaseSocket(io_service),
    acceptor_(io_service), started_(false), observer_(obs)
{
}

ServerSocket::~ServerSocket()
{
}

void ServerSocket::start(int port)
{
  assert( started_ == false );
  tcp::endpoint endpoint(tcp::v4(), port);
  acceptor_.open(endpoint.protocol());
  acceptor_.set_option(asio::socket_base::reuse_address(true));
  acceptor_.bind(endpoint);
  acceptor_.listen();
  started_ = true;
  this->acceptNext();
}

void ServerSocket::removePeer(PeerSocket *peer)
{
  io_service().post(boost::bind(&ServerSocket::doRemovePeer, this, peer));
}

void ServerSocket::acceptNext()
{
  peers_.push_back(new PeerSocket(*this));
  PeerSocket &peer = peers_.back();
  acceptor_.async_accept(
      peer.socket_, peer.peer(),
      boost::bind(&ServerSocket::onAccept, this, asio::placeholders::error));
}

void ServerSocket::onAccept(const boost::system::error_code &ec)
{
  PeerSocket &peer = peers_.back();
  if( !ec ) {
    try {
      peer.socket_.set_option(tcp::no_delay(true));
    } catch(const boost::exception &e) {
      // setting no delay may fail on some systems, ignore error
    }
    peer.readNext();
    try {
      observer_.onPeerConnect(&peer);
    } catch(const CallbackError &e) {
      peer.PacketSocket::processError(std::string("peer connection failed: ")+e.what());
    }
  } else {
    LOG("accept error: %s", ec.message().c_str());
    peer.close();
  }
  this->acceptNext();
}

void ServerSocket::doRemovePeer(PeerSocket *peer)
{
  PeerSocketContainer::iterator it;
  for(it=peers_.begin(); it!=peers_.end(); ++it) {
    if( &(*it) == peer ) {
      try {
        observer_.onPeerDisconnect(peer);
      } catch(const CallbackError &e) {
        LOG("peer disconnect error: %s", e.what());
      }
      peers_.erase(it);
      return;
    }
  }
  assert( !"peer not found" );
}


ClientSocket::ClientSocket(asio::io_service &io_service):
    PacketSocket(io_service),
    timer_(io_service), connected_(false)
{
}

ClientSocket::~ClientSocket()
{
}


void ClientSocket::connect(const char *host, int port, int tout)
{
  tcp::resolver resolver(io_service());
  tcp::resolver::query query(tcp::v4(), host,
                             "0"); // port cannot be an integer :(
  tcp::endpoint ep = *resolver.resolve(query);
  ep.port(port); // set port now
  socket_.async_connect(
      ep, boost::bind(&ClientSocket::onConnect, this, asio::placeholders::error));
  try {
    socket_.set_option(tcp::no_delay(true));
  } catch(const boost::exception &e) {
    // setting no delay may fail on some systems, ignore error
  }
  if( tout >= 0 ) {
    timer_.expires_from_now(boost::posix_time::milliseconds(tout));
    timer_.async_wait(boost::bind(&ClientSocket::onTimeout, this,
                                  asio::placeholders::error));
  }

  connected_ = true;
}

void ClientSocket::disconnect()
{
  assert( connected_ );
  this->close();
  connected_ = false;
}


void ClientSocket::processPacket(const netplay::Packet &pkt)
{
  //TODO
  (void)pkt;
  assert( false );
}

void ClientSocket::processError(const std::string &msg, const boost::system::error_code &ec)
{
  if( ec ) {
    LOG("Client: %s: %s", msg.c_str(), ec.message().c_str());
  } else {
    LOG("Client: %s", msg.c_str());
  }
  this->disconnect();
}

void ClientSocket::onTimeout(const boost::system::error_code &ec)
{
  if( ec != asio::error::operation_aborted ) {
    this->processError("timeout", ec);
  }
}

void ClientSocket::onConnect(const boost::system::error_code &ec)
{
  timer_.cancel();
  if( !ec ) {
    this->readNext();
  } else {
    this->processError("connect error", ec);
  }
}


}

