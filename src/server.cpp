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
      conf_.field_confs.emplace_back();
      FieldConf& field_conf = conf_.field_confs.back();
      field_conf.name = name;
      field_conf.fromIniFile(cfg, field_conf_section);
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


Player& ServerInstance::newLocalPlayer(const std::string& nick)
{
  return this->newPlayer(NULL, nick);
}


void ServerInstance::playerSetNick(Player& pl, const std::string& nick)
{
  assert(pl.local());
  assert(pl.state() == Player::State::LOBBY);

  const std::string old_nick = pl.nick();
  if(nick == old_nick) {
    return; // nothing to do
  }
  pl.setNick(nick);
  observer_.onPlayerChangeNick(pl, old_nick);

  auto event = std::make_unique<netplay::ServerEvent>();
  auto* np_plconf = event->mutable_player_conf();
  np_plconf->set_plid(pl.plid());
  np_plconf->set_nick(nick);
  socket_->broadcastEvent(std::move(event));
}

void ServerInstance::playerSetFieldConf(Player& pl, const FieldConf& conf)
{
  assert(pl.local());
  assert(pl.state() == Player::State::LOBBY);

  pl.setFieldConf(conf);
  observer_.onPlayerChangeFieldConf(pl);

  auto event = std::make_unique<netplay::ServerEvent>();
  auto* np_conf = event->mutable_player_conf();
  np_conf->set_plid(pl.plid());
  auto* np_fc = np_conf->mutable_field_conf();
  conf.toPacket(*np_fc);
  socket_->broadcastEvent(std::move(event));
}

void ServerInstance::playerSetState(Player& pl, Player::State state)
{
  assert(pl.local());
  assert(pl.state() != Player::State::QUIT);

  Player::State old_state = pl.state();
  if(state == old_state) {
    return;  // no change
  }

  bool state_valid = false;
  bool erase_player = false;
  switch(state) {
    case Player::State::QUIT:
      if(pl.field() != nullptr) {
        pl.field()->abort();
        match_.updateTick(); // field lost, tick must be updated
        pl.setField(nullptr);
      }
      erase_player = true;
      state_valid = true;
      break;
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
    LOG("%s(%u): invalid new state: %d", pl.nick().c_str(), pl.plid(), static_cast<int>(state));
    return;
  }

  pl.setState(state);
  LOG("%s(%u): state set to %d", pl.nick().c_str(), pl.plid(), static_cast<int>(state));
  observer_.onPlayerStateChange(pl);

  auto event = std::make_unique<netplay::ServerEvent>();
  auto* np_state = event->mutable_player_state();
  np_state->set_plid(pl.plid());
  np_state->set_state(static_cast<netplay::PktPlayerState::State>(state));
  socket_->broadcastEvent(std::move(event));

  if(erase_player) {
    players_.erase(pl.plid());
  }
  this->checkAllPlayersReady();
}

void ServerInstance::playerSendChat(Player& pl, const std::string& msg)
{
  assert(pl.local());
  observer_.onChat(pl, msg);

  auto event = std::make_unique<netplay::ServerEvent>();
  auto* np_chat = event->mutable_chat();
  np_chat->set_plid(pl.plid());
  np_chat->set_text(msg);
  socket_->broadcastEvent(std::move(event));
}

void ServerInstance::playerStep(Player& pl, KeyState keys)
{
  assert(pl.local() && pl.field() != nullptr);
  this->doStepPlayer(pl, keys);
}


void ServerInstance::onPeerConnect(netplay::PeerSocket& peer)
{
  if(state_ != State::LOBBY) {
    throw netplay::CallbackError("match is running");
  } else if( players_.size() >= conf_.pl_nb_max ) {
    //TODO difference between player max and peer max
    throw netplay::CallbackError("server full");
  }
  LOG("peer connected");

  {
    // send server configuration
    auto event = std::make_unique<netplay::ServerEvent>();
    auto* np_conf = event->mutable_server_conf();
#define SERVER_CONF_EXPR_PKT(n,ini) \
    np_conf->set_##n(conf_.n);
    SERVER_CONF_APPLY(SERVER_CONF_EXPR_PKT);
#undef SERVER_CONF_EXPR_PKT
    auto* np_fcs = np_conf->mutable_field_confs();
    np_fcs->Reserve(conf_.field_confs.size());
    for(auto& fc : conf_.field_confs) {
      auto* np_fc = np_fcs->Add();
      fc.toPacket(*np_fc);
    }
    peer.sendServerEvent(std::move(event));
  }

  {
    // send server state
    auto event = std::make_unique<netplay::ServerEvent>();
    auto* np_state = event->mutable_server_state();
    np_state->set_state(static_cast<netplay::PktServerState::State>(state_));
    peer.sendServerEvent(std::move(event));
  }

  // send information on other players
  PlayerContainer::const_iterator it;
  for(auto const& kv : players_) {
    const Player& pl = *kv.second.get();
    {
      auto event = std::make_unique<netplay::ServerEvent>();
      auto* np_plconf = event->mutable_player_conf();
      np_plconf->set_plid(pl.plid());
      np_plconf->set_nick(pl.nick());
      auto* np_fc = np_plconf->mutable_field_conf();
      pl.fieldConf().toPacket(*np_fc);
      peer.sendServerEvent(std::move(event));
    }
    {
      auto event = std::make_unique<netplay::ServerEvent>();
      auto* np_state = event->mutable_player_state();
      np_state->set_plid(pl.plid());
      np_state->set_state(static_cast<netplay::PktPlayerState::State>(pl.state()));
      peer.sendServerEvent(std::move(event));
    }
  }

  // set read handler
  peer.readNext();
}


