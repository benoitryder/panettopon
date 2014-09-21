#ifdef WIN32
#include <winsock2.h>  // workaround for boost bug
#endif
#include <GL/gl.h>
#include "screen.h"
#include "interface.h"
#include "../log.h"

namespace gui {

Screen::Screen(GuiInterface& intf, const std::string& name):
    intf_(intf), name_(name), container_(*this, ""), focused_(nullptr)
{
  std::string val;
  if(fetchStyle<std::string>("BackgroundImage", val)) {
    background_.img = intf_.res_mgr().getImage(val);
    //XXX assume that the background image always needs wrapping
    sf::Texture::bind(background_.img);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  }
  fetchStyle<sf::Color>("BackgroundColor", background_.color);
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
  w.draw(container_);
}


bool Screen::onInputEvent(const sf::Event& ev)
{
  // handle input for focused widget

  if(!focused_) {
    return false; // nothing to do
  }

  if(focused_->onInputEvent(ev)) {
    return true;
  }

  if(ev.type == sf::Event::KeyPressed) {
    // move focus
    WFocusable* next_focused = NULL;
    if(ev.key.code == sf::Keyboard::Up) {
      next_focused = focused_->neighbor(WFocusable::NEIGHBOR_UP);
    } else if(ev.key.code == sf::Keyboard::Down) {
      next_focused = focused_->neighbor(WFocusable::NEIGHBOR_DOWN);
    } else if(ev.key.code == sf::Keyboard::Left) {
      next_focused = focused_->neighbor(WFocusable::NEIGHBOR_LEFT);
    } else if(ev.key.code == sf::Keyboard::Right) {
      next_focused = focused_->neighbor(WFocusable::NEIGHBOR_RIGHT);
    } else if(ev.key.code == sf::Keyboard::Tab) {
      if(ev.key.shift) {
        next_focused = focused_->neighbor(WFocusable::NEIGHBOR_LEFT);
        if(!next_focused) {
          next_focused = focused_->neighbor(WFocusable::NEIGHBOR_UP);
        }
      } else {
        next_focused = focused_->neighbor(WFocusable::NEIGHBOR_RIGHT);
        if(!next_focused) {
          next_focused = focused_->neighbor(WFocusable::NEIGHBOR_DOWN);
        }
      }
    }
    if(next_focused) {
      this->focus(next_focused);
      return true;
    }
  }

  return false;
}


void Screen::focus(WFocusable* w)
{
  if(focused_) {
    focused_->focus(false);
  }
  focused_ = w;
  if(focused_) {
    focused_->focus(true);
  }
}



void Screen::Background::draw(sf::RenderTarget& target, sf::RenderStates states) const
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


const ResourceManager& Screen::res_mgr() const
{
  return intf_.res_mgr();
}

bool Screen::searchStyle(const std::string& prop, std::string& key) const
{
  const IniFile& style = this->style();
  std::string section = name_;
  for( int i=0; i<10; i++ ) {
    std::string s = IniFile::join(section, prop);
    if(style.has(s)) {
      key = s;
      return true;
    }
    section = style.get({section, "Fallback"}, "");
    if(section.empty()) {
      return false;
    }
  }
  throw StyleError(*this, prop, "too many recursive fallbacks");
}


}

