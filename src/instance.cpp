#include <cassert>
#include "instance.h"


Player::Player(PlId plid):
    plid_(plid), ready_(false), field_(NULL)
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



