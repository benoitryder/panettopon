#include <boost/bind.hpp>
#include <boost/asio/placeholders.hpp>
#include "client.h"
#include "game.h"
#include "log.h"

namespace asio = boost::asio;
using namespace asio::ip;


ClientInstance::ClientInstance(asio::io_service &io_service):
    socket_(*this, io_service), timer_(io_service)
{
}

ClientInstance::~ClientInstance()
{
}

void ClientInstance::connect(const char *host, int port, int tout)
{
  LOG("connecting to %s:%d ...", host, port);
  socket_.connect(host, port, tout);
  LOG("connected");
  state_ = STATE_LOBBY;
  conf_.toDefault();
}

void ClientInstance::disconnect()
{
  //XXX send a proper "quit" message
  socket_.close();
  state_ = STATE_NONE;
  socket_.io_service().stop(); //XXX
}

#if 0
void ClientInstance::sendChat(const std::string &txt)
{
  assert( player_ != NULL );
  netplay::Packet pkt;
  netplay::Chat *np_chat = pkt.mutable_chat();
  np_chat->set_plid(player_->plid());
  np_chat->set_txt(txt);
  socket_.writePacket(pkt);
}

void ClientInstance::sendReady()
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
#endif


void ClientInstance::onClientPacket(const netplay::Packet &pkt)
{
  if( pkt.has_input() ) {
    this->processPacketInput(pkt.input());
  } else if( pkt.has_garbage() ) {
    this->processPacketGarbage(pkt.garbage());
  } else if( pkt.has_field() ) {
    this->processPacketField(pkt.field());
  } else if( pkt.has_player() ) {
    this->processPacketPlayer(pkt.player());
  } else if( pkt.has_server() ) {
    this->processPacketServer(pkt.server());
  } else if( pkt.has_chat() ) {
    const netplay::Chat &np_chat = pkt.chat();
    Player *pl = this->player(np_chat.plid());
    if( pl == NULL ) {
      throw netplay::CallbackError("invalid player");
    }
    //TODO intf_.onChat(pl, np_chat.txt());

  } else if( pkt.has_notification() ) {
    /*TODO
    const netplay::Notification &np_notification = pkt.notification();
    intf_.onNotification(static_cast<Interface::Severity>(np_notification.severity()), np_notification.txt());
    */

  } else {
    throw netplay::CallbackError("invalid packet field");
  }
}

void ClientInstance::onInputTick(const boost::system::error_code &ec)
{
  if( ec == asio::error::operation_aborted ) {
    return;
  }

  netplay::Packet pkt;
  netplay::Input *np_input = pkt.mutable_input();

  for(;;) {
    //XXX optimize the loop on local players with a field to avoid to iterate
    //on all players?
    bool more_steps = false;
    PlayerContainer::iterator it;
    for(it=players_.begin(); it!=players_.end(); ++it) {
      Player *pl = (*it).second;
      Field *fld = pl->field();
      if( !pl->local() || fld == NULL ) {
        continue;
      }
      // note: all local players still playing should have the same tick
      // thus, break instead of continue
      Tick tk = fld->tick();
      if( tk+1 >= match_.tick() + conf_.tk_lag_max ) {
        break;
      }
      //TODO KeyState keys = intf_.getNextInput();

      // step
      fld->step(keys);
      if( tk == match_.tick() ) {
        // don't update tick_ when it will obviously not be modified
        //XXX:check condition
        match_.updateTick();
      }
      //TODO intf_.onFieldStep(fld);

      // send packet
      np_input->set_plid(pl->plid());
      np_input->set_tick(tk);
      np_input->add_keys(keys);
      socket_.writePacket(pkt);

      if( !fld->lost() ) {
        more_steps = true;
      }
    }

    if( more_steps ) {
      tick_clock_ += boost::posix_time::microseconds(conf_.tk_usec);
      boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
      if( tick_clock_ < now )
        continue; // loop
      timer_.expires_from_now(tick_clock_ - now);
      timer_.async_wait(boost::bind(&ClientInstance::onInputTick, this,
                                    asio::placeholders::error));
    }
    break;
  }
}


void ClientInstance::processPacketInput(const netplay::Input &pkt_input)
{
  if( state_ != STATE_GAME ) {
    throw netplay::CallbackError("match is not running");
  }
  //TODO ignore local players
  Player *pl = this->player(pkt_input.plid());
  if( pl == NULL || pl->field() == NULL ) {
    throw netplay::CallbackError("invalid player");
  }
  Field *fld = pl->field();

  Tick tick = pkt_input.tick();
  if( tick < fld->tick() ) {
    throw netplay::CallbackError("input tick in the past");
  }
  // skipped frames
  while( fld->tick() < tick ) {
    this->stepRemoteField(pl, 0);
  }
  // provided frames
  const int keys_nb = pkt_input.keys_size();
  for( int i=0; i<keys_nb; i++ ) {
    this->stepRemoteField(pl, pkt_input.keys(i));
  }
}

