#include "wbutton.h"

namespace gui {


WButton::WButton(float width, float height):
    bg_(sf::Shape::Rectangle(-width/2,-height/2,width,height, sf::Color::Black, 2, sf::Color::White))
{
  this->setColor(sf::Color::White);
}

void WButton::setCaption(const std::string &caption)
{
  caption_.SetString(caption);
  sf::FloatRect r = caption_.GetRect();
  caption_.SetOrigin(r.Width/2, r.Height/2);
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

void WButton::focus(bool focused)
{
  //XXX:temp bad behavior: override user's setColor()
  this->setColor(focused ? sf::Color::Red : sf::Color::White);
}


}

