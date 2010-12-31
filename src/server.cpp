#include <boost/bind.hpp>
#include <boost/asio/placeholders.hpp>
#include "config.h"
#include "interface.h"
#include "server.h"
#include "log.h"

namespace asio = boost::asio;
using namespace asio::ip;


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



ServerPlayer::ServerPlayer(Server *server, PlId plid): Player(plid),
    netplay::PacketSocket(server->io_service()), server_(server),
    has_error_(false)
{
  this->setPktSizeMax( server->conf().pkt_size_max );
}

void ServerPlayer::onError(const char *msg, const boost::system::error_code &ec)
{
  if( has_error_ )
    return;
  has_error_ = true;
  if( ec ) {
    LOG("ServerPlayer[%u]: %s: %s", plid(), msg, ec.message().c_str());
  } else {
    LOG("ServerPlayer[%u]: %s", plid(), msg);
  }
  server_->removePlayer(this);
}

bool ServerPlayer::onPacketReceived(const netplay::Packet &pkt)
{
  return server_->processPacket(this, pkt);
}


const std::string Server::CONF_SECTION("Server");


Server::Server(ServerInterface &intf, asio::io_service &io_service):
    state_(STATE_NONE), match_(*this), accepted_player_(NULL), intf_(intf),
    io_service_(io_service), acceptor_(io_service), current_plid_(0)
{
}

Server::~Server()
{
  delete accepted_player_;
}


void Server::start(int port)
{
  LOG("starting server on port %d", port);
  tcp::endpoint endpoint(tcp::v4(), port);
  acceptor_.open(endpoint.protocol());
  acceptor_.set_option(asio::socket_base::reuse_address(true));
  acceptor_.bind(endpoint);
  acceptor_.listen();
  state_ = STATE_ROOM;
  this->acceptNext();
}

void Server::loadConf(const Config &cfg)
{
  assert( ! this->isStarted() );
  //XXX signed/unsigned not checked
#define SERVER_CONF_EXPR_LOAD(n,ini,t) \
  conf_.n = cfg.get##t(CONF_SECTION, #ini, conf_.n);
  SERVER_CONF_APPLY(SERVER_CONF_EXPR_LOAD);
#undef SERVER_CONF_EXPR_LOAD
}


void Server::initAcceptedPlayer()
{
  assert( state_ == STATE_ROOM );
  assert( accepted_player_ != NULL );

  LOG("init player: %d", accepted_player_->plid());
  try {
    accepted_player_->socket().set_option(tcp::no_delay(true));
  } catch(const boost::exception &e) {
    // setting no delay may fail on some systems, ignore error
  }
  //XXX better default nick handling (or don't use default at all?)
  accepted_player_->setNick("Player");
  intf_.onPlayerJoined(accepted_player_);

  // send init to the new player

  // send server info
  netplay::Packet pkt;
  netplay::Server *np_server = pkt.mutable_server();
  np_server->set_state( static_cast<netplay::Server::State>(state_) );
  netplay::Server::Conf *np_conf = np_server->mutable_conf();
#define SERVER_CONF_EXPR_PKT(n,ini,t) \
  np_conf->set_##n(conf_.n);
  SERVER_CONF_APPLY(SERVER_CONF_EXPR_PKT);
#undef SERVER_CONF_EXPR_PKT
  accepted_player_->writePacket(pkt);
  pkt.clear_server();

  // inform the new player about is ID
  // tell others about their new friend
  netplay::Player *np_player = pkt.mutable_player();
  np_player->set_plid(accepted_player_->plid());
  np_player->set_nick(accepted_player_->nick());
  accepted_player_->writePacket(pkt);
  this->broadcastPacket(pkt);
  // player field not cleared (overwritten just below)

  // send information on other clients to the new one
  PlayerContainer::const_iterator it;
  for( it=players_.begin(); it!=players_.end(); ++it ) {
    np_player->set_plid( (*it).second->plid() );
    np_player->set_nick( (*it).second->nick() );
    np_player->set_ready( (*it).second->ready() );
    accepted_player_->writePacket(pkt);
  }

  // set read handler
  accepted_player_->readNext();

  // put accepted player with his friends
  PlId plid = accepted_player_->plid(); // use a temporary value to help g++
  players_.insert(plid, accepted_player_);
  accepted_player_ = NULL;
}


