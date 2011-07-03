#include "widget.h"
#include "../log.h"

namespace gui {


Widget::Widget():
    focused_(false)
{
  this->setNeighbors(NULL, NULL, NULL, NULL);
}

Widget::~Widget() {}

void Widget::setNeighbors(Widget *up, Widget *down, Widget *left, Widget *right)
{
  neighbors_[NEIGHBOR_UP] = up;
  neighbors_[NEIGHBOR_DOWN] = down;
  neighbors_[NEIGHBOR_LEFT] = left;
  neighbors_[NEIGHBOR_RIGHT] = right;
}


WContainer::WContainer()
{
  drawable_.container = this;
}

void WContainer::draw(sf::RenderTarget &target)
{
  Container::iterator it;
  for( it=widgets.begin(); it!=widgets.end(); ++it ) {
    it->draw(target);
  }
}

void WContainer::setPosition(float x, float y)
{
  drawable_.SetPosition(x, y);
}

void WContainer::Drawable::Render(sf::RenderTarget &target, sf::Renderer &) const
{
  container->draw(target);
}


WButton::WButton(float width, float height):
    bg_(sf::Shape::Rectangle(-width/2,-height/2,width,height, sf::Color::Black, 2, sf::Color::White)),
    callback_(NULL)
{
  this->setColor(sf::Color::White);
}

void WButton::setCaption(const std::string &caption)
{
  caption_.SetString(caption);
  sf::FloatRect r = caption_.GetRect();
  caption_.SetOrigin(r.Width/2, (caption_.GetFont().GetLineSpacing(caption_.GetCharacterSize())+2)/2);
}

void WButton::setColor(const sf::Color &color)
{
  caption_.SetColor(color);
  bg_.SetColor(color);
}

void WButton::draw(sf::RenderTarget &target)
{
  target.Draw(bg_);
  target.Draw(caption_);
}

void WButton::setPosition(float x, float y)
{
  caption_.SetPosition(x, y);
  bg_.SetPosition(x, y);
}

bool WButton::onInputEvent(const sf::Event &ev)
{
  if( ev.Type == sf::Event::KeyPressed ) {
    if( ev.Key.Code == sf::Key::Return ) {
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
  Widget::focus(focused);
  //XXX:temp bad behavior: override user's setColor()
  this->setColor(focused ? sf::Color::Red : sf::Color::White);
}


WLabel::WLabel(): align_(0)
{
  text_.SetColor(sf::Color::White);
}

void WLabel::setText(const std::string &text)
{
  text_.SetString(text);
  this->setTextAlign(align_); // recompute origin
}

void WLabel::draw(sf::RenderTarget &target)
{
  target.Draw(text_);
}

void WLabel::setPosition(float x, float y)
{
  text_.SetPosition(x, y);
}

void WLabel::setTextAlign(int align)
{
  sf::FloatRect r = text_.GetRect();
  float x;
  if( align == 0 ) {
    x = r.Width / 2;
  } else if( align < 0 ) {
    x = 0;
  } else if( align > 0 ) {
    x = r.Width;
  }
  text_.SetOrigin(x, (text_.GetFont().GetLineSpacing(text_.GetCharacterSize())+2)/2);
  align_ = align;
}


WEntry::WEntry(float width, float height):
    bg_(sf::Shape::Rectangle(-width/2,-height/2,width,height, sf::Color::Black, 2, sf::Color::White)),
    cursor_pos_(0)
{
  text_.SetColor(sf::Color::White);
  bg_.SetColor(sf::Color::White);
  const unsigned int full_height = text_.GetFont().GetLineSpacing(text_.GetCharacterSize())+2;
  text_.SetOrigin(width/2-2, full_height/2);
  cursor_ = sf::Shape::Line(1, 0, 1, full_height, 1, text_.GetColor());
}

void WEntry::setText(const std::string &text)
{
  text_.SetString(text);
  if( cursor_pos_ > text_.GetString().GetSize() ) {
    cursor_pos_ = text_.GetString().GetSize();
  }
}

void WEntry::draw(sf::RenderTarget &target)
{
  //TODO handle text overflow
  target.Draw(bg_);
  target.Draw(text_);
  if( this->focused() ) {
    cursor_.SetPosition(text_.TransformToGlobal(text_.GetCharacterPos(cursor_pos_)));
    target.Draw(cursor_);
  }
}

void WEntry::setPosition(float x, float y)
{
  text_.SetPosition(x, y);
  bg_.SetPosition(x, y);
}

bool WEntry::onInputEvent(const sf::Event &ev)
{
  if( ev.Type == sf::Event::TextEntered ) {
    sf::Uint32 c = ev.Text.Unicode;
    if( c >= ' ' && c != 127 ) {  // 127 is DEL sometimes
      sf::String s = text_.GetString();
      s.Insert(cursor_pos_++, c);
      text_.SetString(s);
      return true;
    }
  } else if( ev.Type == sf::Event::KeyPressed ) {
    sf::Key::Code c = ev.Key.Code;
    // move
    if( c == sf::Key::Home ) {
      cursor_pos_ = 0;
    } else if( c == sf::Key::End ) {
      cursor_pos_ = text_.GetString().GetSize();
    } else if( c == sf::Key::Left ) {
      if( cursor_pos_ > 0 ) {
        cursor_pos_--;
      }
    } else if( c == sf::Key::Right ) {
      if( cursor_pos_ < text_.GetString().GetSize() ) {
        cursor_pos_++;
      }
    // edit
    } else if( c == sf::Key::Back ) {
      if( cursor_pos_ > 0 ) {
        sf::String s = text_.GetString();
        s.Erase(--cursor_pos_);
        text_.SetString(s);
      }
    } else if( c == sf::Key::Delete ) {
      if( cursor_pos_ < text_.GetString().GetSize() ) {
        sf::String s = text_.GetString();
        s.Erase(cursor_pos_);
        text_.SetString(s);
      }
    } else {
      return false; // not processed
    }
    return true;
  }
  return false;
}


}

