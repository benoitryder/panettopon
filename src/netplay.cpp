#include <cstring>
#include <boost/bind.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/placeholders.hpp>
#include "netplay.h"
#include "netplay.pb.h"
#include "player.h"
#include "log.h"


namespace asio = boost::asio;


namespace netplay {


PacketSocket::PacketSocket(asio::io_service &io_service):
    socket_(io_service),
    pkt_size_max_(netplay::Server::Conf::default_instance().pkt_size_max()),
    delayed_close_(false),
    read_size_(0), read_buf_(NULL), read_buf_size_(0)
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
    socket_.close();
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
      this->onError("packet is too large");
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
    this->onError("read error", ec);
  }
}

void PacketSocket::onReadData(const boost::system::error_code &ec)
{
  if( delayed_close_ || ec == asio::error::operation_aborted )
    return;
  if( !ec ) {
    netplay::Packet pkt;
    if( !pkt.ParsePartialFromArray(read_buf_, read_size_) ) {
      this->onError("invalid packet");
    } else if( !this->onPacketReceived(pkt) ) {
      LOG("packet processing failed:\n%s", pkt.DebugString().c_str());
      this->onError("packet processing failed");
    } else {
      this->readNext();
    }
  } else {
    this->onError("read error", ec);
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
      if( socket_.is_open() )
        socket_.close();
    }
  } else {
    this->onError("write error", ec);
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


}