void ServerInstance::onPeerDisconnect(netplay::PeerSocket& peer)
{
  for(auto it=peers_.begin(); it!=peers_.end(); ) {
    if((*it).second == &peer) {
      PlId plid = (*it).first;
      ++it; // increment to avoid invalidated iterator
      this->removePlayer(plid);
    } else {
      ++it;
    }
  }
}

void ServerInstance::onPeerClientEvent(netplay::PeerSocket& peer, const netplay::ClientEvent& event)
{
  if(event.has_input()) {
    this->processPktInput(peer, event.input());
  } else if(event.has_garbage_state()) {
    this->processPktGarbageState(peer, event.garbage_state());
  } else {
    throw netplay::CallbackError("invalid packet field");
  }
}

void ServerInstance::onPeerClientCommand(netplay::PeerSocket& peer, const netplay::ClientCommand& command)
{
  auto response = std::make_unique<netplay::ServerResponse>();
  try {
    if(command.has_chat()) {
      this->processPktChat(peer, command.chat());
    } else if(command.has_player_join()) {
      auto rpkt = this->processPktPlayerJoin(peer, command.player_join());
      response->set_allocated_player_join(rpkt.release());
    } else if(command.has_player_conf()) {
      this->processPktPlayerConf(peer, command.player_conf());
    } else if(command.has_player_state()) {
      this->processPktPlayerState(peer, command.player_state());
    } else {
      throw netplay::CallbackError("invalid command field");
    }
  } catch(const netplay::CommandError& e) {
    LOG("command error: %s", e.what());
    response->set_result(netplay::ServerResponse::ERROR);
    response->set_reason(e.what());
  }
  peer.sendServerResponse(std::move(response));
}


void ServerInstance::onGarbageAdd(const Garbage& gb, unsigned int pos)
{
  Player* pl_to = this->player(gb.to);
  Player* pl_from = this->player(gb.from);
  assert( pl_to != NULL );

  auto event = std::make_unique<netplay::ServerEvent>();
  auto* np_garbage = event->mutable_new_garbage();
  np_garbage->set_gbid(gb.gbid);
  np_garbage->set_pos( pos );
  np_garbage->set_plid_to( pl_to->plid() );
  np_garbage->set_plid_from( pl_from == NULL ? 0 : pl_from->plid() );
  np_garbage->set_type(static_cast<netplay::GarbageType>(gb.type));
  np_garbage->set_size(gb.type == Garbage::Type::COMBO ? gb.size.x : gb.size.y);
  socket_->broadcastEvent(std::move(event));
}

void ServerInstance::onGarbageUpdateSize(const Garbage& gb)
{
  auto event = std::make_unique<netplay::ServerEvent>();
  auto* np_garbage = event->mutable_update_garbage();
  np_garbage->set_gbid(gb.gbid);
  np_garbage->set_size(gb.type == Garbage::Type::COMBO ? gb.size.x : gb.size.y);
  socket_->broadcastEvent(std::move(event));
}

void ServerInstance::onGarbageDrop(const Garbage& gb)
{
  Player* pl_to = this->player(gb.to);
  assert( pl_to != NULL );

  auto event = std::make_unique<netplay::ServerEvent>();
  auto* np_state = event->mutable_garbage_state();
  np_state->set_gbid(gb.gbid);
  np_state->set_state(netplay::PktGarbageState::WAIT);
  match_.waitGarbageDrop(gb);
  socket_->broadcastEvent(std::move(event));

  // local player: drop immediately
  if( pl_to->local() ) {
    gb.to->dropNextGarbage();
    auto event = std::make_unique<netplay::ServerEvent>();
    auto* np_state = event->mutable_garbage_state();
    np_state->set_gbid(gb.gbid);
    np_state->set_state(netplay::PktGarbageState::DROP);
    socket_->broadcastEvent(std::move(event));
  }
}


