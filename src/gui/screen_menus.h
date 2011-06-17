#ifndef GUI_SCREEN_MENUS_H_
#define GUI_SCREEN_MENUS_H_

#include "screen.h"

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

/// Server creation (choose port).
class ScreenCreateServer: public ScreenMenu
{
 public:
  ScreenCreateServer(GuiInterface &intf);
  virtual void enter();
  virtual bool onInputEvent(const sf::Event &ev);

 protected:
  void submit();

 private:
  WEntry *entry_port_;
};

/// Lobby (game preparation).
class ScreenLobby: public ScreenMenu
{
 public:
  ScreenLobby(GuiInterface &intf);
  virtual void enter();
  virtual bool onInputEvent(const sf::Event &ev);
};


}

#endif
