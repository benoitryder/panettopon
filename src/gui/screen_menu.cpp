#include "screen_menu.h"
#include "interface.h"

namespace gui {


ScreenMenu::ScreenMenu(GuiInterface &intf):
    Screen(intf), focused_(NULL)
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
  if( focused_ ) {
    if( focused_->onInputEvent(ev) ) {
      return true;
    }
  }
  return false;
}

void ScreenMenu::focus(Widget *w)
{
  if( focused_ ) {
    focused_->focus(false);
  }
  focused_ = w;
  if( focused_ ) {
    focused_->focus(true);
  }
}


}

