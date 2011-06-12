#ifndef GUI_WIDGET_H_
#define GUI_WIDGET_H_

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Window/Event.hpp>

namespace gui {


class Widget
{
 public:
  Widget();
  virtual ~Widget();
  virtual void draw(sf::RenderTarget &target) = 0;
  virtual void setPosition(float x, float y) = 0;
  virtual bool onInputEvent(const sf::Event &) { return false; }
};


}

#endif
