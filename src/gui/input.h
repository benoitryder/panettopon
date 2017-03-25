#ifndef GUI_INPUT_H_
#define GUI_INPUT_H_

#include <stdexcept>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Joystick.hpp>

namespace gui {

struct InvalidInputBindingError: public std::runtime_error
{
  InvalidInputBindingError(const std::string& name, const std::string& msg);
};


/** @brief Binding of a user event
 *
 * A single class for input configuration.
 * Bindings are intended to be matched against received sf::Event.
 *
 * For keyboard keys, binding names are:
 * lowercase letters, digits, one of <tt>();,.'/\\~=-</tt>,
 * \c space, \c return, \c backspace, \c tab, \c escape, \c pause,
 * \c home, \c end, \c pageup, \c pagdown, \c insert, \c delete,
 * \c num0 to \c num9, \c num+, \c num-, \c num*, \c num/
 * \c left, \c right, \c up, \c down,
 * \c f1 to \c f12.
 *
 * For joystick buttons, binding names use this format: \c joyN-B where \c N is
 * a joystick number (starting at 0) and B is a button number (starting at 0),
 * \c up, \c down, \c left or \c right.
 */
class InputBinding
{
 private:
  /** @brief Threshold above which joystick direction is active
   *
   * This is only used for isActive(), not match().
   *
   * @note Chosen value allows to also trigger on diagonals
   */
  static constexpr float JOYSTICK_ACTIVE_THRESHOLD = 65.;

  enum class Type {
    NONE,
    KEYBOARD,
    JOYSTICK,
    MENU,
  };

  /// Special joystick buttons
  enum {
    // positions with "small" deadzone, not match()-able
    JOYSTICK_UP = sf::Joystick::Count + 100,
    JOYSTICK_DOWN,
    JOYSTICK_LEFT,
    JOYSTICK_RIGHT,
  };

  /// Menu type actions
  enum class MenuAction {
    UP, DOWN, LEFT, RIGHT,
    CONFIRM, CANCEL,
    FOCUS_NEXT, FOCUS_PREVIOUS,
  };

 public:
  /// Build a never matched, never active binding
  InputBinding();
  /// Build a menu binding
  constexpr InputBinding(MenuAction action);

  /// Build a binding from its configuration name
  static InputBinding fromName(const std::string& name);

  // Predefined menu bindings
  static const InputBinding MenuUp;
  static const InputBinding MenuDown;
  static const InputBinding MenuLeft;
  static const InputBinding MenuRight;
  static const InputBinding MenuConfirm;
  static const InputBinding MenuCancel;
  static const InputBinding MenuFocusNext;
  static const InputBinding MenuFocusPrevious;


  /** @brief Return true if binding is currently "pressed"
   * @note menu bindings never return true.
   */
  bool isActive() const;
  /** @brief Match the binding against an event
   * @note Special joystick direction bindings never return true.
   */
  bool match(const sf::Event& event) const;

 private:
  Type type_;
  union {
    struct {
      sf::Keyboard::Key code_;
    } keyboard_;
    struct {
      unsigned int id_;
      unsigned int button_;
    } joystick_;
    struct {
      MenuAction action_;
    } menu_;
  };
};


/** @brief Handle and filter SFML input events
 *
 * Filter SFML input events to handle joystick repetition.
 */
class InputHandler
{
  /** @brief Axis value above which direction is triggered
   * @note Chosen value allows to trigger only on straight directions.
   */
  static constexpr float JOYSTICK_THRESHOLD = 95.;

 public:
  InputHandler();

  /** @brief Filter an input event
   *
   * sf::Event::JoystickMoved events are inspected to keep only moves beyond
   * the threshold. Move events on the same axis are filtered out until value
   * goes cross the threshold again.
   *
   * It is then possible to check for actual moves by comparing joystick
   * position to 0.
   *
   * @return \e true if filter should be processed, \e false if it has been filtered out.
   */
  bool filterEvent(const sf::Event& event);

 private:
  /** @brief Current past-threshold position for each axis
   *
   * Possible Values 0, -1 and 1.
   */
  int joystick_axis_pos_[sf::Joystick::Count][sf::Joystick::AxisCount];
};

}

#endif
