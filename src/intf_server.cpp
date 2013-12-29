#include "intf_server.h"
#include "server.h"
#include "inifile.h"
#include "log.h"


BasicServerInterface::BasicServerInterface():
    instance_(NULL)
{
}


bool BasicServerInterface::run(IniFile* cfg)
{
  boost::asio::io_service io_service;
  ServerInstance instance(*this, io_service);
  instance_ = &instance;
  instance.loadConf(*cfg);
  instance.startServer( cfg->get<int>("Global", "Port", DEFAULT_PNP_PORT) );
  io_service.run();
  instance_ = NULL;
  return true;
}

void BasicServerInterface::onChat(Player* pl, const std::string& msg)
{
  LOG("%s(%u): %s", pl->nick().c_str(), pl->plid(), msg.c_str());
}

void BasicServerInterface::onPlayerJoined(Player* pl)
{
  LOG("%s(%u) joined", pl->nick().c_str(), pl->plid());
}

void BasicServerInterface::onPlayerChangeNick(Player* pl, const std::string& nick)
{
  LOG("%s(%u) is now known as %s", nick.c_str(), pl->plid(),
      pl->nick().c_str());
}

void BasicServerInterface::onPlayerStateChange(Player* pl)
{
  Player::State state = pl->state();
  if(state == Player::State::QUIT) {
    LOG("%s(%u) has quit", pl->nick().c_str(), pl->plid());
  } else if(state == Player::State::LOBBY_READY || state == Player::State::GAME_READY) {
    LOG("%s(%u) is ready", pl->nick().c_str(), pl->plid());
  } else if(state == Player::State::LOBBY && instance_->state() == GameInstance::State::LOBBY) {
    LOG("%s(%u) is not ready", pl->nick().c_str(), pl->plid());
  }
}

void BasicServerInterface::onPlayerChangeFieldConf(Player* pl)
{
  //TODO log configuration name
  LOG("%s(%u) changed configuration", pl->nick().c_str(), pl->plid());
}

void BasicServerInterface::onStateChange()
{
  auto state = instance_->state();
  if(state == GameInstance::State::LOBBY) {
    LOG("match end");
  } else if(state == GameInstance::State::GAME_INIT) {
    LOG("match init");
  } else if(state == GameInstance::State::GAME_READY) {
    LOG("match ready");
  } else if(state == GameInstance::State::GAME) {
    LOG("match start");
  }
}

void BasicServerInterface::onPlayerStep(Player* pl)
{
  if( pl->field()->lost() ) {
    LOG("player(%u) lost", pl->plid());
  }
}

