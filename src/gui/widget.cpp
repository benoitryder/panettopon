#include "widget.h"
#include "screen.h"
#include "interface.h"
#include "resources.h"
#include "../log.h"

namespace gui {

Widget::StyleError::StyleError(const Widget& w, const std::string& prop, const std::string& msg):
  std::runtime_error("style error for "+w.screen_.name()+"."+w.name_+"."+prop+": "+msg) {}


Widget::Widget(const Screen& screen, const std::string& name):
    screen_(screen), name_(name), focused_(false)
{
  this->setNeighbors(NULL, NULL, NULL, NULL);

  const IniFile& style = screen.style();
  const std::string section = screen.name()+'.'+name;
  if( style.has(section, "Pos") ) {
    this->SetPosition(style.get<sf::Vector2f>(section, "Pos"));
  }
}

Widget::~Widget() {}

void Widget::setNeighbors(Widget *up, Widget *down, Widget *left, Widget *right)
{
  neighbors_[NEIGHBOR_UP] = up;
  neighbors_[NEIGHBOR_DOWN] = down;
  neighbors_[NEIGHBOR_LEFT] = left;
  neighbors_[NEIGHBOR_RIGHT] = right;
}

bool Widget::searchStyle(const std::string& prop, std::string *key) const
{
  const IniFile& style = screen_.style();
  std::string s;
  if( !name_.empty() ) {
    s = screen_.name()+'.'+name_+'.'+prop;
    if( style.has(s) ) {
      goto found;
    }
  }
  s = screen_.name()+'.'+this->type()+'.'+prop;
  if( style.has(s) ) {
    goto found;
  }
  s = this->type()+'.'+prop;
  if( style.has(s) ) {
    goto found;
  }
  return false;
found:
  *key = s;
  return true;
}

void Widget::applyStyle(sf::Text *text, const std::string prefix)
{
  const IniFile& style = screen_.style();
  ResourceManager& res_mgr = screen_.intf().res_mgr();
  std::string key;

  if( searchStyle(prefix+"Font", &key) ) {
    text->SetFont(*res_mgr.getFont(style.get<std::string>(key)));
  }
  if( searchStyle(prefix+"FontSize", &key) ) {
    text->SetCharacterSize(style.get<unsigned int>(key));
  }
  if( searchStyle(prefix+"FontStyle", &key) ) {
    const std::string val = style.get<std::string>(key);
    int txt_style;
    if( val == "regular" ) {
      txt_style = sf::Text::Regular;
    } else if( val == "bold" ) {
      txt_style = sf::Text::Bold;
    } else if( val == "italic" ) {
      txt_style = sf::Text::Italic;
    } else if( val == "bold,italic" || val == "italic,bold" ) {
      txt_style = sf::Text::Bold|sf::Text::Italic;
    } else {
      throw StyleError(*this, prefix+"FontStyle", "invalid value");
    }
    text->SetStyle(txt_style);
  }
}


const std::string WContainer::type_("Container");
const std::string& WContainer::type() const { return type_; }

WContainer::WContainer(const Screen& screen, const std::string& name):
    Widget(screen, name)
{
}

void WContainer::Render(sf::RenderTarget &target, sf::Renderer &) const
{
  Container::const_iterator it;
  for( it=widgets.begin(); it!=widgets.end(); ++it ) {
    target.Draw(*it);
  }
}


const std::string WButton::type_("Button");
const std::string& WButton::type() const { return type_; }

WButton::WButton(const Screen& screen, const std::string& name):
    Widget(screen, name), callback_(NULL)
{
  const IniFile& style = screen.style();
  ResourceManager& res_mgr = screen.intf().res_mgr();
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
      throw StyleError(*this, "Width", "value must be positive");
    }
  } else {
    throw StyleError(*this, "Width", "not set");
  }

  if( searchStyle("BgImage", &key) ) {
    const sf::Image *img = res_mgr.getImage(style.get<std::string>(key));
    sf::IntRect rect(0, 0, img->GetWidth(), img->GetHeight());
    if( searchStyle("BgImageRect", &key) ) {
      rect = style.get<sf::IntRect>(key);
    }
    unsigned int frame_margin = 0;
    if( searchStyle("BgImageMarginX", &key) ) {
      frame_margin = style.get<unsigned int>(key);
      if( 2*frame_margin >= (unsigned int)rect.Width ) {
        throw StyleError(*this, "BgImageMarginX", "margins larger than image width");
      }
    }
    frame_.create(img, rect, frame_margin);
  } else {
    throw StyleError(*this, "BgImage", "not set");
  }
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
    renderer.SetColor(focus_color_);
  } else {
    renderer.SetColor(color_);
  }
  frame_.render(renderer, width_);
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


