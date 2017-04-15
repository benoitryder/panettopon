#include "client.h"
#include "game.h"
#include "log.h"

namespace asio = boost::asio;
using namespace asio::ip;


ClientInstance::ClientInstance(Observer& obs, asio::io_service& io_service):
    observer_(obs), socket_(std::make_shared<netplay::ClientSocket>(*this, io_service))
{
}

ClientInstance::~ClientInstance()
{
  if(socket_) {
    socket_->close();
  }
}

void ClientInstance::connect(const char* host, int port, int tout)
{
  LOG("connecting to %s:%d ...", host, port);
  socket_->connect(host, port, tout);
}

void ClientInstance::disconnect()
{
  //XXX send a proper "quit" message
  state_ = State::NONE;
  socket_->close();
  socket_.reset();
}

void ClientInstance::newLocalPlayer(const std::string& nick, std::function<void(Player*, const std::string&)> cb)
{
  auto command = std::make_unique<netplay::ClientCommand>();
  auto* np_join = command->mutable_player_join();
  np_join->set_nick(nick);
  auto cb2 = [this,cb](const netplay::ServerResponse& response) {
    this->processNewPlayerResponse(response, cb);
  };
  socket_->sendClientCommand(std::move(command), cb2);
}


void ClientInstance::playerSetNick(Player& pl, const std::string& nick)
{
  assert(pl.local());
  assert(pl.state() == Player::State::LOBBY);
  if(nick == pl.nick()) {
    return; // nothing to do
  }
  pl.setNick(nick);

  auto command = std::make_unique<netplay::ClientCommand>();
  auto* np_conf = command->mutable_player_conf();
  np_conf->set_plid(pl.plid());
  np_conf->set_nick(nick);
  socket_->sendClientCommand(std::move(command), nullptr);
}

void ClientInstance::playerSetFieldConf(Player& pl, const FieldConf& conf)
{
  assert(pl.local());
  assert(pl.state() == Player::State::LOBBY);
  //TODO compare with current conf/name

  auto command = std::make_unique<netplay::ClientCommand>();
  auto* np_conf = command->mutable_player_conf();
  np_conf->set_plid(pl.plid());
  auto* np_fc = np_conf->mutable_field_conf();
  conf.toPacket(*np_fc);
  socket_->sendClientCommand(std::move(command), nullptr);
}

void ClientInstance::playerSetState(Player& pl, Player::State state)
{
  assert(pl.local());
  if(pl.state() == state) {
    return;  // no change
  }

  // QUIT: set state immediately
  if(state == Player::State::QUIT) {
    pl.setState(state);
    LOG("%s(%u): state set to QUIT", pl.nick().c_str(), pl.plid());
    observer_.onPlayerStateChange(pl);
  }

  auto command = std::make_unique<netplay::ClientCommand>();
  auto* np_state = command->mutable_player_state();
  np_state->set_plid(pl.plid());
  np_state->set_state(static_cast<netplay::PktPlayerState::State>(state));
  socket_->sendClientCommand(std::move(command), nullptr);
}

void ClientInstance::playerSendChat(Player& pl, const std::string& msg)
{
  assert(pl.local());

  auto command = std::make_unique<netplay::ClientCommand>();
  auto* np_chat = command->mutable_chat();
  np_chat->set_plid(pl.plid());
  np_chat->set_text(msg);
  socket_->sendClientCommand(std::move(command), nullptr);
}

void ClientInstance::playerStep(Player& pl, KeyState keys)
{
  assert(pl.local() && pl.field() != nullptr);
  Tick tk = pl.field()->tick();
  this->doStepPlayer(pl, keys);

  // send packet
  auto event = std::make_unique<netplay::ClientEvent>();
  auto* np_input = event->mutable_input();
  np_input->set_plid(pl.plid());
  np_input->set_tick(tk);
  np_input->add_keys(keys);
  socket_->sendClientEvent(std::move(event));
}


void ClientInstance::onServerEvent(const netplay::ServerEvent& event)
{
  if(event.has_input()) {
    this->processPktInput(event.input());
  } else if(event.has_new_garbage()) {
    this->processPktNewGarbage(event.new_garbage());
  } else if(event.has_update_garbage()) {
    this->processPktUpdateGarbage(event.update_garbage());
  } else if(event.has_garbage_state()) {
    this->processPktGarbageState(event.garbage_state());
  } else if(event.has_chat()) {
    this->processPktChat(event.chat());
  } else if(event.has_notification()) {
    this->processPktNotification(event.notification());
  } else if(event.has_server_conf()) {
    this->processPktServerConf(event.server_conf());
  } else if(event.has_server_state()) {
    this->processPktServerState(event.server_state());
  } else if(event.has_player_conf()) {
    this->processPktPlayerConf(event.player_conf());
  } else if(event.has_player_state()) {
    this->processPktPlayerState(event.player_state());
  } else if(event.has_player_rank()) {
    this->processPktPlayerRank(event.player_rank());
  } else if(event.has_player_field()) {
    this->processPktPlayerField(event.player_field());
  } else {
    throw netplay::CallbackError("invalid packet field");
  }
}