void Server::setState(State state)
{
  netplay::Packet pkt;
  netplay::Server *np_server = pkt.mutable_server();
  np_server->set_state( static_cast<netplay::Server::State>(state) );
  this->broadcastPacket(pkt);

  if( state != STATE_INIT ) {
    // ready_ flag is needed in prepareMatch(), don't reset it now
    // this should change
    PlayerContainer::iterator it;
    for( it=players_.begin(); it!=players_.end(); ++it )
      (*it).second->setReady(false);
  }
  state_ = state;
}


void Server::prepareMatch()
{
  LOG("prepare match");
  //XXX check ready player count (there should be at least 1 player)

  this->setState(STATE_INIT);
  intf_.onMatchInit(&match_);

  int seed = ::rand(); // common seed for all fields
  netplay::Packet pkt;

  PlayerContainer::iterator it;
  for( it=players_.begin(); it!=players_.end(); ++it ) {
    Player *pl = (*it).second;
    assert( pl != NULL );
    if( ! pl->ready() )
      continue;
    Field *fld = match_.addField(pl, seed);
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
    this->broadcastPacket(pkt);
  }

  this->setState(STATE_READY);
  intf_.onMatchReady(&match_);
}

void Server::startMatch()
{
  LOG("start match");

  match_.start();
  this->setState(STATE_GAME);
  intf_.onMatchStart(&match_);
}

PlId Server::nextPlayerId()
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

void Server::acceptNext(void)
{
  assert( accepted_player_ == NULL );
  PlId plid = this->nextPlayerId();
  accepted_player_ = new ServerPlayer(this, plid);
  acceptor_.async_accept(
      accepted_player_->socket(), accepted_player_->peer(),
      boost::bind(&Server::onAccept, this, asio::placeholders::error));
}

void Server::onAccept(const boost::system::error_code &ec)
{
  if( !ec ) {
    const char *error_txt = NULL;
    if( state_ != STATE_ROOM ) {
      error_txt = "match is running";
    } else if( players_.size() > conf_.pl_nb_max ) {
      error_txt = "server full";
    }
    if( error_txt != NULL ) {
      this->removePlayerWithError(accepted_player_, error_txt);
    } else {
      this->initAcceptedPlayer();
    }
  } else {
    this->removePlayer(accepted_player_);
  }
  this->acceptNext();
}

void Server::broadcastPacket(const netplay::Packet &pkt)
{
  const std::string s = netplay::PacketSocket::serializePacket(pkt);
  PlayerContainer::iterator it;
  for( it=players_.begin(); it!=players_.end(); ++it ) {
    (*it).second->writeRaw(s);
  }
}

void Server::removePlayerAfterWrites(ServerPlayer *pl)
{
  if( pl == accepted_player_ ) {
    removed_players_.push_back( pl );
    accepted_player_ = NULL;
  } else {
    PlayerContainer::iterator it = players_.find(pl->plid());
    assert( it != players_.end() );
    removed_players_.push_back( players_.release(it).release() );
    // tell other players
    netplay::Packet pkt;
    netplay::Player *np_player = pkt.mutable_player();
    np_player->set_plid(pl->plid());
    np_player->set_out(true);
    this->broadcastPacket(pkt);
  }
  if( pl->socket().is_open() ) {
    pl->closeAfterWrites();
  }
}

void Server::removePlayerWithError(ServerPlayer *pl, const char *msg)
{
  netplay::Packet pkt;
  netplay::Notification *notif = pkt.mutable_notification();
  notif->set_txt(msg);
  notif->set_severity(netplay::Notification::ERROR);
  pl->writePacket(pkt);
  this->removePlayerAfterWrites(pl);
}

