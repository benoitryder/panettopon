#include <GL/gl.h>
#include "screen.h"
#include "interface.h"
#include "../log.h"

namespace gui {

Screen::StyleError::StyleError(const Screen& s, const std::string& prop, const std::string& msg):
  std::runtime_error("style error for "+s.name_+"."+prop+": "+msg) {}


Screen::Screen(GuiInterface &intf, const std::string &name):
    intf_(intf), name_(name)
{
  const IniFile& style = this->style();
  std::string key;
  if( searchStyle("BackgroundImage", &key) ) {
    background_.img = intf_.res_mgr().getImage(style.get<std::string>(key));
    //XXX assume that the background image always needs wrapping
    background_.img->bind();
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
  sf::RenderWindow &w = intf_.window();
  w.draw(background_);
}

void ScreenMenu::Background::Render(sf::RenderTarget &target, sf::Renderer &renderer) const
{
  renderer.Clear(color);
  if( img ) {
    renderer.SetColor(color);
    renderer.SetTexture(img);
    const float wx = target.getSize().x / 2. + 1;
    const float wy = target.getSize().y / 2. + 1;
    const float ix = wx / img->getSize().x;
    const float iy = wy / img->getSize().y;
    renderer.Begin(sf::Renderer::QuadList);
    renderer.AddVertex(-wx, +wy, -ix, +iy);
    renderer.AddVertex(-wx, -wy, -ix, -iy);
    renderer.AddVertex(+wx, -wy, +ix, -iy);
    renderer.AddVertex(+wx, +wy, +ix, +iy);
    renderer.End();
  }
}


bool Screen::searchStyle(const std::string& prop, std::string *key) const
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
  throw StyleError(*this, prop, "too many recursive fallbacks");
}


ScreenMenu::ScreenMenu(GuiInterface &intf, const std::string &name):
    Screen(intf, name), container_(*this, ""), focused_(NULL)
{
}

void ScreenMenu::redraw()
{
  Screen::redraw();
  sf::RenderWindow &w = intf_.window();
  w.draw(container_);
}


bool ScreenMenu::onInputEvent(const sf::Event &ev)
{
  if( focused_ ) {
    if( focused_->onInputEvent(ev) ) {
      return true;
    }
  }

  if( ev.type == sf::Event::KeyPressed ) {
    // move focus
    if( focused_ ) {
      WFocusable *next_focused = NULL;
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

void ScreenMenu::focus(WFocusable *w)
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

