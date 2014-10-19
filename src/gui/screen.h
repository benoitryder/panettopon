#ifndef GUI_SCREEN_H_
#define GUI_SCREEN_H_

#include <map>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>
#include "widget.h"
#include "../instance.h"

class IniFile;

namespace gui {

class GuiInterface;


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
  /// Focus a given widget
  void focus(WFocusable* w);

  /** @name Instance observer methods. */
  //@{
  virtual void onChat(Player* , const std::string& ) {}
  virtual void onPlayerJoined(Player* ) {}
  virtual void onPlayerChangeNick(Player*, const std::string&) {}
  virtual void onPlayerStateChange(Player*) {}
  virtual void onPlayerChangeFieldConf(Player*) {}
  virtual void onStateChange() {}
  virtual void onPlayerStep(Player*) {}
  virtual void onPlayerRanked(Player*) {}
  virtual void onNotification(GameInstance::Severity, const std::string&) {}
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

 protected:
  GuiInterface& intf_;
  const std::string name_;
  WContainer container_;
  WFocusable* focused_;
 private:
  /// Dummy drawable, to have a renderer
  class Background: public sf::Drawable {
   public:
    Background(): img(NULL), color(sf::Color::White) {}
    virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const;
    const sf::Texture* img;
    sf::Color color;
  } background_;
};


}

#endif