void Server::removePlayer(ServerPlayer *pl)
{
  intf_.onPlayerQuit(pl);
  if( pl->field() != NULL )
    match_.removeField(pl->field());
  this->removePlayerAfterWrites(pl);
  if( pl->socket().is_open() )
    pl->socket().close();
  io_service_.post(boost::bind(&Server::freePlayerHandler, this, pl));
}

void Server::freePlayerHandler(ServerPlayer *pl)
{
  io_service_.poll(); // make sure to "flush" aborted handlers
  boost::ptr_vector<ServerPlayer>::iterator it;
  for( it=removed_players_.begin(); it!=removed_players_.end(); ++it ) {
    if( &(*it) == pl )
      break;
  }
  assert( it != removed_players_.end() );
  removed_players_.erase(it);
}


bool Server::processPacket(ServerPlayer *pl, const netplay::Packet &pkt)
{
  // minor optimization: only check IsInitialized() of the "used" field.
  if( pkt.has_input() ) {
    const netplay::Input &np_input = pkt.input();
    if( state_ != STATE_GAME || !np_input.IsInitialized()
       || np_input.plid() != pl->plid())
      return false;
    Field *fld = pl->field();
    if( fld == NULL )
      return false;
    if( !match_.processInput(pl, np_input) )
      return false;
    //TODO one call per tick
    intf_.onFieldStep(fld);
    if( !match_.started() ) {
      intf_.onMatchEnd(&match_); //XXX before match_.stop()?
      this->setState(STATE_ROOM);
    }

  } else if( pkt.has_garbage() ) {
    const netplay::Garbage &np_garbage = pkt.garbage();
    if( state_ != STATE_GAME || !np_garbage.IsInitialized() )
      return false;
    if( !match_.processGarbage(pl, pkt.garbage()) )
      return false;

  } else if( pkt.has_player() ) {
    const netplay::Player &np_player = pkt.player();
    if( !np_player.IsInitialized() || np_player.plid() != pl->plid() )
      return false;
    if( np_player.has_out() && np_player.out() ) {
      // player out, ignore all other fields
      this->removePlayer(pl);
    } else {
      netplay::Packet pkt_send;
      netplay::Player *np_player_send = pkt_send.mutable_player();
      bool do_send = false;
      bool become_ready = false;
      if( np_player.has_nick() && np_player.nick() != pl->nick() ) {
        const std::string old_nick = pl->nick();
        pl->setNick( np_player.nick() );
        intf_.onPlayerSetNick(pl, old_nick);
        np_player_send->set_nick( np_player.nick() );
        do_send = true;
      }
      if( np_player.has_ready() && np_player.ready() != pl->ready() ) {
        // check whether change is valid
        // (silently ignore invalid changes)
        if( state_ == STATE_ROOM || state_ == STATE_READY ) {
          pl->setReady(np_player.ready());
          intf_.onPlayerReady(pl);
          np_player_send->set_ready(pl->ready());
          become_ready = pl->ready();
          do_send = true;
        }
      }
      if( do_send ) {
        np_player_send->set_plid( np_player.plid() );
        this->broadcastPacket(pkt_send);
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
          if( state_ == STATE_ROOM ) {
            this->prepareMatch();
          } else if( state_ == STATE_READY ) {
            this->startMatch();
          }
        }
      }
    }

  } else if( pkt.has_chat() ) {
    const netplay::Chat &chat = pkt.chat();
    if( !chat.IsInitialized() || chat.plid() != pl->plid() )
      return false;
    intf_.onChat(pl, chat.txt());
    netplay::Packet pkt_send;
    pkt_send.mutable_chat()->MergeFrom( chat );
    this->broadcastPacket(pkt_send);

  } else {
    return false; // invalid packet field
  }
  return true;
}


ServerMatch::ServerMatch(Server &server):
    server_(server), current_gbid_(0)
{
}

