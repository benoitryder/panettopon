#ifndef GUI_INTERFACE_H_
#define GUI_INTERFACE_H_

#include <string>
#include <SFML/Graphics/RenderWindow.hpp>
#include "resources.h"
#include "../client.h"
#include "../monotone_timer.hpp"

class IniFile;
class ServerInstance;
class ClientInstance;

namespace gui {

class Screen;


class GuiInterface: public ClientInstance::Observer
{
 protected:
  static const std::string CONF_SECTION;

 public:
  GuiInterface();
  virtual ~GuiInterface();
  bool run(IniFile *cfg);
  boost::asio::io_service &io_service() { return io_service_; }

  /** @brief Change the active screen.
   *
   * If \e screen is NULL, it ends the display.
   * The screen will be owned by the interface.
   * Current screen will be deleted before the next redraw.
   */
  void swapScreen(Screen *screen);

  sf::RenderWindow &window() { return window_; }
  ResourceManager &res_mgr() { return res_mgr_; }

  /** @name Instance observer methods. */
  //@{
  virtual void onChat(Player *pl, const std::string &msg);
  virtual void onPlayerJoined(Player *pl);
  virtual void onPlayerChangeNick(Player *pl, const std::string &nick);
  virtual void onPlayerReady(Player *pl);
  virtual void onPlayerQuit(Player *pl);
  virtual void onStateChange(GameInstance::State state);
  virtual void onPlayerStep(Player *pl);
  virtual void onNotification(GameInstance::Severity, const std::string &);
  virtual void onServerDisconnect();
  //@}

  /// Create and start a server.
  void startServer(int port);
  /// Create a client and connect to a server.
  void startClient(const std::string &host, int port);

  /** @brief Destroy current GameInstance, if any.
   *
   * Running server or client is closed and freed.
   */
  void stopInstance();
  /// Return current instance.
  GameInstance *instance() const { return instance_.get(); }
  /// Return current ServerInstance, or NULL.
  ServerInstance *server() const { return server_instance_; }
  /// Return current ClientInstance, or NULL.
  ClientInstance *client() const { return client_instance_; }

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
  std::auto_ptr<Screen> screen_;
  boost::asio::monotone_timer redraw_timer_;

  std::auto_ptr<GameInstance> instance_;
  ServerInstance *server_instance_;
  ClientInstance *client_instance_;
};


}

#endif
