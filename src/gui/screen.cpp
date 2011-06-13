#include "screen.h"
#include "interface.h"
#include "../log.h"

namespace gui {


Screen::Screen(GuiInterface &intf):
    intf_(intf)
{
}

Screen::~Screen() {}
void Screen::enter() {}
void Screen::exit() {}
void Screen::redraw() {}


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

  if( ev.Type == sf::Event::KeyPressed ) {
    // move focus
    if( focused_ ) {
      Widget::Neighbor n;
      switch( ev.Key.Code ) {
        case sf::Key::Up:    n = Widget::NEIGHBOR_UP;    break;
        case sf::Key::Down:  n = Widget::NEIGHBOR_DOWN;  break;
        case sf::Key::Left:  n = Widget::NEIGHBOR_LEFT;  break;
        case sf::Key::Right: n = Widget::NEIGHBOR_RIGHT; break;
        default: n = Widget::NEIGHBOR_NONE; break;
      }
      if( n != Widget::NEIGHBOR_NONE ) {
        Widget *next_focused = focused_->neighbor(n);
        if( next_focused ) {
          this->focus(next_focused);
          return true;
        }
      }
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

