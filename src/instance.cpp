#include <boost/bind.hpp>
#include <boost/asio/placeholders.hpp>
#include "instance.h"
#include "netplay.h"


Player::Player(PlId plid, bool local):
    plid_(plid), local_(local), ready_(false), field_(NULL)
{
  assert( plid > 0 );
}

Player::~Player()
{
}


GameInstance::GameInstance(Observer &obs):
    observer_(obs), state_(STATE_NONE)
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
  observer_.onPlayerStep(pl);
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


GameInputScheduler::GameInputScheduler(GameInstance &instance, boost::asio::io_service &io_service):
    instance_(instance), timer_(io_service)
{
}

void GameInputScheduler::start()
{
  // get local players
  players_.clear();
  GameInstance::PlayerContainer all_players = instance_.players();
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

  for(;;) {
    PlayerContainer::iterator it;
    for(it=players_.begin(); it!=players_.end(); ) {
      Player *pl = (*it);
      Field *fld = pl->field();
      if( !pl->local() || fld == NULL ) {
        continue;
      }
      // note: all local players still playing should have the same tick
      // thus, break instead of continue
      Tick tk = fld->tick();
      if( tk+1 >= instance_.match().tick() + instance_.conf().tk_lag_max ) {
        break;
      }
      instance_.playerStep(pl, this->getNextInput(pl));

      if( fld->lost() ) {
        players_.erase(it++);
      } else {
        ++it;
      }
    }

    if( players_.empty() ) {
      break;
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

