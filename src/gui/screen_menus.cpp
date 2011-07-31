#include <boost/bind.hpp>
#include "screen_menus.h"
#include "screen_game.h"
#include "interface.h"
#include "../server.h"
#include "../log.h"

namespace gui {


ScreenStart::ScreenStart(GuiInterface &intf):
    ScreenMenu(intf, "ScreenStart")
{
}

void ScreenStart::enter()
{
  intf_.stopInstance();
  const ResourceManager& res_mgr = intf_.res_mgr();

  WButton *button_join = new WButton(*this, "JoinServer");
  button_join->setCaption(res_mgr.getLang(name_, "JoinServer"));
  container_.widgets.push_back(button_join);

  WButton *button_create = new WButton(*this, "CreateServer");
  button_create->setCaption(res_mgr.getLang(name_, "CreateServer"));
  container_.widgets.push_back(button_create);

  button_exit_ = new WButton(*this, "Exit");
  button_exit_->setCaption(res_mgr.getLang(name_, "Exit"));
  container_.widgets.push_back(button_exit_);

  button_join->setNeighbors(button_exit_, button_create, NULL, NULL);
  button_create->setNeighbors(button_join, button_exit_, NULL, NULL);
  button_exit_->setNeighbors(button_create, button_join, NULL, NULL);

  button_join->setCallback(boost::bind(&ScreenStart::onJoinServer, this));
  button_create->setCallback(boost::bind(&ScreenStart::onCreateServer, this));
  button_exit_->setCallback(boost::bind(&GuiInterface::swapScreen, &intf_, (gui::Screen*)NULL));

  this->focus(button_join);
}

bool ScreenStart::onInputEvent(const sf::Event &ev)
{
  if( ScreenMenu::onInputEvent(ev) ) {
    return true;
  }
  if( ev.Type == sf::Event::KeyPressed ) {
    if( ev.Key.Code == sf::Keyboard::Escape ) {
      if( focused_ == button_exit_ ) {
        intf_.swapScreen(NULL);
      } else {
        this->focus(button_exit_);
      }
      return true;
    }
  }
  return false;
}

void ScreenStart::onJoinServer()
{
  intf_.swapScreen(new ScreenJoinServer(intf_));
}

void ScreenStart::onCreateServer()
{
  intf_.swapScreen(new ScreenCreateServer(intf_));
}


ScreenJoinServer::ScreenJoinServer(GuiInterface &intf):
    ScreenMenu(intf, "ScreenJoinServer"),
    submitting_(false)
{
}

void ScreenJoinServer::enter()
{
  const ResourceManager& res_mgr = intf_.res_mgr();
  const IniFile& cfg = intf_.cfg();

  WLabel *label = new WLabel(*this, "HostPortLabel");
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

  WButton *button = new WButton(*this, "JoinButton");
  button->setCaption(res_mgr.getLang(name_, "Join"));
  container_.widgets.push_back(button);

  //XXX neighbors should be defined in style.ini too
  entry_host_->setNeighbors(button, entry_port_, NULL, NULL);
  entry_port_->setNeighbors(entry_host_, entry_nick_, NULL, NULL);
  entry_nick_->setNeighbors(entry_port_, button, NULL, NULL);
  button->setNeighbors(entry_nick_, entry_host_, NULL, NULL);

  this->focus(entry_host_);
}

bool ScreenJoinServer::onInputEvent(const sf::Event &ev)
{
  if( submitting_ ) {
    return true;  // ignore input
  }
  if( ScreenMenu::onInputEvent(ev) ) {
    return true;
  }
  if( ev.Type == sf::Event::KeyPressed ) {
    if( ev.Key.Code == sf::Keyboard::Escape ) {
      intf_.swapScreen(new ScreenStart(intf_));
    } else if( ev.Key.Code == sf::Keyboard::Return ) {
      this->submit();
    }
    return true;
  }
  return false;
}

void ScreenJoinServer::onPlayerJoined(Player *pl)
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
  std::string port_str = entry_port_->text();
  char *port_end;
  long port = strtol(port_str.c_str(), &port_end, 0);
  if( port_end != port_str.c_str()+port_str.size() || port <= 0 || port > 65535 ) {
    LOG("invalid port value: %s", port_str.c_str());
  } else if( entry_nick_->text().empty() ) {
    // TODO check nick is valid, not only not empty
    LOG("empty nick");
  } else {
    intf_.cfg().set("Client", "Hostname", entry_host_->text());
    intf_.cfg().set("Global", "Port", port_str);
    intf_.cfg().set("Client", "Nick", entry_nick_->text());
    intf_.startClient(entry_host_->text().c_str(), port);
    intf_.client()->newLocalPlayer(entry_nick_->text());
    submitting_ = true;
  }
}


