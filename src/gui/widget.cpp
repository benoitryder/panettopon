#include "widget.h"
#include "screen.h"
#include "interface.h"
#include "resources.h"
#include "../log.h"

namespace gui {

Widget::Widget(const Screen& screen, const std::string& name):
    Stylable(screen.intf().res_mgr()),
    screen_(screen), name_(name)
{
  const IniFile& style = screen_.style();
  const std::string section = screen_.name()+'.'+name_;
  if( style.has(section, "Pos") ) {
    this->setPosition(style.get<sf::Vector2f>(section, "Pos"));
  }
}

Widget::~Widget() {}

bool Widget::searchStyle(const std::string& prop, std::string* key) const
{
  if( !name_.empty() ) {
    if( screen_.searchStyle(name_+'.'+prop, key) ) {
      return true;
    }
  }
  if( screen_.searchStyle(this->type()+'.'+prop, key) ) {
    return true;
  }
  const IniFile& style = screen_.style();
  std::string s = this->type()+'.'+prop;
  if( style.has(s) ) {
    *key = s;
    return true;
  }
  return false;
}

std::string Widget::styleErrorSection() const
{
  return screen_.name()+"."+(name_.empty() ? type() : name_);
}

WFocusable::WFocusable(const Screen& screen, const std::string& name):
    Widget(screen, name), focused_(false)
{
  this->setNeighbors(NULL, NULL, NULL, NULL);
}

void WFocusable::setNeighbors(WFocusable* up, WFocusable* down, WFocusable* left, WFocusable* right)
{
  neighbors_[NEIGHBOR_UP] = up;
  neighbors_[NEIGHBOR_DOWN] = down;
  neighbors_[NEIGHBOR_LEFT] = left;
  neighbors_[NEIGHBOR_RIGHT] = right;
}


const std::string& WContainer::type() const {
  static const std::string type("Container");
  return type;
}

WContainer::WContainer(const Screen& screen, const std::string& name):
    Widget(screen, name)
{
}

void WContainer::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
  states.transform *= getTransform();
  Container::const_iterator it;
  for( it=widgets.begin(); it!=widgets.end(); ++it ) {
    target.draw(*it);
  }
}


const std::string& WFrame::type() const {
  static const std::string type("Frame");
  return type;
}

WFrame::WFrame(const Screen& screen, const std::string& name):
    Widget(screen, name)
{
  const IniFile& style = screen_.style();
  std::string key;
  if( searchStyle("Size", &key) ) {
    size_ = style.get<sf::Vector2f>(key);
    if( size_.x <= 0 || size_.y <= 0 ) {
      throw StyleError(key, "invalid value");
    }
  } else {
    throw StyleError(*this, "Size", "not set");
  }

  this->applyStyle(&frame_);
}

void WFrame::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
  states.transform *= getTransform();
  frame_.render(target, states, size_);
}


const std::string& WButton::type() const {
  static const std::string type("Button");
  return type;
}

WButton::WButton(const Screen& screen, const std::string& name):
    WFocusable(screen, name), callback_(NULL)
{
  const IniFile& style = screen_.style();
  std::string key;

  this->applyStyle(&caption_);

  if( searchStyle("Color", &key) ) {
    color_ = style.get<sf::Color>(key);
  }
  if( searchStyle("FocusColor", &key) ) {
    focus_color_ = style.get<sf::Color>(key);
  }

  if( searchStyle("Width", &key) ) {
    width_ = style.get<float>(key);
    if( width_ <= 0 ) {
      throw StyleError(key, "value must be positive");
    }
  } else {
    throw StyleError(*this, "Width", "not set");
  }

  this->applyStyle(&frame_);
  this->applyStyle(&focus_frame_, "Focus");
  caption_.setColor(color_);
}

void WButton::setCaption(const std::string& caption)
{
  caption_.setString(caption);
  sf::FloatRect r = caption_.getLocalBounds();
  caption_.setOrigin(r.width/2, (caption_.getFont()->getLineSpacing(caption_.getCharacterSize())+2)/2);
}

void WButton::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
  states.transform *= getTransform();
  if( this->focused() ) {
    focus_frame_.render(target, states, width_);
  } else {
    frame_.render(target, states, width_);
  }
  target.draw(caption_, states);
}

bool WButton::onInputEvent(const sf::Event& ev)
{
  if( ev.type == sf::Event::KeyPressed ) {
    if( ev.key.code == sf::Keyboard::Return ) {
      if( callback_ ) {
        callback_();
        return true;
      }
    }
  }
  return false;
}

