#include "server.h"
#include "netplay.pb.h"
#include "config.h"
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


ServerInstance::ServerInstance(Observer &obs, boost::asio::io_service &io_service):
    observer_(obs), socket_(*this, io_service), gb_distributor_(match_, *this),
    current_plid_(0)
{
}

ServerInstance::~ServerInstance()
{
}

void ServerInstance::loadConf(const Config &cfg)
{
  assert( ! socket_.started() );
  //XXX signed/unsigned not checked
#define SERVER_CONF_EXPR_LOAD(n,ini,t) \
  conf_.n = cfg.get##t(CONF_SECTION, #ini, conf_.n);
  SERVER_CONF_APPLY(SERVER_CONF_EXPR_LOAD);
#undef SERVER_CONF_EXPR_LOAD
  socket_.setPktSizeMax(conf_.pkt_size_max);
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


Player *ServerInstance::newLocalPlayer(const std::string &nick)
{
  return this->newPlayer(NULL, nick);
}


void ServerInstance::playerSetNick(Player *pl, const std::string &nick)
{
  assert( pl->local() );
  if( nick == pl->nick() ) {
    return; // nothing to do
  }
  observer_.onPlayerChangeNick(pl, nick);
  pl->setNick(nick);

  netplay::Packet pkt;
  netplay::Player *np_player = pkt.mutable_player();
  np_player->set_plid(pl->plid());
  np_player->set_nick(nick);
  socket_.broadcastPacket(pkt);
}

void ServerInstance::playerSetReady(Player *pl, bool ready)
{
  assert( pl->local() );
  assert( state_ == STATE_LOBBY || state_ == STATE_READY );
  assert( ready ); //TODO false not supported yet
  if( pl->ready() == ready ) {
    return; // already ready, do nothing
  }
  pl->setReady(ready);
  observer_.onPlayerReady(pl);

  netplay::Packet pkt;
  netplay::Player *np_player = pkt.mutable_player();
  np_player->set_plid(pl->plid());
  np_player->set_ready(ready);
  socket_.broadcastPacket(pkt);

  this->checkAllPlayersReady();
}

void ServerInstance::playerSendChat(Player *pl, const std::string &msg)
{
  assert( pl->local() );
  observer_.onChat(pl, msg);

  netplay::Packet pkt;
  netplay::Chat *np_chat = pkt.mutable_chat();
  np_chat->set_plid(pl->plid());
  np_chat->set_txt(msg);
  socket_.broadcastPacket(pkt);
}

void ServerInstance::playerStep(Player *pl, KeyState keys)
{
  assert( pl->local() && pl->field() != NULL );
  this->doStepPlayer(pl, keys);
}

void ServerInstance::playerQuit(Player *pl)
{
  assert( pl->local() );
  this->removePlayer(pl->plid());
}


void ServerInstance::onPeerConnect(netplay::PeerSocket *peer)
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
  netplay::Server *np_server = pkt.mutable_server();
  np_server->set_state( static_cast<netplay::Server::State>(state_) );
  netplay::Server::Conf *np_conf = np_server->mutable_conf();
#define SERVER_CONF_EXPR_PKT(n,ini,t) \
  np_conf->set_##n(conf_.n);
  SERVER_CONF_APPLY(SERVER_CONF_EXPR_PKT);
#undef SERVER_CONF_EXPR_PKT
  peer->writePacket(pkt);
  pkt.clear_server(); // packet reused below

  // send information on other players
  netplay::Player *np_player = pkt.mutable_player();
  PlayerContainer::const_iterator it;
  for( it=players_.begin(); it!=players_.end(); ++it ) {
    np_player->set_plid( (*it).second->plid() );
    np_player->set_nick( (*it).second->nick() );
    np_player->set_ready( (*it).second->ready() );
    peer->writePacket(pkt);
  }

  // set read handler
  peer->readNext();
}


void ServerInstance::onPeerDisconnect(netplay::PeerSocket *peer)
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

void ServerInstance::onPeerPacket(netplay::PeerSocket *peer, const netplay::Packet &pkt)
{
  if( pkt.has_input() ) {
    this->processPacketInput(peer, pkt.input());
  } else if( pkt.has_garbage() ) {
    this->processPacketGarbage(peer, pkt.garbage());
  } else if( pkt.has_player() ) {
    this->processPacketPlayer(peer, pkt.player());
  } else if( pkt.has_chat() ) {
    const netplay::Chat &np_chat = pkt.chat();
    Player *pl = this->checkPeerPlayer(np_chat.plid(), peer);
    observer_.onChat(pl, np_chat.txt());
    netplay::Packet pkt_send;
    pkt_send.mutable_chat()->MergeFrom( np_chat );
    socket_.broadcastPacket(pkt_send);

  } else {
    throw netplay::CallbackError("invalid packet field");
  }
}


