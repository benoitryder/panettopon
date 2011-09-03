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
  WEntry *entry_nick_;
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
  WEntry *entry_player_nb_;
  WEntry *entry_nick_;
};

/** @brief Lobby (game preparation).
 *
 * Widgets:
 *  - Ready (button)
 *  - PlayerRow (widget type)
 *
 * Style properties:
 *  - PlayerRowsPos, PlayerRowsDY
 */
class ScreenLobby: public ScreenMenu
{
 public:
  ScreenLobby(GuiInterface &intf, Player *pl);
  virtual void enter();
  virtual void redraw();
  virtual bool onInputEvent(const sf::Event &ev);
  virtual void onStateChange(GameInstance::State state);
  virtual void onPlayerJoined(Player *);
  virtual void onPlayerChangeNick(Player *, const std::string &);
  virtual void onPlayerReady(Player *);
  virtual void onPlayerChangeFieldConf(Player *);
  virtual void onPlayerQuit(Player *);

 private:
  /** @brief Display a row of informaion for a player.
   *
   * Style properties:
   *  - NickFont, NickFontSize, NickFontStyle
   *  - ReadyImage, RedayImageRect
   *  - NickX, ConfX, ReadyX
   */
  class WPlayerRow: public Widget
  {
   public:
    WPlayerRow(const Screen& screen, const Player& pl);
    virtual void Render(sf::RenderTarget &target, sf::Renderer &renderer) const;
    /// Update the widget after player state changes
    void update();

   protected:
    virtual const std::string& type() const;

   private:
    const Player& player_;
    sf::Text nick_;
    sf::Text conf_;
    sf::Sprite ready_;
  };

  void submit();
  void updateReadyButtonCaption();
  void updatePlayerRowsPos();

 private:
  Player *player_; ///< Controlled player
  WButton *button_ready_;
  typedef boost::ptr_map<PlId, WPlayerRow> PlayerRowsContainer;
  PlayerRowsContainer player_rows_;
  sf::Vector2f player_rows_pos_;
  float player_rows_dy_;
};


}

#endif
