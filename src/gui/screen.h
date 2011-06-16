#ifndef GUI_SCREEN_H_
#define GUI_SCREEN_H_

#include <map>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>
#include "widget.h"
#include "../instance.h"

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
  /** @brief Process an event.
   * @return true if processed, false otherwise.
   */
  virtual bool onInputEvent(const sf::Event &ev) = 0;

  /** @name Instance observer methods. */
  //@{
  virtual void onChat(Player *, const std::string &) {}
  virtual void onPlayerJoined(Player *) {}
  virtual void onPlayerChangeNick(Player *, const std::string &) {}
  virtual void onPlayerReady(Player *) {}
  virtual void onPlayerQuit(Player *) {}
  virtual void onStateChange() {}
  virtual void onPlayerStep(Player *) {}
  virtual void onNotification(GameInstance::Severity, const std::string &) {}
  virtual void onServerDisconnect() {}
  //@}

 protected:
  GuiInterface &intf_;
};


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
