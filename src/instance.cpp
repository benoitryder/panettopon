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
  PlayerContainer::iterator it;
  for(it=players_.begin(); it!=players_.end(); ++it) {
    if( fld == (*it).second->field() ) {
      return (*it).second;
    }
  }
  return NULL;
}


void GameInstance::stepRemoteField(Player *pl, KeyState keys)
{
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
  //TODO intf_.onFieldStep(fld);
}


