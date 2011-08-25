#include <boost/bind.hpp>
#include <boost/asio/placeholders.hpp>
#include "instance.h"
#include "netplay.h"
#include "log.h"


void ServerConf::toDefault()
{
  const netplay::PktServerConf &np_conf = netplay::PktServerConf::default_instance();
#define SERVER_CONF_EXPR_INIT(n,ini) \
  n = np_conf.n();
  SERVER_CONF_APPLY(SERVER_CONF_EXPR_INIT);
#undef SERVER_CONF_EXPR_INIT
  //TODO define default configuration here
}



Player::Player(PlId plid, bool local):
    plid_(plid), local_(local), ready_(false), field_(NULL)
{
  assert( plid > 0 );
}

Player::~Player()
{
}

void Player::setFieldConf(const FieldConf& conf, const std::string& name)
{
  field_conf_ = conf;
  field_conf_name_ = name;
}


GameInstance::GameInstance():
    state_(STATE_NONE)
{
}

GameInstance::~GameInstance()
{
}


Player *GameInstance::player(PlId plid)
{
  PlayerContainer::iterator it = players_.find(plid);
  return it == players_.end() ? NULL : (*it).second;
}

Player *GameInstance::player(const Field *fld)
{
  //XXX use a map on the instance to optimize?
  if( fld == NULL ) {
    return NULL;
  }
  PlayerContainer::iterator it;
  for(it=players_.begin(); it!=players_.end(); ++it) {
    if( fld == (*it).second->field() ) {
      return (*it).second;
    }
  }
  return NULL;
}


void GameInstance::doStepPlayer(Player *pl, KeyState keys)
{
  Field *fld = pl->field();
  assert( !fld->lost() );
  Tick prev_tick = fld->tick();
  assert( prev_tick+1 < match_.tick() + conf_.tk_lag_max );

  fld->step(keys);
  if( prev_tick == match_.tick() ) {
    // don't update tick_ when it will obviously not be modified
    //XXX:check condition
    match_.updateTick();
  }
  observer().onPlayerStep(pl);
}

void GameInstance::stepRemotePlayer(Player *pl, KeyState keys)
{
  Field *fld = pl->field();
  if( fld->lost() ) {
    throw netplay::CallbackError("field lost, cannot step");
  }
  Tick prev_tick = fld->tick();
  if( prev_tick+1 >= match_.tick() + conf_.tk_lag_max ) {
    throw netplay::CallbackError("maximum lag exceeded");
  }
  this->doStepPlayer(pl, keys);
}


GameInputScheduler::GameInputScheduler(GameInstance &instance, InputProvider &input, boost::asio::io_service &io_service):
    instance_(instance), input_(input), timer_(io_service)
{
}

GameInputScheduler::~GameInputScheduler()
{
  this->stop();
}

void GameInputScheduler::start()
{
  // get local players
  players_.clear();
  GameInstance::PlayerContainer &all_players = instance_.players();
  GameInstance::PlayerContainer::iterator it;
  for(it=all_players.begin(); it!=all_players.end(); ++it) {
    Player *pl = (*it).second;
    if( pl->local() && pl->field() != NULL ) {
      players_.push_back(pl);
    }
  }

  // start timer
  boost::posix_time::time_duration dt = boost::posix_time::microseconds(instance_.conf().tk_usec);
  tick_clock_ = boost::posix_time::microsec_clock::universal_time() + dt;
  timer_.expires_from_now(dt);
  timer_.async_wait(boost::bind(&GameInputScheduler::onInputTick, this,
                                boost::asio::placeholders::error));
}

void GameInputScheduler::stop()
{
  timer_.cancel();
  players_.clear();
}


void GameInputScheduler::onInputTick(const boost::system::error_code &ec)
{
  if( ec == boost::asio::error::operation_aborted ) {
    return;
  }
  assert( !ec );

  for(;;) {
    PlayerContainer::iterator it;
    for(it=players_.begin(); it!=players_.end(); ) {
      Player *pl = (*it);
      if( !pl->local() || pl->field() == NULL ) {
        ++it;
        continue;
      }
      // note: all local players still playing should have the same tick
      // thus, break instead of continue
      Tick tk = pl->field()->tick();
      if( tk+1 >= instance_.match().tick() + instance_.conf().tk_lag_max ) {
        break;
      }
      // note: if this step ends the match, a lot of things may happen
      // be careful when checking/using current state
      instance_.playerStep(pl, input_.getNextInput(pl));
      if( players_.empty() ) {
        return; // scheduler and/or match stopped
      }

      if( pl->field()->lost() ) {
        it = players_.erase(it);
      } else {
        ++it;
      }
    }
    if( players_.empty() ) {
      return;
    }

    tick_clock_ += boost::posix_time::microseconds(instance_.conf().tk_usec);
    boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
    if( tick_clock_ >= now ) {
      timer_.expires_from_now(tick_clock_ - now);
      timer_.async_wait(boost::bind(&GameInputScheduler::onInputTick, this,
                                    boost::asio::placeholders::error));
      break;
    }
  }
}

