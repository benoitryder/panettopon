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
    intf_(intf), name_(name), container_(*this, ""), focused_(nullptr),
    notification_widget_(*this, "Notif")
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

  auto animator = [&](float x) {
    // stop current notif earlier other are waiting
    if(x >= 0.5 && notifications_.size()) {
      notification_anim_.stop();
    }
  };
  notification_anim_ = Animation(animator, TweenLinear, 4000);
  this->addAnimation(notification_anim_);
  notification_anim_.stop();
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
  if(notification_anim_.state() == Animation::State::STOPPED) {
    if(notifications_.size()) {
      // update the widget, restart animation
      notification_widget_.setNotification(notifications_[0]);
      notifications_.erase(notifications_.begin());
      notification_anim_.restart();
    }
  } else if(notification_anim_.state() == Animation::State::RUNNING) {
    w.draw(notification_widget_);
  }
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


void Screen::onNotification(GameInstance::Severity sev, const std::string& msg)
{
  addNotification({sev, msg});
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


void Screen::addNotification(const Notification& notif)
{
  notifications_.push_back(notif);
}

void Screen::clearNotifications()
{
  notifications_.clear();
}


const std::string& WNotification::type() const {
  static const std::string type("Notification");
  return type;
}

WNotification::WNotification(const Screen& screen, const std::string& name):
    Widget(screen, name)
{
  std::string key;

  // load Pos again, to enable fallback based on widget type
  this->setPosition(getStyle<sf::Vector2f>("Pos"));

  applyStyle(text_);
  style_align_.load(*this);
  style_align_.apply(text_);
  applyStyle(frame_);
  width_ = getStyle<float>("Width", key);
  if(width_ <= 0) {
    throw StyleError(key, "value must be positive");
  }
}

void WNotification::setNotification(const Notification& notif)
{
  text_.setString(notif.msg);
  style_align_.apply(text_);
}

void WNotification::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
  states.transform *= getTransform();
  frame_.render(target, states, width_);
  target.draw(text_, states);
}


}

