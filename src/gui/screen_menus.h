#ifndef GUI_SCREEN_MENUS_H_
#define GUI_SCREEN_MENUS_H_

#include <memory>
#include <map>
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
class ScreenStart: public Screen
{
 public:
  ScreenStart(GuiInterface& intf);
  virtual void enter();
  virtual bool onInputEvent(const sf::Event& ev);

 private:
  void onDebugStart();
  void onJoinServer();
  void onCreateServer();

 private:
  WButton* button_exit_;
};

/** @brief Join server (choose host and port).
 *
 * Widgets:
 *  - Title (label)
 *  - Host (entry)
 *  - Port (entry)
 *  - Join (button)
 */
class ScreenJoinServer: public Screen
{
 public:
  ScreenJoinServer(GuiInterface& intf);
  virtual void enter();
  virtual bool onInputEvent(const sf::Event& ev);
  virtual void onPlayerJoined(Player* );
  virtual void onServerConnect(bool success);
  virtual void onServerDisconnect();

 protected:
  void submit();

 private:
  WEntry* entry_host_;
  WEntry* entry_port_;
  WEntry* entry_nick_;
  bool submitting_;
};

/** @brief Server creation (choose port).
 *
 * Widgets:
 *  - PortLabel (label)
 *  - PortEntry (entry)
 *  - Create (button)
 */
class ScreenCreateServer: public Screen
{
 public:
  ScreenCreateServer(GuiInterface& intf);
  virtual void enter();
  virtual bool onInputEvent(const sf::Event& ev);

 protected:
  void submit();

 private:
  WEntry* entry_port_;
  WEntry* entry_player_nb_;
  WEntry* entry_nick_;
};

/** @brief Lobby (game preparation).
 *
 * Widgets:
 *  - PlayerFrame (widget type)
 *
 * Style properties:
 *  - PlayerFramesPos, PlayerFramesDPos
 */
class ScreenLobby: public Screen
{
 public:
  ScreenLobby(GuiInterface& intf, Player* pl);
  virtual void enter();
  virtual void redraw();
  virtual bool onInputEvent(const sf::Event& ev);
  virtual void onStateChange();
  virtual void onServerChangeFieldConfs();
  virtual void onPlayerJoined(Player*);
  virtual void onPlayerChangeNick(Player*, const std::string&);
  virtual void onPlayerStateChange(Player*);
  virtual void onPlayerChangeFieldConf(Player*);

 private:
  /** @brief Display a row of information for a player.
   *
   * Style properties:
   *  - Nick (text)
   *  - Conf (choice)
   *  - Ready (sprite)
   *  - NickX, ReadyX
   */
  class WPlayerFrame: public WContainer
  {
   public:
    WPlayerFrame(const Screen& screen, const Player& pl);
    virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const;
    const Player& player() const { return player_; }
    WChoice& choiceConf() const { return *choice_conf_; }
    /// Update the widget after player state changes
    void update();
    /// Update server list of configurations
    void updateConfItems();

   protected:
    virtual const std::string& type() const;

   private:
    const Player& player_;
    sf::Text nick_;
    WChoice* choice_conf_;
    sf::Sprite ready_;
  };

  void submit();
  void updatePlayerFramesPos();

 private:
  Player* player_; ///< Controlled player
  WFrame* player_frame_;
  WButton* button_ready_;
  typedef std::map<PlId, std::unique_ptr<WPlayerFrame>> PlayerFramesContainer;
  PlayerFramesContainer player_frames_;
  sf::Vector2f player_frames_pos_;
  sf::Vector2f player_frames_dpos_;
};


}

#endif