void ServerInstance::onGarbageAdd(const Garbage *gb, unsigned int pos)
{
  Player *pl_to = this->player(gb->to);
  Player *pl_from = this->player(gb->from);
  assert( pl_to != NULL );

  netplay::Packet pkt;
  netplay::Garbage *np_garbage = pkt.mutable_garbage();
  np_garbage->set_gbid( gb->gbid );
  np_garbage->set_pos( pos );
  np_garbage->set_plid_to( pl_to->plid() );
  np_garbage->set_plid_from( pl_from == NULL ? 0 : pl_from->plid() );
  np_garbage->set_type( static_cast<netplay::Garbage_Type>(gb->type) );
  np_garbage->set_size( gb->type == Garbage::TYPE_COMBO ? gb->size.x : gb->size.y );
  socket_.broadcastPacket(pkt);
}

void ServerInstance::onGarbageUpdateSize(const Garbage *gb)
{
  netplay::Packet pkt;
  netplay::Garbage *np_garbage = pkt.mutable_garbage();
  np_garbage->set_gbid( gb->gbid );
  np_garbage->set_size( gb->type == Garbage::TYPE_COMBO ? gb->size.x : gb->size.y );
  socket_.broadcastPacket(pkt);
}

void ServerInstance::onGarbageDrop(const Garbage *gb)
{
  Player *pl_to = this->player(gb->to);
  assert( pl_to != NULL );

  netplay::Packet pkt;
  netplay::Garbage *np_garbage = pkt.mutable_garbage();
  np_garbage->set_gbid(gb->gbid);
  np_garbage->set_wait(true);
  match_.waitGarbageDrop(gb); // note: reset the gbid
  socket_.broadcastPacket(pkt);

  // local player: drop immediately
  if( pl_to->local() ) {
    gb->to->dropNextGarbage();
    np_garbage->Clear();
    np_garbage->set_gbid(0);
    np_garbage->set_plid_to(pl_to->plid());
    np_garbage->set_drop(true);
    socket_.broadcastPacket(pkt);
  }
}


Player *ServerInstance::newPlayer(netplay::PeerSocket *peer, const std::string &nick)
{
  //TODO protected pointer
  Player *pl = new Player(this->nextPlayerId(), peer==NULL);
  LOG("init player: %d", pl->plid());
  pl->setNick(nick);
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
  netplay::Player *np_player = pkt.mutable_player();
  np_player->set_plid(pl->plid());
  np_player->set_nick(pl->nick());
  socket_.broadcastPacket(pkt, peer);
  if( peer != NULL ) {
    np_player->set_join(true);
    peer->writePacket(pkt);
  }

  return pl;
}

void ServerInstance::removePlayer(PlId plid)
{
  Player *pl = this->player(plid);
  assert( pl != NULL );
  observer_.onPlayerQuit(pl);
  players_.erase(plid);
  peers_.erase(plid);
  // tell other players
  netplay::Packet pkt;
  netplay::Player *np_player = pkt.mutable_player();
  np_player->set_plid(plid);
  np_player->set_out(true);
  socket_.broadcastPacket(pkt);
  // only one player left: abort the game
  if( players_.size() < 2 ) {
    this->stopMatch();
  }
}


void ServerInstance::processPacketInput(netplay::PeerSocket *peer, const netplay::Input &pkt_input)
{
  if( state_ != STATE_GAME ) {
    return;  // ignore remains of the previous match
  }
  Player *pl = this->checkPeerPlayer(pkt_input.plid(), peer);
  //XXX:check field may have win without player knowing it
  // this must not trigger an error
  Field *fld = pl->field();
  if( fld == NULL ) {
    throw netplay::CallbackError("player without a field");
  }

  Tick tick = pkt_input.tick();
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
  const int keys_nb = pkt_input.keys_size();
  for( int i=0; i<keys_nb; i++ ) {
    this->stepRemotePlayer(pl, pkt_input.keys(i));
    if( !match_.started() ) {
      return; // end of match
    }
  }
}

void ServerInstance::processPacketGarbage(netplay::PeerSocket *peer, const netplay::Garbage &pkt_gb)
{
  if( state_ != STATE_GAME ) {
    throw netplay::CallbackError("match is not running");
  }
  if( !pkt_gb.drop() ) {
    throw netplay::CallbackError("garbage drop expected");
  }
  if( !pkt_gb.has_plid_to() ) {
    throw netplay::CallbackError("player not set for garbage drop");
  }
  Player *pl = this->player(pkt_gb.plid_to());
  if( pl == NULL || pl->field() == NULL ) {
    throw netplay::CallbackError("invalid player");
  }
  this->checkPeerPlayer(pl->plid(), peer);

  Field *fld = pl->field();
  if( fld->waitingGarbages().size() == 0 ) {
    throw netplay::CallbackError("no garbage to drop");
  }

  netplay::Packet pkt_send;
  netplay::Garbage *np_garbage_send = pkt_send.mutable_garbage();
  np_garbage_send->set_gbid(0);
  np_garbage_send->set_plid_to(pl->plid());
  np_garbage_send->set_drop(true);
  socket_.broadcastPacket(pkt_send);

  fld->dropNextGarbage();
}

