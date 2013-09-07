#include <boost/asio/placeholders.hpp>
#include "client.h"
#include "game.h"
#include "log.h"

namespace asio = boost::asio;
using namespace asio::ip;


ClientInstance::ClientInstance(Observer& obs, asio::io_service& io_service):
    observer_(obs), socket_(*this, io_service)
{
}

ClientInstance::~ClientInstance()
{
}

void ClientInstance::connect(const char* host, int port, int tout)
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

void ClientInstance::newLocalPlayer(const std::string& nick)
{
  netplay::Packet pkt;
  netplay::PktPlayerJoin* np_join = pkt.mutable_player_join();
  np_join->set_nick(nick);
  socket_.writePacket(pkt);
}


void ClientInstance::playerSetNick(Player* pl, const std::string& nick)
{
  assert( pl->local() );
  assert( state_ == STATE_LOBBY && !pl->ready() );
  if( nick == pl->nick() ) {
    return; // nothing to do
  }
  pl->setNick(nick);

  netplay::Packet pkt;
  netplay::PktPlayerConf* np_conf = pkt.mutable_player_conf();
  np_conf->set_plid(pl->plid());
  np_conf->set_nick(nick);
  socket_.writePacket(pkt);
}

void ClientInstance::playerSetFieldConf(Player* pl, const FieldConf& conf, const std::string& name)
{
  assert( pl->local() );
  assert( state_ == STATE_LOBBY && !pl->ready() );
  //TODO compare with current conf/name

  netplay::Packet pkt;
  netplay::PktPlayerConf* np_conf = pkt.mutable_player_conf();
  np_conf->set_plid(pl->plid());
  netplay::FieldConf* np_fc = np_conf->mutable_field_conf();
  np_fc->set_name(name);
  conf.toPacket(np_fc);
  socket_.writePacket(pkt);
}

void ClientInstance::playerSetReady(Player* pl, bool ready)
{
  assert( pl->local() );
  assert( state_ == STATE_LOBBY || state_ == STATE_READY );
  if( pl->ready() == ready ) {
    return; // already ready, do nothing
  }
  pl->setReady(ready);

  netplay::Packet pkt;
  netplay::PktPlayerState* np_state = pkt.mutable_player_state();
  np_state->set_plid(pl->plid());
  np_state->set_state(netplay::PktPlayerState::READY);
  socket_.writePacket(pkt);
}

void ClientInstance::playerSendChat(Player* pl, const std::string& msg)
{
  assert( pl->local() );

  netplay::Packet pkt;
  netplay::PktChat* np_chat = pkt.mutable_chat();
  np_chat->set_plid(pl->plid());
  np_chat->set_txt(msg);
  socket_.writePacket(pkt);
}

void ClientInstance::playerStep(Player* pl, KeyState keys)
{
  assert( pl->local() && pl->field() != NULL );
  Tick tk = pl->field()->tick();
  this->doStepPlayer(pl, keys);

  // send packet
  netplay::Packet pkt;
  netplay::PktInput* np_input = pkt.mutable_input();
  np_input->set_plid(pl->plid());
  np_input->set_tick(tk);
  np_input->add_keys(keys);
  socket_.writePacket(pkt);
}

void ClientInstance::playerQuit(Player* pl)
{
  assert( pl->local() );

  netplay::Packet pkt;
  netplay::PktPlayerState* np_state = pkt.mutable_player_state();
  np_state->set_plid(pl->plid());
  np_state->set_state(netplay::PktPlayerState::LEAVE);
  socket_.writePacket(pkt);
}


