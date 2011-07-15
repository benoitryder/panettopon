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
  w.Draw(container_);
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
      Widget *next_focused = NULL;
      if( ev.Key.Code == sf::Key::Up ) {
        next_focused = focused_->neighbor(Widget::NEIGHBOR_UP);
      } else if( ev.Key.Code == sf::Key::Down ) {
        next_focused = focused_->neighbor(Widget::NEIGHBOR_DOWN);
      } else if( ev.Key.Code == sf::Key::Left ) {
        next_focused = focused_->neighbor(Widget::NEIGHBOR_LEFT);
      } else if( ev.Key.Code == sf::Key::Right ) {
        next_focused = focused_->neighbor(Widget::NEIGHBOR_RIGHT);
      } else if( ev.Key.Code == sf::Key::Tab ) {
        if( ev.Key.Shift ) {
          next_focused = focused_->neighbor(Widget::NEIGHBOR_LEFT);
          if( ! next_focused ) {
            next_focused = focused_->neighbor(Widget::NEIGHBOR_UP);
          }
        } else {
          next_focused = focused_->neighbor(Widget::NEIGHBOR_RIGHT);
          if( ! next_focused ) {
            next_focused = focused_->neighbor(Widget::NEIGHBOR_DOWN);
          }
        }
      }
      if( next_focused ) {
        this->focus(next_focused);
        return true;
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

