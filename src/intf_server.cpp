#include "intf_server.h"
#include "server.h"
#include "config.h"
#include "log.h"

namespace asio = boost::asio;


bool BasicServerInterface::run(const Config &cfg)
{
  asio::io_service io_service;
  ServerInstance instance(*this, io_service);
  instance.loadConf(cfg);
  instance.startServer( cfg.getInt("Global", "Port", 20102) );
  io_service.run();
  return true;
}

void BasicServerInterface::onChat(const Player *pl, const std::string &msg)
{
  LOG("%s(%u): %s", pl->nick().c_str(), pl->plid(), msg.c_str());
}

void BasicServerInterface::onPlayerJoined(const Player *pl)
{
  LOG("%s(%u) joined", pl->nick().c_str(), pl->plid());
}

void BasicServerInterface::onPlayerSetNick(const Player *pl, const std::string &old_nick)
{
  LOG("%s(%u) is now known as %s", old_nick.c_str(), pl->plid(),
      pl->nick().c_str());
}

void BasicServerInterface::onPlayerReady(const Player *pl)
{
  if( pl->ready() )
    LOG("%s(%u) is ready", pl->nick().c_str(), pl->plid());
  else
    LOG("%s(%u) is not ready anymore", pl->nick().c_str(), pl->plid());
}

void BasicServerInterface::onPlayerQuit(const Player *pl)
{
  LOG("%s(%u) has quit", pl->nick().c_str(), pl->plid());
}

void BasicServerInterface::onMatchInit(const Match *)  { LOG("match init"); }
void BasicServerInterface::onMatchReady(const Match *) { LOG("match ready"); }
void BasicServerInterface::onMatchStart(const Match *) { LOG("match start"); }
void BasicServerInterface::onMatchEnd(const Match *)   { LOG("match end"); }
void BasicServerInterface::onFieldStep(const Field *)  {}
void BasicServerInterface::onFieldLost(const Field *fld)
{
  LOG("field(%u) lost", fld->player() != NULL ? 0 : fld->player()->plid());
}

