#ifndef GUI_INPUT_H_
#define GUI_INPUT_H_

#include <stdexcept>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Joystick.hpp>

class IniFile;

namespace gui {

struct InvalidInputBindingError: public std::runtime_error
{
  InvalidInputBindingError(const std::string& name, const std::string& msg);
};

enum class InputType {
  NONE,
  KEYBOARD,
  JOYSTICK,
  GLOBAL,
};

/** @brief Binding of a user event
 *
 * A single class for input configuration.
 * Bindings are intended to be matched against received sf::Event.
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

 public:
  /// Special joystick buttons for directions
  enum : unsigned int {
    JOYSTICK_UP = sf::Joystick::Count + 100,
    JOYSTICK_DOWN,
    JOYSTICK_LEFT,
    JOYSTICK_RIGHT,
  };

  /// Global actions
  enum class GlobalAction {
    UP, DOWN, LEFT, RIGHT,
    CONFIRM, CANCEL,
    FOCUS_NEXT, FOCUS_PREVIOUS,
  };

  struct Keyboard {
    sf::Keyboard::Key code;
  };

  struct Joystick {
    unsigned int id;
    unsigned int button;
  };

  struct Global {
    GlobalAction action;
  };

  /// Build a never matched, never active binding
  InputBinding(): type(InputType::NONE) {}
  /// Build a global binding
  constexpr InputBinding(GlobalAction action): type(InputType::GLOBAL), global({action}) {}
  /// Build a keyboard binding
  constexpr InputBinding(sf::Keyboard::Key code): type(InputType::KEYBOARD), keyboard({code}) {}
  /// Build a joystick binding
  constexpr InputBinding(Joystick v): type(InputType::JOYSTICK), joystick(v) {}

  /** @brief Build a keyboard binding from its configuration name
   *
   * Valid names are:
   * lowercase letters, digits, one of <tt>();,.'/\\~=-</tt>,
   * \c space, \c return, \c backspace, \c tab, \c escape, \c pause,
   * \c home, \c end, \c pageup, \c pagdown, \c insert, \c delete,
   * \c num0 to \c num9, \c num+, \c num-, \c num*, \c num/
   * \c left, \c right, \c up, \c down,
   * \c f1 to \c f12.
   */
  static InputBinding fromKeyboardName(const std::string& name);

  /** @brief Build a joystick binding from its configuration name
   *
   * Valid names are button numbers, \c up, \c down, \c left, \c right.
   */
  static InputBinding fromJoystickName(const std::string& name);

  /// Change the joystick ID associated to the binding
  void setJoystickId(unsigned int id);

  /** @brief Return true if binding is currently "pressed"
   * @note Global bindings never return true.
   */
  bool isActive() const;
  /// Match the binding against an event
  bool match(const sf::Event& event) const;

  /** @brief Return true if bindings are equivalent
   *
   * Bindings are equivalent if they have the same keys or button (joystick ID
   * is ignored).
   */
  bool isEquivalent(const InputBinding& o) const;

  InputType type;
  union {
    Keyboard keyboard;
    Joystick joystick;
    Global global;
  };
};


/** @brief Set of bindings needed by a player
 *
 * All bindings are assumed to have the same type.
 */
struct InputMapping
{
  InputBinding up = {}, down, left, right;
  InputBinding swap, raise;
  InputBinding confirm, cancel;
  InputBinding focus_next, focus_previous;

  InputType type() const { return up.type; }
  void setJoystickId(unsigned int id);

  /// Parse a mapping from a configuration file
  static InputMapping parse(const IniFile& ini, const std::string section);

  /// Return true if all bindings are equivalent
  bool isEquivalent(const InputMapping& o) const;

  static InputMapping DefaultKeyboardMapping;
  static InputMapping DefaultJoystickMapping;
  /// Mapping with predefined global bindings
  static InputMapping Global;
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
   * @return \e true if event should be processed, \e false if it has been filtered out.
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
