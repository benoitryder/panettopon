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

  // create buttons
  const char *labels[] = {
    "Join server",
    "Create server",
    "Exit",
  };
  const int button_nb = sizeof(labels)/sizeof(*labels);
  const float button_dy = 60;
  WButton *buttons[button_nb];
  int i;
  for( i=0; i<button_nb; i++ ) {
    WButton *button = new WButton(300, 50);
    button->setCaption(labels[i]);
    button->setPosition(0, i*button_dy-(button_nb-1)*button_dy/2);
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

void ScreenStart::onCreateServer()
{
  intf_.swapScreen(new ScreenCreateServer(intf_));
}


ScreenCreateServer::ScreenCreateServer(GuiInterface &intf):
    ScreenMenu(intf)
{
}

void ScreenCreateServer::enter()
{
  WLabel *label = new WLabel();
  label->setText("Port");
  label->setTextAlign(1);
  label->setPosition(-20, -30);

  entry_port_ = new WEntry(100, 40);
  entry_port_->setText("2426"); //XXX define default value in a macro
  entry_port_->setPosition(50, -30);

  WButton *button = new WButton(200, 50);
  button->setCaption("Create");
  button->setPosition(0, 30);

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

