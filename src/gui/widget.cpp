#include "widget.h"
#include "screen.h"
#include "interface.h"
#include "resources.h"
#include "input.h"
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

void WFocusable::setNeighbor(Neighbor n, WFocusable* w)
{
  neighbors_[n] = w;
}

void WFocusable::setNeighbors(WFocusable* up, WFocusable* down, WFocusable* left, WFocusable* right)
{
  neighbors_[NEIGHBOR_UP] = up;
  neighbors_[NEIGHBOR_DOWN] = down;
  neighbors_[NEIGHBOR_LEFT] = left;
  neighbors_[NEIGHBOR_RIGHT] = right;
}

WFocusable* WFocusable::neighborToFocus(const InputMapping& mapping, const sf::Event& ev)
{
  if(!focused_) {
    return nullptr;
  }
  if(mapping.up.match(ev)) {
    return neighbors_[NEIGHBOR_UP];
  } else if(mapping.down.match(ev)) {
    return neighbors_[NEIGHBOR_DOWN];
  } else if(mapping.left.match(ev)) {
    return neighbors_[NEIGHBOR_LEFT];
  } else if(mapping.right.match(ev)) {
    return neighbors_[NEIGHBOR_RIGHT];
  } else if(mapping.focus_next.match(ev)) {
    WFocusable* next = neighbors_[NEIGHBOR_RIGHT];
    if(!next) {
      next = neighbors_[NEIGHBOR_DOWN];
    }
    return next;
  } else if(mapping.focus_previous.match(ev)) {
    WFocusable* next = neighbors_[NEIGHBOR_LEFT];
    if(!next) {
      next = neighbors_[NEIGHBOR_UP];
    }
    return next;
  }

  return nullptr;
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
  for(auto const& w : widgets) {
    target.draw(*w, states);
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
    WFocusable(screen, name), current_style_(&style_), callback_(NULL)
{
  style_.load(*this);
  style_focus_.load(StyleLoaderPrefix(*this, "Focus", true));
  current_style_->apply(*this);
}

void WButton::setCaption(const std::string& caption)
{
  caption_.setString(caption);
  current_style_->align.apply(caption_);
}

void WButton::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
  states.transform *= getTransform();
  frame_.render(target, states, current_style_->width);
  target.draw(caption_, states);
}

bool WButton::onInputEvent(const InputMapping& mapping, const sf::Event& ev)
{
  if(mapping.confirm.match(ev)) {
    if( callback_ ) {
      callback_();
      return true;
    }
  }
  return false;
}

void WButton::focus(bool focused)
{
  WFocusable::focus(focused);
  if(focused) {
    current_style_ = &style_focus_;
  } else {
    current_style_ = &style_;
  }
  current_style_->apply(*this);
}


void WButton::Style::load(const StyleLoader& loader)
{
  std::string key;

  text.load(loader);
  align.load(loader);
  frame.load(loader);

  width = loader.getStyle<float>("Width", key);
  if(width <= 0) {
    throw StyleError(key, "value must be positive");
  }
}

void WButton::Style::apply(WButton& o)
{
  text.apply(o.caption_);
  align.apply(o.caption_);
  frame.apply(o.frame_);
}


const std::string& WLabel::type() const {
  static const std::string type("Label");
  return type;
}

WLabel::WLabel(const Screen& screen, const std::string& name):
    Widget(screen, name)
{
  applyStyle(text_);
  style_align_.load(*this);
  style_align_.apply(text_);
}

void WLabel::setText(const std::string& text)
{
  text_.setString(text);
  style_align_.apply(text_);
}

void WLabel::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
  states.transform *= getTransform();
  target.draw(text_, states);
}


const std::string& WEntry::type() const {
  static const std::string type("Entry");
  return type;
}

WEntry::WEntry(const Screen& screen, const std::string& name, bool auto_active):
    WFocusable(screen, name), cursor_pos_(0), active_(false), auto_active_(auto_active),
    current_style_(&style_)
{
  style_.load(*this);
  style_focus_.load(StyleLoaderPrefix(*this, "Focus", true));
  style_active_.load(StyleLoaderPrefix(*this, "Active", true));
  current_style_->apply(*this);
}

void WEntry::setText(const std::string& text)
{
  text_.setString(text);
  this->updateTextDisplay(true);
}

void WEntry::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
  states.transform *= getTransform();
  frame_.render(target, states, current_style_->width);
  if(active_) {
    target.draw(text_sprite_, states);
    target.draw(cursor_, states);
  } else {
    target.draw(text_, states);
  }
}

