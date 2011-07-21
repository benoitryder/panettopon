#include "screen.h"
#include "interface.h"
#include "../log.h"

namespace gui {


Screen::Screen(GuiInterface &intf, const std::string &name):
    intf_(intf), name_(name)
{
}

Screen::~Screen() {}
void Screen::enter() {}
void Screen::exit() {}
void Screen::redraw() {}


ScreenMenu::ScreenMenu(GuiInterface &intf, const std::string &name):
    Screen(intf, name), focused_(NULL)
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
      if( ev.Key.Code == sf::Keyboard::Up ) {
        next_focused = focused_->neighbor(Widget::NEIGHBOR_UP);
      } else if( ev.Key.Code == sf::Keyboard::Down ) {
        next_focused = focused_->neighbor(Widget::NEIGHBOR_DOWN);
      } else if( ev.Key.Code == sf::Keyboard::Left ) {
        next_focused = focused_->neighbor(Widget::NEIGHBOR_LEFT);
      } else if( ev.Key.Code == sf::Keyboard::Right ) {
        next_focused = focused_->neighbor(Widget::NEIGHBOR_RIGHT);
      } else if( ev.Key.Code == sf::Keyboard::Tab ) {
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

