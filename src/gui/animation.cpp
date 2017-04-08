#include <cassert>
#include <cmath>
#include <stdexcept>
#include <SFML/Graphics/Transformable.hpp>
#include "animation.h"

namespace gui {


Animation::Animation():
    state_(State::NONE)
{
}


Animation::Animation(const Animator& animator, const Tween& tween, unsigned long duration, bool loop):
    animator_(animator),
    tween_(tween),
    state_(State::STARTED),
    duration_(duration),
    loop_(loop)
{
  assert(duration_ > 0);
}

void Animation::restart()
{
  assert(state_ != State::NONE);
  state_ = State::STARTED;
}

void Animation::stop()
{
  assert(state_ != State::NONE);
  state_ = State::STOPPED;
}

void Animation::update(unsigned long time)
{
  assert(state_ != State::NONE);
  if(state_ == State::STARTED) {
    start_time_ = time;
    state_ = State::RUNNING;
  } else if(state_ != State::RUNNING) {
    return;  // nothing to do
  }

  float progress = static_cast<float>(time - start_time_) / duration_;
  if(loop_) {
    progress -= std::floor(progress);
  } else if(progress > 1.0) {
    progress = 1.0;
    state_ = State::STOPPED;
  }

  animator_(tween_(progress));
}


AnimatorPosition::AnimatorPosition(sf::Transformable& animated, const sf::Vector2f& from, const sf::Vector2f& to):
    animated_(animated), from_(from), move_(to-from)
{
}

void AnimatorPosition::operator()(float progress)
{
  animated_.setPosition(from_ + move_ * progress);
}


}


using namespace gui;


AnimationBind<sf::Transformable&> IniFileConverter<AnimationBind<sf::Transformable&>>::parse(const std::string& value)
{
  size_t pos = 0;

  AnimatorBind<sf::Transformable&> animator;
  pos = parsing::convertUntil(value, pos, ':', animator);
  Tween tween;
  pos = parsing::convertUntil(value, pos, ':', tween);
  float duration_seconds;
  pos = parsing::castUntil(value, pos, ':', duration_seconds);
  unsigned long duration = duration_seconds * 1000;

  bool loop = false;
  if(pos != std::string::npos) {
    if(value.substr(pos) == "loop") {
      loop = true;
    } else {
      throw std::invalid_argument("invalid animation loop parameter");
    }
  }

  return [=](sf::Transformable& o) { return Animation(animator(o), tween, duration, loop); };
}


AnimatorBind<sf::Transformable&> IniFileConverter<AnimatorBind<sf::Transformable&>>::parse(const std::string& value)
{
  size_t pos = 0;
  std::string name;
  pos = parsing::castUntil(value, pos, ',', name);
  if(name == "position") {
    float x0, x1, y0, y1;
    pos = parsing::castUntil(value, pos, ',', x0);
    pos = parsing::castUntil(value, pos, ',', y0);
    pos = parsing::castUntil(value, pos, ',', x1);
    pos = parsing::castUntil(value, pos, 0, y1);
    return [=](sf::Transformable& o) { return AnimatorPosition(o, {x0, y0}, {x1, y1}); };
  } else {
    throw std::invalid_argument("unknown animator name");
  }
}


Tween IniFileConverter<Tween>::parse(const std::string& value)
{
  size_t pos = 0;
  std::string name;
  pos = parsing::castUntil(value, pos, ',', name);
  if(name == "linear") {
    return TweenLinear;
  } else {
    throw std::invalid_argument("unknown tween name");
  }
}

