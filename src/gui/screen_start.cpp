#include "screen_start.h"
#include "interface.h"
#include "wbutton.h"

namespace gui {


ScreenStart::ScreenStart(GuiInterface &intf):
    ScreenMenu(intf)
{
}

void ScreenStart::enter()
{
  WButton *button = new WButton(300,50);
  button->setCaption("Exit");
  container_.widgets.push_back(button);
}

bool ScreenStart::onInputEvent(const sf::Event &ev)
{
  if( ScreenMenu::onInputEvent(ev) ) {
    return true;
  }
  if( ev.Type == sf::Event::KeyPressed ) {
    if( ev.Key.Code == sf::Key::Escape ) {
      intf_.swapScreen(NULL);
      return true;
    }
  }
  return false;
}


}

