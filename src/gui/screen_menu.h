#ifndef GUI_SCREEN_MENU_H_
#define GUI_SCREEN_MENU_H_

#include "screen.h"
#include "wcontainer.h"

namespace gui {

class GuiInterface;


/// Screen with widgets, used for menus.
class ScreenMenu: public Screen
{
 public:
  ScreenMenu(GuiInterface &intf);
  virtual void redraw();
  virtual bool onInputEvent(const sf::Event &ev);

 protected:
  WContainer container_;
};


}

#endif
