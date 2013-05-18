#include "server.h"
#include "netplay.pb.h"
#include "inifile.h"
#include "log.h"



const std::string ServerInstance::CONF_SECTION("Server");

//XXX:temp
static const FieldConf default_field_conf = {
  /* swap_tk        */   3,
  /* raise_tk       */ 500,
  /* raise_steps    */  16,
  /* stop_combo_0   */  42,
  /* stop_combo_k   */  10,
  /* stop_chain_0   */  85,
  /* stop_chain_k   */  10,
  /* lost_tk        */  60,
  /* gb_hang_tk     */  90,
  /* flash_tk       */  36,
  /* levitate_tk    */  12,
  /* pop_tk         */   8,
  /* pop0_tk        */  19,
  /* transform_tk   */  14,
  /* color_nb       */   6,
  /* raise_adjacent */ FieldConf::ADJACENT_ALWAYS,
};


ServerInstance::ServerInstance(Observer& obs, boost::asio::io_service& io_service):
    observer_(obs), socket_(*this, io_service), gb_distributor_(match_, *this),
    current_plid_(0)
{
}

ServerInstance::~ServerInstance()
{
}

void ServerInstance::loadConf(const IniFile& cfg)
{
  assert( ! socket_.started() );
  //XXX signed/unsigned and type boundaries not checked
#define SERVER_CONF_EXPR_LOAD(n,ini) \
  conf_.n = cfg.get(CONF_SECTION, #ini, conf_.n);
  SERVER_CONF_APPLY(SERVER_CONF_EXPR_LOAD);
#undef SERVER_CONF_EXPR_LOAD

  conf_.field_confs.clear();
  const std::string s_conf = cfg.get(CONF_SECTION, "FieldConfsList", "");
  if( !s_conf.empty() ) {
    size_t pos = 0;
    for(;;) {
      size_t pos2 = s_conf.find(',', pos);
      const std::string name = s_conf.substr(pos, pos2-pos);
      if( name.empty() ) {
        throw std::runtime_error("empty field configuration name");
      }
      const std::string field_conf_section = CONF_SECTION+".FieldConfs"+name;
      FieldConf* fc = &conf_.field_confs[name];
#define FIELD_CONF_EXPR_INIT(n,ini) \
      fc->n = cfg.get<decltype(fc->n)>(field_conf_section, #ini);
      FIELD_CONF_APPLY(FIELD_CONF_EXPR_INIT);
#undef FIELD_CONF_EXPR_INIT
      const std::string s_raise_adjacent = cfg.get<std::string>(field_conf_section, "RaiseAdjacent");
      if( s_raise_adjacent == "never" ) {
        fc->raise_adjacent = FieldConf::ADJACENT_NEVER;
      } else if( s_raise_adjacent == "always" ) {
        fc->raise_adjacent = FieldConf::ADJACENT_ALWAYS;
      } else if( s_raise_adjacent == "alternate" ) {
        fc->raise_adjacent = FieldConf::ADJACENT_ALTERNATE;
      } else {
        throw std::runtime_error("invalid RaiseAdjacent value: "+s_raise_adjacent);
      }
      if( !fc->isValid() ) {
        throw std::runtime_error("invalid configuration: "+name);
      }

      if( pos2 == std::string::npos ) {
        break;
      }
      pos = pos2;
    }
  }

  //TODO:temp
  if( conf_.field_confs.size() < 1 ) {
    conf_.field_confs["default"] = default_field_conf;
  }
}

void ServerInstance::startServer(int port)
{
  assert( state_ == STATE_NONE );
  LOG("starting server on port %d", port);
  socket_.start(port);
  state_ = STATE_LOBBY;
}

void ServerInstance::stopServer()
{
  state_ = STATE_NONE;
  socket_.close();
}


Player* ServerInstance::newLocalPlayer(const std::string& nick)
{
  return this->newPlayer(NULL, nick);
}


void ServerInstance::playerSetNick(Player* pl, const std::string& nick)
{
  assert( pl->local() );
  assert( state_ == STATE_LOBBY && !pl->ready() );
  if( nick == pl->nick() ) {
    return; // nothing to do
  }
  const std::string old_nick = pl->nick();
  pl->setNick(nick);
  observer_.onPlayerChangeNick(pl, old_nick);

  netplay::Packet pkt;
  netplay::PktPlayerConf* np_plconf = pkt.mutable_player_conf();
  np_plconf->set_plid(pl->plid());
  np_plconf->set_nick(nick);
  socket_.broadcastPacket(pkt);
}

void ServerInstance::playerSetFieldConf(Player* pl, const FieldConf& conf, const std::string& name)
{
  assert( pl->local() );
  assert( state_ == STATE_LOBBY && !pl->ready() );
  pl->setFieldConf(conf, name);

  netplay::Packet pkt;
  netplay::PktPlayerConf* np_conf = pkt.mutable_player_conf();
  np_conf->set_plid(pl->plid());
  netplay::FieldConf* np_fc = np_conf->mutable_field_conf();
  np_fc->set_name(name);
  conf.toPacket(np_fc);
  socket_.broadcastPacket(pkt);
}

void ServerInstance::playerSetReady(Player* pl, bool ready)
{
  assert( pl->local() );
  assert( state_ == STATE_LOBBY || state_ == STATE_READY );
  if( pl->ready() == ready ) {
    return; // already ready, do nothing
  }
  pl->setReady(ready);
  observer_.onPlayerReady(pl);

  netplay::Packet pkt;
  netplay::PktPlayerState* np_state = pkt.mutable_player_state();
  np_state->set_plid(pl->plid());
  np_state->set_state(netplay::PktPlayerState::READY);
  socket_.broadcastPacket(pkt);

  this->checkAllPlayersReady();
}

void ServerInstance::playerSendChat(Player* pl, const std::string& msg)
{
  assert( pl->local() );
  observer_.onChat(pl, msg);

  netplay::Packet pkt;
  netplay::PktChat* np_chat = pkt.mutable_chat();
  np_chat->set_plid(pl->plid());
  np_chat->set_txt(msg);
  socket_.broadcastPacket(pkt);
}

void ServerInstance::playerStep(Player* pl, KeyState keys)
{
  assert( pl->local() && pl->field() != NULL );
  this->doStepPlayer(pl, keys);
}

void ServerInstance::playerQuit(Player* pl)
{
  assert( pl->local() );
  this->removePlayer(pl->plid());
}


void ServerInstance::onPeerConnect(netplay::PeerSocket* peer)
{
  if( state_ != STATE_LOBBY ) {
    throw netplay::CallbackError("match is running");
  } else if( players_.size() >= conf_.pl_nb_max ) {
    //TODO difference between player max en peer max
    throw netplay::CallbackError("server full");
  }
  LOG("peer connected");

  netplay::Packet pkt;

  // send server info
  netplay::PktServerConf* np_conf = pkt.mutable_server_conf();
#define SERVER_CONF_EXPR_PKT(n,ini) \
  np_conf->set_##n(conf_.n);
  SERVER_CONF_APPLY(SERVER_CONF_EXPR_PKT);
#undef SERVER_CONF_EXPR_PKT
  google::protobuf::RepeatedPtrField<netplay::FieldConf>* np_fcs = np_conf->mutable_field_confs();
  np_fcs->Reserve(conf_.field_confs.size());
  std::map<std::string, FieldConf>::const_iterator fcit;
  for( fcit=conf_.field_confs.begin(); fcit!=conf_.field_confs.end(); ++fcit ) {
    netplay::FieldConf* np_fc = np_fcs->Add();
    np_fc->set_name( (*fcit).first );
    (*fcit).second.toPacket(np_fc);
  }
  peer->writePacket(pkt);
  pkt.clear_server_conf();

  netplay::PktServerState* np_state = pkt.mutable_server_state();
  np_state->set_state( static_cast<netplay::PktServerState::State>(state_) );
  peer->writePacket(pkt);
  pkt.clear_server_state();

  // send information on other players
  PlayerContainer::const_iterator it;
  for( it=players_.begin(); it!=players_.end(); ++it ) {
    const Player* pl = (*it).second;
    netplay::PktPlayerConf* np_plconf = pkt.mutable_player_conf();
    np_plconf->set_plid( pl->plid() );
    np_plconf->set_nick( pl->nick() );
    netplay::FieldConf* np_fc = np_plconf->mutable_field_conf();
    pl->fieldConf().toPacket(np_fc);
    peer->writePacket(pkt);
    pkt.clear_player_conf();

    if( pl->ready() ) {
      netplay::PktPlayerState* np_state = pkt.mutable_player_state();
      np_state->set_plid( pl->plid() );
      np_state->set_state(netplay::PktPlayerState::READY);
      peer->writePacket(pkt);
      pkt.clear_player_state();
    }
  }

  // set read handler
  peer->readNext();
}


void ServerInstance::onPeerDisconnect(netplay::PeerSocket* peer)
{
  PeerContainer::const_iterator it;
  for(it=peers_.begin(); it!=peers_.end(); ) {
    if( (*it).second == peer ) {
      PlId plid = (*it).first;
      ++it; // increment to avoid invalidated iterator
      this->removePlayer(plid);
    } else {
      ++it;
    }
  }
}

void ServerInstance::onPeerPacket(netplay::PeerSocket* peer, const netplay::Packet& pkt)
{
  if( pkt.has_input() ) {
    this->processPktInput(peer, pkt.input());
  } else if( pkt.has_garbage_state() ) {
    this->processPktGarbageState(peer, pkt.garbage_state());
  } else if( pkt.has_player_join() ) {
    this->processPktPlayerJoin(peer, pkt.player_join());
  } else if( pkt.has_player_conf() ) {
    this->processPktPlayerConf(peer, pkt.player_conf());
  } else if( pkt.has_player_state() ) {
    this->processPktPlayerState(peer, pkt.player_state());
  } else if( pkt.has_chat() ) {
    const netplay::PktChat& pkt_chat = pkt.chat();
    Player* pl = this->checkPeerPlayer(pkt_chat.plid(), peer);
    observer_.onChat(pl, pkt_chat.txt());
    netplay::Packet pkt_send;
    pkt_send.mutable_chat()->MergeFrom(pkt_chat);
    socket_.broadcastPacket(pkt_send);

  } else {
    throw netplay::CallbackError("invalid packet field");
  }
}


void ServerInstance::onGarbageAdd(const Garbage* gb, unsigned int pos)
{
  Player* pl_to = this->player(gb->to);
  Player* pl_from = this->player(gb->from);
  assert( pl_to != NULL );

  netplay::Packet pkt;
  netplay::PktNewGarbage* np_garbage = pkt.mutable_new_garbage();
  np_garbage->set_gbid( gb->gbid );
  np_garbage->set_pos( pos );
  np_garbage->set_plid_to( pl_to->plid() );
  np_garbage->set_plid_from( pl_from == NULL ? 0 : pl_from->plid() );
  np_garbage->set_type( static_cast<netplay::GarbageType>(gb->type) );
  np_garbage->set_size( gb->type == Garbage::TYPE_COMBO ? gb->size.x : gb->size.y );
  socket_.broadcastPacket(pkt);
}

void ServerInstance::onGarbageUpdateSize(const Garbage* gb)
{
  netplay::Packet pkt;
  netplay::PktUpdateGarbage* np_garbage = pkt.mutable_update_garbage();
  np_garbage->set_gbid( gb->gbid );
  np_garbage->set_size( gb->type == Garbage::TYPE_COMBO ? gb->size.x : gb->size.y );
  socket_.broadcastPacket(pkt);
}

void ServerInstance::onGarbageDrop(const Garbage* gb)
{
  Player* pl_to = this->player(gb->to);
  assert( pl_to != NULL );

  netplay::Packet pkt;
  netplay::PktGarbageState* np_state = pkt.mutable_garbage_state();
  np_state->set_gbid(gb->gbid);
  np_state->set_state(netplay::PktGarbageState::WAIT);
  match_.waitGarbageDrop(gb);
  socket_.broadcastPacket(pkt);

  // local player: drop immediately
  if( pl_to->local() ) {
    gb->to->dropNextGarbage();
    np_state->set_state(netplay::PktGarbageState::DROP);
    socket_.broadcastPacket(pkt);
  }
}


Player* ServerInstance::newPlayer(netplay::PeerSocket* peer, const std::string& nick)
{
  //TODO protected pointer
  Player* pl = new Player(this->nextPlayerId(), peer==NULL);
  LOG("init player: %d", pl->plid());
  pl->setNick(nick);
  auto fc_it = conf_.field_confs.begin(); //TODO:temp default configuration
  pl->setFieldConf(fc_it->second, fc_it->first);
  // put accepted player with his friends
  PlId plid = pl->plid(); // use a temporary value to help g++
  players_.insert(plid, pl);
  if( peer != NULL ) {
    peers_[plid] = peer; // associate the player to its peer
  }
  observer_.onPlayerJoined(pl);

  // inform the new player about his ID (if remote)
  // tell others about their new friend
  netplay::Packet pkt;
  netplay::PktPlayerConf* np_conf = pkt.mutable_player_conf();
  np_conf->set_plid(pl->plid());
  np_conf->set_nick(pl->nick());
  netplay::FieldConf* np_fc = np_conf->mutable_field_conf();
  pl->fieldConf().toPacket(np_fc);
  socket_.broadcastPacket(pkt, peer);
  if( peer != NULL ) {
    np_conf->set_join(true);
    peer->writePacket(pkt);
  }

  return pl;
}

void ServerInstance::removePlayer(PlId plid)
{
  Player* pl = this->player(plid);
  assert( pl != NULL );
  observer_.onPlayerQuit(pl);
  players_.erase(plid);
  peers_.erase(plid);
  // tell other players
  netplay::Packet pkt;
  netplay::PktPlayerState* np_state = pkt.mutable_player_state();
  np_state->set_plid(plid);
  np_state->set_state(netplay::PktPlayerState::LEAVE);
  socket_.broadcastPacket(pkt);
  // only one player left: abort the game
  if( players_.size() < 2 ) {
    this->stopMatch();
  }
}


void ServerInstance::processPktInput(netplay::PeerSocket* peer, const netplay::PktInput& pkt)
{
  if( state_ != STATE_GAME ) {
    return;  // ignore remains of the previous match
  }
  Player* pl = this->checkPeerPlayer(pkt.plid(), peer);
  //XXX:check field may have win without player knowing it
  // this must not trigger an error
  Field* fld = pl->field();
  if( fld == NULL ) {
    throw netplay::CallbackError("player without a field");
  }

  Tick tick = pkt.tick();
  if( tick < fld->tick() ) {
    throw netplay::CallbackError("input tick in the past");
  }
  // skipped frames
  while( fld->tick() < tick ) {
    this->stepRemotePlayer(pl, 0);
    if( !match_.started() ) {
      return; // end of match
    }
  }
  // provided frames
  const int keys_nb = pkt.keys_size();
  for( int i=0; i<keys_nb; i++ ) {
    this->stepRemotePlayer(pl, pkt.keys(i));
    if( !match_.started() ) {
      return; // end of match
    }
  }
}

void ServerInstance::processPktGarbageState(netplay::PeerSocket* peer, const netplay::PktGarbageState& pkt)
{
  if( state_ != STATE_GAME ) {
    throw netplay::CallbackError("match is not running");
  }
  if( pkt.state() != netplay::PktGarbageState::DROP ) {
    throw netplay::CallbackError("unexpected garbage state");
  }

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
  this->checkPeerPlayer(pl->plid(), peer);
  if( fld->waitingGarbages().size() == 0 || fld->waitingGarbages().front().gbid != gb->gbid ) {
    throw netplay::CallbackError("invalid dropped garbage");
  }

  netplay::Packet pkt_send;
  netplay::PktGarbageState* np_state = pkt_send.mutable_garbage_state();
  np_state->set_gbid(gb->gbid);
  np_state->set_state(netplay::PktGarbageState::DROP);
  socket_.broadcastPacket(pkt_send);

  fld->dropNextGarbage();
}

void ServerInstance::processPktPlayerJoin(netplay::PeerSocket* peer, const netplay::PktPlayerJoin& pkt)
{
  if( state_ != STATE_LOBBY ) {
    throw netplay::CallbackError("match is running");
  } else if( players_.size() > conf_.pl_nb_max ) {
    //TODO don't close the connection if the server is full
    // only send back an error
    throw netplay::CallbackError("server full");
  }
  this->newPlayer(peer, pkt.has_nick() ? pkt.nick() : "Player");
}

void ServerInstance::processPktPlayerConf(netplay::PeerSocket* peer, const netplay::PktPlayerConf& pkt)
{
  if( state_ != STATE_LOBBY ) {
    throw netplay::CallbackError("match is running");
  }
  netplay::Packet pkt_send;
  netplay::PktPlayerConf* np_plconf = pkt_send.mutable_player_conf();

  Player* pl = this->checkPeerPlayer(pkt.plid(), peer);
  if( pl->ready() ) {
    throw netplay::CallbackError("invalid when player is ready");
  }
  bool do_send = false;
  if( pkt.has_nick() && pkt.nick() != pl->nick() ) {
    const std::string old_nick = pl->nick();
    pl->setNick( pkt.nick() );
    observer_.onPlayerChangeNick(pl, old_nick);
    np_plconf->set_nick( pkt.nick() );
    do_send = true;
  }
  if( pkt.has_field_conf() ) {
    const std::string name = pkt.field_conf().name();
    //TODO compare with previous conf/name, ignore if not changed
    do_send = true;
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
    if( do_send ) {
      netplay::FieldConf* np_fc = np_plconf->mutable_field_conf();
      np_fc->set_name(name);
      pl->fieldConf().toPacket(np_fc);
      observer_.onPlayerChangeFieldConf(pl);
    }
  }
  if( do_send ) {
    np_plconf->set_plid( pl->plid() );
    socket_.broadcastPacket(pkt_send);
  }
}

void ServerInstance::processPktPlayerState(netplay::PeerSocket* peer, const netplay::PktPlayerState& pkt)
{
  Player* pl = this->checkPeerPlayer(pkt.plid(), peer);

  bool do_send = false;
  netplay::PktPlayerState::State state = pkt.state();
  // Invalid state changes are silently ignored

  if( state == netplay::PktPlayerState::LEAVE ) {
    do_send = true;
    this->removePlayer(pl->plid()); // note: don't disconnect the peer
  } else if( state == netplay::PktPlayerState::PREPARE ) {
    if( state_ == STATE_LOBBY && pl->ready() ) {
      pl->setReady(false);
      do_send = true;
    }
  } else if( state == netplay::PktPlayerState::READY ) {
    if( !pl->ready() ) {
      pl->setReady(true);
      do_send = true;
    }
  }

  if( do_send ) {
    netplay::Packet pkt_send;
    *pkt_send.mutable_player_state() = pkt;
    socket_.broadcastPacket(pkt_send);
  }
  if( do_send && state == netplay::PktPlayerState::READY ) {
    this->checkAllPlayersReady();
  }
}


Player* ServerInstance::checkPeerPlayer(PlId plid, const netplay::PeerSocket* peer)
{
  PeerContainer::const_iterator peer_it = peers_.find(plid);
  if( peer_it == peers_.end() || (*peer_it).second != peer ) {
    throw netplay::CallbackError("invalid player");
  }
  return this->player(plid);
}


void ServerInstance::checkAllPlayersReady()
{
  unsigned int nb_ready = 0;
  PlayerContainer::const_iterator it;
  for( it=players_.begin(); it!=players_.end(); ++it ) {
    if( (*it).second->ready() ) {
      nb_ready++;
    }
  }
  if( nb_ready == conf_.pl_nb_max ) {
    if( state_ == STATE_LOBBY ) {
      this->prepareMatch();
    } else if( state_ == STATE_READY ) {
      this->startMatch();
    }
  }
}


void ServerInstance::setState(State state)
{
  netplay::Packet pkt;
  netplay::PktServerState* np_state = pkt.mutable_server_state();
  np_state->set_state( static_cast<netplay::PktServerState::State>(state) );
  socket_.broadcastPacket(pkt);

  if( state != STATE_INIT ) {
    // ready_ flag is needed in prepareMatch(), don't reset it now
    PlayerContainer::iterator it;
    for( it=players_.begin(); it!=players_.end(); ++it ) {
      (*it).second->setReady(false);
    }
  }
  state_ = state;
  observer_.onStateChange(state);
}

void ServerInstance::prepareMatch()
{
  LOG("prepare match");
  //XXX check ready player count (there should be at least 1 player)

  this->setState(STATE_INIT);

  int seed = ::rand(); // common seed for all fields
  netplay::Packet pkt;

  PlayerContainer::iterator it;
  for( it=players_.begin(); it!=players_.end(); ++it ) {
    Player* pl = (*it).second;
    if( ! pl->ready() ) {
      continue;
    }
    Field* fld = match_.newField(pl->fieldConf(), seed);
    pl->setField(fld);
    fld->fillRandom(6);

    netplay::PktPlayerField* np_field = pkt.mutable_player_field();
    np_field->Clear();
    np_field->set_plid(pl->plid());
    np_field->set_seed(fld->seed()); //note: seed changed due to fillRandom()
    fld->setGridContentToPacket(np_field->mutable_blocks());
    socket_.broadcastPacket(pkt);
  }

  this->setState(STATE_READY);
}

void ServerInstance::startMatch()
{
  LOG("start match");

  gb_distributor_.reset();
  match_.start();
  this->setState(STATE_GAME);
}

void ServerInstance::stopMatch()
{
  // ignore if match is already being stopped
  if( state_ == STATE_LOBBY ) {
    return;
  }
  LOG("stop match");

  PlayerContainer::iterator it;
  for(it=players_.begin(); it!=players_.end(); ++it) {
    (*it).second->setField(NULL);
  }
  match_.stop();
  this->setState(STATE_LOBBY);
}


void ServerInstance::doStepPlayer(Player* pl, KeyState keys)
{
  Tick prev_tick = pl->field()->tick();
  GameInstance::doStepPlayer(pl, keys);

  //XXX Current implementation does not group Input packets.
  netplay::Packet pkt_send;
  netplay::PktInput* np_input_send = pkt_send.mutable_input();
  np_input_send->set_plid(pl->plid());
  np_input_send->set_tick(prev_tick);
  np_input_send->add_keys(keys);

  // don't send packet back to remote players
  PeerContainer::const_iterator peer_it = peers_.find(pl->plid());
  netplay::PeerSocket* peer = peer_it == peers_.end() ? NULL : (*peer_it).second;
  socket_.broadcastPacket(pkt_send, peer);
  pkt_send.clear_input(); // packet reused below

  // update garbages
  //TODO check for clients which never send back the drop packets
  gb_distributor_.updateGarbages(pl->field());

  // ranking
  std::vector<const Field*> ranked;
  bool end_of_match = match_.updateRanks(ranked);
  netplay::PktPlayerRank* np_rank = pkt_send.mutable_player_rank();
  std::vector<const Field*>::iterator it;
  for(it=ranked.begin(); it!=ranked.end(); ++it) {
    Player* pl = this->player(*it);
    // don't send packet for aborted fields (no player)
    if( pl != NULL ) {
      np_rank->set_plid( pl->plid() );
      np_rank->set_rank( (*it)->rank() );
      socket_.broadcastPacket(pkt_send);
    }
  }
  if( end_of_match ) {
    this->stopMatch();
  }
}


PlId ServerInstance::nextPlayerId()
{
  for(;;) {
    current_plid_++;
    if( current_plid_ <= 0 ) { // overflow
      current_plid_ = 1;
    }
    if( players_.find(current_plid_) == players_.end() ) {
      break; // not already in use
    }
  }
  return current_plid_;
}


