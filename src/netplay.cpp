#include <cstring>
#include <functional>
#include <boost/asio.hpp>
#include "netplay.h"
#include "netplay.pb.h"
#include "log.h"


namespace asio = boost::asio;
using namespace asio::ip;


namespace netplay {


const uint32_t BaseSocket::pkt_size_max = 50*1024;

BaseSocket::BaseSocket(asio::io_service& io_service):
    socket_(io_service)
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


PacketSocket::PacketSocket(asio::io_service& io_service):
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

void PacketSocket::onReadSize(const boost::system::error_code& ec)
{
  if( delayed_close_ || ec == asio::error::operation_aborted ) {
    return;
  }
  if( !ec ) {
    uint32_t n_size;
    ::memcpy(&n_size, read_size_buf_, sizeof(n_size));
    read_size_ = asio::detail::socket_ops::network_to_host_long(n_size);
    if( read_size_ > pkt_size_max ) {
      this->processError("packet is too large");
    } else if( read_size_ == 0 ) {
      this->readNext(); // ignore null size
    } else {
      // resize buffer if needed (double size)
      if( read_size_ > read_buf_size_ ) {
        assert( read_buf_size_ != 0 || read_buf_ == NULL );
        delete[] read_buf_;
        if( read_buf_size_ == 0 ) {
          read_buf_size_ = pkt_size_max/16;
        } else if( read_buf_size_ >= pkt_size_max/2 ) {
          read_buf_size_ = pkt_size_max;
        } else {
          read_buf_size_ *= 2;
        }
        read_buf_ = new char[read_buf_size_];
      }
      auto self = std::static_pointer_cast<PacketSocket>(shared_from_this());
      asio::async_read(
          socket_, asio::buffer(read_buf_, read_size_),
          std::bind(&PacketSocket::onReadData, self, std::placeholders::_1));
    }
  } else {
    this->processError("read error", ec);
  }
}

void PacketSocket::onReadData(const boost::system::error_code& ec)
{
  if( delayed_close_ || ec == asio::error::operation_aborted ) {
    return;
  }
  if( !ec ) {
    Packet pkt;
    if(!pkt.ParseFromArray(read_buf_, read_size_)) {
      this->processError("invalid packet");
    } else if(pkt.pkt_case() == Packet::PKT_NOT_SET) {
      LOG("packet without data");
      this->processError("no packet data");
    } else {
      try {
        this->processPacket(pkt);
        this->readNext();
      } catch(const CallbackError& e) {
        LOG("packet processing failed:\n%s", pkt.DebugString().c_str());
        this->processError(std::string("packet processing failed: ")+e.what());
      }
    }
  } else {
    this->processError("read error", ec);
  }
}

std::string PacketSocket::serializePacket(const Packet& pkt)
{
  // prepare buffer
  uint32_t pkt_size = pkt.ByteSize();
  const size_t buf_size = sizeof(pkt_size) + pkt_size;
  char buf[buf_size];

  // write packet size
  uint32_t n_pkt_size = asio::detail::socket_ops::host_to_network_long(pkt_size);
  ::memcpy(buf, &n_pkt_size, sizeof(n_pkt_size));

  // serialize message
  if( !pkt.SerializeToArray(buf+sizeof(pkt_size), pkt_size) ) {
    throw std::runtime_error("packet serialization failed");
  }

  return std::string(buf, sizeof(buf));
}

void PacketSocket::readNext()
{
  auto self = std::static_pointer_cast<PacketSocket>(shared_from_this());
  asio::async_read(
      socket_, asio::buffer(read_size_buf_, sizeof(read_size_buf_)),
      std::bind(&PacketSocket::onReadSize, self, std::placeholders::_1));
}

void PacketSocket::onWrite(const boost::system::error_code& ec)
{
  if( ec == asio::error::operation_aborted ) {
    return;
  }
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
  auto self = std::static_pointer_cast<PacketSocket>(shared_from_this());
  asio::async_write(
      socket_, asio::buffer(write_queue_.front()),
      std::bind(&PacketSocket::onWrite, self, std::placeholders::_1));
}

void PacketSocket::writeRaw(const std::string& s)
{
  assert( s.size() > 0 && s.size() <= pkt_size_max );
  bool process_next = write_queue_.empty();
  write_queue_.push(s);
  if( process_next ) {
    this->writeNext();
  }
}


PeerSocket::PeerSocket(ServerSocket& server):
    PacketSocket(server.io_service()),
    server_(&server), has_error_(false)
{
}

void PeerSocket::processError(const std::string& msg, const boost::system::error_code& ec)
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
  auto event = std::make_unique<ServerEvent>();
  auto* notif = event->mutable_notification();
  notif->set_text(msg);
  notif->set_severity(PktNotification::ERROR);
  this->sendServerEvent(std::move(event));

  this->closeAfterWrites();
}

void PeerSocket::processPacket(const Packet& pkt)
{
  if(server_) {
    if(pkt.has_client_event()) {
      server_->observer_.onPeerClientEvent(*this, pkt.client_event());
    } else if(pkt.has_client_command()) {
      server_->observer_.onPeerClientCommand(*this, pkt.client_command());
    }
  }
}