void ServerMatch::start()
{
  Match::start();
  FieldContainer::iterator it;
  for( it=fields_.begin(); it!=fields_.end(); ++it ) {
    targets_chain_[&(*it)] = it;
    targets_combo_[&(*it)] = it;
  }
}

void ServerMatch::stop()
{
  targets_chain_.clear();
  targets_combo_.clear();
  Match::stop();
}


bool ServerMatch::processInput(ServerPlayer *pl, const netplay::Input &np_input)
{
  //XXX:check field may have win without player knowing it
  // this must not trigger an error
  Field *fld = pl->field();
  assert( fld != NULL );
  Tick tick = np_input.tick();
  if( tick < fld->tick() )
    return false;

  //XXX return earlier if match ended
  // skipped frames
  while( fld->tick() < tick ) {
    if( !this->stepField(pl, 0) )
      return false;
  }
  // given frames
  const int keys_nb = np_input.keys_size();
  for( int i=0; i<keys_nb; i++ ) {
    if( !this->stepField(pl, np_input.keys(i)) )
      return false;
  }
  return true;
}

bool ServerMatch::processGarbage(ServerPlayer *pl, const netplay::Garbage &np_garbage)
{
  if( !np_garbage.drop() )
    return false;
  GbWaitingMap::iterator it = gbs_wait_.find(np_garbage.gbid());
  if( it == gbs_wait_.end() || (*it).second->to != pl->field() )
    return false;
  netplay::Packet pkt_send;
  netplay::Garbage *np_garbage_send = pkt_send.mutable_garbage();
  np_garbage_send->set_gbid( (*it).second->gbid );
  np_garbage_send->set_drop(true);
  assert( pl->field()->waitingGarbage(0).gbid == (*it).second->gbid );
  gbs_wait_.erase(it);
  pl->field()->dropNextGarbage();
  server_.broadcastPacket(pkt_send);
  return true;
}


bool ServerMatch::stepField(ServerPlayer *pl, KeyState keys)
{
  if( !this->started() )
    return true; // match ended XXX:check
  //XXX Current implementation does not group Input packets.
  Field *fld = pl->field();
  if( fld->lost() )
    return false;
  Tick prev_tick = fld->tick();
  if( prev_tick+1 >= this->tick() + server_.conf().tk_lag_max )
    return false;

  fld->step(keys);
  if( prev_tick == this->tick() ) {
    // don't update tick_ when it will obviously not be modified
    this->updateTick();
  }

  netplay::Packet pkt_send;
  netplay::Input *np_input_send = pkt_send.mutable_input();
  np_input_send->set_plid(pl->plid());
  np_input_send->set_tick(prev_tick);
  np_input_send->add_keys(keys);
  server_.broadcastPacket(pkt_send);
  pkt_send.clear_input(); // packet reused for ranking

  // cancel chain garbage
  if( fld->chain() < 2 ) {
    GbChainMap::iterator it = gbs_chain_.find(fld);
    if( it != gbs_chain_.end() )
      gbs_chain_.erase(it);
  }

  this->distributeGarbages(pl);
  //TODO check for clients which never send back the drop packets
  this->dropGarbages(pl);

  // compute ranks, check for end of match
  // common case: no draw, no simultaneous death, 1 tick at a time
  // ranking algorithm does not have to be optimized for special cases
  std::vector<Field *> to_rank;
  to_rank.reserve(fields_.size());
  unsigned int no_rank_nb = 0;
  FieldContainer::iterator it;
  for( it=fields_.begin(); it!=fields_.end(); ++it ) {
    if( it->rank() != 0 )
      continue;
    no_rank_nb++;
    //XXX rank aborted fields in Match::removeField() ?
    if( it->lost() && it->tick() <= this->tick() )
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
        server_.broadcastPacket(pkt_send);
      }
      rank++;
      no_rank_nb--;
    }
  }

  // one (or no) remaining player: end of match
  if( no_rank_nb < 2 ) {
    netplay::Field *np_field_send = pkt_send.mutable_field();
    FieldContainer::iterator it;
    for( it=fields_.begin(); it!=fields_.end(); ++it ) {
      if( (*it).rank() == 0 )
        continue;
      it->setRank(1);
      // don't send packet for aborted fields (no player)
      if( it->player() != NULL ) {
        np_field_send->set_plid(it->player()->plid());
        np_field_send->set_rank( it->rank() );
        server_.broadcastPacket(pkt_send);
      }
      break;
    }

    this->stop(); //XXX is this a good idea to do it here?
  }

  return true;
}