Player& ServerInstance::newPlayer(netplay::PeerSocket* peer, const std::string& nick)
{
  auto pl_unique = std::make_unique<Player>(this->nextPlayerId(), peer == nullptr);
  Player& pl = *pl_unique.get();
  PlId plid = pl.plid();
  LOG("init player: %d", plid);
  pl.setState(Player::State::LOBBY);
  pl.setNick(nick);
  pl.setFieldConf(conf_.field_confs[0]);
  // put accepted player with his friends
  players_.emplace(plid, std::move(pl_unique));
  if( peer != NULL ) {
    peers_[plid] = peer; // associate the player to its peer
  }
  observer_.onPlayerJoined(pl);

  // tell others players about their new friend
  // the peer (if any) will be informed last, through the ServerResponse
  auto event = std::make_unique<netplay::ServerEvent>();
  auto* np_conf = event->mutable_player_conf();
  np_conf->set_plid(pl.plid());
  np_conf->set_nick(pl.nick());
  auto* np_fc = np_conf->mutable_field_conf();
  pl.fieldConf().toPacket(*np_fc);
  socket_->broadcastEvent(std::move(event), peer);

  return pl;
}

void ServerInstance::removePlayer(PlId plid)
{
  Player* pl = this->player(plid);
  assert( pl != NULL );
  pl->setState(Player::State::QUIT);
  LOG("%s(%u): state set to QUIT", pl->nick().c_str(), pl->plid());
  observer_.onPlayerStateChange(*pl);
  players_.erase(plid);
  peers_.erase(plid);

  // update ranks (this will stop match if necessary)
  // the ranking message must to be sent before the QUIT or the player
  // will not exist anymore on client side
  if(state_ == State::GAME) {
    this->updateRanks();
  }

  // tell other players
  auto event = std::make_unique<netplay::ServerEvent>();
  auto* np_state = event->mutable_player_state();
  np_state->set_plid(plid);
  np_state->set_state(netplay::PktPlayerState::QUIT);
  socket_->broadcastEvent(std::move(event));
}


void ServerInstance::processPktInput(netplay::PeerSocket& peer, const netplay::PktInput& pkt)
{
  if(state_ != State::GAME) {
    return;  // ignore remains of the previous match
  }
  Player& pl = this->checkPeerPlayer(pkt.plid(), peer);
  //XXX:check field may have win without player knowing it
  // this must not trigger an error
  Field* fld = pl.field();
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

void ServerInstance::processPktGarbageState(netplay::PeerSocket& peer, const netplay::PktGarbageState& pkt)
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
  const Garbage& gb = *(*it).second;
  Field* fld = gb.to;
  Player* pl = this->player(fld);
  if( pl == NULL ) {
    throw netplay::CallbackError("invalid player");
  }
  this->checkPeerPlayer(pl->plid(), peer);
  if(fld->waitingGarbages().size() == 0 || fld->waitingGarbages().front()->gbid != gb.gbid) {
    throw netplay::CallbackError("invalid dropped garbage");
  }

  auto event = std::make_unique<netplay::ServerEvent>();
  auto* np_state = event->mutable_garbage_state();
  np_state->set_gbid(gb.gbid);
  np_state->set_state(netplay::PktGarbageState::DROP);
  socket_->broadcastEvent(std::move(event));

  fld->dropNextGarbage();
}

void ServerInstance::processPktChat(netplay::PeerSocket& peer, const netplay::PktChat& pkt)
{
  Player& pl = this->checkPeerPlayer(pkt.plid(), peer);
  observer_.onChat(pl, pkt.text());
}

std::unique_ptr<netplay::PktPlayerConf> ServerInstance::processPktPlayerJoin(netplay::PeerSocket& peer, const netplay::PktPlayerJoin& pkt)
{
  if(state_ != State::LOBBY) {
    throw netplay::CallbackError("match is running");
  } else if( players_.size() > conf_.pl_nb_max ) {
    throw netplay::CommandError("server full");
  }
  Player& pl = this->newPlayer(&peer, pkt.nick().empty() ? "Player" : pkt.nick());

  auto response = std::make_unique<netplay::PktPlayerConf>();
  response->set_plid(pl.plid());
  response->set_nick(pl.nick());
  pl.fieldConf().toPacket(*response->mutable_field_conf());
  return std::move(response);
}

