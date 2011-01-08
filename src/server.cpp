#include <boost/bind.hpp>
#include "server.h"
#include "netplay.pb.h"
#include "config.h"
#include "log.h"



void ServerConf::toDefault()
{
  const netplay::Server::Conf &np_conf = netplay::Server::Conf::default_instance();
#define SERVER_CONF_EXPR_INIT(n,ini,t) \
  n = np_conf.n();
  SERVER_CONF_APPLY(SERVER_CONF_EXPR_INIT);
#undef SERVER_CONF_EXPR_INIT
}


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
  /* gb_wait_tk     */  90+60,
  /* flash_tk       */  36,
  /* levitate_tk    */  12,
  /* pop_tk         */   8,
  /* pop0_tk        */  19,
  /* transform_tk   */  14,
  /* color_nb       */   6,
  /* raise_adjacent */ FieldConf::ADJACENT_ALWAYS,
};


ServerInstance::ServerInstance(boost::asio::io_service &io_service):
    socket_(*this, io_service), gb_distributor_(match_, *this),
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
  LOG("starting server on port %d", port);
  socket_.start(port);
  state_ = STATE_LOBBY;
}


Player *ServerInstance::newLocalPlayer()
{
  return this->newPlayer(NULL);
}


void ServerInstance::onPeerConnect(netplay::PeerSocket *peer)
{
  if( state_ != STATE_LOBBY ) {
    throw netplay::CallbackError("match is running");
  } else if( players_.size() > conf_.pl_nb_max ) {
    throw netplay::CallbackError("server full");
  }
  this->newPlayer(peer);
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
    /*TODO Player *pl =*/ this->checkPeerPlayer(np_chat.plid(), peer);
    //TODO intf_.onChat(pl, np_chat.txt());
    netplay::Packet pkt_send;
    pkt_send.mutable_chat()->MergeFrom( np_chat );
    socket_.broadcastPacket(pkt_send);

  } else {
    throw netplay::CallbackError("invalid packet field");
  }
}


void ServerInstance::onGarbageAdd(const Garbage *gb, unsigned int pos)
{
  Player *pl_to = this->field2player(gb->to);
  Player *pl_from = this->field2player(gb->from);
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
  Player *pl_to = this->field2player(gb->to);
  assert( pl_to != NULL );
  PeerContainer::const_iterator peer_it = peers_.find(pl_to->plid());

  netplay::Packet pkt;
  netplay::Garbage *np_garbage = pkt.mutable_garbage();
  np_garbage->set_gbid( gb->gbid );
  np_garbage->set_drop( true );
  // local player: drop immediately
  if( peer_it == peers_.end() ) {
    gb->to->dropNextGarbage();
    socket_.broadcastPacket(pkt);
  } else {
    (*peer_it).second->writePacket(pkt);
  }
}


