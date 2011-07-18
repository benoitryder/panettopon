#ifndef GUI_SCREEN_MENUS_H_
#define GUI_SCREEN_MENUS_H_

#include "screen.h"
#include "resources.h"

namespace gui {

class GuiInterface;


/** @brief The very first screen.
 *
 * Style entries:
 *  - ButtonStyle: section for button style
 *  - ButtonRect: geometry of the first button, height is used for space
 *  between buttons
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
  StyleButton style_button_;
  WButton *button_exit_;
};

/** @brief Join server (choose host and port).
 *
 * Style entries:
 *  - ButtonStyle: section for button style
 *  - EntryStyle: section for entry style
 *  - TitleLabelPos: position of title label
 *  - HostEntryRect: geometry of the host entry (height ignored)
 *  - PortEntryRect: geometry of the port entry (height ignored)
 *  - JoinButtonRect: geometry of the join button (height ignored)
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
  StyleButton style_button_;
  StyleButton style_entry_;
  WEntry *entry_host_;
  WEntry *entry_port_;
  bool submitting_;
};

/** @brief Server creation (choose port).
 *
 * Style entries:
 *  - ButtonStyle: section for button style
 *  - EntryStyle: section for entry style
 *  - PortLabelPos: position of port label
 *  - PortEntryRect: geometry of the port entry (height ignored)
 *  - CreateButtonRect: geometry of the create button (height ignored)
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
  StyleButton style_button_;
  StyleButton style_entry_;
  WEntry *entry_port_;
};

/** @brief Lobby (game preparation).
 *
 * Style entries:
 *  - ButtonStyle: section for button style
 *  - EntryStyle: section for entry style
 *  - ReadyButtonRect: geometry of the ready button (height ignored)
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
  StyleButton style_button_;
  StyleButton style_entry_;
  WButton *button_ready_;
};


}

#endif
