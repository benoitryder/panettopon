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
  static constexpr float JOYSTICK_DEADZONE = 25.;
  static constexpr float JOYSTICK_MENU_DEADZONE = 95.;

  enum class Type {
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
  /// Build a binding from its configuration name
  InputBinding(const std::string& name);
  /// Build a menu binding
  constexpr InputBinding(MenuAction action);

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

}

#endif
