#include <boost/bind.hpp>
#include <boost/asio/placeholders.hpp>
#include "interface.h"
#include "client.h"
#include "game.h"
#include "log.h"

namespace asio = boost::asio;
using namespace asio::ip;


bool ClientMatch::processInput(Field *fld, const netplay::Input &np_input)
{
  //XXX same as ServerMatch
  assert( fld != NULL );
  Tick tick = np_input.tick();
  if( tick < fld->tick() )
    return false;

  // skipped frames
  while( fld->tick() < tick ) {
    if( !this->stepField(fld, 0) )
      return false;
  }
  // given frames
  const int keys_nb = np_input.keys_size();
  for( int i=0; i<keys_nb; i++ ) {
    if( !this->stepField(fld, np_input.keys(i)) )
      return false;
  }
  return true;
}

bool ClientMatch::processGarbage(const netplay::Garbage &np_garbage)
{
  GbWaitingMap::iterator it = gbs_wait_.find(np_garbage.gbid());
  Garbage *gb = it == gbs_wait_.end() ? NULL : (*it).second;
  if( np_garbage.drop() ) {
    if( gb == NULL )
      return true; // ignore, one of our garbage, already dropped
    Field *my_fld = client_.player()->field();
    if( gb->to == my_fld ) {
      netplay::Packet pkt_send;
      netplay::Garbage *np_garbage_send = pkt_send.mutable_garbage();
      np_garbage_send->set_gbid( gb->gbid );
      np_garbage_send->set_drop(true);
      client_.writePacket(pkt_send);
    }
    assert( gb->to->waitingGarbage(0).gbid == gb->gbid );
    gbs_wait_.erase(it);
    gb->to->dropNextGarbage();
  } else {
    if( gb == NULL ) {
      if( !np_garbage.has_plid_to() || !np_garbage.has_pos() ||
         !np_garbage.has_type() || !np_garbage.has_size() )
        return false;
      gb = new Garbage;
      std::auto_ptr<Garbage> agb(gb);
      gb->gbid = np_garbage.gbid();

      Player *pl = client_.player(np_garbage.plid_to());
      if( pl == NULL || pl->field() == NULL )
        return false;
      gb->to = pl->field();
      gb->to->insertWaitingGarbage(gb, np_garbage.pos()); //XXX check pos?
      //XXX size may be unset

      if( np_garbage.has_plid_from() ) {
        pl = client_.player(np_garbage.plid_from());
        if( pl == NULL || pl->field() == NULL )
          return false;
        gb->from = pl->field();
      } else {
        gb->from = NULL;
      }
      agb.release();
      gbs_wait_[gb->gbid] = gb;
    }
    if( np_garbage.has_plid_to() &&
       np_garbage.plid_to() != gb->to->player()->plid() ) {
      if( !np_garbage.has_pos() )
        return false;
      Player *pl = client_.player(np_garbage.plid_to());
      if( pl == NULL || pl->field() == NULL )
        return false;
      gb->to->removeWaitingGarbage(gb);
      gb->to = pl->field();
      gb->to->insertWaitingGarbage(gb, np_garbage.pos()); //XXX check pos?
    }
    if( np_garbage.has_type() ) {
      const Garbage::Type type = static_cast<Garbage::Type>(np_garbage.type());
      if( type != gb->type ) {
        if( !np_garbage.has_size() )
          return false;
        gb->type = type;
      }
    }
    if( np_garbage.has_size() ) {
      assert( np_garbage.size() > 0 );
      if( gb->type == Garbage::TYPE_CHAIN )
        gb->size = FieldPos(FIELD_WIDTH, np_garbage.size());
      else if( gb->type == Garbage::TYPE_COMBO )
        gb->size = FieldPos(np_garbage.size(), 1);
      else
        assert( !"not supported yet" );
    }
  }
  return true;
}

bool ClientMatch::stepField(Field *fld, KeyState keys)
{
  if( fld->lost() )
    return false;
  Tick prev_tick = fld->tick();
  if( prev_tick+1 >= this->tick() + client_.conf().tk_lag_max )
    return false;

  fld->step(keys);
  if( prev_tick == this->tick() ) {
    // don't update tick_ when it will obviously not be modified
    this->updateTick();
  }
  return true;
}



Client::Client(ClientInterface &intf, asio::io_service &io_service):
    socket_(io_service), state_(STATE_NONE), match_(*this),
    player_(NULL), intf_(intf), timer_(io_service)
{
}

