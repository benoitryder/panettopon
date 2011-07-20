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
    if( ev.Key.Code == sf::Keyboard::Return ) {
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


WEntry::WEntry(const StyleButton &style, float width):
    style_(style), width_(width),
    text_("", *style.font, style.font_size),
    cursor_pos_(0)
{
  unsigned int text_height = style.font->GetLineSpacing(style.font_size)+2;
  text_img_.Create(width-2*style.margin_left, text_height);
  text_sprite_.SetOrigin(width/2-style_.margin_left, text_height/2.);
  text_sprite_.SetImage(text_img_.GetImage(), true);
  cursor_ = sf::Shape::Line(0, 0, 0, text_height, 1, sf::Color::White);
  cursor_.SetOrigin(text_sprite_.GetOrigin());
}

void WEntry::setText(const std::string &text)
{
  text_.SetString(text);
  this->updateTextDisplay(true);
}

void WEntry::Render(sf::RenderTarget &target, sf::Renderer &renderer) const
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

  target.Draw(text_sprite_);
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
      this->setText(s);
      return true;
    }
  } else if( ev.Type == sf::Event::KeyPressed ) {
    sf::Keyboard::Key c = ev.Key.Code;
    // move
    if( c == sf::Keyboard::Home ) {
      cursor_pos_ = 0;
      this->updateTextDisplay();
    } else if( c == sf::Keyboard::End ) {
      cursor_pos_ = text_.GetString().GetSize();
      this->updateTextDisplay();
    } else if( c == sf::Keyboard::Left ) {
      if( cursor_pos_ > 0 ) {
        cursor_pos_--;
        this->updateTextDisplay();
      }
    } else if( c == sf::Keyboard::Right ) {
      if( cursor_pos_ < text_.GetString().GetSize() ) {
        cursor_pos_++;
        this->updateTextDisplay();
      }
    // edit
    } else if( c == sf::Keyboard::Back ) {
      if( cursor_pos_ > 0 ) {
        sf::String s = text_.GetString();
        s.Erase(--cursor_pos_);
        this->setText(s);
      }
    } else if( c == sf::Keyboard::Delete ) {
      if( cursor_pos_ < text_.GetString().GetSize() ) {
        sf::String s = text_.GetString();
        s.Erase(cursor_pos_);
        this->setText(s);
      }
    } else {
      return false; // not processed
    }
    return true;
  }
  return false;
}

void WEntry::updateTextDisplay(bool force)
{
  const size_t len = text_.GetString().GetSize();
  if( cursor_pos_ > len ) {
    cursor_pos_ = len;
  }

  const float text_width = text_img_.GetWidth();
  const float cursor_pos_x = text_.GetCharacterPos(cursor_pos_).x;
  float x = -text_.GetPosition().x;

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

  cursor_.SetX(cursor_pos_x-x);
  // redraw only if needed
  if( force ) {
    text_img_.Clear(sf::Color(0,0,0,0));
    text_.SetX(-x);
    text_img_.Draw(text_);
    text_img_.Display();
  }
}


}

