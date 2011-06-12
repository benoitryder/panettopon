#ifndef GUI_INTERFACE_H_
#define GUI_INTERFACE_H_

#include <string>
#include <boost/scoped_ptr.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include "resources.h"
#include "../monotone_timer.hpp"

class Config;

namespace gui {

class Screen;


class GuiInterface
{
 protected:
  static const std::string CONF_SECTION;

 public:
  GuiInterface();
  virtual ~GuiInterface();
  bool run(const Config &cfg);

  /** @brief Change the active screen.
   *
   * If \e screen is NULL, it ends the display.
   * The screen will be owned by the interface.
   * Current screen will be deleted before the next redraw.
   */
  void swapScreen(Screen *screen);

  sf::RenderWindow &window() { return window_; }

 private:
  boost::asio::io_service io_service_;

  /// Various configuration values.
  struct {
    unsigned int redraw_dt;
    bool fullscreen;
    unsigned int screen_width, screen_height;
  } conf_;

  bool initDisplay();
  void endDisplay();
  void onRedrawTick(const boost::system::error_code &ec);

  sf::RenderWindow window_;
  ResourceManager res_mgr_;
  boost::scoped_ptr<Screen> screen_;
  boost::scoped_ptr<Screen> prev_screen_;
  boost::asio::monotone_timer redraw_timer_;
};


}

#endif