void ServerInstance::processPacketPlayer(netplay::PeerSocket *peer, const netplay::Player &pkt_pl)
{
  if( pkt_pl.join() ) {
    // new player, ignore all other fields
    if( state_ != STATE_LOBBY ) {
      throw netplay::CallbackError("match is running");
    } else if( players_.size() > conf_.pl_nb_max ) {
      //TODO don't close the connection if the server is full
      // only send back an error
      throw netplay::CallbackError("server full");
    }
    if( pkt_pl.plid() != 0 || pkt_pl.has_ready() ) {
      throw netplay::CallbackError("invalid fields");
    }
    this->newPlayer(peer, pkt_pl.has_nick() ? pkt_pl.nick() : "Player");

  } else if( pkt_pl.out() ) {
    // player out
    Player *pl = this->checkPeerPlayer(pkt_pl.plid(), peer);
    this->removePlayer(pl->plid()); // note: don't disconnect the peer

  } else {
    Player *pl = this->checkPeerPlayer(pkt_pl.plid(), peer);

    netplay::Packet pkt_send;
    netplay::Player *np_player = pkt_send.mutable_player();
    bool do_send = false;
    bool become_ready = false;
    if( pkt_pl.has_nick() && pkt_pl.nick() != pl->nick() ) {
      observer_.onPlayerChangeNick(pl, pkt_pl.nick());
      pl->setNick( pkt_pl.nick() );
      np_player->set_nick( pkt_pl.nick() );
      do_send = true;
    }
    if( pkt_pl.has_ready() && pkt_pl.ready() != pl->ready() ) {
      // check whether change is valid
      // (silently ignore invalid changes)
      if( state_ == STATE_LOBBY || state_ == STATE_READY ) {
        pl->setReady(pkt_pl.ready());
        observer_.onPlayerReady(pl);
        np_player->set_ready(pl->ready());
        become_ready = pl->ready();
        do_send = true;
      }
    }
    if( do_send ) {
      np_player->set_plid( pkt_pl.plid() );
      socket_.broadcastPacket(pkt_send);
    }

    if( become_ready ) {
      this->checkAllPlayersReady();
    }
  }
}


Player *ServerInstance::checkPeerPlayer(PlId plid, const netplay::PeerSocket *peer)
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
    if( (*it).second->ready() )
      nb_ready++;
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
  netplay::Server *np_server = pkt.mutable_server();
  np_server->set_state( static_cast<netplay::Server::State>(state) );
  socket_.broadcastPacket(pkt);

  if( state != STATE_INIT ) {
    // ready_ flag is needed in prepareMatch(), don't reset it now
    PlayerContainer::iterator it;
    for( it=players_.begin(); it!=players_.end(); ++it )
      (*it).second->setReady(false);
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
    Player *pl = (*it).second;
    if( ! pl->ready() ) {
      continue;
    }
    Field *fld = match_.newField(seed);
    pl->setField(fld);
    fld->setConf( default_field_conf ); //XXX:temp
    fld->fillRandom(6);

    netplay::Field *np_field = pkt.mutable_field();
    np_field->Clear();
    np_field->set_plid(pl->plid());
    np_field->set_seed(fld->seed()); //note: seed changed due to fillRandom()
    np_field->set_rank(0);

    // conf
    netplay::Field::Conf *np_conf = np_field->mutable_conf();
#define FIELD_CONF_EXPR_INIT(n) \
    np_conf->set_##n( fld->conf().n );
    FIELD_CONF_APPLY(FIELD_CONF_EXPR_INIT);
#undef FIELD_CONF_EXPR_INIT
    np_conf->set_raise_adjacent( static_cast<netplay::Field_Conf_RaiseAdjacent>(fld->conf().raise_adjacent) );
    // grid
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


void ServerInstance::doStepPlayer(Player *pl, KeyState keys)
{
  Tick prev_tick = pl->field()->tick();
  GameInstance::doStepPlayer(pl, keys);

  //XXX Current implementation does not group Input packets.
  netplay::Packet pkt_send;
  netplay::Input *np_input_send = pkt_send.mutable_input();
  np_input_send->set_plid(pl->plid());
  np_input_send->set_tick(prev_tick);
  np_input_send->add_keys(keys);

  // don't send packet back to remote players
  PeerContainer::const_iterator peer_it = peers_.find(pl->plid());
  netplay::PeerSocket *peer = peer_it == peers_.end() ? NULL : (*peer_it).second;
  socket_.broadcastPacket(pkt_send, peer);
  pkt_send.clear_input(); // packet reused below

  // update garbages
  //TODO check for clients which never send back the drop packets
  gb_distributor_.updateGarbages(pl->field());

  // ranking
  std::vector<const Field *> ranked;
  bool end_of_match = match_.updateRanks(ranked);
  netplay::Field *np_field_send = pkt_send.mutable_field();
  std::vector<const Field *>::iterator it;
  for(it=ranked.begin(); it!=ranked.end(); ++it) {
    Player *pl = this->player(*it);
    // don't send packet for aborted fields (no player)
    if( pl != NULL ) {
      np_field_send->set_plid( pl->plid() );
      np_field_send->set_rank( (*it)->rank() );
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
    if( current_plid_ <= 0 ) // overflow
      current_plid_ = 1;
    if( players_.find(current_plid_) == players_.end() )
      break; // not already in use
  }
  return current_plid_;
}


