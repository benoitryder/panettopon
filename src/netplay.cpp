#include <cstring>
#include <boost/bind.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/placeholders.hpp>
#include "netplay.h"
#include "netplay.pb.h"
#include "deletion_handler.h"
#include "log.h"


namespace asio = boost::asio;
using namespace asio::ip;


namespace netplay {


BaseSocket::BaseSocket(asio::io_service &io_service):
    socket_(io_service),
    pkt_size_max_(netplay::PktServerConf::default_instance().pkt_size_max())
{
}

BaseSocket::~BaseSocket()
{
}

void BaseSocket::close()
{
  // socket may be closed due to the error, but we still have to trigger events
  if( socket_.is_open() ) {
    socket_.close();
  }
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
  if( ec == asio::error::operation_aborted )
    return;
  if( !ec ) {
    write_queue_.pop();
    if( ! write_queue_.empty() ) {
      this->writeNext();
    } else if( delayed_close_ ) {
      this->close();
    }
  } else {
    if( delayed_close_ ) {
      this->close();
    } else {
      this->processError("write error", ec);
    }
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
    PacketSocket(server.io_service()),
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
  netplay::PktNotification *notif = pkt.mutable_notification();
  notif->set_txt(msg);
  notif->set_severity(netplay::PktNotification::ERROR);
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
  ServerSocket::PeerSocketContainer &peers = server_.peers_;
  ServerSocket::PeerSocketContainer::iterator it;
  for(it=peers.begin(); it!=peers.end(); ++it) {
    if( &(*it) == this ) {
      server_.peers_del_.push_back(peers.release(it).release());
      io_service().post(boost::bind(&ServerSocket::doRemovePeer, &server_, this));
      return;
    }
  }
  assert( !"peer not found" );
}


ServerSocket::ServerSocket(Observer &obs, asio::io_service &io_service):
    acceptor_(io_service), started_(false), observer_(obs),
    pkt_size_max_(netplay::PktServerConf::default_instance().pkt_size_max())
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

void ServerSocket::close()
{
  // socket may be closed due to the error, but we still have to trigger events
  if( acceptor_.is_open() ) {
    acceptor_.close();
  }
}


void ServerSocket::broadcastPacket(const netplay::Packet &pkt, const PeerSocket *except/*=NULL*/)
{
  const std::string s = PacketSocket::serializePacket(pkt);
  PeerSocketContainer::iterator it;
  for( it=peers_.begin(); it!=peers_.end(); ++it ) {
    if( &(*it) != except ) {
      (*it).writeRaw(s);
    }
  }
}


void ServerSocket::acceptNext()
{
  assert( peer_accept_.get() == NULL );
  peer_accept_ = std::auto_ptr<PeerSocket>(new PeerSocket(*this));
  acceptor_.async_accept(
      peer_accept_->socket_, peer_accept_->peer(),
      boost::bind(&ServerSocket::onAccept, this, asio::placeholders::error));
}

void ServerSocket::onAccept(const boost::system::error_code &ec)
{
  if( ec == asio::error::operation_aborted ) {
    return;
  } else if( !ec ) {
    peers_.push_back(peer_accept_);
    PeerSocket &peer = peers_.back();
    try {
      peer.socket_.set_option(tcp::no_delay(true));
    } catch(const boost::exception &e) {
      // setting no delay may fail on some systems, ignore error
    }
    try {
      observer_.onPeerConnect(&peer);
    } catch(const CallbackError &e) {
      peer.PacketSocket::processError(std::string("peer connection failed: ")+e.what());
    }
  } else {
    LOG("accept error: %s", ec.message().c_str());
    peer_accept_->close();
    io_service().post(deletion_handler<PeerSocket>(peer_accept_));
  }
  this->acceptNext();
}

void ServerSocket::doRemovePeer(PeerSocket *peer)
{
  PeerSocketContainer::iterator it;
  for(it=peers_del_.begin(); it!=peers_del_.end(); ++it) {
    if( &(*it) == peer ) {
      try {
        observer_.onPeerDisconnect(peer);
      } catch(const CallbackError &e) {
        LOG("peer disconnect error: %s", e.what());
      }
      io_service().post(deletion_handler<PeerSocket>(peers_del_.release(it).release()));
      return;
    }
  }
  assert( !"peer not found" );
}


ClientSocket::ClientSocket(Observer &obs, asio::io_service &io_service):
    PacketSocket(io_service),
    observer_(obs), timer_(io_service), connected_(false)
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

void ClientSocket::close()
{
  if( !connected_ ) {
    return; // already closed/closing
  }
  connected_ = false;
  PacketSocket::close();
  observer_.onServerDisconnect();
}


void ClientSocket::processPacket(const netplay::Packet &pkt)
{
  observer_.onClientPacket(pkt);
}

void ClientSocket::processError(const std::string &msg, const boost::system::error_code &ec)
{
  if( !connected_ ) {
    return; // disconnected by a previous error
  }
  if( ec ) {
    LOG("Client: %s: %s", msg.c_str(), ec.message().c_str());
  } else {
    LOG("Client: %s", msg.c_str());
  }
  this->close();
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

