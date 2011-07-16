#include "widget.h"
#include "resources.h"
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
}

void WContainer::Render(sf::RenderTarget &target, sf::Renderer &) const
{
  Container::const_iterator it;
  for( it=widgets.begin(); it!=widgets.end(); ++it ) {
    target.Draw(*it);
  }
}


WButton::WButton(const StyleButton &style, float width):
    style_(style), caption_("", *style.font, style.font_size),
    callback_(NULL), width_(width)
{
}

void WButton::setCaption(const std::string &caption)
{
  caption_.SetString(caption);
  sf::FloatRect r = caption_.GetRect();
  caption_.SetOrigin(r.Width/2, (caption_.GetFont().GetLineSpacing(caption_.GetCharacterSize())+2)/2);
}

void WButton::Render(sf::RenderTarget &target, sf::Renderer &renderer) const
{
  if( this->focused() ) {
    renderer.SetColor(style_.focus_color);
  } else {
    renderer.SetColor(style_.color);
  }
  const float height = style_.tiles.left.rect().Height;
  const float side_width = style_.tiles.left.rect().Width;
  const float middle_width = width_ - 2*side_width;
  style_.tiles.left.render(renderer, -width_/2, -height/2);
  style_.tiles.right.render(renderer, middle_width/2, -height/2);
  style_.tiles.middle.render(renderer, -middle_width/2, -height/2, middle_width, height);
  target.Draw(caption_);
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


WLabel::WLabel(): align_(0)
{
  text_.SetColor(sf::Color::White);
}

void WLabel::setText(const std::string &text)
{
  text_.SetString(text);
  this->setTextAlign(align_); // recompute origin
}

void WLabel::Render(sf::RenderTarget &target, sf::Renderer &) const
{
  target.Draw(text_);
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
  cursor_ = sf::Shape::Line(0, 0, 0, full_height, 1, text_.GetColor());
  cursor_.SetOrigin(width/2-2, full_height/2);
}

void WEntry::setText(const std::string &text)
{
  text_.SetString(text);
  if( cursor_pos_ > text_.GetString().GetSize() ) {
    cursor_pos_ = text_.GetString().GetSize();
  }
  cursor_.SetPosition(text_.GetCharacterPos(cursor_pos_));
}

void WEntry::Render(sf::RenderTarget &target, sf::Renderer &) const
{
  //TODO handle text overflow
  target.Draw(bg_);
  target.Draw(text_);
  if( this->focused() ) {
    target.Draw(cursor_);
  }
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
    cursor_.SetPosition(text_.GetCharacterPos(cursor_pos_));
    return true;
  }
  return false;
}


}

