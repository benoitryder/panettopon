#include <cassert>
#include "netplay.pb.h"
#include "player.h"
#include "game.h"
#include "log.h"


void ServerConf::toDefault()
{
  const netplay::Server::Conf &np_conf = netplay::Server::Conf::default_instance();
#define SERVER_CONF_EXPR_INIT(n,ini,t) \
  n = np_conf.n();
  SERVER_CONF_APPLY(SERVER_CONF_EXPR_INIT);
#undef SERVER_CONF_EXPR_INIT
}


Player::Player(PlId plid):
    plid_(plid), ready_(false), field_(NULL)
{
  assert( plid > 0 );
}

Player::~Player()
{
  LOG("~Player() %u", plid_); //XXX:debug
}

