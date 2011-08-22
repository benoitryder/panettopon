#include <boost/asio/placeholders.hpp>
#include "client.h"
#include "game.h"
#include "log.h"

namespace asio = boost::asio;
using namespace asio::ip;


ClientInstance::ClientInstance(Observer &obs, asio::io_service &io_service):
    observer_(obs), socket_(*this, io_service)
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
  state_ = STATE_NONE;
  socket_.close();
}

void ClientInstance::newLocalPlayer(const std::string &nick)
{
  netplay::Packet pkt;
  netplay::Player *np_player = pkt.mutable_player();
  np_player->set_plid(0);
  np_player->set_join(true);
  np_player->set_nick(nick);
  socket_.writePacket(pkt);
}


void ClientInstance::playerSetNick(Player *pl, const std::string &nick)
{
  assert( pl->local() );
  if( nick == pl->nick() ) {
    return; // nothing to do
  }
  pl->setNick(nick);

  netplay::Packet pkt;
  netplay::Player *np_player = pkt.mutable_player();
  np_player->set_plid(pl->plid());
  np_player->set_nick(nick);
  socket_.writePacket(pkt);
}

void ClientInstance::playerSetReady(Player *pl, bool ready)
{
  assert( pl->local() );
  assert( state_ == STATE_LOBBY || state_ == STATE_READY );
  if( pl->ready() == ready ) {
    return; // already ready, do nothing
  }
  pl->setReady(ready);

  netplay::Packet pkt;
  netplay::Player *np_player = pkt.mutable_player();
  np_player->set_plid(pl->plid());
  np_player->set_ready(ready);
  socket_.writePacket(pkt);
}

void ClientInstance::playerSendChat(Player *pl, const std::string &msg)
{
  assert( pl->local() );

  netplay::Packet pkt;
  netplay::Chat *np_chat = pkt.mutable_chat();
  np_chat->set_plid(pl->plid());
  np_chat->set_txt(msg);
  socket_.writePacket(pkt);
}

void ClientInstance::playerStep(Player *pl, KeyState keys)
{
  assert( pl->local() && pl->field() != NULL );
  Tick tk = pl->field()->tick();
  this->doStepPlayer(pl, keys);

  // send packet
  netplay::Packet pkt;
  netplay::Input *np_input = pkt.mutable_input();
  np_input->set_plid(pl->plid());
  np_input->set_tick(tk);
  np_input->add_keys(keys);
  socket_.writePacket(pkt);
}

void ClientInstance::playerQuit(Player *pl)
{
  assert( pl->local() );

  netplay::Packet pkt;
  netplay::Player *np_player = pkt.mutable_player();
  np_player->set_plid(pl->plid());
  np_player->set_out(true);
  socket_.writePacket(pkt);
}


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
    observer_.onChat(pl, np_chat.txt());

  } else if( pkt.has_notification() ) {
    const netplay::Notification &np_notification = pkt.notification();
    observer_.onNotification(static_cast<GameInstance::Severity>(np_notification.severity()), np_notification.txt());

  } else {
    throw netplay::CallbackError("invalid packet field");
  }
}

void ClientInstance::onServerDisconnect()
{
  LOG("disconnected");
  observer_.onServerDisconnect();
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
    this->stepRemotePlayer(pl, 0);
  }
  // provided frames
  const int keys_nb = pkt_input.keys_size();
  for( int i=0; i<keys_nb; i++ ) {
    this->stepRemotePlayer(pl, pkt_input.keys(i));
  }
}

