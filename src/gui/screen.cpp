#ifdef WIN32
#include <winsock2.h>  // workaround for boost bug
#endif
#include <GL/gl.h>
#include "screen.h"
#include "interface.h"
#include "input.h"
#include "../log.h"

namespace gui {

Screen::Screen(GuiInterface& intf, const std::string& name):
    intf_(intf), name_(name), container_(*this, ""), focused_(nullptr)
{
  std::string val;
  if(fetchStyle<std::string>("BackgroundImage", val)) {
    background_.img = &intf_.res_mgr().getImage(val);
    // note: assume that the background image always needs wrapping
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

  if(focused_->onInputEvent(InputMapping::Global, ev)) {
    return true;
  }

  WFocusable* next_focused = focused_->neighborToFocus(InputMapping::Global, ev);
  if(next_focused) {
    this->focus(next_focused);
    return true;
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
    const auto& view_size = target.getView().getSize();
    const float wx = view_size.x / 2.;
    const float wy = view_size.y / 2.;
    const sf::Vertex vertices[] = {
      {{-wx, +wy}, {-wx, +wy}},
      {{-wx, -wy}, {-wx, -wy}},
      {{+wx, -wy}, {+wx, -wy}},
      {{+wx, +wy}, {+wx, +wy}},
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


void Screen::updateAnimations(unsigned long time)
{
  for(auto& ani : animations_) {
    ani->update(time);
  }
}

void Screen::addAnimation(Animation& animation)
{
  animations_.push_back(&animation);
}

void Screen::removeAnimation(const Animation& animation)
{
  for(auto it=animations_.begin(); it!=animations_.end(); ++it) {
    if(*it == &animation) {
      animations_.erase(it);
      return;
    }
  }
  assert(false);  // not found
}


}

