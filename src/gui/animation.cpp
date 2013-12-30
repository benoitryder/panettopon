#include <cassert>
#include <cmath>
#include <stdexcept>
#include <SFML/Graphics/Transformable.hpp>
#include "animation.h"

namespace gui {


Animation::Animation(Animator animator):
    animator_(animator),
    state_(State::NONE)
{
}


void Animation::start(float duration, bool loop)
{
  assert(duration > 0);
  duration_ = duration;
  loop_ = loop;
  state_ = State::STOPPED;
  this->restart();
}


void Animation::restart()
{
  if(state_ == State::NONE) {
    throw std::runtime_error("animation never started, cannot restart it");
  }
  this->stop();
  state_ = State::STARTED;
}

void Animation::stop()
{
  if(state_ == State::NONE) {
    return;  // nothing to do
  }
  state_ = State::STOPPED;
}

void Animation::update(float time)
{
  if(state_ == State::STARTED) {
    start_time_ = time;
    state_ = State::RUNNING;
  } else if(state_ != State::RUNNING) {
    return;  // nothing to do
  }

  float progress = (time - start_time_) / duration_;
  if(loop_) {
    progress -= std::floor(progress);
  } else if(progress > 1.0) {
    progress = 1.0;
    state_ = State::STOPPED;
  }
  animator_(progress);
}


AnimatorPosition::AnimatorPosition(sf::Transformable& animated, const sf::Vector2f& from, const sf::Vector2f& to, const Tween& tween):
    animated_(animated), tween_(tween),
    from_(from), move_(to-from)
{
}

void AnimatorPosition::operator()(float progress)
{
  animated_.setPosition(from_ + move_ * tween_(progress));
}


}

