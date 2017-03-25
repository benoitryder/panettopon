#include <boost/lexical_cast.hpp>
#include "input.h"

namespace gui {


InvalidInputBindingError::InvalidInputBindingError(const std::string& name, const std::string& msg):
    std::runtime_error("invalid binding: " + name + " (" + msg + ")")
{
}


InputBinding::InputBinding(const std::string& name)
{
  if(name.substr(0, 3) == "joy") {
    type_ = Type::JOYSTICK;
    if(name.size() < 6) {
      throw InvalidInputBindingError(name, "invalid format");
    }
    if(name[3] < '0' && name[3] > '9') {
      throw InvalidInputBindingError(name, "invalid joystick number");
    }
    joystick_.id_ = name[3] - '0';
    if(joystick_.id_ >= sf::Joystick::Count) {
      throw InvalidInputBindingError(name, "bad joystick number");
    }
    std::string button = name.substr(4);
    if(button == "up") {
      joystick_.button_ = JOYSTICK_UP;
    } else if(button == "down") {
      joystick_.button_ = JOYSTICK_DOWN;
    } else if(button == "left") {
      joystick_.button_ = JOYSTICK_LEFT;
    } else if(button == "right") {
      joystick_.button_ = JOYSTICK_RIGHT;
    } else {
      try {
        joystick_.button_ = boost::lexical_cast<unsigned int>(button);
      } catch(const boost::bad_lexical_cast&) {
        throw InvalidInputBindingError(name, "bad joystick button number");
      }
      if(joystick_.button_ >= sf::Joystick::ButtonCount) {
        throw InvalidInputBindingError(name, "bad joystick button number");
      }
    }

  } else {
    type_ = Type::KEYBOARD;
    if(name.size() == 1) {
      char c = name[0];
      if(c >= 'a' && c <= 'z') {
        keyboard_.code_ = static_cast<sf::Keyboard::Key>(sf::Keyboard::A + (c - 'a'));
      } else if(c >= '0' && c <= '9') {
        keyboard_.code_ = static_cast<sf::Keyboard::Key>(sf::Keyboard::Num0 + (c - '0'));
      } else if(c == '[') {
        keyboard_.code_ = sf::Keyboard::LBracket;
      } else if(c == ']') {
        keyboard_.code_ = sf::Keyboard::RBracket;
      } else if(c == ';') {
        keyboard_.code_ = sf::Keyboard::SemiColon;
      } else if(c == ',') {
        keyboard_.code_ = sf::Keyboard::Comma;
      } else if(c == '.') {
        keyboard_.code_ = sf::Keyboard::Period;
      } else if(c == '\'') {
        keyboard_.code_ = sf::Keyboard::Quote;
      } else if(c == '/') {
        keyboard_.code_ = sf::Keyboard::Slash;
      } else if(c == '\\') {
        keyboard_.code_ = sf::Keyboard::BackSlash;
      } else if(c == '~') {
        keyboard_.code_ = sf::Keyboard::Tilde;
      } else if(c == '=') {
        keyboard_.code_ = sf::Keyboard::Equal;
      } else if(c == '-') {
        keyboard_.code_ = sf::Keyboard::Dash;
      } else {
        throw InvalidInputBindingError(name, "invalid keyboard key");
      }
    } else if(name == "space") {
      keyboard_.code_ = sf::Keyboard::Space;
    } else if(name == "return") {
      keyboard_.code_ = sf::Keyboard::Return;
    } else if(name == "backspace") {
      keyboard_.code_ = sf::Keyboard::BackSpace;
    } else if(name == "tab") {
      keyboard_.code_ = sf::Keyboard::Tab;
    } else if(name == "escape") {
      keyboard_.code_ = sf::Keyboard::Escape;
    } else if(name == "pause") {
      keyboard_.code_ = sf::Keyboard::Pause;
    } else if(name == "home") {
      keyboard_.code_ = sf::Keyboard::Home;
    } else if(name == "end") {
      keyboard_.code_ = sf::Keyboard::End;
    } else if(name == "pagedown") {
      keyboard_.code_ = sf::Keyboard::PageDown;
    } else if(name == "pageup") {
      keyboard_.code_ = sf::Keyboard::PageDown;
    } else if(name == "insert") {
      keyboard_.code_ = sf::Keyboard::Insert;
    } else if(name == "delete") {
      keyboard_.code_ = sf::Keyboard::Delete;
    } else if(name == "num0") {
      keyboard_.code_ = sf::Keyboard::Numpad0;
    } else if(name == "num1") {
      keyboard_.code_ = sf::Keyboard::Numpad1;
    } else if(name == "num2") {
      keyboard_.code_ = sf::Keyboard::Numpad2;
    } else if(name == "num3") {
      keyboard_.code_ = sf::Keyboard::Numpad3;
    } else if(name == "num4") {
      keyboard_.code_ = sf::Keyboard::Numpad4;
    } else if(name == "num5") {
      keyboard_.code_ = sf::Keyboard::Numpad5;
    } else if(name == "num6") {
      keyboard_.code_ = sf::Keyboard::Numpad6;
    } else if(name == "num7") {
      keyboard_.code_ = sf::Keyboard::Numpad7;
    } else if(name == "num8") {
      keyboard_.code_ = sf::Keyboard::Numpad8;
    } else if(name == "num9") {
      keyboard_.code_ = sf::Keyboard::Numpad9;
    } else if(name == "num+") {
      keyboard_.code_ = sf::Keyboard::Add;
    } else if(name == "num-") {
      keyboard_.code_ = sf::Keyboard::Subtract;
    } else if(name == "num*") {
      keyboard_.code_ = sf::Keyboard::Multiply;
    } else if(name == "num/") {
      keyboard_.code_ = sf::Keyboard::Divide;
    } else if(name == "left") {
      keyboard_.code_ = sf::Keyboard::Left;
    } else if(name == "right") {
      keyboard_.code_ = sf::Keyboard::Right;
    } else if(name == "up") {
      keyboard_.code_ = sf::Keyboard::Up;
    } else if(name == "down") {
      keyboard_.code_ = sf::Keyboard::Down;
    } else if(name == "f1") {
      keyboard_.code_ = sf::Keyboard::F1;
    } else if(name == "f2") {
      keyboard_.code_ = sf::Keyboard::F2;
    } else if(name == "f3") {
      keyboard_.code_ = sf::Keyboard::F3;
    } else if(name == "f4") {
      keyboard_.code_ = sf::Keyboard::F4;
    } else if(name == "f5") {
      keyboard_.code_ = sf::Keyboard::F5;
    } else if(name == "f6") {
      keyboard_.code_ = sf::Keyboard::F6;
    } else if(name == "f7") {
      keyboard_.code_ = sf::Keyboard::F7;
    } else if(name == "f8") {
      keyboard_.code_ = sf::Keyboard::F8;
    } else if(name == "f9") {
      keyboard_.code_ = sf::Keyboard::F9;
    } else if(name == "f10") {
      keyboard_.code_ = sf::Keyboard::F10;
    } else if(name == "f11") {
      keyboard_.code_ = sf::Keyboard::F11;
    } else if(name == "f12") {
      keyboard_.code_ = sf::Keyboard::F12;
    } else {
      throw InvalidInputBindingError(name, "invalid keyboard key");
    }
  }
}


constexpr InputBinding::InputBinding(MenuAction action):
    type_(Type::MENU), menu_({action})
{
}

const InputBinding InputBinding::MenuUp{MenuAction::UP};
const InputBinding InputBinding::MenuDown{MenuAction::DOWN};
const InputBinding InputBinding::MenuLeft{MenuAction::LEFT};
const InputBinding InputBinding::MenuRight{MenuAction::RIGHT};
const InputBinding InputBinding::MenuConfirm{MenuAction::CONFIRM};
const InputBinding InputBinding::MenuCancel{MenuAction::CANCEL};
const InputBinding InputBinding::MenuFocusNext{MenuAction::FOCUS_NEXT};
const InputBinding InputBinding::MenuFocusPrevious{MenuAction::FOCUS_PREVIOUS};


bool InputBinding::isActive() const
{
  if(type_ == Type::KEYBOARD) {
    return sf::Keyboard::isKeyPressed(keyboard_.code_);

  } else if(type_ == Type::JOYSTICK) {
    if(joystick_.button_ == JOYSTICK_UP) {
      return sf::Joystick::getAxisPosition(joystick_.id_, sf::Joystick::Axis::Y) < -JOYSTICK_ACTIVE_THRESHOLD ||
          sf::Joystick::getAxisPosition(joystick_.id_, sf::Joystick::Axis::PovY) > JOYSTICK_ACTIVE_THRESHOLD;
    } else if(joystick_.button_ == JOYSTICK_DOWN) {
      return sf::Joystick::getAxisPosition(joystick_.id_, sf::Joystick::Axis::Y) > JOYSTICK_ACTIVE_THRESHOLD ||
          sf::Joystick::getAxisPosition(joystick_.id_, sf::Joystick::Axis::PovY) < -JOYSTICK_ACTIVE_THRESHOLD;
    } else if(joystick_.button_ == JOYSTICK_LEFT) {
      return sf::Joystick::getAxisPosition(joystick_.id_, sf::Joystick::Axis::X) < -JOYSTICK_ACTIVE_THRESHOLD ||
          sf::Joystick::getAxisPosition(joystick_.id_, sf::Joystick::Axis::PovX) < -JOYSTICK_ACTIVE_THRESHOLD;
    } else if(joystick_.button_ == JOYSTICK_RIGHT) {
      return sf::Joystick::getAxisPosition(joystick_.id_, sf::Joystick::Axis::X) > JOYSTICK_ACTIVE_THRESHOLD ||
          sf::Joystick::getAxisPosition(joystick_.id_, sf::Joystick::Axis::PovX) > JOYSTICK_ACTIVE_THRESHOLD;
    } else {
      return sf::Joystick::isButtonPressed(joystick_.id_, joystick_.button_);
    }

  } else if(type_ == Type::MENU) {
    return false;
  }
  return false;
}


bool InputBinding::match(const sf::Event& event) const
{
  if(type_ == Type::KEYBOARD) {
    if(event.type == sf::Event::KeyPressed) {
      return event.key.code == keyboard_.code_;
    }

  } else if(type_ == Type::JOYSTICK) {
    if(event.type == sf::Event::JoystickButtonPressed) {
      return event.joystickButton.joystickId == joystick_.id_ && event.joystickButton.button == joystick_.button_;
    }

  } else if(type_ == Type::MENU) {
    if(event.type == sf::Event::KeyPressed) {
      const auto& ev = event.key;
      switch(menu_.action_) {
        case MenuAction::UP:
          return ev.code == sf::Keyboard::Up;
        case MenuAction::DOWN:
          return ev.code == sf::Keyboard::Down;
        case MenuAction::LEFT:
          return ev.code == sf::Keyboard::Left;
        case MenuAction::RIGHT:
          return ev.code == sf::Keyboard::Right;
        case MenuAction::CONFIRM:
          return ev.code == sf::Keyboard::Return;
        case MenuAction::CANCEL:
          return ev.code == sf::Keyboard::Escape;
        case MenuAction::FOCUS_NEXT:
          return ev.code == sf::Keyboard::Tab && !ev.shift;
        case MenuAction::FOCUS_PREVIOUS:
          return ev.code == sf::Keyboard::Tab && ev.shift;
        default:
          return false;
      }
    } else if(event.type == sf::Event::JoystickButtonPressed) {
      const auto& ev = event.joystickButton;
      switch(menu_.action_) {
        case MenuAction::CONFIRM:
          return ev.button == 0;
        case MenuAction::CANCEL:
          return ev.button == 1;
        case MenuAction::FOCUS_NEXT:
          return ev.button == 5;
        case MenuAction::FOCUS_PREVIOUS:
          return ev.button == 4;
        default:
          return false;
      }
    } else if(event.type == sf::Event::JoystickMoved) {
      const auto& ev = event.joystickMove;
      switch(menu_.action_) {
        case MenuAction::UP:
          return (ev.axis == sf::Joystick::Axis::Y && ev.position < 0)
              || (ev.axis == sf::Joystick::Axis::PovY && ev.position > 0);
        case MenuAction::DOWN:
          return (ev.axis == sf::Joystick::Axis::Y && ev.position > 0)
              || (ev.axis == sf::Joystick::Axis::PovY && ev.position < 0);
        case MenuAction::LEFT:
          return (ev.axis == sf::Joystick::Axis::X || ev.axis == sf::Joystick::Axis::PovX) && ev.position < 0;
        case MenuAction::RIGHT:
          return (ev.axis == sf::Joystick::Axis::X || ev.axis == sf::Joystick::Axis::PovX) && ev.position > 0;
        default:
          return false;
      }
    }
  }
  return false;
}


InputHandler::InputHandler():
    joystick_axis_pos_()
{
}


bool InputHandler::filterEvent(const sf::Event& event)
{
  if(event.type == sf::Event::JoystickMoved) {
    const auto& ev = event.joystickMove;
    int new_pos;
    if(ev.position > JOYSTICK_THRESHOLD) {
      new_pos = 1;
    } else if(ev.position < -JOYSTICK_THRESHOLD) {
      new_pos = -1;
    } else {
      new_pos = 0;
    }
    if(joystick_axis_pos_[ev.joystickId][ev.axis] == new_pos) {
      return false;
    } else {
      joystick_axis_pos_[ev.joystickId][ev.axis] = new_pos;
      return new_pos != 0;
    }
  } else {
    return true;
  }
}


}

