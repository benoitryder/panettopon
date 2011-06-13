#ifndef GUI_SCREEN_MENU_H_
#define GUI_SCREEN_MENU_H_

#include <map>
#include "screen.h"
#include "widget.h"

namespace gui {

class GuiInterface;


/// Screen with widgets, used for menus.
class ScreenMenu: public Screen
{
 public:
  ScreenMenu(GuiInterface &intf);
  virtual void redraw();
  virtual bool onInputEvent(const sf::Event &ev);
  void focus(Widget *w);

 protected:
  WContainer container_;
  Widget *focused_;
};


}

#endif
