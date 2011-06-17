#include "intf_server.h"
#include "server.h"
#include "config.h"
#include "log.h"


BasicServerInterface::BasicServerInterface():
    instance_(NULL)
{
}


bool BasicServerInterface::run(const Config &cfg)
{
  boost::asio::io_service io_service;
  ServerInstance instance(*this, io_service);
  instance_ = &instance;
  instance.loadConf(cfg);
  instance.startServer( cfg.getInt("Global", "Port", 2426) );
  io_service.run();
  instance_ = NULL;
  return true;
}

void BasicServerInterface::onChat(Player *pl, const std::string &msg)
{
  LOG("%s(%u): %s", pl->nick().c_str(), pl->plid(), msg.c_str());
}

void BasicServerInterface::onPlayerJoined(Player *pl)
{
  LOG("%s(%u) joined", pl->nick().c_str(), pl->plid());
}

void BasicServerInterface::onPlayerChangeNick(Player *pl, const std::string &old_nick)
{
  LOG("%s(%u) is now known as %s", old_nick.c_str(), pl->plid(),
      pl->nick().c_str());
}

void BasicServerInterface::onPlayerReady(Player *pl)
{
  if( pl->ready() ) {
    LOG("%s(%u) is ready", pl->nick().c_str(), pl->plid());
  } else {
    LOG("%s(%u) is not ready anymore", pl->nick().c_str(), pl->plid());
  }
}

void BasicServerInterface::onPlayerQuit(Player *pl)
{
  LOG("%s(%u) has quit", pl->nick().c_str(), pl->plid());
}

void BasicServerInterface::onStateChange(GameInstance::State state)
{
  if( state == GameInstance::STATE_LOBBY ) {
    LOG("match end");
  } else if( state == GameInstance::STATE_INIT ) {
    LOG("match init");
  } else if( state == GameInstance::STATE_READY ) {
    LOG("match ready");
  } else if( state == GameInstance::STATE_GAME ) {
    LOG("match start");
  }
}

void BasicServerInterface::onPlayerStep(Player *pl)
{
  if( pl->field()->lost() ) {
    LOG("player(%u) lost", pl->plid());
  }
}