void ServerMatch::distributeGarbages(ServerPlayer *pl)
{
  Field *fld = pl->field();
  const Field::StepInfo info = fld->stepInfo();

  if( info.combo == 0 )
    return; // no match, no new garbages
  // check if there is an opponent
  // if there is only one, keep it (faster target choice for 2-player game)
  Field *single_opponent = NULL;
  FieldContainer::iterator it;
  for( it=fields_.begin(); it!=fields_.end(); ++it ) {
    if( &(*it) == fld || it->lost() )
      continue;
    if( single_opponent == NULL ) {
      single_opponent = &(*it);
    } else {
      single_opponent = NULL;
      break;
    }
  }
  if( single_opponent == NULL && it==fields_.end() )
    return; // no opponent, no target

  // note: opponent count will never increase, we don't have to update targets

  void updateTick();
  // maps it if there is only one opponent

  if( info.chain == 2 ) {
    Field *target_fld = single_opponent;
    if( target_fld == NULL ) {
      // get player with the least chain garbages
      GbTargetMap::iterator targets_it = targets_chain_.find(fld);
      assert( targets_it != targets_chain_.end() );
      FieldContainer::iterator it = (*targets_it).second;
      unsigned int min = -1; // overflow: max value
      FieldContainer::iterator it_min = fields_.end();
      do {
        if( it == fields_.end() )
          it = fields_.begin();
        Field *fld2 = &(*it);
        if( fld2 == fld || fld2->lost() )
          continue;
        size_t nb_chain = 0;
        const size_t nb_gb = fld2->waitingGarbageCount();
        for( nb_chain=0; nb_chain<nb_gb; nb_chain++ ) {
          // chain garbages are put at the beginning
          if( fld2->waitingGarbage(nb_chain).type != Garbage::TYPE_CHAIN )
            break;
        }
        if( nb_chain < min ) {
          min = nb_chain;
          target_fld = fld2;
          it_min = it;
          if( min == 0 )
            break; // 0 is the least we can found
        }
      } while( it != (*targets_it).second );
      assert( target_fld != NULL );
      (*targets_it).second = it_min;
    }
    this->addGarbage(fld, target_fld, Garbage::TYPE_CHAIN, 1);

  } else if( info.chain > 2 ) {
    this->increaseChainGarbage(fld);
  }

  // combo garbage
  // with a width of 6, values match the original PdP rules
  if( info.combo > 3 ) {
    Field *target_fld = single_opponent;
    if( target_fld == NULL ) {
      // get the next target player
      GbTargetMap::iterator targets_it = targets_combo_.find(fld);
      assert( targets_it != targets_combo_.end() );
      FieldContainer::iterator it;
      for( it=++(*targets_it).second; ; ++it ) {
        if( it == fields_.end() )
          it = fields_.begin();
        target_fld = &(*it);
        if( target_fld == fld || target_fld->lost() )
          continue;
        (*targets_it).second = it;
        break;
      }
      assert( target_fld != NULL );
    }

    if( info.combo-1 <= FIELD_WIDTH ) {
      // one block
      this->addGarbage(fld, target_fld, Garbage::TYPE_COMBO, info.combo-1);
    } else if( info.combo <= 2*FIELD_WIDTH ) {
      // two blocks
      unsigned int n = (info.combo > FIELD_WIDTH*3/2) ? info.combo : info.combo-1;
      this->addGarbage(fld, target_fld, Garbage::TYPE_COMBO, n/2);
      this->addGarbage(fld, target_fld, Garbage::TYPE_COMBO, n/2+n%2);
    } else {
      // n blocks
      unsigned int n;
      if( info.combo == 2*FIELD_WIDTH+1 )
        n = 3;
      else if( info.combo <= 3*FIELD_WIDTH+1 )
        n = 4;
      else if( info.combo <= 4*FIELD_WIDTH+2 )
        n = 6;
      else
        n = 8;
      while( n-- > 0 )
        this->addGarbage(fld, target_fld, Garbage::TYPE_COMBO, FIELD_WIDTH);
    }
  }
}

