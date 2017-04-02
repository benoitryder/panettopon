#ifndef GUI_SCREEN_MENUS_H_
#define GUI_SCREEN_MENUS_H_

#include <memory>
#include <map>
#include "screen.h"
#include "resources.h"
#include "input.h"

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
  virtual void onPlayerJoined(Player&);
  virtual void onServerConnect(bool success);
  virtual void onServerDisconnect();

 protected:
  void submit(const sf::Event& ev);

 private:
  WEntry* entry_host_;
  WEntry* entry_port_;
  WEntry* entry_nick_;
  sf::Event submitting_event_;
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
  void submit(const sf::Event& ev);

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
  ScreenLobby(GuiInterface& intf);
  virtual void enter();
  virtual void redraw();
  virtual bool onInputEvent(const sf::Event& ev);
  virtual void onStateChange();
  virtual void onServerChangeFieldConfs();
  virtual void onPlayerJoined(Player&);
  virtual void onPlayerChangeNick(Player&, const std::string&);
  virtual void onPlayerStateChange(Player&);
  virtual void onPlayerChangeFieldConf(Player&);

  /** @brief Return an unused input mapping
   *
   * \a event should be be the event that added a new player to the lobby.
   * It defines which kind of mapping should be created.
   *
   * If no input mapping is available, maping type is NONE.
   */
  InputMapping getUnusedInputMapping(const sf::Event& event);

  void addLocalPlayer(Player& pl, const InputMapping& mapping);

 private:
  void addRemotePlayer(Player& pl);

  /** @brief Display information for a player.
   *
   * Style properties:
   *  - Border (frame)
   *  - Nick (label)
   *  - Conf (choice)
   *  - Ready (sprite)
   *  - Ready.Pos
   */
  class WPlayerFrame: public WContainer
  {
   public:
    WPlayerFrame(ScreenLobby& screen, Player& pl, const InputMapping& mapping={});
    virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const;
    const Player& player() const { return player_; }
    const InputMapping& mapping() const { return mapping_; }
    WFrame& frame() const { return *frame_; }
    /// Update the widget after player state changes
    void update();
    /// Update mapping choices
    void updateMappingItems();
    /// Update server list of configurations
    void updateConfItems();
    /** @brief Process events on the frame
     *
     * @warn The widget may call the destruction of its player and itself.
     * They should not be accessed after calling this method.
     */
    bool onInputEvent(const sf::Event& ev);

   protected:
    virtual const std::string& type() const;

    /// Focus a given widget, or nothing
    void focus(WFocusable* w);
    /// Callback for mapping choice change
    void onMappingChange();
    /// Callback for conf choice change
    void onConfChange();

   private:
    ScreenLobby& screen_lobby_;
    Player& player_;
    InputMapping mapping_;
    WFrame* frame_;
    WLabel* nick_;
    WChoice* choice_mapping_;
    WChoice* choice_conf_;
    sf::Sprite ready_;
    WFocusable* focused_;
    std::vector<std::reference_wrapper<const InputMapping>> choice_mapping_values_;
  };

  void setMapping(const InputMapping& mapping);
  bool isMappingUsed(const InputMapping& mapping);
  void updatePlayerFramesLayout();

  WButton* button_ready_;
  typedef std::map<PlId, std::unique_ptr<WPlayerFrame>> PlayerFramesContainer;
  PlayerFramesContainer player_frames_;
  sf::Vector2f player_frames_pos_;
  sf::Vector2f player_frames_dpos_;
};


}

#endif