void ClientInstance::processPacketGarbage(const netplay::Garbage &pkt_gb)
{
  if( state_ != STATE_GAME ) {
    throw netplay::CallbackError("match is not running");
  }

  const Match::GbWaitingMap gbs_wait = match_.waitingGarbages();
  Match::GbWaitingMap::const_iterator it = gbs_wait.find(pkt_gb.gbid());
  Garbage *gb = it == gbs_wait.end() ? NULL : (*it).second;

  if( pkt_gb.drop() ) {
    // drop garbage
    if( gb == NULL ) {
      return; // ignore, one of our garbages, already dropped
    }
    Player *pl = this->player(gb->to);
    if( pl->local() ) {
      netplay::Packet pkt;
      netplay::Garbage *pkt_gb = pkt.mutable_garbage();
      pkt_gb->set_gbid( gb->gbid );
      pkt_gb->set_drop(true);
      socket_.writePacket(pkt);
    }
    match_.dropGarbage(gb);

  } else if( gb == NULL ) {
    // new garbage
    if( !pkt_gb.has_plid_to() || !pkt_gb.has_pos() ||
       !pkt_gb.has_type() || !pkt_gb.has_size() ) {
      throw netplay::CallbackError("incomplete new garbage information");
    }
    gb = new Garbage;
    std::auto_ptr<Garbage> agb(gb);
    gb->gbid = pkt_gb.gbid();

    Player *pl_to = this->player(pkt_gb.plid_to());
    if( pl_to == NULL || pl_to->field() == NULL ) {
      throw netplay::CallbackError("invalid garbage target");
    }
    gb->to = pl_to->field();

    if( pkt_gb.has_plid_from() ) {
      Player *pl_from = this->player(pkt_gb.plid_from());
      if( pl_from == NULL || pl_from->field() == NULL ) {
        throw netplay::CallbackError("invalid garbage origin");
      }
      gb->from = pl_from->field();
    } else {
      gb->from = NULL;
    }

    gb->type = static_cast<Garbage::Type>(pkt_gb.type());
    if( pkt_gb.size() == 0 ) {
      throw netplay::CallbackError("invalid garbage size");
    }
    if( gb->type == Garbage::TYPE_CHAIN ) {
      gb->size = FieldPos(FIELD_WIDTH, pkt_gb.size());
    } else if( gb->type == Garbage::TYPE_COMBO ) {
      gb->size = FieldPos(pkt_gb.size(), 1);
    } else {
      assert( !"not supported yet" );
    }

    if( pkt_gb.pos() > gb->to->waitingGarbageCount() ) {
      throw netplay::CallbackError("invalid garbage position");
    }
    agb.release();
    match_.addGarbage(gb, pkt_gb.pos());

  } else {
    // update existing garbage

    if( pkt_gb.has_plid_to() ) {
      Player *pl_to = this->player(gb->to);
      assert( pl_to != NULL );
      if( pkt_gb.plid_to() != pl_to->plid() ) {
        // actual change
        pl_to = this->player(pkt_gb.plid_to());
        if( pl_to != NULL && pl_to->field() != NULL ) {
          throw netplay::CallbackError("invalid garbage target");
        }
        if( !pkt_gb.has_pos() ) {
          throw netplay::CallbackError("garbage position missing");
        }
      }
      if( pkt_gb.pos() > pl_to->field()->waitingGarbageCount() ) {
        throw netplay::CallbackError("invalid garbage position");
      }
      gb->to->removeWaitingGarbage(gb);
      gb->to = pl_to->field();
      gb->to->insertWaitingGarbage(gb, pkt_gb.pos());
    }

    if( pkt_gb.has_type() ) {
      const Garbage::Type type = static_cast<Garbage::Type>(pkt_gb.type());
      if( type != gb->type ) {
        if( !pkt_gb.has_size() ) {
          throw netplay::CallbackError("garbage size missing");
        }
        gb->type = type;
      }
    }

    if( pkt_gb.has_size() ) {
      if( pkt_gb.size() == 0 ) {
        throw netplay::CallbackError("invalid garbage size");
      }
      if( gb->type == Garbage::TYPE_CHAIN ) {
        gb->size = FieldPos(FIELD_WIDTH, pkt_gb.size());
      } else if( gb->type == Garbage::TYPE_COMBO ) {
        gb->size = FieldPos(pkt_gb.size(), 1);
      } else {
        assert( !"not supported yet" );
      }
    }
  }
}

