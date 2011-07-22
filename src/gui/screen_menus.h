#ifndef GUI_SCREEN_MENUS_H_
#define GUI_SCREEN_MENUS_H_

#include "screen.h"
#include "resources.h"

namespace gui {

class GuiInterface;


/** @brief The very first screen.
 *
 * Widget:
 *  - JoinServer (button)
 *  - CreateServer (button)
 *  - Exit (button)
 */
class ScreenStart: public ScreenMenu
{
 public:
  ScreenStart(GuiInterface &intf);
  virtual void enter();
  virtual bool onInputEvent(const sf::Event &ev);

 private:
  void onJoinServer();
  void onCreateServer();

 private:
  WButton *button_exit_;
};

/** @brief Join server (choose host and port).
 *
 * Widgets:
 *  - Title (label)
 *  - Host (entry)
 *  - Port (entry)
 *  - Join (button)
 */
class ScreenJoinServer: public ScreenMenu
{
 public:
  ScreenJoinServer(GuiInterface &intf);
  virtual void enter();
  virtual bool onInputEvent(const sf::Event &ev);
  virtual void onPlayerJoined(Player *);
  virtual void onServerDisconnect();

 protected:
  void submit();

 private:
  WEntry *entry_host_;
  WEntry *entry_port_;
  bool submitting_;
};

/** @brief Server creation (choose port).
 *
 * Widgets:
 *  - PortLabel (label)
 *  - PortEntry (entry)
 *  - Create (button)
 */
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

/** @brief Lobby (game preparation).
 *
 * Style entries:
 *  - Ready (button)
 */
class ScreenLobby: public ScreenMenu
{
 public:
  ScreenLobby(GuiInterface &intf, Player *pl);
  virtual void enter();
  virtual bool onInputEvent(const sf::Event &ev);
  virtual void onStateChange(GameInstance::State state);

 private:
  void submit();
  void updateReadyButtonCaption();

 private:
  Player *player_; ///< Controlled player
  WButton *button_ready_;
};


}

#endif
