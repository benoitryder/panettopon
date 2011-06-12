#ifndef GUI_WBUTTON_H_
#define GUI_WBUTTON_H_

#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/Shape.hpp>
#include "widget.h"

namespace gui {


class WButton: public Widget
{
 public:
  WButton(float width, float height);
  void setCaption(const std::string &caption);
  void setColor(const sf::Color &color);
  virtual void draw(sf::RenderTarget &target);
  virtual void setPosition(float x, float y);

 private:
  sf::Text caption_;  ///< Button caption
  sf::Shape bg_;  ///< Background shape
};


}

#endif