Player *ServerInstance::newPlayer(netplay::PeerSocket *peer)
{
  //TODO protected pointer
  Player *pl = new Player(this->nextPlayerId());
  LOG("init player: %d", pl->plid());
  //XXX better default nick handling (or don't use default at all?)
  pl->setNick("Player");
  //TODO intf_.onPlayerJoined(pl);

  // packet reused
  netplay::Packet pkt;

  if( peer != NULL ) {
    // send init to the new player (server info)
    netplay::Server *np_server = pkt.mutable_server();
    np_server->set_state( static_cast<netplay::Server::State>(state_) );
    netplay::Server::Conf *np_conf = np_server->mutable_conf();
#define SERVER_CONF_EXPR_PKT(n,ini,t) \
    np_conf->set_##n(conf_.n);
    SERVER_CONF_APPLY(SERVER_CONF_EXPR_PKT);
#undef SERVER_CONF_EXPR_PKT
    peer->writePacket(pkt);
    pkt.clear_server();
  }

  // inform the new player about his ID (if remote)
  // tell others about their new friend
  netplay::Player *np_player = pkt.mutable_player();
  np_player->set_plid(pl->plid());
  np_player->set_nick(pl->nick());
  if( peer != NULL ) {
    peer->writePacket(pkt);
  }
  socket_.broadcastPacket(pkt);
  // player field not cleared (overwritten just below)

  if( peer != NULL ) {
    // send information on other players to the new one
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

  // put accepted player with his friends
  PlId plid = pl->plid(); // use a temporary value to help g++
  players_.insert(plid, pl);
  if( peer != NULL ) {
    peers_[plid] = peer; // associated player to its peer
  }

  return pl;
}

void ServerInstance::removePlayer(PlId plid)
{
  //TODO intf_.onPlayerQuit(pl);
  players_.erase(plid);
  peers_.erase(plid);
  // tell other players
  netplay::Packet pkt;
  netplay::Player *np_player = pkt.mutable_player();
  np_player->set_plid(plid);
  np_player->set_out(true);
  socket_.broadcastPacket(pkt);
}


void ServerInstance::processPacketInput(netplay::PeerSocket *peer, const netplay::Input &pkt_input)
{
  if( state_ != STATE_GAME ) {
    throw netplay::CallbackError("game is not running");
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
  //XXX return earlier if match ended
  // skipped frames
  while( fld->tick() < tick ) {
    //TODO throw exceptions on error in stepField()
    this->stepField(pl, 0);
    //TODO intf_.onFieldStep(fld);
  }
  // given frames
  const int keys_nb = pkt_input.keys_size();
  for( int i=0; i<keys_nb; i++ ) {
    this->stepField(pl, pkt_input.keys(i));
    //TODO intf_.onFieldStep(fld);
  }

  if( !match_.started() ) {
    this->stopMatch();
  }
}

void ServerInstance::processPacketGarbage(netplay::PeerSocket *peer, const netplay::Garbage &pkt_gb)
{
  if( state_ != STATE_GAME ) {
    throw netplay::CallbackError("game is not running");
  }
  if( !pkt_gb.drop() ) {
    throw netplay::CallbackError("garbage drop expected");
  }

  const Match::GbWaitingMap gbs_wait = match_.waitingGarbages();
  Match::GbWaitingMap::const_iterator it = gbs_wait.find(pkt_gb.gbid());
  if( it == gbs_wait.end() ) {
    throw netplay::CallbackError("garbage not found");
  }
  Garbage *gb = (*it).second;

  Player *pl = this->field2player(gb->to);
  assert( pl != NULL );
  this->checkPeerPlayer(pl->plid(), peer);
  if( gb->to->waitingGarbage(0).gbid == gb->gbid ) {
    throw netplay::CallbackError("unexpected dropped garbage");
  }
  assert( gb->to->waitingGarbage(0).gbid == gb->gbid );

  netplay::Packet pkt_send;
  netplay::Garbage *np_garbage_send = pkt_send.mutable_garbage();
  np_garbage_send->set_gbid( gb->gbid );
  np_garbage_send->set_drop(true);
  socket_.broadcastPacket(pkt_send);

  match_.dropGarbage(gb);
}

void ServerInstance::processPacketPlayer(netplay::PeerSocket *peer, const netplay::Player &pkt_pl)
{
  Player *pl = this->checkPeerPlayer(pkt_pl.plid(), peer);
  if( pkt_pl.has_out() && pkt_pl.out() ) {
    // player out, ignore all other fields
    this->removePlayer(pl->plid()); // note: don't disconnect the peer
    return;
  }

  netplay::Packet pkt_send;
  netplay::Player *np_player = pkt_send.mutable_player();
  bool do_send = false;
  bool become_ready = false;
  if( pkt_pl.has_nick() && pkt_pl.nick() != pl->nick() ) {
    //TODO intf_.onPlayerChangeNick(pl, pkt_pl.nick());
    pl->setNick( pkt_pl.nick() );
    np_player->set_nick( pkt_pl.nick() );
    do_send = true;
  }
  if( pkt_pl.has_ready() && pkt_pl.ready() != pl->ready() ) {
    // check whether change is valid
    // (silently ignore invalid changes)
    if( state_ == STATE_LOBBY || state_ == STATE_READY ) {
      pl->setReady(pkt_pl.ready());
      //TODO intf_.onPlayerReady(pl);
      np_player->set_ready(pl->ready());
      become_ready = pl->ready();
      do_send = true;
    }
  }
  if( do_send ) {
    np_player->set_plid( pkt_pl.plid() );
    socket_.broadcastPacket(pkt_send);
  }

  // another player is ready, check if all are
  if( become_ready ) {
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
}


Player *ServerInstance::checkPeerPlayer(PlId plid, const netplay::PeerSocket *peer)
{
  PeerContainer::const_iterator peer_it = peers_.find(plid);
  if( peer_it == peers_.end() || (*peer_it).second != peer ) {
    throw netplay::CallbackError("invalid player");
  }
  PlayerContainer::iterator pl_it = players_.find(plid);
  assert( pl_it != players_.end() );
  return (*pl_it).second;
}


void ServerInstance::setState(State state)
{
  netplay::Packet pkt;
  netplay::Server *np_server = pkt.mutable_server();
  np_server->set_state( static_cast<netplay::Server::State>(state) );
  socket_.broadcastPacket(pkt);

  if( state != STATE_INIT ) {
    // ready_ flag is needed in prepareMatch(), don't reset it now
    //TODO always needed?
    PlayerContainer::iterator it;
    for( it=players_.begin(); it!=players_.end(); ++it )
      (*it).second->setReady(false);
  }
  state_ = state;
}

void ServerInstance::prepareMatch()
{
  LOG("prepare match");
  //XXX check ready player count (there should be at least 1 player)

  this->setState(STATE_INIT);
  //TODO intf_.onMatchInit(&match_);

  int seed = ::rand(); // common seed for all fields
  netplay::Packet pkt;

  //TODO recheck this part
  PlayerContainer::iterator it;
  for( it=players_.begin(); it!=players_.end(); ++it ) {
    Player *pl = (*it).second;
    assert( pl != NULL );
    if( ! pl->ready() ) {
      continue;
    }
    Field *fld = match_.newField(seed);
    pl->setField(fld);
    fld->setConf( default_field_conf ); //XXX:temp
    assert( fld->conf().gb_wait_tk > conf_.tk_lag_max );
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
  //TODO intf_.onMatchReady(&match_);
}

void ServerInstance::startMatch()
{
  LOG("start match");

  match_.start();
  /*TODO
  FieldContainer::iterator it;
  for( it=fields_.begin(); it!=fields_.end(); ++it ) {
    targets_chain_[&(*it)] = it;
    targets_combo_[&(*it)] = it;
  }
  */
  this->setState(STATE_GAME);
  //TODO intf_.onMatchStart(&match_);
}

void ServerInstance::stopMatch()
{
  LOG("stop match");

  PlayerContainer::iterator it;
  for(it=players_.begin(); it!=players_.end(); ++it) {
    (*it).second->setField(NULL);
  }
  match_.stop();
  /*TODO
  targets_chain_.clear();
  targets_combo_.clear();
  */
  this->setState(STATE_LOBBY);
  //TODO intf_.onMatchEnd(&match_);
}


void ServerInstance::stepField(Player *pl, KeyState keys)
{
  if( !match_.started() ) {
    return;  //XXX:check match ended
  }
  //XXX Current implementation does not group Input packets.
  Field *fld = pl->field();
  if( fld->lost() ) {
    throw netplay::CallbackError("field lost, cannot step");
  }
  Tick prev_tick = fld->tick();
  if( prev_tick+1 >= match_.tick() + conf_.tk_lag_max ) {
    throw netplay::CallbackError("maximum lag exceeded");
  }

  fld->step(keys);
  if( prev_tick == match_.tick() ) {
    // don't update tick_ when it will obviously not be modified
    //XXX:check condition
    match_.updateTick();
  }

  netplay::Packet pkt_send;
  netplay::Input *np_input_send = pkt_send.mutable_input();
  np_input_send->set_plid(pl->plid());
  np_input_send->set_tick(prev_tick);
  np_input_send->add_keys(keys);
  socket_.broadcastPacket(pkt_send);
  pkt_send.clear_input(); // packet reused below

  // update garbages
  //TODO check for clients which never send back the drop packets
  gb_distributor_.updateGarbages(fld);

  //TODO do ranking in match
  // compute ranks, check for end of match
  // common case: no draw, no simultaneous death, 1 tick at a time
  // ranking algorithm does not have to be optimized for special cases
  std::vector<Field *> to_rank;
  to_rank.reserve(fields_.size());
  unsigned int no_rank_nb = 0;
  Match::FieldContainer::iterator it;
  for( it=fields_.begin(); it!=fields_.end(); ++it ) {
    if( it->rank() != 0 )
      continue;
    no_rank_nb++;
    //XXX rank aborted fields in Match::removeField() ?
    if( it->lost() && it->tick() <= match_.tick() )
      to_rank.push_back(&(*it));
  }

  if( !to_rank.empty() ) {
    // best player first (lowest tick)
    std::sort(to_rank.begin(), to_rank.end(),
              boost::bind(&Field::tick, _1) <
              boost::bind(&Field::tick, _2));
    unsigned int rank = no_rank_nb - to_rank.size() + 1;
    netplay::Field *np_field_send = pkt_send.mutable_field();
    std::vector<Field *>::iterator it;
    for( it=to_rank.begin(); it!=to_rank.end(); ++it ) {
      if( it!=to_rank.begin() && (*it)->tick() == (*(it-1))->tick() )
        (*it)->setRank( (*(it-1))->rank() );
      else
        (*it)->setRank( rank );
      // don't send packet for aborted fields (no player)
      if( (*it)->player() != NULL ) {
        np_field_send->set_plid((*it)->player()->plid());
        np_field_send->set_rank( (*it)->rank() );
        socket_.broadcastPacket(pkt_send);
      }
      rank++;
      no_rank_nb--;
    }
  }

  // one (or no) remaining player: end of match
  if( no_rank_nb < 2 ) {
    netplay::Field *np_field_send = pkt_send.mutable_field();
    Match::FieldContainer::iterator it;
    for( it=fields_.begin(); it!=fields_.end(); ++it ) {
      if( (*it).rank() == 0 )
        continue;
      it->setRank(1);
      // don't send packet for aborted fields (no player)
      if( it->player() != NULL ) {
        np_field_send->set_plid(it->player()->plid());
        np_field_send->set_rank( it->rank() );
        socket_.broadcastPacket(pkt_send);
      }
      break;
    }

    this->stopMatch(); //XXX is this a good idea to do it here?
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

Player *ServerInstance::field2player(const Field *fld)
{
  //XXX use a map on the instance to optimize?
  PlayerContainer::iterator it;
  for(it=players_.begin(); it!=players_.end(); ++it) {
    if( fld == (*it).second->field() ) {
      return (*it).second;
    }
  }
}


