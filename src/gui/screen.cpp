#ifdef WIN32
#include <winsock2.h>  // workaround for boost bug
#endif
#include <GL/gl.h>
#include "screen.h"
#include "interface.h"
#include "../log.h"

namespace gui {

Screen::Screen(GuiInterface& intf, const std::string& name):
    Stylable(intf.res_mgr()),
    intf_(intf), name_(name)
{
  const IniFile& style = this->style();
  std::string key;
  if( searchStyle("BackgroundImage", &key) ) {
    background_.img = intf_.res_mgr().getImage(style.get<std::string>(key));
    //XXX assume that the background image always needs wrapping
    sf::Texture::bind(background_.img);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  }
  if( searchStyle("BackgroundColor", &key) ) {
    background_.color = style.get<sf::Color>(key);
  }
}

Screen::~Screen() {}

const IniFile& Screen::style() const
{
  return intf_.res_mgr().style();
}

void Screen::enter() {}
void Screen::exit() {}
void Screen::redraw()
{
  sf::RenderWindow& w = intf_.window();
  w.draw(background_);
}

void ScreenMenu::Background::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
  target.clear(color);
  if( img ) {
    states.texture = img;
    states.blendMode = sf::BlendMultiply;
    const float wx = target.getSize().x / 2. + 1;
    const float wy = target.getSize().y / 2. + 1;
    const sf::Vertex vertices[] = {
      sf::Vertex(sf::Vector2f(-wx, +wy), sf::Vector2f(-wx, +wy)),
      sf::Vertex(sf::Vector2f(-wx, -wy), sf::Vector2f(-wx, -wy)),
      sf::Vertex(sf::Vector2f(+wx, -wy), sf::Vector2f(+wx, -wy)),
      sf::Vertex(sf::Vector2f(+wx, +wy), sf::Vector2f(+wx, +wy)),
    };
    target.draw(vertices, sizeof(vertices)/sizeof(*vertices), sf::Quads, states);
  }
}


bool Screen::searchStyle(const std::string& prop, std::string* key) const
{
  const IniFile& style = this->style();
  std::string section = name_;
  for( int i=0; i<10; i++ ) {
    if( style.has(section, prop) ) {
      *key = section+'.'+prop;
      return true;
    }
    section = style.get(section, "Fallback", "");
    if( section.empty() ) {
      return false;
    }
  }
  throw Stylable::StyleError(*this, prop, "too many recursive fallbacks");
}


ScreenMenu::ScreenMenu(GuiInterface& intf, const std::string& name):
    Screen(intf, name), container_(*this, ""), focused_(NULL)
{
}

void ScreenMenu::redraw()
{
  Screen::redraw();
  sf::RenderWindow& w = intf_.window();
  w.draw(container_);
}


bool ScreenMenu::onInputEvent(const sf::Event& ev)
{
  if( focused_ ) {
    if( focused_->onInputEvent(ev) ) {
      return true;
    }
  }

  if( ev.type == sf::Event::KeyPressed ) {
    // move focus
    if( focused_ ) {
      WFocusable* next_focused = NULL;
      if( ev.key.code == sf::Keyboard::Up ) {
        next_focused = focused_->neighbor(WFocusable::NEIGHBOR_UP);
      } else if( ev.key.code == sf::Keyboard::Down ) {
        next_focused = focused_->neighbor(WFocusable::NEIGHBOR_DOWN);
      } else if( ev.key.code == sf::Keyboard::Left ) {
        next_focused = focused_->neighbor(WFocusable::NEIGHBOR_LEFT);
      } else if( ev.key.code == sf::Keyboard::Right ) {
        next_focused = focused_->neighbor(WFocusable::NEIGHBOR_RIGHT);
      } else if( ev.key.code == sf::Keyboard::Tab ) {
        if( ev.key.shift ) {
          next_focused = focused_->neighbor(WFocusable::NEIGHBOR_LEFT);
          if( ! next_focused ) {
            next_focused = focused_->neighbor(WFocusable::NEIGHBOR_UP);
          }
        } else {
          next_focused = focused_->neighbor(WFocusable::NEIGHBOR_RIGHT);
          if( ! next_focused ) {
            next_focused = focused_->neighbor(WFocusable::NEIGHBOR_DOWN);
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

void ScreenMenu::focus(WFocusable* w)
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

