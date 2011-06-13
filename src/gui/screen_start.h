#ifndef GUI_SCREEN_START_H_
#define GUI_SCREEN_START_H_

#include "screen_menu.h"
#include "widget.h"

namespace gui {

class GuiInterface;


/// The very first screen.
class ScreenStart: public ScreenMenu
{
 public:
  ScreenStart(GuiInterface &intf);
  virtual void enter();
  virtual bool onInputEvent(const sf::Event &ev);

 protected:
  WButton *button_exit_;
};


}

#endif
