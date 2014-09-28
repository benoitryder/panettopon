#include <memory>
#include "server.h"
#include "netplay.pb.h"
#include "inifile.h"
#include "log.h"



const std::string ServerInstance::CONF_SECTION("Server");

ServerInstance::ServerInstance(Observer& obs, boost::asio::io_service& io_service):
    observer_(obs), socket_(std::make_shared<netplay::ServerSocket>(*this, io_service)), gb_distributor_(match_, *this),
    current_plid_(0)
{
}

ServerInstance::~ServerInstance()
{
  if(socket_) {
    socket_->close();
  }
}

void ServerInstance::loadConf(const IniFile& cfg)
{
  assert( ! socket_->started() );
  //XXX signed/unsigned and type boundaries not checked
#define SERVER_CONF_EXPR_LOAD(n,ini) \
  conf_.n = cfg.get({CONF_SECTION, #ini}, conf_.n);
  SERVER_CONF_APPLY(SERVER_CONF_EXPR_LOAD);
#undef SERVER_CONF_EXPR_LOAD

  conf_.field_confs.clear();
  const std::string s_conf = cfg.get({CONF_SECTION, "FieldConfsList"}, "");
  if( !s_conf.empty() ) {
    size_t pos = 0;
    for(;;) {
      size_t pos2 = s_conf.find(',', pos);
      const std::string name = s_conf.substr(pos, pos2-pos);
      if( name.empty() ) {
        throw std::runtime_error("empty field configuration name");
      }
      const std::string field_conf_section = IniFile::join("FieldConf", name);
      conf_.field_confs[name].fromIniFile(cfg, field_conf_section);
      if( pos2 == std::string::npos ) {
        break;
      }
      pos = pos2 + 1;
    }
  }

  if(conf_.field_confs.size() < 1) {
    throw std::runtime_error("no field configuration defined");
  }
}

void ServerInstance::startServer(int port)
{
  assert(state_ == State::NONE);
  LOG("starting server on port %d", port);
  socket_->start(port);
  state_ = State::LOBBY;
}

void ServerInstance::stopServer()
{
  state_ = State::NONE;
  socket_->close();
}


Player* ServerInstance::newLocalPlayer(const std::string& nick)
{
  return this->newPlayer(NULL, nick);
}


void ServerInstance::playerSetNick(Player* pl, const std::string& nick)
{
  assert(pl->local());
  assert(pl->state() == Player::State::LOBBY);

  const std::string old_nick = pl->nick();
  if(nick == old_nick) {
    return; // nothing to do
  }
  pl->setNick(nick);
  observer_.onPlayerChangeNick(pl, old_nick);

  netplay::Packet pkt;
  netplay::PktPlayerConf* np_plconf = pkt.mutable_player_conf();
  np_plconf->set_plid(pl->plid());
  np_plconf->set_nick(nick);
  socket_->broadcastPacket(pkt);
}

void ServerInstance::playerSetFieldConf(Player* pl, const FieldConf& conf, const std::string& name)
{
  assert(pl->local());
  assert(pl->state() == Player::State::LOBBY);

  pl->setFieldConf(conf, name);
  observer_.onPlayerChangeFieldConf(pl);

  netplay::Packet pkt;
  netplay::PktPlayerConf* np_conf = pkt.mutable_player_conf();
  np_conf->set_plid(pl->plid());
  netplay::FieldConf* np_fc = np_conf->mutable_field_conf();
  np_fc->set_name(name);
  conf.toPacket(np_fc);
  socket_->broadcastPacket(pkt);
}

void ServerInstance::playerSetState(Player* pl, Player::State state)
{
  assert(pl->local());
  assert(pl->state() != Player::State::QUIT);

  Player::State old_state = pl->state();
  if(state == old_state) {
    return;  // no change
  }

  bool state_valid = false;
  switch(state) {
    case Player::State::QUIT:
      if(pl->field() != NULL) {
        pl->field()->abort();
        match_.updateTick(); // field lost, tick must be updated
        pl->setField(NULL);
      }
      players_.erase(pl->plid());
      state_valid = true;
      //TODO special processing
    case Player::State::LOBBY:
      state_valid = state_ == State::LOBBY || state_ == State::GAME;
      break;
    case Player::State::LOBBY_READY:
      state_valid = state_ == State::LOBBY;
      break;
    case Player::State::GAME_READY:
      state_valid = state_ == State::GAME_READY && old_state == Player::State::GAME_INIT;
      break;
    case Player::State::GAME_INIT:
    case Player::State::GAME:
    default:
      state_valid = false;  // unsettable states
      break;
  }
  assert(state_valid);
  if(!state_valid) {
    LOG("%s(%u): invalid new state: %d", pl->nick().c_str(), pl->plid(), static_cast<int>(state));
    return;
  }

  pl->setState(state);
  LOG("%s(%u): state set to %d", pl->nick().c_str(), pl->plid(), static_cast<int>(state));
  observer_.onPlayerStateChange(pl);

  netplay::Packet pkt;
  netplay::PktPlayerState* np_state = pkt.mutable_player_state();
  np_state->set_plid(pl->plid());
  np_state->set_state(static_cast<netplay::PktPlayerState::State>(state));
  socket_->broadcastPacket(pkt);

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
  socket_->broadcastPacket(pkt);
}

void ServerInstance::playerStep(Player* pl, KeyState keys)
{
  assert( pl->local() && pl->field() != NULL );
  this->doStepPlayer(pl, keys);
}


void ServerInstance::onPeerConnect(netplay::PeerSocket* peer)
{
  if(state_ != State::LOBBY) {
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

    netplay::PktPlayerState* np_state = pkt.mutable_player_state();
    np_state->set_plid(pl->plid());
    np_state->set_state(static_cast<netplay::PktPlayerState::State>(pl->state()));
    peer->writePacket(pkt);
    pkt.clear_player_state();
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
    socket_->broadcastPacket(pkt_send);

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
  socket_->broadcastPacket(pkt);
}

void ServerInstance::onGarbageUpdateSize(const Garbage* gb)
{
  netplay::Packet pkt;
  netplay::PktUpdateGarbage* np_garbage = pkt.mutable_update_garbage();
  np_garbage->set_gbid( gb->gbid );
  np_garbage->set_size( gb->type == Garbage::TYPE_COMBO ? gb->size.x : gb->size.y );
  socket_->broadcastPacket(pkt);
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
  socket_->broadcastPacket(pkt);

  // local player: drop immediately
  if( pl_to->local() ) {
    gb->to->dropNextGarbage();
    np_state->set_state(netplay::PktGarbageState::DROP);
    socket_->broadcastPacket(pkt);
  }
}


Player* ServerInstance::newPlayer(netplay::PeerSocket* peer, const std::string& nick)
{
  auto pl_unique = std::unique_ptr<Player>(new Player(this->nextPlayerId(), peer==NULL));
  Player* pl = pl_unique.get();
  LOG("init player: %d", pl->plid());
  pl->setState(Player::State::LOBBY);
  pl->setNick(nick);
  auto fc_it = conf_.field_confs.begin();
  pl->setFieldConf(fc_it->second, fc_it->first);
  // put accepted player with his friends
  PlId plid = pl->plid(); // use a temporary value to help g++
  players_.insert(plid, pl_unique.release());
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
  socket_->broadcastPacket(pkt, peer);
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
  pl->setState(Player::State::QUIT);
  LOG("%s(%u): state set to QUIT", pl->nick().c_str(), pl->plid());
  observer_.onPlayerStateChange(pl);
  players_.erase(plid);
  peers_.erase(plid);

  // update ranks (this will stop match if necessary)
  // the ranking message must to be sent before the QUIT or the player
  // will not exist anymore on client side
  if(state_ == State::GAME) {
    this->updateRanks();
  }

  // tell other players
  netplay::Packet pkt;
  netplay::PktPlayerState* np_state = pkt.mutable_player_state();
  np_state->set_plid(plid);
  np_state->set_state(netplay::PktPlayerState::QUIT);
  socket_->broadcastPacket(pkt);
}


void ServerInstance::processPktInput(netplay::PeerSocket* peer, const netplay::PktInput& pkt)
{
  if(state_ != State::GAME) {
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
  if(state_ != State::GAME) {
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
  socket_->broadcastPacket(pkt_send);

  fld->dropNextGarbage();
}

void ServerInstance::processPktPlayerJoin(netplay::PeerSocket* peer, const netplay::PktPlayerJoin& pkt)
{
  if(state_ != State::LOBBY) {
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
  Player* pl = this->checkPeerPlayer(pkt.plid(), peer);
  if(pl->state() != Player::State::LOBBY) {
    throw netplay::CallbackError("invalid when player is not in lobby");
  }

  netplay::Packet pkt_send;
  netplay::PktPlayerConf* np_plconf = pkt_send.mutable_player_conf();

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
    socket_->broadcastPacket(pkt_send);
  }
}

void ServerInstance::processPktPlayerState(netplay::PeerSocket* peer, const netplay::PktPlayerState& pkt)
{
  Player* pl = this->checkPeerPlayer(pkt.plid(), peer);
  if(pl->state() == Player::State::QUIT) {
    return;  // quitting player cannot be modified
  }

  Player::State new_state = static_cast<Player::State>(pkt.state());
  Player::State old_state = pl->state();
  if(new_state == old_state) {
    return;  // no change
  }

  bool state_valid = false;
  switch(new_state) {
    case Player::State::QUIT:
      this->removePlayer(pl->plid()); // note: don't disconnect the peer
      // QUIT state is broadcasted by removePlayer(), don't send it twice
      this->checkAllPlayersReady();
      return;
    case Player::State::LOBBY:
      state_valid = state_ == State::LOBBY || state_ == State::GAME;
      break;
    case Player::State::LOBBY_READY:
      state_valid = state_ == State::LOBBY;
      break;
    case Player::State::GAME_READY:
      state_valid = state_ == State::GAME_READY && old_state == Player::State::GAME_INIT;
      break;
    case Player::State::GAME_INIT:
    case Player::State::GAME:
    default:
      state_valid = false;  // unsettable states
      break;
  }
  if(!state_valid) {
    LOG("%s(%u): invalid new state: %d", pl->nick().c_str(), pl->plid(), static_cast<int>(new_state));
    return;
  }

  pl->setState(new_state);
  LOG("%s(%u): state set to %d", pl->nick().c_str(), pl->plid(), static_cast<int>(new_state));
  observer_.onPlayerStateChange(pl);

  netplay::Packet pkt_send;
  *pkt_send.mutable_player_state() = pkt;
  socket_->broadcastPacket(pkt_send);

  this->checkAllPlayersReady();
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
  if(state_ == State::LOBBY) {
    // check for pl_nb_max ready players
    unsigned int nb_ready = 0;
    for(auto const& p : players_) {
      if(p.second->state() == Player::State::LOBBY_READY) {
        nb_ready++;
      }
    }
    if(nb_ready == conf_.pl_nb_max) {
      this->prepareMatch();
    }

  } else if(state_ == State::GAME_READY) {
    // no more players in GAME_INIT state
    for(auto const& p : players_) {
      if(p.second->state() == Player::State::GAME_INIT) {
        return;
      }
    }
    this->startMatch();

  } else {
    // nothing to check
  }
}


void ServerInstance::setState(State state)
{
  netplay::Packet pkt;
  netplay::PktServerState* np_state = pkt.mutable_server_state();
  np_state->set_state( static_cast<netplay::PktServerState::State>(state) );
  socket_->broadcastPacket(pkt);

  // implicit player state changes
  if(state == State::GAME_INIT) {
    for(auto const& p : players_) {
      if(p.second->state() == Player::State::LOBBY_READY) {
        p.second->setState(Player::State::GAME_INIT);
      }
    }
  } else if(state == State::GAME) {
    for(auto const& p : players_) {
      if(p.second->state() == Player::State::GAME_READY) {
        p.second->setState(Player::State::GAME);
      }
    }
  }

  state_ = state;
  LOG("server: state set to %d", static_cast<int>(state));
  observer_.onStateChange();
}

void ServerInstance::prepareMatch()
{
  LOG("prepare match");
  //XXX check ready player count (there should be at least 1 player)

  match_.clear();
  this->setState(State::GAME_INIT);

  int seed = ::rand(); // common seed for all fields
  netplay::Packet pkt;

  PlayerContainer::iterator it;
  for( it=players_.begin(); it!=players_.end(); ++it ) {
    Player* pl = (*it).second;
    if(pl->state() != Player::State::GAME_INIT) {
      continue;
    }
    Field* fld = match_.addField(pl->fieldConf(), seed);
    pl->setField(fld);
    fld->fillRandom(6);

    netplay::PktPlayerField* np_field = pkt.mutable_player_field();
    np_field->Clear();
    np_field->set_plid(pl->plid());
    np_field->set_seed(fld->seed()); //note: seed changed due to fillRandom()
    fld->setGridContentToPacket(np_field->mutable_blocks());
    socket_->broadcastPacket(pkt);
  }

  this->setState(State::GAME_READY);
}

void ServerInstance::startMatch()
{
  LOG("start match");

  gb_distributor_.reset();
  match_.start();
  this->setState(State::GAME);
}

void ServerInstance::stopMatch()
{
  // ignore if match is already being stopped
  if(state_ == State::LOBBY) {
    return;
  }
  LOG("stop match");

  PlayerContainer::iterator it;
  for(it=players_.begin(); it!=players_.end(); ++it) {
    (*it).second->setField(NULL);
  }
  match_.stop();
  this->setState(State::LOBBY);
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
  socket_->broadcastPacket(pkt_send, peer);
  pkt_send.clear_input(); // packet reused below

  // update garbages
  //TODO check for clients which never send back the drop packets
  gb_distributor_.updateGarbages(pl->field());

  this->updateRanks();
}


void ServerInstance::updateRanks()
{
  std::vector<const Field*> ranked;
  bool end_of_match = match_.updateRanks(ranked);

  netplay::Packet pkt_send;
  netplay::PktPlayerRank* np_rank = pkt_send.mutable_player_rank();
  for(auto const& fld : ranked) {
    Player* pl = this->player(fld);
    assert(pl);
    LOG("%s(%u): ranked %u", pl->nick().c_str(), pl->plid(), fld->rank());
    np_rank->set_plid(pl->plid());
    np_rank->set_rank(fld->rank());
    socket_->broadcastPacket(pkt_send);
    observer_.onPlayerRanked(pl);
  }
  if(end_of_match) {
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