void ServerInstance::processPktPlayerConf(netplay::PeerSocket& peer, const netplay::PktPlayerConf& pkt)
{
  Player& pl = this->checkPeerPlayer(pkt.plid(), peer);
  if(pl.state() != Player::State::LOBBY) {
    throw netplay::CallbackError("invalid when player is not in lobby");
  }

  auto event = std::make_unique<netplay::ServerEvent>();
  auto* np_plconf = event->mutable_player_conf();

  bool do_send = false;
  if(!pkt.nick().empty() && pkt.nick() != pl.nick() ) {
    const std::string old_nick = pl.nick();
    pl.setNick( pkt.nick() );
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
      pl.setFieldConf(conf);
    } else {
      auto* fc = conf_.fieldConf(name);
      if(!fc) {
        throw netplay::CommandError("invalid configuration name: "+name);
      }
      pl.setFieldConf(*fc);
    }
    if( do_send ) {
      netplay::FieldConf* np_fc = np_plconf->mutable_field_conf();
      pl.fieldConf().toPacket(*np_fc);
      observer_.onPlayerChangeFieldConf(pl);
    }
  }
  if( do_send ) {
    np_plconf->set_plid(pl.plid());
    socket_->broadcastEvent(std::move(event));
  }
}

void ServerInstance::processPktPlayerState(netplay::PeerSocket& peer, const netplay::PktPlayerState& pkt)
{
  Player& pl = this->checkPeerPlayer(pkt.plid(), peer);
  if(pl.state() == Player::State::QUIT) {
    throw netplay::CommandError("player is quitting");
  }

  Player::State new_state = static_cast<Player::State>(pkt.state());
  Player::State old_state = pl.state();
  if(new_state == old_state) {
    return;  // no change
  }

  bool state_valid = false;
  switch(new_state) {
    case Player::State::QUIT:
      this->removePlayer(pl.plid()); // note: don't disconnect the peer
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
    LOG("%s(%u): invalid new state: %d", pl.nick().c_str(), pl.plid(), static_cast<int>(new_state));
    throw netplay::CommandError("invalid new state");
  }

  pl.setState(new_state);
  LOG("%s(%u): state set to %d", pl.nick().c_str(), pl.plid(), static_cast<int>(new_state));
  observer_.onPlayerStateChange(pl);

  auto event = std::make_unique<netplay::ServerEvent>();
  *event->mutable_player_state() = pkt;
  socket_->broadcastEvent(std::move(event));

  this->checkAllPlayersReady();
}


Player& ServerInstance::checkPeerPlayer(PlId plid, const netplay::PeerSocket& peer)
{
  auto peer_it = peers_.find(plid);
  if(peer_it == peers_.end() || (*peer_it).second != &peer) {
    throw netplay::CallbackError("invalid player");
  }
  return *this->player(plid);
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
  auto event = std::make_unique<netplay::ServerEvent>();
  auto* np_state = event->mutable_server_state();
  np_state->set_state(static_cast<netplay::PktServerState::State>(state));
  socket_->broadcastEvent(std::move(event));

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
  for(auto& kv : players_) {
    Player& pl = *kv.second.get();
    if(pl.state() != Player::State::GAME_INIT) {
      continue;
    }
    Field& fld = match_.addField(pl.fieldConf(), seed);
    pl.setField(&fld);
    fld.fillRandom(6);

    auto event = std::make_unique<netplay::ServerEvent>();
    auto* np_field = event->mutable_player_field();
    np_field->set_plid(pl.plid());
    np_field->set_seed(fld.seed()); //note: seed changed due to fillRandom()
    fld.setGridContentToPacket(*np_field->mutable_blocks());
    socket_->broadcastEvent(std::move(event));
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


void ServerInstance::doStepPlayer(Player& pl, KeyState keys)
{
  Tick prev_tick = pl.field()->tick();
  GameInstance::doStepPlayer(pl, keys);

  //XXX Current implementation does not group Input packets.
  auto event = std::make_unique<netplay::ServerEvent>();
  auto* np_input_send = event->mutable_input();
  np_input_send->set_plid(pl.plid());
  np_input_send->set_tick(prev_tick);
  np_input_send->add_keys(keys);

  // don't send packet back to remote players
  auto peer_it = peers_.find(pl.plid());
  netplay::PeerSocket* peer = peer_it == peers_.end() ? nullptr : (*peer_it).second;
  socket_->broadcastEvent(std::move(event), peer);

  // update garbages
  //TODO check for clients who never send back the drop packets
  gb_distributor_.updateGarbages(*pl.field());

  this->updateRanks();
}


void ServerInstance::updateRanks()
{
  std::vector<const Field*> ranked;
  bool end_of_match = match_.updateRanks(ranked);

  for(auto const& fld : ranked) {
    Player* pl = this->player(fld);
    assert(pl);
    LOG("%s(%u): ranked %u", pl->nick().c_str(), pl->plid(), fld->rank());

    auto event = std::make_unique<netplay::ServerEvent>();
    auto* np_rank = event->mutable_player_rank();
    np_rank->set_plid(pl->plid());
    np_rank->set_rank(fld->rank());
    socket_->broadcastEvent(std::move(event));
    observer_.onPlayerRanked(*pl);
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


