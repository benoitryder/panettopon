#ifndef GUI_ANIMATION_H_
#define GUI_ANIMATION_H_

#include <functional>

namespace sf {
  class Transformable;
}

namespace gui {


/** @brief GUI animation
 *
 * Animation is a general term for any time-based changes. This includes widget
 * moves, animated sprite, fading, zooming, ...
 *
 * Each animation requires:
 *  - an animation functor, which will update animation state
 *  - timing information (duration, looping, ...)
 *
 * Time provided to time() must use the same scale than duration (typically,
 * seconds). It must be positive and increase between two update calls.
 */
class Animation
{
 public:
  /** @brief Animation functor to update animation state
   *
   * First parameter is the animation progress as a number in [0,1].
   */
  typedef std::function<void(float)> Animator;

  /// Animation state
  enum class State {
    NONE = 0,  ///< not started, not configured
    STARTED,  ///< animation started, waiting for first update
    RUNNING,  ///< animation is running
    PAUSED,  ///< animation has been stopped
    STOPPED,  ///< animation has been stopped, or just configured
  };

  Animation(Animator animator);

  /// Return animation state
  State state() const { return state_; }

  /** @brief Start animation for given period
   *
   * If animation is running, it is restarted using provided parameters.
   */
  void start(float duration, bool loop=false);
  /// Restart animation with current parameters
  void restart();
  /// Stop animation
  void stop();

  /// Update animation state
  void update(float time);

 private:
  Animator animator_;
  State state_;  ///< animation state
  float start_time_;  ///< animation start time
  float duration_;  ///< animation duration (or looping period)
  bool loop_;  ///< true if animation is looping
};


/** @brief Type used for animation tweening
 *
 * Tween instances define how to interpolate values to animate.
 *
 * First parameter is the animation progress as a number in [0,1].
 */
typedef std::function<float(float)> Tween;

/// Linear tween (identity)
const Tween TweenLinear = [](float x) { return x; };


/// Animate position
class AnimatorPosition
{
 public:
  AnimatorPosition(sf::Transformable& animated, const sf::Vector2f& from, const sf::Vector2f& to, const Tween& tween=TweenLinear);
  void operator()(float progress);
 private:
  sf::Transformable& animated_;
  Tween tween_;
  sf::Vector2f from_;
  sf::Vector2f move_;
};


}

#endif