void ClientInstance::processPacketGarbage(const netplay::Garbage &pkt_gb)
{
  if( state_ != STATE_GAME ) {
    throw netplay::CallbackError("match is not running");
  }

  const Match::GbHangingMap gbs_hang = match_.hangingGarbages();
  Match::GbHangingMap::const_iterator it = gbs_hang.find(pkt_gb.gbid());
  Garbage *gb = it == gbs_hang.end() ? NULL : (*it).second;

  if( pkt_gb.wait() ) {
    // garbage from hanging to wait (gbid is not significant)
    if( gb == NULL ) {
      throw netplay::CallbackError("garbage to wait not found");
    }
    match_.waitGarbageDrop(gb);
    Player *pl = this->player(gb->to);
    if( pl->local() ) {
      // one of our garbages, drop it
      netplay::Packet pkt;
      netplay::Garbage *pkt_gb = pkt.mutable_garbage();
      pkt_gb->set_gbid( gb->gbid );
      pkt_gb->set_plid_to(pl->plid());
      pkt_gb->set_drop(true);
      socket_.writePacket(pkt);
      gb->to->dropNextGarbage();
    }

  } else if( pkt_gb.drop() ) {
    // drop garbage
    Player *pl = this->player(pkt_gb.plid_to());
    if( pl == NULL || pl->field() == NULL ) {
      throw netplay::CallbackError("invalid field for dropped garbage");
    }
    if( !pl->local() ) { // ignore our garbages (already dropped)
      pl->field()->dropNextGarbage();
    }

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

    if( pkt_gb.pos() > gb->to->hangingGarbageCount() ) {
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
      if( pkt_gb.pos() > pl_to->field()->hangingGarbageCount() ) {
        throw netplay::CallbackError("invalid garbage position");
      }
      gb->to->removeHangingGarbage(gb);
      gb->to = pl_to->field();
      gb->to->insertHangingGarbage(gb, pkt_gb.pos());
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
    Field *fld = match_.newField(pl->fieldConf(), pkt_fld.seed());
    pl->setField(fld);
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
    if( fld == NULL || pkt_fld.has_seed() ||
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
    if( !pkt_pl.has_nick() || !pkt_pl.has_field_conf() ) {
      throw netplay::CallbackError("invalid fields");
    }
    //TODO check we asked for a new local player
    pl = new Player(pkt_pl.plid(), pkt_pl.join());
    pl->setNick( pkt_pl.nick() );
    PlId plid = pl->plid(); // use a temporary value to help g++
    players_.insert(plid, pl);
    if( pkt_pl.has_ready() ) {
      pl->setReady(pkt_pl.ready());
    }
    FieldConf conf;
    conf.fromPacket(pkt_pl.field_conf());
    pl->setFieldConf(conf);
    observer_.onPlayerJoined(pl);

  } else if( pkt_pl.out() ) {
    observer_.onPlayerQuit(pl);
    if( pl->field() != NULL ) {
      match_.removeField(pl->field());
      pl->setField(NULL);
    }
    players_.erase(pl->plid());

  } else {
    if( pkt_pl.has_nick() ) {
      const std::string old_nick = pl->nick();
      pl->setNick( pkt_pl.nick() );
      observer_.onPlayerChangeNick(pl, old_nick);
    }
    if( pkt_pl.has_ready() ) {
      //XXX check whether change is valid?
      pl->setReady(pkt_pl.ready());
      observer_.onPlayerReady(pl);
    }
    if( pkt_pl.has_field_conf() ) {
      FieldConf conf;
      conf.fromPacket(pkt_pl.field_conf());
      pl->setFieldConf(conf);
      observer_.onPlayerChangeFieldConf(pl);
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
#define SERVER_CONF_EXPR_PKT(n,ini) \
    conf_.n = np_conf.n();
    SERVER_CONF_APPLY(SERVER_CONF_EXPR_PKT);
#undef SERVER_CONF_EXPR_PKT
    google::protobuf::RepeatedPtrField<netplay::FieldConf> np_fcs = np_conf.field_confs();
    google::protobuf::RepeatedPtrField<netplay::FieldConf>::const_iterator fcit;
    conf_.field_confs.clear();
    for( fcit=np_fcs.begin(); fcit!=np_fcs.end(); ++fcit ) {
      if( !fcit->has_name() || fcit->name().empty() ) {
        throw netplay::CallbackError("unnamed server field configuration");
      }
      conf_.field_confs[fcit->name()].fromPacket(*fcit);
    }
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
      observer_.onStateChange(state_);
    } else if( new_state == STATE_READY ) {
      state_ = new_state;
      // init fields for match
      match_.start();
      observer_.onStateChange(state_);
    } else if( new_state == STATE_GAME ) {
      state_ = new_state;
      observer_.onStateChange(state_);
    } else if( new_state == STATE_LOBBY ) {
      this->stopMatch();
    }
  }
}


void ClientInstance::stopMatch()
{
  LOG("stop match");

  PlayerContainer::iterator it;
  for(it=players_.begin(); it!=players_.end(); ++it) {
    (*it).second->setField(NULL);
  }
  match_.stop();
  state_ = STATE_LOBBY;
  observer_.onStateChange(state_);
}

