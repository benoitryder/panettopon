#include <boost/bind.hpp>
#include "screen_menus.h"
#include "screen_game.h"
#include "interface.h"
#include "../server.h"
#include "../log.h"

namespace gui {


ScreenStart::ScreenStart(GuiInterface &intf):
    ScreenMenu(intf)
{
}

void ScreenStart::enter()
{
  intf_.stopInstance();
  ResourceManager &res_mgr = intf_.res_mgr();
  const std::string lang_section("ScreenStart");
  style_button_.load(&res_mgr); //XXX:temp

  // create buttons
  const std::string labels[] = {
    res_mgr.getLang(lang_section, "JoinServer"),
    res_mgr.getLang(lang_section, "CreateServer"),
    res_mgr.getLang(lang_section, "Exit"),
  };
  const int button_nb = sizeof(labels)/sizeof(*labels);
  const float button_dy = 60;
  WButton *buttons[button_nb];
  int i;
  for( i=0; i<button_nb; i++ ) {
    WButton *button = new WButton(style_button_, 300);
    button->setCaption(labels[i]);
    button->SetPosition(0, i*button_dy-(button_nb-1)*button_dy/2);
    buttons[i] = button;
    container_.widgets.push_back(button);
  }
  for( i=0; i<button_nb; i++ ) {
    buttons[i]->setNeighbors(
        buttons[ i-1<0 ? button_nb-1 : i-1 ],
        buttons[ i+1>=button_nb ? 0 : i+1 ],
        NULL, NULL );
  }
  button_exit_ = buttons[button_nb-1];

  buttons[0]->setCallback(boost::bind(&ScreenStart::onJoinServer, this));
  buttons[1]->setCallback(boost::bind(&ScreenStart::onCreateServer, this));
  button_exit_->setCallback(boost::bind(&GuiInterface::swapScreen, &intf_, (gui::Screen*)NULL));

  this->focus(buttons[0]);
}

bool ScreenStart::onInputEvent(const sf::Event &ev)
{
  if( ScreenMenu::onInputEvent(ev) ) {
    return true;
  }
  if( ev.Type == sf::Event::KeyPressed ) {
    if( ev.Key.Code == sf::Key::Escape ) {
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
    ScreenMenu(intf),
    submitting_(false)
{
}

void ScreenJoinServer::enter()
{
  ResourceManager &res_mgr = intf_.res_mgr();
  const std::string lang_section("ScreenJoinServer");
  style_button_.load(&res_mgr);

  WLabel *label = new WLabel();
  label->setText(res_mgr.getLang(lang_section, "HostPort"));
  label->setTextAlign(0);
  label->SetPosition(0, -60);

  entry_host_ = new WEntry(300, 40);
  entry_host_->setText("localhost");
  entry_host_->SetPosition(-55, 0);

  entry_port_ = new WEntry(100, 40);
  entry_port_->setText(QUOTE(DEFAULT_PNP_PORT));
  entry_port_->SetPosition(155, 0);

  WButton *button = new WButton(style_button_, 200);
  button->setCaption(res_mgr.getLang(lang_section, "Join"));
  button->SetPosition(0, 60);

  container_.widgets.push_back(label);
  container_.widgets.push_back(entry_host_);
  container_.widgets.push_back(entry_port_);
  container_.widgets.push_back(button);

  entry_host_->setNeighbors(button, button, entry_port_, entry_port_);
  entry_port_->setNeighbors(button, button, entry_host_, entry_host_);
  button->setNeighbors(entry_host_, entry_host_, NULL, NULL);

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
    if( ev.Key.Code == sf::Key::Escape ) {
      intf_.swapScreen(new ScreenStart(intf_));
    } else if( ev.Key.Code == sf::Key::Return ) {
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
  } else {
    intf_.startClient(entry_host_->text().c_str(), port);
    intf_.client()->newLocalPlayer("Client"); //XXX temporary player name
    submitting_ = true;
  }
}


ScreenCreateServer::ScreenCreateServer(GuiInterface &intf):
    ScreenMenu(intf)
{
}

void ScreenCreateServer::enter()
{
  ResourceManager &res_mgr = intf_.res_mgr();
  const std::string lang_section("ScreenCreateServer");
  style_button_.load(&res_mgr);

  WLabel *label = new WLabel();
  label->setText(res_mgr.getLang(lang_section, "Port"));
  label->setTextAlign(1);
  label->SetPosition(-20, -30);

  entry_port_ = new WEntry(100, 40);
  entry_port_->setText(QUOTE(DEFAULT_PNP_PORT));
  entry_port_->SetPosition(50, -30);

  WButton *button = new WButton(style_button_, 200);
  button->setCaption(res_mgr.getLang(lang_section, "Create"));
  button->SetPosition(0, 30);

  container_.widgets.push_back(label);
  container_.widgets.push_back(entry_port_);
  container_.widgets.push_back(button);

  entry_port_->setNeighbors(button, button, NULL, NULL);
  button->setNeighbors(entry_port_, entry_port_, NULL, NULL);

  this->focus(entry_port_);
}

bool ScreenCreateServer::onInputEvent(const sf::Event &ev)
{
  if( ScreenMenu::onInputEvent(ev) ) {
    return true;
  }
  if( ev.Type == sf::Event::KeyPressed ) {
    if( ev.Key.Code == sf::Key::Escape ) {
      intf_.swapScreen(new ScreenStart(intf_));
    } else if( ev.Key.Code == sf::Key::Return ) {
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
  } else {
    intf_.startServer(port);
    Player *pl = intf_.server()->newLocalPlayer("Server"); //XXX temporary player name
    intf_.swapScreen(new ScreenLobby(intf_, pl));
  }
}


ScreenLobby::ScreenLobby(GuiInterface &intf, Player *pl):
    ScreenMenu(intf),
    player_(pl)
{
}

void ScreenLobby::enter()
{
  assert( player_ );
  GameInstance *instance = intf_.instance();
  assert( instance );
  if( instance->state() == GameInstance::STATE_LOBBY ) {
    instance->playerSetReady(player_, true);
  }
}

bool ScreenLobby::onInputEvent(const sf::Event &ev)
{
  if( ScreenMenu::onInputEvent(ev) ) {
    return true;
  }
  if( ev.Type == sf::Event::KeyPressed ) {
    if( ev.Key.Code == sf::Key::Escape ) {
      intf_.swapScreen(new ScreenStart(intf_));
      return true;
    }
  }
  return false;
}

void ScreenLobby::onStateChange(GameInstance::State state)
{
  if( state == GameInstance::STATE_LOBBY ) {
    intf_.instance()->playerSetReady(player_, true);
  } else if( state == GameInstance::STATE_INIT ) {
    intf_.swapScreen(new ScreenGame(intf_, player_));
  }
}


}

