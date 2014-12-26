#include "widget.h"
#include "screen.h"
#include "interface.h"
#include "resources.h"
#include "../log.h"

namespace gui {

Widget::Widget(const Screen& screen, const std::string& name):
    screen_(screen), name_(name)
{
  const IniFile& style = screen_.style();
  const std::string key = IniFile::join(screen_.name(), name_, "Pos");
  if(style.has(key)) {
    this->setPosition(style.get<sf::Vector2f>(key));
  }
}

Widget::~Widget() {}

const ResourceManager& Widget::res_mgr() const
{
  return screen_.intf().res_mgr();
}

bool Widget::searchStyle(const std::string& prop, std::string& key) const
{
  if(!name_.empty()) {
    if(screen_.StyleLoader::searchStyle({name_, prop}, key)) {
      return true;
    }
  }
  if(screen_.StyleLoader::searchStyle({this->type(), prop}, key)) {
    return true;
  }
  const IniFile& style = screen_.style();
  std::string s = IniFile::join(this->type(), prop);
  if(style.has(s)) {
    key = s;
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
  std::string key;

  size_ = getStyle<sf::Vector2f>("Size", key);
  if(size_.x <= 0 || size_.y <= 0) {
    throw StyleError(key, "invalid value");
  }

  this->applyStyle(frame_);
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
  style_.load(*this);
  style_focus_.load(StyleLoaderPrefix(*this, "Focus", true));
  style_.apply(*this);
}

void WButton::setCaption(const std::string& caption)
{
  caption_.setString(caption);
  sf::FloatRect r = caption_.getLocalBounds();
  caption_.setOrigin(r.width/2, (caption_.getFont()->getLineSpacing(caption_.getCharacterSize()))/2);
}

void WButton::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
  states.transform *= getTransform();
  frame_.render(target, states, width_);
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
  if(focused) {
    style_focus_.apply(*this);
  } else {
    style_.apply(*this);
  }
}


void WButton::Style::load(const StyleLoader& loader)
{
  std::string key;

  text.load(loader);
  frame.load(loader);

  width = loader.getStyle<float>("Width", key);
  if(width <= 0) {
    throw StyleError(key, "value must be positive");
  }
}

void WButton::Style::apply(WButton& o)
{
  text.apply(o.caption_);
  frame.apply(o.frame_);
  o.width_ = width;
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

  applyStyle(text_);
  if(searchStyle("TextAlign", key)) {
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
  text_.setOrigin(x, text_.getFont()->getLineSpacing(text_.getCharacterSize())/2);
  align_ = align;
}


const std::string& WEntry::type() const {
  static const std::string type("Entry");
  return type;
}

WEntry::WEntry(const Screen& screen, const std::string& name):
    WFocusable(screen, name), cursor_pos_(0)
{
  style_.load(*this);
  style_focus_.load(StyleLoaderPrefix(*this, "Focus", true));
  style_.apply(*this);
  cursor_.x = text_sprite_.getOrigin().x;
}

void WEntry::setText(const std::string& text)
{
  text_.setString(text);
  this->updateTextDisplay(true);
}

void WEntry::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
  states.transform *= getTransform();
  frame_.render(target, states, width_);
  target.draw(text_sprite_, states);
  if(focused()) {
    target.draw(cursor_, states);
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
  if(focused) {
    style_focus_.apply(*this);
  } else {
    style_.apply(*this);
  }
}


void WEntry::updateTextDisplay(bool force)
{
  const size_t len = text_.getString().getSize();
  if( cursor_pos_ > len ) {
    cursor_pos_ = len;
  }

  const float text_width = text_img_.getSize().x;
  float x = -text_.getPosition().x;
  const float cursor_pos_x = text_.findCharacterPos(cursor_pos_).x + x;

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


void WEntry::Style::load(const StyleLoader& loader)
{
  std::string key;

  text.load(loader);
  frame.load(loader);

  width = loader.getStyle<float>("Width", key);
  if(width <= 0) {
    throw StyleError(key, "value must be positive");
  }

  std::pair<unsigned int, unsigned int> text_margins(0, 0);
  loader.fetchStyle<decltype(text_margins)>("TextMarginsX", text_margins);
  text_margin_left = text_margins.first;
  text_margin_right = text_margins.second;
}

void WEntry::Style::apply(WEntry& o)
{
  text.apply(o.text_);
  frame.apply(o.frame_);
  o.width_ = width;

  unsigned int text_height = o.text_.getFont()->getLineSpacing(o.text_.getCharacterSize());
  o.text_img_.create(width-(text_margin_left+text_margin_right), text_height);
  o.text_sprite_.setOrigin(width/2.-text_margin_left, text_height/2.);
  o.text_sprite_.setTexture(o.text_img_.getTexture(), true);
  o.cursor_.setHeight(text_height);
  o.cursor_.setColor(text.color);
  o.updateTextDisplay(true);
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
  style_.load(*this);
  style_focus_.load(StyleLoaderPrefix(*this, "Focus", true));
  style_.apply(*this);
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
  text_.setOrigin(r.width/2, text_.getFont()->getLineSpacing(text_.getCharacterSize())/2);
}

bool WChoice::selectValue(const ItemContainer::value_type& v)
{
  auto it = std::find(items_.begin(), items_.end(), v);
  if(it != items_.end()) {
    select(it - items_.begin());
    return true;
  }
  return false;
}

void WChoice::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
  states.transform *= getTransform();
  frame_.render(target, states, width_);
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
  if(focused) {
    style_focus_.apply(*this);
  } else {
    style_.apply(*this);
  }
}


void WChoice::Style::load(const StyleLoader& loader)
{
  std::string key;

  text.load(loader);
  frame.load(loader);

  width = loader.getStyle<float>("Width", key);
  if(width <= 0) {
    throw StyleError(key, "value must be positive");
  }
}

void WChoice::Style::apply(WChoice& o)
{
  text.apply(o.text_);
  frame.apply(o.frame_);
  o.width_ = width;
}


}