void ClientInstance::processPacketField(const netplay::Field &pkt_fld)
{
  Player *pl = this->player(pkt_fld.plid());
  if( pl == NULL ) {
    throw netplay::CallbackError("invalid player");
  }

  if( state_ == STATE_INIT ) {
    if( pl->field() != NULL ) {
      throw netplay::CallbackError("field already initialized");
    }
    if( pkt_fld.has_tick() || !pkt_fld.has_seed() ) {
      throw netplay::CallbackError("invalid fields");
    }
    Field *fld = match_.newField(pkt_fld.seed());
    pl->setField(fld);
    // conf
    const netplay::Field::Conf &np_conf = pkt_fld.conf();
    FieldConf conf;
#define FIELD_CONF_EXPR_INIT(n) \
    if( !np_conf.has_##n() ) throw netplay::CallbackError("conf field missing: " #n); \
    conf.n = np_conf.n();
    FIELD_CONF_APPLY(FIELD_CONF_EXPR_INIT);
#undef FIELD_CONF_EXPR_INIT
    conf.raise_adjacent = static_cast<FieldConf::RaiseAdjacent>(np_conf.raise_adjacent());
    if( !conf.isValid() ) {
      throw netplay::CallbackError("invalid configuration");
    }
    // check client conf against server conf
    if( conf.gb_wait_tk <= conf_.tk_lag_max ) {
      LOG("garbage waiting time must be lower than lag limit");
      throw netplay::CallbackError("unsupported configuration");
    }
    fld->setConf(conf);
    // grid
    if( pkt_fld.blocks_size() > 0 ) {
      //TODO throw exceptions in setGridContentFromPacket
      fld->setGridContentFromPacket(pkt_fld.blocks());
    }

  } else if( state_ == STATE_GAME ) {
    Field *fld = pl->field();
    if( fld == NULL ) {
      throw netplay::CallbackError("field not initialized");
    }
    if( fld == NULL || pkt_fld.has_seed() || pkt_fld.has_conf() ||
       pkt_fld.blocks_size() > 0 ) {
      throw netplay::CallbackError("invalid fields");
    }
    // note: field tick not used
    // rank
    if( pkt_fld.has_rank() ) {
      fld->setRank(pkt_fld.rank()); //TODO may fail if already set
    }

  } else {
    throw netplay::CallbackError("invalid in current state");
  }
}

void ClientInstance::processPacketPlayer(const netplay::Player &pkt_pl)
{
  Player *pl = this->player(pkt_pl.plid());
  if( pl == NULL ) {
    // new player
    if( !pkt_pl.has_nick() || pkt_pl.has_ready() ) {
      throw netplay::CallbackError("invalid fields");
    }
    //TODO check we asked for a new local player
    pl = new Player(pkt_pl.plid(), pkt_pl.join());
    PlId plid = pl->plid(); // use a temporary value to help g++
    players_.insert(plid, pl);

    //TODO intf_.onPlayerJoined(pl);

  } else if( pkt_pl.out() ) {
    //TODO intf_.onPlayerQuit(pl);
    if( pl->field() != NULL ) {
      match_.removeField(pl->field());
      pl->setField(NULL);
    }
    players_.erase(pl->plid());

  } else {
    if( pkt_pl.has_nick() ) {
      //TODO intf_.onPlayerSetNick(pl, pl->nick());
      pl->setNick( pkt_pl.nick() );
    }
    if( pkt_pl.has_ready() ) {
      //XXX check whether change is valid?
      pl->setReady(pkt_pl.ready());
      //TODO intf_.onPlayerReady(pl);
    }
  }
}

void ClientInstance::processPacketServer(const netplay::Server &pkt_server)
{
  if( pkt_server.has_conf() ) {
    if( state_ != STATE_LOBBY &&
       !(pkt_server.has_state() &&
         pkt_server.state() == netplay::Server::LOBBY) ) {
      throw netplay::CallbackError("invalid in current state");
    }
    const netplay::Server::Conf &np_conf = pkt_server.conf();
#define SERVER_CONF_EXPR_PKT(n,ini,t) \
    conf_.n = np_conf.n();
    SERVER_CONF_APPLY(SERVER_CONF_EXPR_PKT);
#undef SERVER_CONF_EXPR_PKT
  }

  if( pkt_server.has_state() ) {
    //TODO check whether state change is valid
    State new_state = static_cast<State>(pkt_server.state());
    if( new_state == state_ ) {
      return; // nothing to do, should not happen though
    }
    // automatically reset ready flag
    PlayerContainer::iterator it;
    for( it=players_.begin(); it!=players_.end(); ++it ) {
      (*it).second->setReady(false);
    }

    if( new_state == STATE_INIT ) {
      if( match_.started() ) {
        this->stopMatch();
      }
      state_ = new_state;
      //TODO intf_.onMatchInit(&match_);
    } else if( new_state == STATE_READY ) {
      state_ = new_state;
      // init fields for match
      match_.start();
      //TODO intf_.onMatchReady(&match_);
    } else if( new_state == STATE_GAME ) {
      state_ = new_state;
      //TODO intf_.onMatchStart(&match_);
      boost::posix_time::time_duration dt = boost::posix_time::microseconds(conf_.tk_usec);
      tick_clock_ = boost::posix_time::microsec_clock::universal_time() + dt;
      timer_.expires_from_now(dt);
      timer_.async_wait(boost::bind(&ClientInstance::onInputTick, this,
                                    asio::placeholders::error));
    } else if( new_state == STATE_LOBBY ) {
      this->stopMatch();
    }
  }
}


void ClientInstance::stopMatch()
{
  LOG("stop match");

  timer_.cancel();
  PlayerContainer::iterator it;
  for(it=players_.begin(); it!=players_.end(); ++it) {
    (*it).second->setField(NULL);
  }
  match_.stop();
  state_ = STATE_LOBBY;
  //TODO intf_.onMatchEnd(&match_);
}

