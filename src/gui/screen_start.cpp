#include "screen_start.h"
#include "interface.h"
#include "wbutton.h"

namespace gui {


ScreenStart::ScreenStart(GuiInterface &intf):
    Screen(intf)
{
}

void ScreenStart::enter()
{
  WButton *button = new WButton(300,50);
  button->setCaption("Exit");
  container_.widgets.push_back(button);
}

void ScreenStart::redraw()
{
  sf::RenderWindow &w = intf_.window();
  w.Clear(sf::Color(48,48,48));
  container_.draw(w);
}

bool ScreenStart::onInputEvent(const sf::Event &ev)
{
  if( ev.Type == sf::Event::KeyPressed ) {
    if( ev.Key.Code == sf::Key::Escape ) {
      intf_.swapScreen(NULL);
      return true;
    }
  }
  return false;
}


}