void WButton::focus(bool focused)
{
  WFocusable::focus(focused);
  caption_.setColor(focused ? focus_color_ : color_);
}


const std::string& WLabel::type() const {
  static const std::string type("Label");
  return type;
}

WLabel::WLabel(const Screen& screen, const std::string& name):
    Widget(screen, name), align_(0)
{
  const IniFile& style = screen_.style();
  std::string key;

  this->applyStyle(&text_);
  if( searchStyle("Color", &key) ) {
    text_.setColor(style.get<sf::Color>(key));
  }
  if( searchStyle("TextAlign", &key) ) {
    const std::string align = style.get<std::string>(key);
    if( align == "left" ) {
      align_ = -1;
    } else if( align == "center" ) {
      align_ = 0;
    } else if( align == "right" ) {
      align_ = 1;
    } else {
      throw StyleError(key, "invalid value");
    }
  }
}

void WLabel::setText(const std::string& text)
{
  text_.setString(text);
  this->setTextAlign(align_); // recompute origin
}

void WLabel::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
  states.transform *= getTransform();
  target.draw(text_, states);
}

void WLabel::setTextAlign(int align)
{
  sf::FloatRect r = text_.getLocalBounds();
  float x;
  if( align == 0 ) {
    x = r.width / 2;
  } else if( align < 0 ) {
    x = 0;
  } else if( align > 0 ) {
    x = r.width;
  }
  text_.setOrigin(x, (text_.getFont()->getLineSpacing(text_.getCharacterSize())+2)/2);
  align_ = align;
}


const std::string& WEntry::type() const {
  static const std::string type("Entry");
  return type;
}

WEntry::WEntry(const Screen& screen, const std::string& name):
    WFocusable(screen, name), cursor_pos_(0)
{
  const IniFile& style = screen_.style();
  std::string key;

  this->applyStyle(&text_);

  if( searchStyle("Color", &key) ) {
    color_ = style.get<sf::Color>(key);
  }
  if( searchStyle("FocusColor", &key) ) {
    focus_color_ = style.get<sf::Color>(key);
  }

  if( searchStyle("Width", &key) ) {
    width_ = style.get<float>(key);
    if( width_ <= 0 ) {
      throw StyleError(key, "value must be positive");
    }
  } else {
    throw StyleError(*this, "Width", "not set");
  }
  std::pair<unsigned int, unsigned int> text_margins(0, 0);
  if( searchStyle("TextMarginsX", &key) ) {
    text_margins = style.get<decltype(text_margins)>(key);
  }
  unsigned int text_height = text_.getFont()->getLineSpacing(text_.getCharacterSize())+2;
  text_img_.create(width_-(text_margins.first+text_margins.second), text_height);
  text_sprite_.setOrigin(width_/2.-text_margins.first, text_height/2.);
  text_sprite_.setTexture(text_img_.getTexture(), true);
  text_sprite_.setColor(color_);
  cursor_.setHeight(text_height);
  cursor_.setColor(focus_color_);
  cursor_.x = text_sprite_.getOrigin().x;

  this->applyStyle(&frame_);
  this->applyStyle(&focus_frame_, "Focus");
}

void WEntry::setText(const std::string& text)
{
  text_.setString(text);
  this->updateTextDisplay(true);
}

void WEntry::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
  states.transform *= getTransform();
  if( this->focused() ) {
    focus_frame_.render(target, states, width_);
    target.draw(text_sprite_, states);
    target.draw(cursor_, states);
  } else {
    frame_.render(target, states, width_);
    target.draw(text_sprite_, states);
  }
}

bool WEntry::onInputEvent(const sf::Event& ev)
{
  if( ev.type == sf::Event::TextEntered ) {
    sf::Uint32 c = ev.text.unicode;
    if( c >= ' ' && c != 127 ) {  // 127 is DEL sometimes
      sf::String s = text_.getString();
      s.insert(cursor_pos_++, c);
      this->setText(s);
      return true;
    }
  } else if( ev.type == sf::Event::KeyPressed ) {
    sf::Keyboard::Key c = ev.key.code;
    // move
    if( c == sf::Keyboard::Home ) {
      cursor_pos_ = 0;
      this->updateTextDisplay();
    } else if( c == sf::Keyboard::End ) {
      cursor_pos_ = text_.getString().getSize();
      this->updateTextDisplay();
    } else if( c == sf::Keyboard::Left ) {
      if( cursor_pos_ > 0 ) {
        cursor_pos_--;
        this->updateTextDisplay();
      }
    } else if( c == sf::Keyboard::Right ) {
      if( cursor_pos_ < text_.getString().getSize() ) {
        cursor_pos_++;
        this->updateTextDisplay();
      }
    // edit
    } else if( c == sf::Keyboard::BackSpace ) {
      if( cursor_pos_ > 0 ) {
        sf::String s = text_.getString();
        s.erase(--cursor_pos_);
        this->setText(s);
      }
    } else if( c == sf::Keyboard::Delete ) {
      if( cursor_pos_ < text_.getString().getSize() ) {
        sf::String s = text_.getString();
        s.erase(cursor_pos_);
        this->setText(s);
      }
    } else {
      return false; // not processed
    }
    return true;
  }
  return false;
}

