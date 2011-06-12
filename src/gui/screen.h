#ifndef GUI_SCREEN_H_
#define GUI_SCREEN_H_

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>

namespace gui {

class GuiInterface;


class Screen
{
 public:
  Screen(GuiInterface &intf);
  virtual ~Screen();

  virtual void enter();
  virtual void exit();
  virtual void redraw();
  virtual void onInputEvent(const sf::Event &ev) = 0;

 protected:
  GuiInterface &intf_;
};


}

#endif
