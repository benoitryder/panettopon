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