void ServerMatch::dropGarbages(ServerPlayer *pl)
{
  const Field *fld = pl->field();
  const size_t n = fld->waitingGarbageCount();
  for( size_t i=0; i<n; i++ ) {
    const Garbage &gb = fld->waitingGarbage(i);
    if( gb.ntick == 0 )
      continue; // already being dropped
    if( gb.ntick <= fld->tick() ) {
      GbWaitingMap::iterator it = gbs_wait_.find(gb.gbid);
      assert( it != gbs_wait_.end() );
      (*it).second->ntick = 0;
      netplay::Packet pkt_send;
      netplay::Garbage *np_garbage = pkt_send.mutable_garbage();
      np_garbage->set_gbid(gb.gbid);
      np_garbage->set_drop(true);
      pl->writePacket(pkt_send);
    }
    break;
  }
}


void ServerMatch::addGarbage(Field *from, Field *to, Garbage::Type type, int size)
{
  // get next garbage ID
  for(;;) {
    current_gbid_++;
    if( current_gbid_ <= 0 ) // overflow
      current_gbid_ = 1;
    if( gbs_wait_.find(current_gbid_) == gbs_wait_.end() )
      break; // not already in use
  }

  assert( to != NULL );

  Garbage *gb = new Garbage;
  gb->gbid = current_gbid_;
  gb->from = from;
  gb->to = to;
  gb->type = type;
  int pos;
  if( type == Garbage::TYPE_CHAIN ) {
    gb->size = FieldPos(FIELD_WIDTH, size);
    const int n = to->waitingGarbageCount();
    for( pos=0; pos<n; pos++ ) {
      if( to->waitingGarbage(pos).type == Garbage::TYPE_CHAIN )
        break;
    }
    gbs_chain_[from] = gb;
  } else if( type == Garbage::TYPE_COMBO ) {
    gb->size = FieldPos(size, 1);
    pos = -1; // push back
  } else {
    assert( !"not supported yet" );
  }
  gb->ntick = to->tick() + to->conf().gb_wait_tk;

  to->insertWaitingGarbage(gb, pos);
  gbs_wait_[gb->gbid] = gb;

  netplay::Packet pkt_send;
  netplay::Garbage *np_garbage = pkt_send.mutable_garbage();
  np_garbage->set_gbid(gb->gbid);
  np_garbage->set_plid_to( to->player()->plid() );
  np_garbage->set_plid_from( from == NULL ? 0 : from->player()->plid() );
  np_garbage->set_pos( pos );
  np_garbage->set_type( static_cast<netplay::Garbage_Type>(type) );
  np_garbage->set_size( size );
  server_.broadcastPacket(pkt_send);
}

void ServerMatch::increaseChainGarbage(Field *from)
{
  GbChainMap::iterator it = gbs_chain_.find(from);
  assert( it != gbs_chain_.end() );
  Garbage *gb = (*it).second;
  assert( gb->type == Garbage::TYPE_CHAIN );
  gb->size.y++;
  gb->ntick = gb->to->tick() + gb->to->conf().gb_wait_tk;

  netplay::Packet pkt_send;
  netplay::Garbage *np_garbage = pkt_send.mutable_garbage();
  np_garbage->set_gbid( gb->gbid );
  np_garbage->set_size( gb->size.y );
  server_.broadcastPacket(pkt_send);
}