bool WEntry::onInputEvent(const InputMapping& mapping, const sf::Event& ev)
{
  if(active_) {
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
      // validate or cancel
      } else if(!auto_active_ && (c == sf::Keyboard::Return || c == sf::Keyboard::Escape)) {
        this->deactivate(c == sf::Keyboard::Return);
        return true;
      // prevent focus switching when active (typed text has to be validated)
      } else if(!auto_active_ && this->neighborToFocus(mapping, ev)) {
        return true;
      } else {
        return false; // not processed
      }
      return true;
    }
  } else if(!auto_active_) {
    if(mapping.confirm.match(ev)) {
      this->activate();
      return true;
    }
  }
  return false;
}

void WEntry::focus(bool focused)
{
  WFocusable::focus(focused);
  if(focused) {
    if(auto_active_) {
      this->activate();
    } else {
      current_style_ = &style_focus_;
      current_style_->apply(*this);
    }
  } else {
    if(active_) {
      this->deactivate(auto_active_);
    } else {
      current_style_ = &style_;
      current_style_->apply(*this);
    }
  }
}


void WEntry::updateTextDisplay(bool force)
{
  const Style& style = *current_style_;
  if(active_) {
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

    cursor_.x = cursor_pos_x - x - style.width/2 + current_style_->text_margin_left;
    // redraw only if needed
    if( force ) {
      text_img_.clear(sf::Color(0,0,0,0));
      text_.setPosition(-x, 0);
      text_img_.draw(text_);
      text_img_.display();
    }
  } else {
    float x;
    // don't use getLocalBounds(), it's width is slightly off
    float width = text_.findCharacterPos(text_.getString().getSize()).x - text_.getPosition().x;
    switch(style.xalign) {
      case XAlign::LEFT:
        x = style.text_margin_left - style.width/2;
        break;
      case XAlign::CENTER:
        x = (style.text_margin_left - style.text_margin_right)/2 - width/2;
        break;
      case XAlign::RIGHT:
        x = style.width/2 - style.text_margin_right - width;
        break;
    }
    text_.setPosition(x, -text_.getFont()->getLineSpacing(text_.getCharacterSize())/2);
  }
}

void WEntry::activate()
{
  active_ = true;
  assert(focused());
  current_style_ = &style_active_;
  // reset cursor at the end
  cursor_pos_ = text_.getString().getSize();
  // but ensure there is no extra space right
  text_.setPosition(0, 0);
  screen_.intf().setTextInput(true);
  current_style_->apply(*this);
}

void WEntry::deactivate(bool validate)
{
  active_ = false;
  if(focused()) {
    current_style_ = &style_focus_;
  } else {
    current_style_ = &style_;
  }
  screen_.intf().setTextInput(false);
  current_style_->apply(*this);
  if(callback_) {
    callback_(validate);
  }
}


void WEntry::Style::load(const StyleLoader& loader)
{
  std::string key;

  text.load(loader);
  frame.load(loader);

  if(!loader.fetchStyle<XAlign>("XAlign", xalign)) {
    xalign = XAlign::LEFT;
  }

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
  vertices_[0] = {{0, -h/2}};
  vertices_[1] = {{0, h/2}};
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
    WFocusable(screen, name), current_style_(&style_), callback_(nullptr)
{
  style_.load(*this);
  style_focus_.load(StyleLoaderPrefix(*this, "Focus", true));
  current_style_->apply(*this);
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
  assert(i < items_.size());
  text_.setString(items_[i]);
  current_style_->align.apply(text_);
  index_ = i;
  if(callback_) {
    callback_();
  }
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

unsigned int WChoice::addItem(const ItemContainer::value_type& v)
{
  items_.push_back(v);
  return items_.size()-1;
}

void WChoice::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
  states.transform *= getTransform();
  frame_.render(target, states, current_style_->width);
  target.draw(text_, states);
}

bool WChoice::onInputEvent(const InputMapping& mapping, const sf::Event& ev)
{
  if(mapping.left.match(ev)) {
    this->select( index_ == 0 ? items_.size()-1 : index_-1 );
    return true;
  } else if(mapping.right.match(ev)) {
    this->select( index_ == items_.size()-1 ? 0 : index_+1 );
    return true;
  }
  return false;
}

void WChoice::focus(bool focused)
{
  WFocusable::focus(focused);
  if(focused) {
    current_style_ = &style_focus_;
  } else {
    current_style_ = &style_;
  }
  current_style_->apply(*this);
}


void WChoice::Style::load(const StyleLoader& loader)
{
  std::string key;

  text.load(loader);
  align.load(loader);
  frame.load(loader);

  width = loader.getStyle<float>("Width", key);
  if(width <= 0) {
    throw StyleError(key, "value must be positive");
  }
}

void WChoice::Style::apply(WChoice& o)
{
  text.apply(o.text_);
  align.apply(o.text_);
  frame.apply(o.frame_);
}


}

