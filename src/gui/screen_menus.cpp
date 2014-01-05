#include <functional>
#include "screen_menus.h"
#include "screen_game.h"
#include "interface.h"
#include "../server.h"
#include "../log.h"

namespace gui {


ScreenStart::ScreenStart(GuiInterface& intf):
    ScreenMenu(intf, "ScreenStart")
{
}

void ScreenStart::enter()
{
  intf_.stopInstance();
  const ResourceManager& res_mgr = intf_.res_mgr();

  WButton* button_join = new WButton(*this, "JoinServer");
  button_join->setCaption(res_mgr.getLang(name_, "JoinServer"));
  container_.widgets.push_back(button_join);

  WButton* button_create = new WButton(*this, "CreateServer");
  button_create->setCaption(res_mgr.getLang(name_, "CreateServer"));
  container_.widgets.push_back(button_create);

  button_exit_ = new WButton(*this, "Exit");
  button_exit_->setCaption(res_mgr.getLang(name_, "Exit"));
  container_.widgets.push_back(button_exit_);

  button_join->setNeighbors(button_exit_, button_create, NULL, NULL);
  button_create->setNeighbors(button_join, button_exit_, NULL, NULL);
  button_exit_->setNeighbors(button_create, button_join, NULL, NULL);

  button_join->setCallback(std::bind(&ScreenStart::onJoinServer, this));
  button_create->setCallback(std::bind(&ScreenStart::onCreateServer, this));
  button_exit_->setCallback(std::bind(&GuiInterface::swapScreen, &intf_, nullptr));

  this->focus(button_join);

  // "Debug start" button, only when debug mode is enabled
  if(intf_.cfg().get<bool>("Global", "Debug", false)) {
    WButton* button_debugstart = new WButton(*this, "DebugStart");
    button_debugstart->setCaption(res_mgr.getLang(name_, "DebugStart"));
    container_.widgets.push_back(button_debugstart);

    button_debugstart->setNeighbors(button_exit_, button_join, NULL, NULL);
    button_debugstart->setCallback(std::bind(&ScreenStart::onDebugStart, this));
    button_join->setNeighbors(button_debugstart, button_create, NULL, NULL);
    button_exit_->setNeighbors(button_create, button_debugstart, NULL, NULL);

    this->focus(button_debugstart);
  }
}

bool ScreenStart::onInputEvent(const sf::Event& ev)
{
  if( ScreenMenu::onInputEvent(ev) ) {
    return true;
  }
  if( ev.type == sf::Event::KeyPressed ) {
    if( ev.key.code == sf::Keyboard::Escape ) {
      if( focused_ == button_exit_ ) {
        intf_.swapScreen(nullptr);
      } else {
        this->focus(button_exit_);
      }
      return true;
    }
  }
  return false;
}

void ScreenStart::onDebugStart()
{
  // create server and players
  intf_.startServer(intf_.cfg().get<uint16_t>("Global", "Port"));
  Player* pl1 = intf_.server()->newLocalPlayer("Player 1");
  Player* pl2 = intf_.server()->newLocalPlayer("Player 2");
  // swap to screen game
  intf_.swapScreen(new ScreenGame(intf_, pl1));
  // lobby: players are ready
  GameInstance* instance = intf_.instance();
  instance->playerSetState(pl1, Player::State::LOBBY_READY);
  instance->playerSetState(pl2, Player::State::LOBBY_READY);
  // game start: players are ready (again)
  instance->playerSetState(pl1, Player::State::GAME_READY);
  instance->playerSetState(pl2, Player::State::GAME_READY);
}

void ScreenStart::onJoinServer()
{
  intf_.swapScreen(new ScreenJoinServer(intf_));
}

void ScreenStart::onCreateServer()
{
  intf_.swapScreen(new ScreenCreateServer(intf_));
}


ScreenJoinServer::ScreenJoinServer(GuiInterface& intf):
    ScreenMenu(intf, "ScreenJoinServer"),
    submitting_(false)
{
}

void ScreenJoinServer::enter()
{
  const ResourceManager& res_mgr = intf_.res_mgr();
  const IniFile& cfg = intf_.cfg();

  WLabel* label = new WLabel(*this, "HostPortLabel");
  label->setText(res_mgr.getLang(name_, "HostPort"));
  container_.widgets.push_back(label);

  entry_host_ = new WEntry(*this, "HostEntry");
  entry_host_->setText(cfg.get("Client", "Hostname", ""));
  container_.widgets.push_back(entry_host_);

  entry_port_ = new WEntry(*this, "PortEntry");
  entry_port_->setText(cfg.get("Global", "Port", ""));
  container_.widgets.push_back(entry_port_);

  label = new WLabel(*this, "NickLabel");
  label->setText(res_mgr.getLang(name_, "PlayerName"));
  container_.widgets.push_back(label);

  entry_nick_ = new WEntry(*this, "NickEntry");
  entry_nick_->setText(cfg.get("Client", "Nick", "Player"));
  container_.widgets.push_back(entry_nick_);

  WButton* button = new WButton(*this, "JoinButton");
  button->setCaption(res_mgr.getLang(name_, "Join"));
  container_.widgets.push_back(button);

  //XXX neighbors should be defined in style.ini too
  entry_host_->setNeighbors(button, entry_port_, NULL, NULL);
  entry_port_->setNeighbors(entry_host_, entry_nick_, NULL, NULL);
  entry_nick_->setNeighbors(entry_port_, button, NULL, NULL);
  button->setNeighbors(entry_nick_, entry_host_, NULL, NULL);

  this->focus(entry_host_);
}

bool ScreenJoinServer::onInputEvent(const sf::Event& ev)
{
  if( submitting_ ) {
    return true;  // ignore input
  }
  if( ScreenMenu::onInputEvent(ev) ) {
    return true;
  }
  if( ev.type == sf::Event::KeyPressed ) {
    if( ev.key.code == sf::Keyboard::Escape ) {
      intf_.swapScreen(new ScreenStart(intf_));
    } else if( ev.key.code == sf::Keyboard::Return ) {
      this->submit();
    }
    return true;
  }
  return false;
}

void ScreenJoinServer::onPlayerJoined(Player* pl)
{
  if( pl->local() ) {
    intf_.swapScreen(new ScreenLobby(intf_, pl));
  }
}

void ScreenJoinServer::onServerDisconnect()
{
  submitting_ = false;
  intf_.stopInstance();
}

void ScreenJoinServer::submit()
{
  assert( !submitting_ );
  //TODO display errors to the user
  if( entry_nick_->text().empty() ) {
    // TODO check nick is valid, not only not empty
    LOG("empty nick");
    return;
  } else if( entry_host_->text().empty() ) {
    LOG("empty hostname");
    return;
  }
  uint16_t port;
  try {
    port = boost::lexical_cast<uint16_t>(entry_port_->text());
  } catch(const boost::bad_lexical_cast&) {
    LOG("invalid port value: %s", entry_port_->text().c_str());
    return;
  }

  intf_.cfg().set("Client", "Hostname", entry_host_->text());
  intf_.cfg().set("Global", "Port", port);
  intf_.cfg().set("Client", "Nick", entry_nick_->text());
  intf_.startClient(entry_host_->text(), port);
  intf_.client()->newLocalPlayer(entry_nick_->text());
  submitting_ = true;
}


ScreenCreateServer::ScreenCreateServer(GuiInterface& intf):
    ScreenMenu(intf, "ScreenCreateServer")
{
}

void ScreenCreateServer::enter()
{
  const ResourceManager& res_mgr = intf_.res_mgr();
  const IniFile& cfg = intf_.cfg();
  // reload configuration from values

  WLabel* label = new WLabel(*this, "PortLabel");
  label->setText(res_mgr.getLang(name_, "Port"));
  container_.widgets.push_back(label);

  entry_port_ = new WEntry(*this, "PortEntry");
  entry_port_->setText(cfg.get("Global", "Port", ""));
  container_.widgets.push_back(entry_port_);

  label = new WLabel(*this, "NickLabel");
  label->setText(res_mgr.getLang(name_, "PlayerName"));
  container_.widgets.push_back(label);

  entry_nick_ = new WEntry(*this, "NickEntry");
  entry_nick_->setText(cfg.get("Client", "Nick", ""));
  container_.widgets.push_back(entry_nick_);

  label = new WLabel(*this, "PlayerNbLabel");
  label->setText(res_mgr.getLang(name_, "PlayerNumber"));
  container_.widgets.push_back(label);

  entry_player_nb_ = new WEntry(*this, "PlayerNbEntry");
  entry_player_nb_->setText(cfg.get("Server", "PlayerNumber", ""));
  container_.widgets.push_back(entry_player_nb_);

  WButton* button = new WButton(*this, "CreateButton");
  button->setCaption(res_mgr.getLang(name_, "Create"));
  container_.widgets.push_back(button);

  entry_port_->setNeighbors(button, entry_player_nb_, NULL, NULL);
  entry_player_nb_->setNeighbors(entry_port_, entry_nick_, NULL, NULL);
  entry_nick_->setNeighbors(entry_player_nb_, button, NULL, NULL);
  button->setNeighbors(entry_nick_, entry_port_, NULL, NULL);

  this->focus(entry_port_);
}

bool ScreenCreateServer::onInputEvent(const sf::Event& ev)
{
  if( ScreenMenu::onInputEvent(ev) ) {
    return true;
  }
  if( ev.type == sf::Event::KeyPressed ) {
    if( ev.key.code == sf::Keyboard::Escape ) {
      intf_.swapScreen(new ScreenStart(intf_));
    } else if( ev.key.code == sf::Keyboard::Return ) {
      this->submit();
    }
    return true;
  }
  return false;
}

void ScreenCreateServer::submit()
{
  //TODO display errors to the user
  if( entry_nick_->text().empty() ) {
    // TODO check nick is valid, not only not empty
    LOG("empty nick");
    return;
  }
  uint16_t port;
  try {
    port = boost::lexical_cast<uint16_t>(entry_port_->text());
  } catch(const boost::bad_lexical_cast&) {
    LOG("invalid port value: %s", entry_port_->text().c_str());
    return;
  }
  unsigned int player_nb;
  try {
    player_nb = boost::lexical_cast<unsigned int>(entry_player_nb_->text());
  } catch(const boost::bad_lexical_cast&) {
    LOG("invalid player number: %s", entry_player_nb_->text().c_str());
    return;
  }

  intf_.cfg().set("Global", "Port", port);
  intf_.cfg().set("Client", "Nick", entry_nick_->text());
  intf_.cfg().set("Server", "PlayerNumber", player_nb);
  intf_.startServer(port);
  Player* pl = intf_.server()->newLocalPlayer(entry_nick_->text());
  intf_.swapScreen(new ScreenLobby(intf_, pl));
}


ScreenLobby::ScreenLobby(GuiInterface& intf, Player* pl):
    ScreenMenu(intf, "ScreenLobby"),
    player_(pl)
{
}

void ScreenLobby::enter()
{
  assert( player_ );
  assert( intf_.instance() );
  const ResourceManager& res_mgr = intf_.res_mgr();

  button_ready_ = new WButton(*this, "Ready");
  button_ready_->setCaption(res_mgr.getLang(name_, "Ready"));
  button_ready_->setCallback(std::bind(&ScreenLobby::submit, this));
  container_.widgets.push_back(button_ready_);

  button_ready_->setNeighbors(NULL, NULL, NULL, NULL);

  const IniFile& style = this->style();
  player_rows_pos_ = style.get<sf::Vector2f>(name_, "PlayerRowsPos");
  player_rows_dy_ = style.get<float>(name_, "PlayerRowsDY");

  // add already connected players
  // set state of local players to LOBBY
  for(auto const& p : intf_.instance()->players()) {
    Player& pl = *p.second;
    PlId plid = pl.plid(); // intermediate variable because a ref is required
    player_rows_.insert(plid, new WPlayerRow(*this, pl));
    if(pl.local()) {
      intf_.instance()->playerSetState(&pl, Player::State::LOBBY);
    }
  }

  this->updateReadyButtonCaption();
  this->updatePlayerRowsPos();
  this->focus(button_ready_);
}

void ScreenLobby::redraw()
{
  ScreenMenu::redraw();

  sf::RenderWindow& w = intf_.window();
  PlayerRowsContainer::const_iterator it;
  for( it=player_rows_.begin(); it!=player_rows_.end(); ++it ) {
    w.draw(*(*it).second);
  }
}


bool ScreenLobby::onInputEvent(const sf::Event& ev)
{
  if( ScreenMenu::onInputEvent(ev) ) {
    return true;
  }
  if( ev.type == sf::Event::KeyPressed ) {
    if( ev.key.code == sf::Keyboard::Escape ) {
      intf_.swapScreen(new ScreenStart(intf_));
    }
    return true;
  }
  return false;
}

void ScreenLobby::onStateChange()
{
  auto state = intf_.instance()->state();
  if(state == GameInstance::State::LOBBY) {
    this->updateReadyButtonCaption();
  } else if(state == GameInstance::State::GAME_INIT) {
    intf_.swapScreen(new ScreenGame(intf_, player_));
  }
}

void ScreenLobby::onPlayerJoined(Player* pl)
{
  PlId plid = pl->plid(); // intermediate variable to help g++
  player_rows_.insert(plid, new WPlayerRow(*this, *pl));
  this->updatePlayerRowsPos();
}

void ScreenLobby::onPlayerChangeNick(Player* pl, const std::string& )
{
  player_rows_.find(pl->plid())->second->update();
}

void ScreenLobby::onPlayerStateChange(Player* pl)
{
  if(pl->state() == Player::State::QUIT) {
    player_rows_.erase(pl->plid());
    this->updatePlayerRowsPos();
  } else {
    player_rows_.find(pl->plid())->second->update();
  }
}

void ScreenLobby::onPlayerChangeFieldConf(Player* pl)
{
  player_rows_.find(pl->plid())->second->update();
}


void ScreenLobby::submit()
{
  GameInstance* instance = intf_.instance();
  assert( instance );
  if(instance->state() == GameInstance::State::LOBBY) {
    instance->playerSetState(player_, player_->state() == Player::State::LOBBY ? Player::State::LOBBY_READY : Player::State::LOBBY);
    this->updateReadyButtonCaption();
  }
}

void ScreenLobby::updateReadyButtonCaption()
{
  const std::string caption = player_->state() == Player::State::LOBBY_READY ? "Waiting" : "Ready";
  button_ready_->setCaption(intf_.res_mgr().getLang("ScreenLobby", caption));
}

void ScreenLobby::updatePlayerRowsPos()
{
  PlayerRowsContainer::iterator it;
  float y = player_rows_pos_.y;
  for( it=player_rows_.begin(); it!=player_rows_.end(); ++it ) {
    (*it).second->setPosition(player_rows_pos_.x, y);
    y += player_rows_dy_;
  }
}


const std::string& ScreenLobby::WPlayerRow::type() const {
  static const std::string type("PlayerRow");
  return type;
}

ScreenLobby::WPlayerRow::WPlayerRow(const Screen& screen, const Player& pl):
    Widget(screen, ""), player_(pl)
{
  const IniFile& style = screen_.style();
  std::string key;

  this->applyStyle(nick_, "Nick");
  nick_.setOrigin(0, (nick_.getFont()->getLineSpacing(nick_.getCharacterSize())+2)/2);
  if( searchStyle("NickX", &key) ) {
    nick_.setPosition(style.get<float>(key), 0);
  } else {
    throw StyleError(*this, "NickX", "not set");
  }

  this->applyStyle(conf_, "Conf");
  conf_.setOrigin(0, (conf_.getFont()->getLineSpacing(conf_.getCharacterSize())+2)/2);
  if( searchStyle("ConfX", &key) ) {
    conf_.setPosition(style.get<float>(key), 0);
  } else {
    throw StyleError(*this, "ConfX", "not set");
  }

  this->applyStyle(ready_, "Ready");
  ready_.setOrigin(ready_.getLocalBounds().width/2.f, ready_.getLocalBounds().height/2.f);
  if( searchStyle("ReadyX", &key) ) {
    ready_.setPosition(style.get<float>(key), 0);
  } else {
    throw StyleError(*this, "ReadyX", "not set");
  }

  this->update();
}

void ScreenLobby::WPlayerRow::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
  states.transform *= getTransform();
  target.draw(nick_, states);
  target.draw(conf_, states);
  if(player_.state() == Player::State::LOBBY_READY) {
    target.draw(ready_, states);
  }
}

void ScreenLobby::WPlayerRow::update()
{
  nick_.setString(player_.nick());
  conf_.setString(player_.fieldConfName());
}


}