void PeerSocket::close()
{
  PacketSocket::close();
  // remove from server peers
  if(server_) {
    ServerSocket::PeerSocketContainer& peers = server_->peers_;
    for(auto it=peers.begin(); it!=peers.end(); ++it) {
      if( (*it).get() == this ) {
        peers.erase(it);
        server_->observer_.onPeerDisconnect(*(*it).get());
        server_ = nullptr;
        return;
      }
    }
    assert( !"peer not found" );
  }
}

void PeerSocket::sendServerEvent(std::unique_ptr<ServerEvent> event)
{
  Packet pkt;
  pkt.set_allocated_server_event(event.release());
  this->writePacket(pkt);
}

void PeerSocket::sendServerResponse(std::unique_ptr<ServerResponse> response)
{
  Packet pkt;
  pkt.set_allocated_server_response(response.release());
  this->writePacket(pkt);
}


ServerSocket::ServerSocket(Observer& obs, asio::io_service& io_service):
    acceptor_(io_service), started_(false), observer_(obs)
{
}

ServerSocket::~ServerSocket()
{
}

void ServerSocket::start(int port)
{
  assert( started_ == false );
  tcp::endpoint endpoint(tcp::v6(), port);
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
  // close all peers
  while(!peers_.empty()) {
    peers_.back()->close();
  }
}


void ServerSocket::broadcastEvent(std::unique_ptr<ServerEvent> event, const PeerSocket* except)
{
  Packet pkt;
  pkt.set_allocated_server_event(event.release());
  const std::string s = PacketSocket::serializePacket(pkt);
  for(auto& peer : peers_) {
    if(peer.get() != except) {
      peer->writeRaw(s);
    }
  }
}


void ServerSocket::acceptNext()
{
  assert(!peer_accept_);
  peer_accept_ = std::make_shared<PeerSocket>(*this);
  auto self = shared_from_this();
  acceptor_.async_accept(
      peer_accept_->socket_, peer_accept_->peer(),
      std::bind(&ServerSocket::onAccept, self, std::placeholders::_1));
}

void ServerSocket::onAccept(const boost::system::error_code& ec)
{
  if( ec == asio::error::operation_aborted ) {
    return;
  } else if( !ec ) {
    peers_.push_back(peer_accept_);
    peer_accept_.reset();
    PeerSocket& peer = *peers_.back();
    try {
      peer.socket_.set_option(tcp::no_delay(true));
    } catch(const boost::exception& e) {
      // setting no delay may fail on some systems, ignore error
    }
    try {
      observer_.onPeerConnect(peer);
    } catch(const CallbackError& e) {
      peer.PacketSocket::processError(std::string("peer connection failed: ")+e.what());
    }
  } else {
    LOG("accept error: %s", ec.message().c_str());
    peer_accept_->close();
    peer_accept_.reset();
  }
  this->acceptNext();
}


ClientSocket::ClientSocket(Observer& obs, asio::io_service& io_service):
    PacketSocket(io_service),
    observer_(obs), timer_(io_service), connected_(false)
{
}

ClientSocket::~ClientSocket()
{
}


void ClientSocket::connect(const char* host, int port, int tout)
{
  tcp::resolver resolver(io_service());
  auto ep_it = resolver.resolve({host, std::to_string(port)});
  auto self = std::static_pointer_cast<ClientSocket>(shared_from_this());
  boost::asio::async_connect(socket_, ep_it, std::bind(&ClientSocket::onConnect, self, std::placeholders::_1));
  try {
    socket_.set_option(tcp::no_delay(true));
  } catch(const boost::exception& e) {
    // setting no delay may fail on some systems, ignore error
  }
  if( tout >= 0 ) {
    timer_.expires_from_now(boost::posix_time::milliseconds(tout));
    timer_.async_wait(std::bind(&ClientSocket::onTimeout, self, std::placeholders::_1));
  }
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

void ClientSocket::sendClientEvent(std::unique_ptr<ClientEvent> event)
{
  Packet pkt;
  pkt.set_allocated_client_event(event.release());
  this->writePacket(pkt);
}

void ClientSocket::sendClientCommand(std::unique_ptr<ClientCommand> command, CommandCallback cb)
{
  Packet pkt;
  pkt.set_allocated_client_command(command.release());
  this->writePacket(pkt);
  command_callbacks_.push(cb);
}


void ClientSocket::processPacket(const Packet& pkt)
{
  if(pkt.has_server_event()) {
    observer_.onServerEvent(pkt.server_event());
  } else if(pkt.has_server_response()) {
    if(command_callbacks_.size()) {
      auto callback = command_callbacks_.front();
      command_callbacks_.pop();
      if(callback) {
        callback(pkt.server_response());
      }
    } else {
      throw CallbackError("server response received but no command issued");
    }
  }
}

void ClientSocket::processError(const std::string& msg, const boost::system::error_code& ec)
{
  if( ec ) {
    LOG("Client: %s: %s", msg.c_str(), ec.message().c_str());
  } else {
    LOG("Client: %s", msg.c_str());
  }
  if( !connected_ ) {
    return; // disconnected by a previous error
  }
  this->close();
}

void ClientSocket::onTimeout(const boost::system::error_code& ec)
{
  if( ec != asio::error::operation_aborted ) {
    this->processError("timeout", ec);
    observer_.onServerConnect(false);
  }
}

void ClientSocket::onConnect(const boost::system::error_code& ec)
{
  connected_ = !ec;
  timer_.cancel();
  if(connected_) {
    this->readNext();
  } else {
    this->processError("connect error", ec);
  }
  observer_.onServerConnect(connected_);
}


}