void WEntry::focus(bool focused)
{
  WFocusable::focus(focused);
  text_sprite_.setColor(focused ? focus_color_ : color_);
}


void WEntry::updateTextDisplay(bool force)
{
  const size_t len = text_.getString().getSize();
  if( cursor_pos_ > len ) {
    cursor_pos_ = len;
  }

  const float text_width = text_img_.getSize().x;
  const float cursor_pos_x = text_.findCharacterPos(cursor_pos_).x;
  float x = -text_.getPosition().x;

  if( cursor_pos_x - x > text_width ) {
    x = cursor_pos_x - text_width;
    force = true;
  } else if( cursor_pos_x < x ) {
    x = cursor_pos_x - text_width/4; // 4 is an arbitrary value
    if( x < 0 ) {
      x = 0;
    }
    force = true;
  }

  cursor_.x = cursor_pos_x - x - text_width/2;
  // redraw only if needed
  if( force ) {
    text_img_.clear(sf::Color(0,0,0,0));
    text_.setPosition(-x, 0);
    text_img_.draw(text_);
    text_img_.display();
  }
}


WEntry::Cursor::Cursor():
    x(0), color_(sf::Color::White)
{
}

void WEntry::Cursor::setHeight(float h)
{
  vertices_[0] = sf::Vertex(sf::Vector2f(0, -h/2));
  vertices_[1] = sf::Vertex(sf::Vector2f(0, h/2));
}

void WEntry::Cursor::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
  states.transform.translate(x, 0);
  target.draw(vertices_, 2, sf::Lines, states);
}

void WEntry::Cursor::setColor(const sf::Color& c)
{
  vertices_[0].color = c;
  vertices_[1].color = c;
}

const std::string& WChoice::type() const {
  static const std::string type("Choice");
  return type;
}

WChoice::WChoice(const Screen& screen, const std::string& name):
    WFocusable(screen, name)
{
  const IniFile& style = screen_.style();
  std::string key;

  this->applyStyle(&text_);

  if( searchStyle("Color", &key) ) {
    color_ = style.get<sf::Color>(key);
  }
  if( searchStyle("FocusColor", &key) ) {
    focus_color_ = style.get<sf::Color>(key);
  }

  if( searchStyle("Width", &key) ) {
    width_ = style.get<float>(key);
    if( width_ <= 0 ) {
      throw StyleError(key, "value must be positive");
    }
  } else {
    throw StyleError(*this, "Width", "not set");
  }

  this->applyStyle(&frame_);
  this->applyStyle(&focus_frame_, "Focus");
  text_.setColor(color_);
}

void WChoice::setItems(const ItemContainer& items)
{
  if( items.size() == 0 ) {
    throw std::runtime_error("empty items list");
  }
  items_ = items;
  if( index_ >= items_.size() ) {
    this->select(0);
  }
}

void WChoice::select(unsigned int i)
{
  text_.setString(items_[i]);
  index_ = i;
  sf::FloatRect r = text_.getLocalBounds();
  text_.setOrigin(r.width/2, (text_.getFont()->getLineSpacing(text_.getCharacterSize())+2)/2);
}

void WChoice::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
  states.transform *= getTransform();
  if( this->focused() ) {
    focus_frame_.render(target, states, width_);
  } else {
    frame_.render(target, states, width_);
  }
  target.draw(text_, states);
}

bool WChoice::onInputEvent(const sf::Event& ev)
{
  if( ev.type == sf::Event::KeyPressed ) {
    sf::Keyboard::Key c = ev.key.code;
    if( c == sf::Keyboard::Left ) {
      this->select( index_ == 0 ? items_.size()-1 : index_-1 );
    } else if( c == sf::Keyboard::Right ) {
      this->select( index_ == items_.size()-1 ? 0 : index_+1 );
    } else {
      return false; // not processed
    }
    return true;
  }
  return false;
}

void WChoice::focus(bool focused)
{
  WFocusable::focus(focused);
  text_.setColor(focused ? focus_color_ : color_);
}


}

