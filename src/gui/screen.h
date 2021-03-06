#ifndef GUI_SCREEN_H_
#define GUI_SCREEN_H_

#include <map>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>
#include "widget.h"
#include "animation.h"
#include "../instance.h"

class IniFile;

namespace gui {

class GuiInterface;
class Screen;


/** @brief Notification text
 *
 * Style properties:
 *  - StyleText properties
 *  - StyleTextAlign properties
 *  - ImageFrameX properties
 *  - Width
 */
class WNotification: public Widget
{
 public:
  struct Notification {
    typedef GameInstance::Severity Severity;
    Severity sev;
    std::string msg;
  };

  WNotification(const Screen& screen, const std::string& name);
  void setNotification(const Notification& notif);
  virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const;

 protected:
  virtual const std::string& type() const;

 private:
  sf::Text text_;
  ImageFrameX frame_;
  float width_;
  StyleTextAlign style_align_;
};


/** @brief Basic screen
 *
 * Style properties:
 *  - BackgroundImage, BackgroundColor
 */
class Screen: public StyleLoader
{
 public:
  Screen(GuiInterface& intf, const std::string& name);
  virtual ~Screen();
  const std::string& name() const { return name_; }
  GuiInterface& intf() const { return intf_; }
  const IniFile& style() const;

  virtual void enter();
  virtual void exit();
  virtual void redraw();
  /** @brief Process an event.
   * @return true if processed, false otherwise.
   */
  virtual bool onInputEvent(const sf::Event& ev) = 0;
  /// Focus a given widget, or nothing
  void focus(WFocusable* w);

  /** @name Instance observer methods. */
  //@{
  virtual void onChat(Player&, const std::string& ) {}
  virtual void onPlayerJoined(Player&) {}
  virtual void onPlayerChangeNick(Player&, const std::string&) {}
  virtual void onPlayerStateChange(Player&) {}
  virtual void onPlayerChangeFieldConf(Player&) {}
  virtual void onStateChange() {}
  virtual void onServerChangeFieldConfs() {}
  virtual void onPlayerStep(Player&) {}
  virtual void onPlayerRanked(Player&) {}
  virtual void onNotification(GameInstance::Severity, const std::string&);
  virtual void onServerConnect(bool) {}
  virtual void onServerDisconnect() {}
  //@}

  virtual const ResourceManager& res_mgr() const;
  /** @brief Get style entry key for a given property.
   *
   * The requested property is searched in the screen's section and
   * recursively in the section given by the \e Fallback property.
   *
   * @return true if found, false otherwise.
   */
  virtual bool searchStyle(const std::string& prop, std::string& key) const;
  virtual std::string styleErrorSection() const { return name_; }

  /// Update all registered animations
  void updateAnimations(unsigned long time);
  void addAnimation(Animation& animation);
  void removeAnimation(const Animation& animation);

  typedef WNotification::Notification Notification;
  /// Add a notification
  void addNotification(const Notification& notif);
  /// Clear all notifications (current and pending)
  void clearNotifications();

 protected:
  GuiInterface& intf_;
  const std::string name_;
  WContainer container_;
  WFocusable* focused_;
  std::vector<Animation*> animations_;
 private:
  /// Dummy drawable, to have a renderer
  class Background: public sf::Drawable {
   public:
    Background(): img(NULL), color(sf::Color::White) {}
    virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const;
    const sf::Texture* img;
    sf::Color color;
  } background_;

  std::vector<Notification> notifications_;
  WNotification notification_widget_;
  Animation notification_anim_;
};


}

#endif
