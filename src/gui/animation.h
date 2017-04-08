#ifndef GUI_ANIMATION_H_
#define GUI_ANIMATION_H_

#include <functional>
#include "../inifile.h"


namespace sf {
  class Transformable;
}

namespace gui {

/** @brief Functor to update animation state
 *
 * First parameter is the animation progress as a number in [0,1].
 */
typedef std::function<void(float)> Animator;
/** @brief Functor to interpolate time progress into animation progress
 */
typedef std::function<float(float)> Tween;


/** @brief GUI animation
 *
 * Animation is a general term for any time-based changes. This includes widget
 * moves, animated sprite, fading, zooming, ...
 *
 * Time provided to update() must use the same scale as duration (typically,
 * milliseconds). It must be positive and increase between two calls.
 */
class Animation
{
 public:
  /// Animation state
  enum class State {
    NONE,  ///< animation not configured
    STARTED,  ///< animation started, waiting for first update
    RUNNING,  ///< animation is running
    STOPPED,  ///< animation has been stopped
  };

  Animation();
  Animation(const Animator& animator, const Tween& tween, unsigned long duration, bool loop=false);

  State state() const { return state_; }
  void restart();
  void stop();

  /// Update animation state
  void update(unsigned long time);

 private:
  Animator animator_;
  Tween tween_;
  State state_;
  unsigned long start_time_;  ///< animation start time
  unsigned long duration_;  ///< animation duration (or looping period)
  bool loop_;  ///< true if animation is looping
};


/** @brief Type for binded animation
 *
 * Animators are constructed with the animated object, which may not be known
 * immediately (e.g. when parsing a style).
 *
 * This type can be used for unresolved animations.
 */
template<typename ... Args> using AnimationBind = std::function<Animation(Args...)>;

template<typename ... Args> using AnimatorBind = std::function<Animator(Args...)>;


/// Animate position
class AnimatorPosition
{
 public:
  AnimatorPosition(sf::Transformable& animated, const sf::Vector2f& from, const sf::Vector2f& to);
  void operator()(float progress);
 private:
  sf::Transformable& animated_;
  sf::Vector2f from_;
  sf::Vector2f move_;
};


inline float TweenLinear(float x) { return x; }
inline float TweenLinearLoop(float x) { return x <= 0.5 ? 2*x : 2-2*x; }
float TweenSin(float x);
float TweenSinLoop(float x);

}


/** @brief Parse a (binded) animation style value
 *
 * Format: <tt>animator;tween;duration[;loop]</tt>
 */
template <> struct IniFileConverter<gui::AnimationBind<sf::Transformable&>>
{
  static gui::AnimationBind<sf::Transformable&> parse(const std::string& value);
};

/** @brief Parse a (binded) animator
 *
 * Formats:
 *  - <tt>position,x0,y0,x1,y0</tt>
 */
template <> struct IniFileConverter<gui::AnimatorBind<sf::Transformable&>>
{
  static gui::AnimatorBind<sf::Transformable&> parse(const std::string& value);
};

/** @brief Parse a twee
 *
 * Formats:
 *  - <tt>linear</tt>
 *  - <tt>linearloop</tt>
 *  - <tt>sin</tt>
 */
template <> struct IniFileConverter<gui::Tween>
{
  static gui::Tween parse(const std::string& value);
};


#endif