ScreenCreateServer::ScreenCreateServer(GuiInterface &intf):
    ScreenMenu(intf, "ScreenCreateServer")
{
}

void ScreenCreateServer::enter()
{
  const ResourceManager& res_mgr = intf_.res_mgr();
  const IniFile& cfg = intf_.cfg();

  WLabel *label = new WLabel(*this, "PortLabel");
  label->setText(res_mgr.getLang(name_, "Port"));
  container_.widgets.push_back(label);

  entry_port_ = new WEntry(*this, "PortEntry");
  entry_port_->setText(cfg.get("Global", "Port", ""));
  container_.widgets.push_back(entry_port_);

  label = new WLabel(*this, "NickLabel");
  label->setText(res_mgr.getLang(name_, "PlayerName"));
  container_.widgets.push_back(label);

  entry_nick_ = new WEntry(*this, "NickEntry");
  entry_nick_->setText(cfg.get("Client", "Nick", "Player"));
  container_.widgets.push_back(entry_nick_);

  WButton *button = new WButton(*this, "CreateButton");
  button->setCaption(res_mgr.getLang(name_, "Create"));
  container_.widgets.push_back(button);

  entry_port_->setNeighbors(button, entry_nick_, NULL, NULL);
  entry_nick_->setNeighbors(entry_port_, button, NULL, NULL);
  button->setNeighbors(entry_nick_, entry_port_, NULL, NULL);

  this->focus(entry_port_);
}

bool ScreenCreateServer::onInputEvent(const sf::Event &ev)
{
  if( ScreenMenu::onInputEvent(ev) ) {
    return true;
  }
  if( ev.Type == sf::Event::KeyPressed ) {
    if( ev.Key.Code == sf::Keyboard::Escape ) {
      intf_.swapScreen(new ScreenStart(intf_));
    } else if( ev.Key.Code == sf::Keyboard::Return ) {
      this->submit();
    }
    return true;
  }
  return false;
}

void ScreenCreateServer::submit()
{
  std::string port_str = entry_port_->text();
  char *port_end;
  long port = strtol(port_str.c_str(), &port_end, 0);
  if( port_end != port_str.c_str()+port_str.size() || port <= 0 || port > 65535 ) {
    LOG("invalid port value: %s", port_str.c_str());
  } else if( entry_nick_->text().empty() ) {
    // TODO check nick is valid, not only not empty
    LOG("empty nick");
  } else {
    intf_.cfg().set("Global", "Port", port_str);
    intf_.cfg().set("Client", "Nick", entry_nick_->text());
    intf_.startServer(port);
    Player *pl = intf_.server()->newLocalPlayer(entry_nick_->text());
    intf_.swapScreen(new ScreenLobby(intf_, pl));
  }
}