const std::string WLabel::type_("Label");
const std::string& WLabel::type() const { return type_; }

WLabel::WLabel(const Screen& screen, const std::string& name):
    Widget(screen, name), align_(0)
{
  const IniFile& style = screen.style();
  std::string key;

  this->applyStyle(&text_);
  if( searchStyle("Color", &key) ) {
    text_.SetColor(style.get<sf::Color>(key));
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
      throw StyleError(*this, "TextAlign", "invalid value");
    }
  }
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


const std::string WEntry::type_("Entry");
const std::string& WEntry::type() const { return type_; }

WEntry::WEntry(const Screen& screen, const std::string& name):
    Widget(screen, name), cursor_pos_(0)
{
  const IniFile& style = screen.style();
  ResourceManager& res_mgr = screen.intf().res_mgr();
  std::string key;

  this->applyStyle(&text_);

  if( searchStyle("Color", &key) ) {
    color_ = style.get<sf::Color>(key);
  }
  if( searchStyle("FocusColor", &key) ) {
    focus_color_ = style.get<sf::Color>(key);
  }

  //XXX factorize some code with WButton
  if( searchStyle("Width", &key) ) {
    width_ = style.get<float>(key);
    if( width_ <= 0 ) {
      throw StyleError(*this, "Width", "value must be positive");
    }
  } else {
    throw StyleError(*this, "Width", "not set");
  }
  unsigned int text_margin = 0;
  if( searchStyle("TextMarginX", &key) ) {
    text_margin = style.get<unsigned int>(key);
  }
  unsigned int text_height = text_.GetFont().GetLineSpacing(text_.GetCharacterSize())+2;
  text_img_.Create(width_-2*text_margin, text_height);
  text_sprite_.SetOrigin(width_/2.-text_margin, text_height/2.);
  text_sprite_.SetImage(text_img_.GetImage(), true);
  cursor_ = sf::Shape::Line(0, 0, 0, text_height, 1, sf::Color::White);
  cursor_.SetOrigin(text_sprite_.GetOrigin());

  if( searchStyle("BgImage", &key) ) {
    const sf::Image *img = res_mgr.getImage(style.get<std::string>(key));
    sf::IntRect rect(0, 0, img->GetWidth(), img->GetHeight());
    if( searchStyle("BgImageRect", &key) ) {
      rect = style.get<sf::IntRect>(key);
    }
    unsigned int frame_margin = 0;
    if( searchStyle("BgImageMarginX", &key) ) {
      frame_margin = style.get<unsigned int>(key);
      if( 2*frame_margin >= (unsigned int)rect.Width ) {
        throw StyleError(*this, "BgImageMarginX", "margins larger than image width");
      }
    }
    frame_.create(img, rect, frame_margin);
  } else {
    throw StyleError(*this, "BgImage", "not set");
  }
}

void WEntry::setText(const std::string &text)
{
  text_.SetString(text);
  this->updateTextDisplay(true);
}

void WEntry::Render(sf::RenderTarget &target, sf::Renderer &renderer) const
{
  if( this->focused() ) {
    renderer.SetColor(focus_color_);
  } else {
    renderer.SetColor(color_);
  }

  frame_.render(renderer, width_);
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

