#ifndef GUI_INTERFACE_H_
#define GUI_INTERFACE_H_

#include <memory>
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
  bool run(IniFile* cfg);
  boost::asio::io_service &io_service() { return io_service_; }
  IniFile& cfg() const { return *cfg_; }

  /** @brief Change the active screen.
   *
   * If \e screen is NULL, it ends the display.
   * The screen will be owned by the interface.
   * Current screen will be deleted before the next redraw.
   */
  void swapScreen(Screen* screen);

  sf::RenderWindow& window() { return window_; }
  bool focused() const { return focused_; }
  const ResourceManager& res_mgr() const { return res_mgr_; }

  /** @name Instance observer methods. */
  //@{
  virtual void onChat(Player* pl, const std::string& msg);
  virtual void onPlayerJoined(Player* pl);
  virtual void onPlayerChangeNick(Player* pl, const std::string& nick);
  virtual void onPlayerStateChange(Player* pl);
  virtual void onPlayerChangeFieldConf(Player* pl);
  virtual void onStateChange();
  virtual void onPlayerStep(Player* pl);
  virtual void onPlayerRanked(Player* pl);
  virtual void onNotification(GameInstance::Severity, const std::string& );
  virtual void onServerDisconnect();
  //@}

  /// Create and start a server.
  void startServer(int port);
  /// Create a client and connect to a server.
  void startClient(const std::string& host, int port);

  /** @brief Destroy current GameInstance, if any.
   *
   * Running server or client is closed and freed.
   */
  void stopInstance();
  /// Return current instance.
  GameInstance* instance() const { return instance_.get(); }
  /// Return current ServerInstance, or NULL.
  ServerInstance* server() const { return server_instance_; }
  /// Return current ClientInstance, or NULL.
  ClientInstance* client() const { return client_instance_; }

 private:
  boost::asio::io_service io_service_;
  IniFile* cfg_;

  /// Various configuration values.
  struct {
    unsigned int redraw_dt;
    bool fullscreen;
    unsigned int screen_width, screen_height;
  } window_conf_;

  bool initDisplay();
  void endDisplay();
  void enterFirstScreen();
  void onRedrawTick(const boost::system::error_code& ec);

  sf::RenderWindow window_;
  bool focused_;
  ResourceManager res_mgr_;
  std::unique_ptr<Screen> screen_;
  boost::asio::monotone_timer redraw_timer_;

  std::unique_ptr<GameInstance> instance_;
  ServerInstance* server_instance_;
  ClientInstance* client_instance_;
};


}

#endif