ScreenLobby::ScreenLobby(GuiInterface &intf, Player *pl):
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
  button_ready_->setCallback(boost::bind(&ScreenLobby::submit, this));
  container_.widgets.push_back(button_ready_);

  button_ready_->setNeighbors(NULL, NULL, NULL, NULL);

  const IniFile& style = this->style();
  player_rows_pos_ = style.get<sf::Vector2f>(name_, "PlayerRowsPos");
  player_rows_dy_ = style.get<float>(name_, "PlayerRowsDY");

  // add already connected players
  const GameInstance::PlayerContainer& players = intf_.instance()->players();
  GameInstance::PlayerContainer::const_iterator plit;
  for( plit=players.begin(); plit!=players.end(); ++plit ) {
    const Player& pl = *(*plit).second;
    PlId plid = pl.plid(); // intermediate variable to help g++
    player_rows_.insert(plid, new WPlayerRow(*this, pl));
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
    w.Draw(*(*it).second);
  }
}


bool ScreenLobby::onInputEvent(const sf::Event &ev)
{
  if( ScreenMenu::onInputEvent(ev) ) {
    return true;
  }
  if( ev.Type == sf::Event::KeyPressed ) {
    if( ev.Key.Code == sf::Keyboard::Escape ) {
      intf_.swapScreen(new ScreenStart(intf_));
    }
    return true;
  }
  return false;
}

void ScreenLobby::onStateChange(GameInstance::State state)
{
  if( state == GameInstance::STATE_LOBBY ) {
    this->updateReadyButtonCaption();
  } else if( state == GameInstance::STATE_INIT ) {
    intf_.swapScreen(new ScreenGame(intf_, player_));
  }
}

void ScreenLobby::onPlayerJoined(Player *pl)
{
  PlId plid = pl->plid(); // intermediate variable to help g++
  player_rows_.insert(plid, new WPlayerRow(*this, *pl));
  this->updatePlayerRowsPos();
}

void ScreenLobby::onPlayerChangeNick(Player *pl, const std::string &)
{
  player_rows_.find(pl->plid())->second->update();
}

void ScreenLobby::onPlayerReady(Player *pl)
{
  player_rows_.find(pl->plid())->second->update();
}

void ScreenLobby::onPlayerQuit(Player *pl)
{
  player_rows_.erase(pl->plid());
  this->updatePlayerRowsPos();
}


void ScreenLobby::submit()
{
  GameInstance *instance = intf_.instance();
  assert( instance );
  if( instance->state() == GameInstance::STATE_LOBBY ) {
    instance->playerSetReady(player_, !player_->ready());
    this->updateReadyButtonCaption();
  }
}

void ScreenLobby::updateReadyButtonCaption()
{
  const std::string caption = player_->ready() ? "Waiting" : "Ready";
  button_ready_->setCaption(intf_.res_mgr().getLang("ScreenLobby", caption));
}

void ScreenLobby::updatePlayerRowsPos()
{
  PlayerRowsContainer::iterator it;
  float y = player_rows_pos_.y;
  for( it=player_rows_.begin(); it!=player_rows_.end(); ++it ) {
    (*it).second->SetPosition(player_rows_pos_.x, y);
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

  this->applyStyle(&nick_, "Nick");
  nick_.SetOrigin(0, (nick_.GetFont().GetLineSpacing(nick_.GetCharacterSize())+2)/2);
  if( searchStyle("NickX", &key) ) {
    nick_.SetX(style.get<float>(key));
  } else {
    throw StyleError(*this, "NickX", "not set");
  }

  this->applyStyle(&ready_, "Ready");
  ready_.SetOrigin(ready_.GetSize()/2.f);
  if( searchStyle("ReadyX", &key) ) {
    ready_.SetX(style.get<float>(key));
  } else {
    throw StyleError(*this, "ReadyX", "not set");
  }

  this->update();
}

void ScreenLobby::WPlayerRow::Render(sf::RenderTarget &target, sf::Renderer &) const
{
  target.Draw(nick_);
  if( player_.ready() ) {
    target.Draw(ready_);
  }
}

void ScreenLobby::WPlayerRow::update()
{
  nick_.SetString(player_.nick());
}


}

