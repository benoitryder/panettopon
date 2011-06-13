#include "screen_menu.h"
#include "interface.h"

namespace gui {


ScreenMenu::ScreenMenu(GuiInterface &intf):
    Screen(intf)
{
}

void ScreenMenu::redraw()
{
  sf::RenderWindow &w = intf_.window();
  w.Clear(sf::Color(48,48,48)); //XXX:tmp
  container_.draw(w);
}

bool ScreenMenu::onInputEvent(const sf::Event &ev)
{
  (void)ev;
  return false;
}


}

