#include <boost/lexical_cast.hpp>
#include "input.h"
#include "../inifile.h"

namespace gui {


InvalidInputBindingError::InvalidInputBindingError(const std::string& name, const std::string& msg):
    std::runtime_error("invalid binding: " + name + " (" + msg + ")")
{
}


InputBinding InputBinding::fromKeyboardName(const std::string& name)
{
  sf::Keyboard::Key code;
  if(name.size() == 1) {
    char c = name[0];
    if(c >= 'a' && c <= 'z') {
      code = static_cast<sf::Keyboard::Key>(sf::Keyboard::A + (c - 'a'));
    } else if(c >= '0' && c <= '9') {
      code = static_cast<sf::Keyboard::Key>(sf::Keyboard::Num0 + (c - '0'));
    } else if(c == '[') {
      code = sf::Keyboard::LBracket;
    } else if(c == ']') {
      code = sf::Keyboard::RBracket;
    } else if(c == ';') {
      code = sf::Keyboard::SemiColon;
    } else if(c == ',') {
      code = sf::Keyboard::Comma;
    } else if(c == '.') {
      code = sf::Keyboard::Period;
    } else if(c == '\'') {
      code = sf::Keyboard::Quote;
    } else if(c == '/') {
      code = sf::Keyboard::Slash;
    } else if(c == '\\') {
      code = sf::Keyboard::BackSlash;
    } else if(c == '~') {
      code = sf::Keyboard::Tilde;
    } else if(c == '=') {
      code = sf::Keyboard::Equal;
    } else if(c == '-') {
      code = sf::Keyboard::Dash;
    } else {
      throw InvalidInputBindingError(name, "invalid keyboard key");
    }
  } else if(name == "space") {
    code = sf::Keyboard::Space;
  } else if(name == "return") {
    code = sf::Keyboard::Return;
  } else if(name == "backspace") {
    code = sf::Keyboard::BackSpace;
  } else if(name == "tab") {
    code = sf::Keyboard::Tab;
  } else if(name == "escape") {
    code = sf::Keyboard::Escape;
  } else if(name == "pause") {
    code = sf::Keyboard::Pause;
  } else if(name == "home") {
    code = sf::Keyboard::Home;
  } else if(name == "end") {
    code = sf::Keyboard::End;
  } else if(name == "pagedown") {
    code = sf::Keyboard::PageDown;
  } else if(name == "pageup") {
    code = sf::Keyboard::PageDown;
  } else if(name == "insert") {
    code = sf::Keyboard::Insert;
  } else if(name == "delete") {
    code = sf::Keyboard::Delete;
  } else if(name == "num0") {
    code = sf::Keyboard::Numpad0;
  } else if(name == "num1") {
    code = sf::Keyboard::Numpad1;
  } else if(name == "num2") {
    code = sf::Keyboard::Numpad2;
  } else if(name == "num3") {
    code = sf::Keyboard::Numpad3;
  } else if(name == "num4") {
    code = sf::Keyboard::Numpad4;
  } else if(name == "num5") {
    code = sf::Keyboard::Numpad5;
  } else if(name == "num6") {
    code = sf::Keyboard::Numpad6;
  } else if(name == "num7") {
    code = sf::Keyboard::Numpad7;
  } else if(name == "num8") {
    code = sf::Keyboard::Numpad8;
  } else if(name == "num9") {
    code = sf::Keyboard::Numpad9;
  } else if(name == "num+") {
    code = sf::Keyboard::Add;
  } else if(name == "num-") {
    code = sf::Keyboard::Subtract;
  } else if(name == "num*") {
    code = sf::Keyboard::Multiply;
  } else if(name == "num/") {
    code = sf::Keyboard::Divide;
  } else if(name == "left") {
    code = sf::Keyboard::Left;
  } else if(name == "right") {
    code = sf::Keyboard::Right;
  } else if(name == "up") {
    code = sf::Keyboard::Up;
  } else if(name == "down") {
    code = sf::Keyboard::Down;
  } else if(name == "f1") {
    code = sf::Keyboard::F1;
  } else if(name == "f2") {
    code = sf::Keyboard::F2;
  } else if(name == "f3") {
    code = sf::Keyboard::F3;
  } else if(name == "f4") {
    code = sf::Keyboard::F4;
  } else if(name == "f5") {
    code = sf::Keyboard::F5;
  } else if(name == "f6") {
    code = sf::Keyboard::F6;
  } else if(name == "f7") {
    code = sf::Keyboard::F7;
  } else if(name == "f8") {
    code = sf::Keyboard::F8;
  } else if(name == "f9") {
    code = sf::Keyboard::F9;
  } else if(name == "f10") {
    code = sf::Keyboard::F10;
  } else if(name == "f11") {
    code = sf::Keyboard::F11;
  } else if(name == "f12") {
    code = sf::Keyboard::F12;
  } else {
    throw InvalidInputBindingError(name, "invalid keyboard key");
  }

  return InputBinding(code);
}

InputBinding InputBinding::fromJoystickName(const std::string& name)
{
  unsigned int button = 0;
  if(name == "up") {
    button = JOYSTICK_UP;
  } else if(name == "down") {
    button = JOYSTICK_DOWN;
  } else if(name == "left") {
    button = JOYSTICK_LEFT;
  } else if(name == "right") {
    button = JOYSTICK_RIGHT;
  } else {
    try {
      button = boost::lexical_cast<unsigned int>(name);
    } catch(const boost::bad_lexical_cast&) {
      throw InvalidInputBindingError(name, "bad joystick button number");
    }
    if(button >= sf::Joystick::ButtonCount) {
      throw InvalidInputBindingError(name, "bad joystick button number");
    }
  }

  return InputBinding(Joystick{0, button});
}

void InputBinding::setJoystickId(unsigned int id)
{
  assert(type_ == InputType::JOYSTICK);
  if(type_ == InputType::JOYSTICK) {
    joystick_.id_ = id;
  }
}


const InputBinding InputBinding::GlobalUp{GlobalAction::UP};
const InputBinding InputBinding::GlobalDown{GlobalAction::DOWN};
const InputBinding InputBinding::GlobalLeft{GlobalAction::LEFT};
const InputBinding InputBinding::GlobalRight{GlobalAction::RIGHT};
const InputBinding InputBinding::GlobalConfirm{GlobalAction::CONFIRM};
const InputBinding InputBinding::GlobalCancel{GlobalAction::CANCEL};
const InputBinding InputBinding::GlobalFocusNext{GlobalAction::FOCUS_NEXT};
const InputBinding InputBinding::GlobalFocusPrevious{GlobalAction::FOCUS_PREVIOUS};


bool InputBinding::isActive() const
{
  if(type_ == InputType::KEYBOARD) {
    return sf::Keyboard::isKeyPressed(keyboard_.code_);

  } else if(type_ == InputType::JOYSTICK) {
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

  } else if(type_ == InputType::GLOBAL) {
    return false;
  }
  return false;
}


bool InputBinding::match(const sf::Event& event) const
{
  if(type_ == InputType::KEYBOARD) {
    if(event.type == sf::Event::KeyPressed) {
      return event.key.code == keyboard_.code_;
    }

  } else if(type_ == InputType::JOYSTICK) {
    if(event.type == sf::Event::JoystickButtonPressed) {
      return event.joystickButton.joystickId == joystick_.id_ && event.joystickButton.button == joystick_.button_;
    } else if(event.type == sf::Event::JoystickMoved) {
      const auto& ev = event.joystickMove;
      if(ev.joystickId == joystick_.id_) {
        switch(joystick_.button_) {
          case JOYSTICK_UP:
            return (ev.axis == sf::Joystick::Axis::Y && ev.position < 0)
                || (ev.axis == sf::Joystick::Axis::PovY && ev.position > 0);
          case JOYSTICK_DOWN:
            return (ev.axis == sf::Joystick::Axis::Y && ev.position > 0)
                || (ev.axis == sf::Joystick::Axis::PovY && ev.position < 0);
          case JOYSTICK_LEFT:
            return (ev.axis == sf::Joystick::Axis::X || ev.axis == sf::Joystick::Axis::PovX) && ev.position < 0;
          case JOYSTICK_RIGHT:
            return (ev.axis == sf::Joystick::Axis::X || ev.axis == sf::Joystick::Axis::PovX) && ev.position > 0;
          default:
            return false;
        }
      }
    }

  } else if(type_ == InputType::GLOBAL) {
    if(event.type == sf::Event::KeyPressed) {
      const auto& ev = event.key;
      switch(global_.action_) {
        case GlobalAction::UP:
          return ev.code == sf::Keyboard::Up;
        case GlobalAction::DOWN:
          return ev.code == sf::Keyboard::Down;
        case GlobalAction::LEFT:
          return ev.code == sf::Keyboard::Left;
        case GlobalAction::RIGHT:
          return ev.code == sf::Keyboard::Right;
        case GlobalAction::CONFIRM:
          return ev.code == sf::Keyboard::Return;
        case GlobalAction::CANCEL:
          return ev.code == sf::Keyboard::Escape;
        case GlobalAction::FOCUS_NEXT:
          return ev.code == sf::Keyboard::Tab && !ev.shift;
        case GlobalAction::FOCUS_PREVIOUS:
          return ev.code == sf::Keyboard::Tab && ev.shift;
        default:
          return false;
      }
    } else if(event.type == sf::Event::JoystickButtonPressed) {
      const auto& ev = event.joystickButton;
      switch(global_.action_) {
        case GlobalAction::CONFIRM:
          return ev.button == 0;
        case GlobalAction::CANCEL:
          return ev.button == 1;
        case GlobalAction::FOCUS_NEXT:
          return ev.button == 5;
        case GlobalAction::FOCUS_PREVIOUS:
          return ev.button == 4;
        default:
          return false;
      }
    } else if(event.type == sf::Event::JoystickMoved) {
      const auto& ev = event.joystickMove;
      switch(global_.action_) {
        case GlobalAction::UP:
          return (ev.axis == sf::Joystick::Axis::Y && ev.position < 0)
              || (ev.axis == sf::Joystick::Axis::PovY && ev.position > 0);
        case GlobalAction::DOWN:
          return (ev.axis == sf::Joystick::Axis::Y && ev.position > 0)
              || (ev.axis == sf::Joystick::Axis::PovY && ev.position < 0);
        case GlobalAction::LEFT:
          return (ev.axis == sf::Joystick::Axis::X || ev.axis == sf::Joystick::Axis::PovX) && ev.position < 0;
        case GlobalAction::RIGHT:
          return (ev.axis == sf::Joystick::Axis::X || ev.axis == sf::Joystick::Axis::PovX) && ev.position > 0;
        default:
          return false;
      }
    }
  }
  return false;
}


void InputMapping::setJoystickId(unsigned int id)
{
  assert(type() == InputType::JOYSTICK);
  if(type() == InputType::JOYSTICK) {
    up.setJoystickId(id);
    down.setJoystickId(id);
    left.setJoystickId(id);
    right.setJoystickId(id);
    swap.setJoystickId(id);
    raise.setJoystickId(id);
    confirm.setJoystickId(id);
    cancel.setJoystickId(id);
  }
}


InputMapping InputMapping::parse(const IniFile& ini, const std::string section)
{
  InputMapping mapping;
  const std::string str_type = ini.get({section, "Type"}, "");
  if(str_type == "keyboard") {
    mapping.up = InputBinding::fromKeyboardName(ini.get<std::string>({section, "Up"}));
    mapping.down = InputBinding::fromKeyboardName(ini.get<std::string>({section, "Down"}));
    mapping.left = InputBinding::fromKeyboardName(ini.get<std::string>({section, "Left"}));
    mapping.right = InputBinding::fromKeyboardName(ini.get<std::string>({section, "Right"}));
    mapping.swap = InputBinding::fromKeyboardName(ini.get<std::string>({section, "Swap"}));
    mapping.raise = InputBinding::fromKeyboardName(ini.get<std::string>({section, "Raise"}));
    mapping.confirm = InputBinding::fromKeyboardName(ini.get<std::string>({section, "Confirm"}));
    mapping.cancel = InputBinding::fromKeyboardName(ini.get<std::string>({section, "Cancel"}));
  } else if(str_type == "joystick") {
    mapping.up = InputBinding({0, InputBinding::JOYSTICK_UP});
    mapping.down = InputBinding({0, InputBinding::JOYSTICK_DOWN});
    mapping.left = InputBinding({0, InputBinding::JOYSTICK_LEFT});
    mapping.right = InputBinding({0, InputBinding::JOYSTICK_RIGHT});
    mapping.swap = InputBinding::fromJoystickName(ini.get<std::string>({section, "Swap"}));
    mapping.raise = InputBinding::fromJoystickName(ini.get<std::string>({section, "Raise"}));
    mapping.confirm = InputBinding::fromJoystickName(ini.get<std::string>({section, "Confirm"}));
    mapping.cancel = InputBinding::fromJoystickName(ini.get<std::string>({section, "Cancel"}));
  } else {
    throw std::runtime_error("invalid mapping type: "+str_type);
  }
  return mapping;
}


InputMapping InputMapping::DefaultKeyboardMapping{
  .up = InputBinding(sf::Keyboard::Up),
  .down = InputBinding(sf::Keyboard::Down),
  .left = InputBinding(sf::Keyboard::Left),
  .right = InputBinding(sf::Keyboard::Right),
  .swap = InputBinding(sf::Keyboard::D),
  .raise = InputBinding(sf::Keyboard::F),
  .confirm = InputBinding(sf::Keyboard::Return),
  .cancel = InputBinding(sf::Keyboard::Escape),
};

InputMapping InputMapping::DefaultJoystickMapping{
  .up = InputBinding({0, InputBinding::JOYSTICK_UP}),
  .down = InputBinding({0, InputBinding::JOYSTICK_DOWN}),
  .left = InputBinding({0, InputBinding::JOYSTICK_LEFT}),
  .right = InputBinding({0, InputBinding::JOYSTICK_RIGHT}),
  .swap = InputBinding({0, 0}),
  .raise = InputBinding({0, 4}),
  .confirm = InputBinding({0, 0}),
  .cancel = InputBinding({0, 1}),
};



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