void ClientInstance::onClientPacket(const netplay::Packet& pkt)
{
  if( pkt.has_input() ) {
    this->processPktInput(pkt.input());
  } else if( pkt.has_new_garbage() ) {
    this->processPktNewGarbage(pkt.new_garbage());
  } else if( pkt.has_update_garbage() ) {
    this->processPktUpdateGarbage(pkt.update_garbage());
  } else if( pkt.has_garbage_state() ) {
    this->processPktGarbageState(pkt.garbage_state());
  } else if( pkt.has_server_conf() ) {
    this->processPktServerConf(pkt.server_conf());
  } else if( pkt.has_server_state() ) {
    this->processPktServerState(pkt.server_state());
  } else if( pkt.has_player_conf() ) {
    this->processPktPlayerConf(pkt.player_conf());
  } else if( pkt.has_player_state() ) {
    this->processPktPlayerState(pkt.player_state());
  } else if( pkt.has_player_rank() ) {
    this->processPktPlayerRank(pkt.player_rank());
  } else if( pkt.has_player_field() ) {
    this->processPktPlayerField(pkt.player_field());

  } else if( pkt.has_chat() ) {
    const netplay::PktChat& np_chat = pkt.chat();
    Player* pl = this->player(np_chat.plid());
    if( pl == NULL ) {
      throw netplay::CallbackError("invalid player");
    }
    observer_.onChat(pl, np_chat.txt());

  } else if( pkt.has_notification() ) {
    const netplay::PktNotification& np_notification = pkt.notification();
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


void ClientInstance::processPktInput(const netplay::PktInput& pkt)
{
  if( state_ != STATE_GAME ) {
    throw netplay::CallbackError("match is not running");
  }
  Player* pl = this->player(pkt.plid());
  if( pl == NULL || pl->field() == NULL ) {
    throw netplay::CallbackError("invalid player");
  }
  if( pl->local() ) {
    return; // already processed
  }
  Field* fld = pl->field();

  Tick tick = pkt.tick();
  if( tick < fld->tick() ) {
    throw netplay::CallbackError("input tick in the past");
  }
  // skipped frames
  while( fld->tick() < tick ) {
    this->stepRemotePlayer(pl, 0);
  }
  // provided frames
  const int keys_nb = pkt.keys_size();
  for( int i=0; i<keys_nb; i++ ) {
    this->stepRemotePlayer(pl, pkt.keys(i));
  }
}

void ClientInstance::processPktNewGarbage(const netplay::PktNewGarbage& pkt)
{
  if( state_ != STATE_GAME ) {
    throw netplay::CallbackError("match is not running");
  }

  std::unique_ptr<Garbage> gb(new Garbage);
  gb->gbid = pkt.gbid();

  Player* pl_to = this->player(pkt.plid_to());
  if( pl_to == NULL || pl_to->field() == NULL ) {
    throw netplay::CallbackError("invalid garbage target");
  }
  gb->to = pl_to->field();

  if( pkt.has_plid_from() ) {
    Player* pl_from = this->player(pkt.plid_from());
    if( pl_from == NULL || pl_from->field() == NULL ) {
      throw netplay::CallbackError("invalid garbage origin");
    }
    gb->from = pl_from->field();
  } else {
    gb->from = NULL;
  }

  gb->type = static_cast<Garbage::Type>(pkt.type());
  if( pkt.size() == 0 ) {
    throw netplay::CallbackError("invalid garbage size");
  }
  if( gb->type == Garbage::TYPE_CHAIN ) {
    gb->size = FieldPos(FIELD_WIDTH, pkt.size());
  } else if( gb->type == Garbage::TYPE_COMBO ) {
    gb->size = FieldPos(pkt.size(), 1);
  } else {
    assert( !"not supported yet" );
  }

  if( pkt.pos() > gb->to->hangingGarbageCount() ) {
    throw netplay::CallbackError("invalid garbage position");
  }
  match_.addGarbage(gb.release(), pkt.pos());
}

void ClientInstance::processPktUpdateGarbage(const netplay::PktUpdateGarbage& pkt)
{
  if( state_ != STATE_GAME ) {
    throw netplay::CallbackError("match is not running");
  }
  const Match::GarbageMap gbs_hang = match_.hangingGarbages();
  Match::GarbageMap::const_iterator it = gbs_hang.find(pkt.gbid());
  if( it == gbs_hang.end() ) {
    throw netplay::CallbackError("garbage not found");
  }
  Garbage* gb = (*it).second;

  if( pkt.has_pos() ) {
    Player* pl_to = this->player(gb->to);
    assert( pl_to != NULL );

    if( pkt.has_plid_to() && pkt.plid_to() != pl_to->plid() ) {
      // actual target change
      pl_to = this->player(pkt.plid_to());
      if( pl_to != NULL && pl_to->field() != NULL ) {
        throw netplay::CallbackError("invalid garbage target");
      }
    }
    if( pkt.pos() > pl_to->field()->hangingGarbageCount() ) {
      throw netplay::CallbackError("invalid garbage position");
    }
    gb->to->removeHangingGarbage(gb);
    gb->to = pl_to->field(); // no-op if plid_to did not changed
    gb->to->insertHangingGarbage(gb, pkt.pos());
  } else if( pkt.has_plid_to() ) {
    throw netplay::CallbackError("garbage position missing");
  }

  if( pkt.has_size() ) {
    if( pkt.size() == 0 ) {
      throw netplay::CallbackError("invalid garbage size");
    }
    if( gb->type == Garbage::TYPE_CHAIN ) {
      gb->size = FieldPos(FIELD_WIDTH, pkt.size());
    } else if( gb->type == Garbage::TYPE_COMBO ) {
      gb->size = FieldPos(pkt.size(), 1);
    } else {
      assert( !"not supported yet" );
    }
  }
}

void ClientInstance::processPktGarbageState(const netplay::PktGarbageState& pkt)
{
  if( state_ != STATE_GAME ) {
    throw netplay::CallbackError("match is not running");
  }

  netplay::PktGarbageState::State state = pkt.state();

  if( state == netplay::PktGarbageState::WAIT ) {
    // garbage from hanging to wait
    const Match::GarbageMap gbs_hang = match_.hangingGarbages();
    Match::GarbageMap::const_iterator it = gbs_hang.find(pkt.gbid());
    if( it == gbs_hang.end() ) {
      throw netplay::CallbackError("garbage not found");
    }
    Garbage* gb = (*it).second;
    match_.waitGarbageDrop(gb);
    Player* pl = this->player(gb->to);
    if( pl->local() ) {
      // one of our garbages, drop it
      netplay::Packet pkt_send;
      netplay::PktGarbageState* np_state = pkt_send.mutable_garbage_state();
      np_state->set_gbid( gb->gbid );
      np_state->set_state(netplay::PktGarbageState::DROP);
      socket_.writePacket(pkt_send);
      gb->to->dropNextGarbage();
    }

  } else if( state == netplay::PktGarbageState::DROP ) {
    // drop garbage
    const Match::GarbageMap gbs_wait = match_.waitingGarbages();
    Match::GarbageMap::const_iterator it = gbs_wait.find(pkt.gbid());
    if( it == gbs_wait.end() ) {
      throw netplay::CallbackError("garbage not found");
    }
    Garbage* gb = (*it).second;
    Field* fld = gb->to;
    Player* pl = this->player(fld);
    if( pl == NULL ) {
      throw netplay::CallbackError("invalid player");
    }
    if( !pl->local() ) { // ignore our garbages (already dropped)
      if( fld->waitingGarbages().size() == 0 || fld->waitingGarbages().front().gbid != gb->gbid ) {
        throw netplay::CallbackError("invalid dropped garbage");
      }
      pl->field()->dropNextGarbage();
    }
  }
}

void ClientInstance::processPktServerConf(const netplay::PktServerConf& pkt)
{
  if( state_ != STATE_LOBBY ) {
    throw netplay::CallbackError("invalid in current state");
  }
#define SERVER_CONF_EXPR_PKT(n,ini) \
  conf_.n = pkt.n();
  SERVER_CONF_APPLY(SERVER_CONF_EXPR_PKT);
#undef SERVER_CONF_EXPR_PKT
  google::protobuf::RepeatedPtrField<netplay::FieldConf> np_fcs = pkt.field_confs();
  if( np_fcs.size() > 0 ) {
    google::protobuf::RepeatedPtrField<netplay::FieldConf>::const_iterator fcit;
    conf_.field_confs.clear();
    for( fcit=np_fcs.begin(); fcit!=np_fcs.end(); ++fcit ) {
      if( fcit->name().empty() ) {
        throw netplay::CallbackError("unnamed server field configuration");
      }
      conf_.field_confs[fcit->name()].fromPacket(*fcit);
    }
  }
  // even when np_fcs.size() > 0 to handle the init case (first conf packet)
  if( conf_.field_confs.size() == 0 ) {
    throw netplay::CallbackError("no field configuration");
  }
}

void ClientInstance::processPktServerState(const netplay::PktServerState& pkt)
{
  //TODO check whether state change is valid
  State new_state = static_cast<State>(pkt.state());
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

void ClientInstance::processPktPlayerConf(const netplay::PktPlayerConf& pkt)
{
  Player* pl = this->player(pkt.plid());
  if( pl == NULL ) {
    // new player
    if( !pkt.has_nick() || !pkt.has_field_conf() ) {
      throw netplay::CallbackError("missing fields");
    }
    //TODO check we asked for a new local player
    pl = new Player(pkt.plid(), pkt.join());
    pl->setNick( pkt.nick() );
    PlId plid = pl->plid(); // use a temporary value to help g++
    players_.insert(plid, pl);
    FieldConf conf;
    conf.fromPacket(pkt.field_conf());
    pl->setFieldConf(conf, pkt.field_conf().name());
    observer_.onPlayerJoined(pl);
  } else {
    // update player
    if( pkt.has_nick() ) {
      const std::string old_nick = pl->nick();
      pl->setNick( pkt.nick() );
      observer_.onPlayerChangeNick(pl, old_nick);
    }
    if( pkt.has_field_conf() ) {
      const std::string name = pkt.field_conf().name();
      if( name.empty() ) {
        FieldConf conf;
        conf.fromPacket(pkt.field_conf());
        pl->setFieldConf(conf, name);
      } else {
        auto fc_it = conf_.field_confs.find(name);
        if( fc_it == conf_.field_confs.end() ) {
          throw netplay::CallbackError("invalid configuration name: "+name);
        }
        pl->setFieldConf(fc_it->second, name);
      }
      observer_.onPlayerChangeFieldConf(pl);
    }
  }
}

void ClientInstance::processPktPlayerState(const netplay::PktPlayerState& pkt)
{
  Player* pl = this->player(pkt.plid());
  if( pl == NULL ) {
    throw netplay::CallbackError("invalid player");
  }

  netplay::PktPlayerState::State state = pkt.state();
  if( state == netplay::PktPlayerState::LEAVE ) {
    observer_.onPlayerQuit(pl);
    if( pl->field() != NULL ) {
      match_.removeField(pl->field());
      pl->setField(NULL);
    }
    players_.erase(pl->plid());
  } else if( state == netplay::PktPlayerState::PREPARE ) {
    if( state_ != STATE_LOBBY ) {
      throw netplay::CallbackError("invalid in current state");
    }
    if( pl->ready() ) {
      pl->setReady(false);
      observer_.onPlayerReady(pl);
    }
  } else if( state == netplay::PktPlayerState::READY ) {
    if( !pl->ready() ) {
      pl->setReady(true);
      observer_.onPlayerReady(pl);
    }
  }
}

void ClientInstance::processPktPlayerRank(const netplay::PktPlayerRank& pkt)
{
  if( state_ != STATE_GAME ) {
    throw netplay::CallbackError("invalid in current state");
  }
  Player* pl = this->player(pkt.plid());
  if( pl == NULL || pl->field() == NULL ) {
    throw netplay::CallbackError("invalid player");
  }
  pl->field()->setRank(pkt.rank()); //TODO may fail if already set
}

void ClientInstance::processPktPlayerField(const netplay::PktPlayerField& pkt)
{
  //XXX support sending of current game fields to new clients
  if( state_ != STATE_INIT ) {
    throw netplay::CallbackError("invalid in current state");
  }
  Player* pl = this->player(pkt.plid());
  if( pl == NULL ) {
    throw netplay::CallbackError("invalid player");
  }
  if( pl->field() != NULL ) {
    throw netplay::CallbackError("field already initialized");
  }

  Field* fld = match_.newField(pl->fieldConf(), pkt.seed());
  pl->setField(fld);
  if( pkt.blocks_size() > 0 ) {
    //TODO throw exceptions in setGridContentFromPacket
    fld->setGridContentFromPacket(pkt.blocks());
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