void Client::connect(const char *host, int port, int tout)
{
  LOG("connecting to %s:%d ...", host, port);
  socket_.connect(host, port, tout);
  LOG("connected");
  state_ = STATE_LOBBY;
  conf_.toDefault();
}

void Client::disconnect()
{
  //XXX send a proper "quit" message
  socket_.close();
  state_ = STATE_NONE;
  socket_.io_service().stop(); //XXX
}

void Client::sendChat(const std::string &txt)
{
  assert( player_ != NULL );
  netplay::Packet pkt;
  netplay::Chat *np_chat = pkt.mutable_chat();
  np_chat->set_plid(player_->plid());
  np_chat->set_txt(txt);
  socket_.writePacket(pkt);
}

void Client::sendReady()
{
  assert( player_ != NULL );
  assert( state_ == STATE_LOBBY || state_ == STATE_READY );
  if( player_->ready() )
    return; // already ready, do nothing
  netplay::Packet pkt;
  netplay::Player *np_player = pkt.mutable_player();
  np_player->set_plid(player_->plid());
  np_player->set_ready(true);
  socket_.writePacket(pkt);
}


bool Client::onPacketReceived(const netplay::Packet &pkt)
{
  if( !pkt.IsInitialized() )
    return false;

  if( pkt.has_input() ) {
    if( state_ != STATE_GAME )
      return false;
    const netplay::Input &np_input = pkt.input();
    PlId plid = np_input.plid();
    if( plid == player_->plid() )
      return true; // ignore
    Player *pl = this->player(plid);
    if( pl == NULL || pl->field() == NULL )
      return false;
    if( !match_.processInput(pl->field(), np_input) )
      return false;
    //TODO one call per tick
    intf_.onFieldStep(pl->field());

  } else if( pkt.has_garbage() ) {
    if( state_ != STATE_GAME )
      return false;
    if( !match_.processGarbage(pkt.garbage()) )
      return false;

  } else if( pkt.has_field() ) {
    const netplay::Field &np_field = pkt.field();
    Player *pl = this->player(np_field.plid());
    if( pl == NULL )
      return false;
    // init
    if( state_ == STATE_INIT ) {
      if( pl->field() != NULL || np_field.has_tick() || !np_field.has_seed() )
        return false; //XXX should rank be invalid too?
      Field *fld = match_.addField(pl, np_field.seed());
      // conf
      const netplay::Field::Conf &np_conf = np_field.conf();
      FieldConf conf;
#define FIELD_CONF_EXPR_INIT(n) \
      if( !np_conf.has_##n() ) return false; \
      conf.n = np_conf.n();
      FIELD_CONF_APPLY(FIELD_CONF_EXPR_INIT);
#undef FIELD_CONF_EXPR_INIT
      conf.raise_adjacent = static_cast<FieldConf::RaiseAdjacent>(np_conf.raise_adjacent());
      if( !conf.isValid() )
        return false;
      // check client conf against server conf
      if( conf.gb_wait_tk <= conf_.tk_lag_max ) {
        LOG("garbage waiting time must be lower lag limit");
        return false;
      }
      fld->setConf(conf);
      // grid
      if( np_field.blocks_size() > 0 )
        if( !fld->setGridContentFromPacket(np_field.blocks()) )
          return false;
    } else if( state_ == STATE_GAME ) {
      Field *fld = pl->field();
      if( fld == NULL || np_field.has_seed() || np_field.has_conf() ||
         np_field.blocks_size() > 0 )
        return false;
      // note: field tick not used
      // rank
      if( np_field.has_rank() ) {
        fld->setRank(np_field.rank()); //TODO may fail if already set
      }
    } else {
      return false;
    }

  } else if( pkt.has_player() ) {
    const netplay::Player &np_player = pkt.player();
    PlId plid = np_player.plid();
    Player *pl = this->player(plid);
    if( pl == NULL ) {
      pl = new Player(np_player.plid());
      players_.insert( plid, pl );
      // first player: this is us
      if( player_ == NULL ) {
        player_ = pl;
      } else {
        intf_.onPlayerJoined(pl);
      }
    }
    if( np_player.has_out() && np_player.out() ) {
      // player out, ignore all other fields
      if( player_ == pl ) {
        //XXX remove ourselves, should exits
        this->disconnect();
      } else {
        intf_.onPlayerQuit(pl);
        if( pl->field() != NULL )
          match_.removeField(pl->field());
        players_.erase(pl->plid());
      }
    } else {
      if( np_player.has_nick() ) {
        const std::string &old_nick = pl->nick();
        pl->setNick( np_player.nick() );
        intf_.onPlayerSetNick(pl, old_nick);
      }
      if( np_player.has_ready() ) {
        //XXX check whether change is valid?
        pl->setReady(np_player.ready());
        intf_.onPlayerReady(pl);
      }
      //XXX:temp immediately ready
      if( player_ != pl && (state_ == STATE_LOBBY || state_ == STATE_READY)
         && !player_->ready() )
        this->sendReady();
    }

  } else if( pkt.has_server() ) {
    const netplay::Server &np_server = pkt.server();
    if( np_server.has_conf() ) {
      if( state_ != STATE_LOBBY &&
         !(np_server.has_state() &&
           np_server.state() == netplay::Server::LOBBY) )
        return false;
      const netplay::Server::Conf &np_conf = np_server.conf();
#define SERVER_CONF_EXPR_PKT(n,ini,t) \
      conf_.n = np_conf.n();
      SERVER_CONF_APPLY(SERVER_CONF_EXPR_PKT);
#undef SERVER_CONF_EXPR_PKT
    }
    if( np_server.has_state() ) {
      //TODO check whether state change is valid
      State prev_state = state_;
      state_ = static_cast<State>(np_server.state());
      // automatically reset ready flag
      PlayerContainer::iterator it;
      for( it=players_.begin(); it!=players_.end(); ++it )
        (*it).second->setReady(false);

      if( state_ == STATE_INIT ) {
        if( match_.started() ) 
          match_.stop(); // prepare for init
        intf_.onMatchInit(&match_);
      } else if( state_ == STATE_READY ) {
        // init fields for match
        match_.start();
        intf_.onMatchReady(&match_);
      } else if( state_ == STATE_GAME ) {
        intf_.onMatchStart(&match_);
        boost::posix_time::time_duration dt = boost::posix_time::microseconds(conf_.tk_usec);
        tick_clock_ = boost::posix_time::microsec_clock::universal_time() + dt;
        timer_.expires_from_now(dt);
        timer_.async_wait(boost::bind(&Client::onInputTick, this,
                                      asio::placeholders::error));
      } else if( state_ == STATE_LOBBY && prev_state == STATE_GAME ) {
        timer_.cancel();
        match_.stop();
        intf_.onMatchEnd(&match_); //XXX before match_.stop()?
      }

      //XXX:temp immediately ready
      if( player_ != NULL && (state_ == STATE_LOBBY || state_ == STATE_READY) )
        this->sendReady();
    }

  } else if( pkt.has_chat() ) {
    const netplay::Chat &np_chat = pkt.chat();
    Player *pl = this->player(np_chat.plid());
    if( pl == NULL ) {
      LOG("unknown plid: %u", np_chat.plid());
      return false;
    }
    intf_.onChat(pl, np_chat.txt());

  } else if( pkt.has_notification() ) {
    const netplay::Notification &np_notification = pkt.notification();
    intf_.onNotification(static_cast<Interface::Severity>(np_notification.severity()), np_notification.txt());

  } else {
    return false; // invalid packet field
  }
  return true;
}

void Client::onInputTick(const boost::system::error_code &ec)
{
  if( ec == asio::error::operation_aborted )
    return;
  for(;;) {
    KeyState keys = intf_.getNextInput();
    Field *fld = player_->field();
    Tick tk = fld->tick();
    if( match_.stepField(fld, keys) ) {
      intf_.onFieldStep(fld);
      netplay::Packet pkt;
      netplay::Input *np_input = pkt.mutable_input();
      np_input->set_plid(player_->plid());
      np_input->set_tick(tk);
      np_input->add_keys(keys);
      socket_.writePacket(pkt);
    }
    if( !fld->lost() ) {
      tick_clock_ += boost::posix_time::microseconds(conf_.tk_usec);
      boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
      if( tick_clock_ < now )
        continue; // loop
      timer_.expires_from_now(tick_clock_ - now);
      timer_.async_wait(boost::bind(&Client::onInputTick, this,
                                    asio::placeholders::error));
    }
    break;
  }
}

