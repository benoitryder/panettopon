#ifndef GUI_SCREEN_START_H_
#define GUI_SCREEN_START_H_

#include "screen.h"
#include "wcontainer.h"

namespace gui {

class GuiInterface;


/// The very first screen.
class ScreenStart: public Screen
{
 public:
  ScreenStart(GuiInterface &intf);
  virtual void enter();
  virtual void redraw();
  virtual bool onInputEvent(const sf::Event &ev);

 protected:
  WContainer container_;
};


}

#endif