void ClientInstance::onServerConnect(bool success)
{
  if(success) {
    LOG("connected");
    state_ = State::LOBBY;
    conf_.toDefault();
  }
  observer_.onServerConnect(success);
}

void ClientInstance::onServerDisconnect()
{
  LOG("disconnected");
  observer_.onServerDisconnect();
}


void ClientInstance::processPktInput(const netplay::PktInput& pkt)
{
  if(state_ != State::GAME) {
    throw netplay::CallbackError("match is not running");
  }
  Player* pl = this->player(pkt.plid());
  if( pl == NULL || pl->field() == NULL ) {
    throw netplay::CallbackError("invalid player");
  }
  if( pl->local() ) {
    return; // already processed
  }
  Field& fld = *pl->field();

  Tick tick = pkt.tick();
  if( tick < fld.tick() ) {
    throw netplay::CallbackError("input tick in the past");
  }
  // skipped frames
  while(fld.tick() < tick) {
    this->stepRemotePlayer(*pl, 0);
  }
  // provided frames
  const int keys_nb = pkt.keys_size();
  for( int i=0; i<keys_nb; i++ ) {
    this->stepRemotePlayer(*pl, pkt.keys(i));
  }
}

void ClientInstance::processPktNewGarbage(const netplay::PktNewGarbage& pkt)
{
  if(state_ != State::GAME) {
    throw netplay::CallbackError("match is not running");
  }

  auto gb = std::make_unique<Garbage>();
  gb->gbid = pkt.gbid();

  Player* pl_to = this->player(pkt.plid_to());
  if( pl_to == NULL || pl_to->field() == NULL ) {
    throw netplay::CallbackError("invalid garbage target");
  }
  gb->to = pl_to->field();

  if(pkt.plid_from() ) {
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
  if( gb->type == Garbage::Type::CHAIN ) {
    gb->size = FieldPos(FIELD_WIDTH, pkt.size());
  } else if( gb->type == Garbage::Type::COMBO ) {
    gb->size = FieldPos(pkt.size(), 1);
  } else {
    assert( !"not supported yet" );
  }

  if( pkt.pos() > gb->to->hangingGarbageCount() ) {
    throw netplay::CallbackError("invalid garbage position");
  }
  match_.addGarbage(std::move(gb), pkt.pos());
}

void ClientInstance::processPktUpdateGarbage(const netplay::PktUpdateGarbage& pkt)
{
  if(state_ != State::GAME) {
    throw netplay::CallbackError("match is not running");
  }
  const Match::GarbageMap gbs_hang = match_.hangingGarbages();
  Match::GarbageMap::const_iterator it = gbs_hang.find(pkt.gbid());
  if( it == gbs_hang.end() ) {
    throw netplay::CallbackError("garbage not found");
  }
  Garbage& gb = *(*it).second;

  Player* pl_to = this->player(gb.to);
  assert( pl_to != NULL );

  if(pkt.plid_to() && pkt.plid_to() != pl_to->plid()) {
    // actual target change
    pl_to = this->player(pkt.plid_to());
    if( pl_to == NULL || pl_to->field() == NULL ) {
      throw netplay::CallbackError("invalid garbage target");
    }
  }
  if( pkt.pos() > pl_to->field()->hangingGarbageCount() ) {
    throw netplay::CallbackError("invalid garbage position");
  }
  auto ptr = gb.to->removeHangingGarbage(gb);
  gb.to = pl_to->field(); // no-op if plid_to did not changed
  gb.to->insertHangingGarbage(std::move(ptr), pkt.pos());

  if( pkt.size() != 0 ) {
    if( gb.type == Garbage::Type::CHAIN ) {
      gb.size = FieldPos(FIELD_WIDTH, pkt.size());
    } else if( gb.type == Garbage::Type::COMBO ) {
      gb.size = FieldPos(pkt.size(), 1);
    } else {
      assert( !"not supported yet" );
    }
  }
}

void ClientInstance::processPktGarbageState(const netplay::PktGarbageState& pkt)
{
  if(state_ != State::GAME) {
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
    Garbage& gb = *(*it).second;
    match_.waitGarbageDrop(gb);
    Player* pl = this->player(gb.to);
    assert(pl);
    if( pl->local() ) {
      // one of our garbages, drop it
      auto event = std::make_unique<netplay::ClientEvent>();
      netplay::PktGarbageState* np_state = event->mutable_garbage_state();
      np_state->set_gbid(gb.gbid);
      np_state->set_state(netplay::PktGarbageState::DROP);
      socket_->sendClientEvent(std::move(event));
      gb.to->dropNextGarbage();
    }

  } else if( state == netplay::PktGarbageState::DROP ) {
    // drop garbage
    const Match::GarbageMap gbs_wait = match_.waitingGarbages();
    Match::GarbageMap::const_iterator it = gbs_wait.find(pkt.gbid());
    if( it == gbs_wait.end() ) {
      throw netplay::CallbackError("garbage not found");
    }
    Garbage& gb = *(*it).second;
    Field* fld = gb.to;
    Player* pl = this->player(fld);
    if( pl == NULL ) {
      throw netplay::CallbackError("invalid player");
    }
    if( !pl->local() ) { // ignore our garbages (already dropped)
      if(fld->waitingGarbages().size() == 0 || fld->waitingGarbages().front()->gbid != gb.gbid) {
        throw netplay::CallbackError("invalid dropped garbage");
      }
      pl->field()->dropNextGarbage();
    }
  }
}

void ClientInstance::processPktChat(const netplay::PktChat& pkt)
{
  Player* pl = this->player(pkt.plid());
  if(!pl) {
    throw netplay::CallbackError("invalid player");
  }
  observer_.onChat(*pl, pkt.text());
}

void ClientInstance::processPktNotification(const netplay::PktNotification& pkt)
{
  observer_.onNotification(static_cast<GameInstance::Severity>(pkt.severity()), pkt.text());
}

void ClientInstance::processPktServerConf(const netplay::PktServerConf& pkt)
{
  if(state_ != State::LOBBY) {
    throw netplay::CallbackError("invalid in current state");
  }
#define SERVER_CONF_EXPR_PKT(n,ini) \
  conf_.n = pkt.n();
  SERVER_CONF_APPLY(SERVER_CONF_EXPR_PKT);
#undef SERVER_CONF_EXPR_PKT
  google::protobuf::RepeatedPtrField<netplay::FieldConf> np_fcs = pkt.field_confs();
  if( np_fcs.size() > 0 ) {
    google::protobuf::RepeatedPtrField<netplay::FieldConf>::const_iterator fcit;
    std::set<std::string> field_conf_names; // to check for duplicates
    conf_.field_confs.clear();
    for( fcit=np_fcs.begin(); fcit!=np_fcs.end(); ++fcit ) {
      const std::string& name = fcit->name();
      if(name.empty()) {
        throw netplay::CallbackError("unnamed server field configuration");
      }
      if(!field_conf_names.insert(name).second) {
        throw netplay::CallbackError("duplicate field configuration name: "+name);
      }
      conf_.field_confs.emplace_back();
      conf_.field_confs.back().fromPacket(*fcit);
    }
  }
  // even when np_fcs.size() > 0 to handle the init case (first conf packet)
  if( conf_.field_confs.size() == 0 ) {
    throw netplay::CallbackError("no field configuration");
  }

  if(np_fcs.size() > 0) {
    observer_.onServerChangeFieldConfs();
  }
}

void ClientInstance::processPktServerState(const netplay::PktServerState& pkt)
{
  //TODO check whether state change is valid
  State new_state = static_cast<State>(pkt.state());
  if(new_state == state_) {
    return; // nothing to do, should not happen though
  }

  if(new_state == State::GAME_INIT) {
    match_.clear();
    state_ = new_state;
    // implicit player state changes
    for(auto const& p : players_) {
      if(p.second->state() == Player::State::LOBBY_READY) {
        p.second->setState(Player::State::GAME_INIT);
      }
    }
    LOG("client: state set to GAME_INIT");
    observer_.onStateChange();

  } else if(new_state == State::GAME_READY) {
    state_ = new_state;
    LOG("client: state set to GAME_READY");
    // init fields for match
    match_.start();
    observer_.onStateChange();

  } else if(new_state == State::GAME) {
    state_ = new_state;
    // implicit player state changes
    for(auto const& p : players_) {
      if(p.second->state() == Player::State::GAME_READY) {
        p.second->setState(Player::State::GAME);
      }
    }

    LOG("client: state set to GAME");
    observer_.onStateChange();

  } else if(new_state == State::LOBBY) {
    if(match_.started()) {
      this->stopMatch();
    }
  }
}

void ClientInstance::processPktPlayerConf(const netplay::PktPlayerConf& pkt)
{
  Player* pl = this->player(pkt.plid());
  if(!pl) {
    // new player
    this->createNewPlayer(pkt, false);
  } else {
    // update player
    if(!pkt.nick().empty()) {
      const std::string old_nick = pl->nick();
      pl->setNick( pkt.nick() );
      observer_.onPlayerChangeNick(*pl, old_nick);
    }
    if( pkt.has_field_conf() ) {
      const std::string name = pkt.field_conf().name();
      if( name.empty() ) {
        FieldConf conf;
        conf.fromPacket(pkt.field_conf());
        pl->setFieldConf(conf);
      } else {
        auto* fc = conf_.fieldConf(name);
        if(!fc) {
          throw netplay::CallbackError("invalid configuration name: "+name);
        }
        pl->setFieldConf(*fc);
      }
      observer_.onPlayerChangeFieldConf(*pl);
    }
  }
}

void ClientInstance::processPktPlayerState(const netplay::PktPlayerState& pkt)
{
  Player* pl = this->player(pkt.plid());
  if(pl == NULL) {
    throw netplay::CallbackError("invalid player");
  }
  if(pl->state() == Player::State::QUIT) {
    return;  // quitting player cannot be modified
  }

  Player::State new_state = static_cast<Player::State>(pkt.state());
  Player::State old_state = pl->state();
  if(new_state == old_state) {
    return;  // no change
  }

  bool state_valid = false;
  bool erase_player = false;
  switch(new_state) {
    case Player::State::QUIT:
      if(pl->field() != NULL) {
        pl->field()->abort();
        match_.updateTick(); // field lost, tick must be updated
        pl->setField(NULL);
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
      throw netplay::CallbackError("unsettable state");
  }

  if(!state_valid) {
    throw netplay::CallbackError("invalid new state");
  }

  pl->setState(new_state);
  LOG("%s(%u): state set to %d", pl->nick().c_str(), pl->plid(), static_cast<int>(new_state));
  observer_.onPlayerStateChange(*pl);

  if(erase_player) {
    players_.erase(pl->plid());
  }
}

void ClientInstance::processPktPlayerRank(const netplay::PktPlayerRank& pkt)
{
  if(state_ != State::GAME) {
    throw netplay::CallbackError("invalid in current state");
  }
  Player* pl = this->player(pkt.plid());
  if( pl == NULL || pl->field() == NULL ) {
    throw netplay::CallbackError("invalid player");
  }
  pl->field()->setRank(pkt.rank()); //TODO may fail if already set
  LOG("%s(%u): ranked %u", pl->nick().c_str(), pl->plid(), pl->field()->rank());
  observer_.onPlayerRanked(*pl);
}

void ClientInstance::processPktPlayerField(const netplay::PktPlayerField& pkt)
{
  //XXX support sending of current game fields to new clients
  if(state_ != State::GAME_INIT) {
    throw netplay::CallbackError("invalid in current state");
  }
  Player* pl = this->player(pkt.plid());
  if( pl == NULL ) {
    throw netplay::CallbackError("invalid player");
  }
  if( pl->field() != NULL ) {
    throw netplay::CallbackError("field already initialized");
  }

  Field& fld = match_.addField(pl->fieldConf(), pkt.seed());
  pl->setField(&fld);
  if( pkt.blocks_size() > 0 ) {
    if(!fld.setGridContentFromPacket(pkt.blocks())) {
      throw netplay::CallbackError("invalid field content");
    }
  }
}

void ClientInstance::processNewPlayerResponse(const netplay::ServerResponse& response, NewPlayerCallback cb)
{
  if(response.result() == netplay::ServerResponse::OK) {
    if(!response.has_player_join()) {
      throw netplay::CallbackError("missing response field");
    }
    Player& pl = this->createNewPlayer(response.player_join(), true);
    cb(&pl, response.reason());
  } else {
    cb(nullptr, response.reason());
  }
}

Player& ClientInstance::createNewPlayer(const netplay::PktPlayerConf& pkt, bool local)
{
  if(pkt.nick().empty() || !pkt.has_field_conf()) {
    throw netplay::CallbackError("missing fields");
  }
  auto pl_ptr = std::make_unique<Player>(pkt.plid(), local);
  Player& pl = *pl_ptr.get();
  pl.setState(Player::State::LOBBY);
  pl.setNick(pkt.nick());
  players_.emplace(pl.plid(), std::move(pl_ptr));
  FieldConf conf;
  conf.fromPacket(pkt.field_conf());
  pl.setFieldConf(conf);
  observer_.onPlayerJoined(pl);
  return pl;
}


void ClientInstance::stopMatch()
{
  LOG("stop match");

  PlayerContainer::iterator it;
  for(it=players_.begin(); it!=players_.end(); ++it) {
    (*it).second->setField(NULL);
  }
  match_.stop();
  state_ = State::LOBBY;
  LOG("client: state set to LOBBY");
  observer_.onStateChange();
}

