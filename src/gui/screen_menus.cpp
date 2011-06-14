#include <boost/bind.hpp>
#include "screen_menus.h"
#include "interface.h"

namespace gui {


ScreenStart::ScreenStart(GuiInterface &intf):
    ScreenMenu(intf)
{
}

void ScreenStart::enter()
{
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


}

