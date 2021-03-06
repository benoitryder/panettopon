#ifndef GUI_INTERFACE_H_
#define GUI_INTERFACE_H_

#include <memory>
#include <string>
#include <vector>
#include <SFML/Graphics/RenderWindow.hpp>
#include "resources.h"
#include "input.h"
#include "../client.h"
#include "../monotone_timer.hpp"

class IniFile;
class ServerInstance;
class ClientInstance;

namespace gui {

class Screen;

/** @brief Global style entries
 *
 * Style entries:
 *  - Color.Neutral: neutral block/field color
 *  - Color.N: block/field color N
 */
struct StyleGlobal
{
  /// List of block/field colors
  std::vector<sf::Color> colors;

  void load(const StyleLoader& loader);
};


class GuiInterface: public ClientInstance::Observer
{
 protected:
  static const std::string CONF_SECTION;

 public:
  /// Configured input mappings, grouped by type
  struct InputMappings {
    std::vector<InputMapping> joystick;
    std::vector<InputMapping> keyboard;
  };

  /// Reference block size used to compute VIEW_SIZE
  static const unsigned int REF_BLOCK_SIZE;
  /** @brief Minimal display size of a file
   *
   * Value is used to compute view size; two fields must fit side by side.
   *
   * With this value, 4 fields fit on 1080p with 64px blocks. Minimal height
   * has been increased to have an exact x2 zoom with this resolution.
   */
  static const sf::Vector2f REF_FIELD_SIZE;

  GuiInterface();
  virtual ~GuiInterface();
  bool run(IniFile& cfg);
  boost::asio::io_service &io_service() { return io_service_; }
  IniFile& cfg() const { return *cfg_; }
  const StyleGlobal& style() const { return style_; }
  const InputMappings& inputMappings() const { return input_mappings_; }

  /** @brief Change the active screen.
   *
   * If \e screen is NULL, it ends the display.
   * The screen will be owned by the interface.
   * Current screen will be deleted before the next redraw.
   */
  void swapScreen(std::unique_ptr<Screen> screen);

  /// Enable or disable text input mode
  void setTextInput(bool enable) { input_handler_.setTextInput(enable); }
  bool textInput() const { return input_handler_.textInput(); }


  sf::RenderWindow& window() { return window_; }
  bool focused() const { return focused_; }
  const ResourceManager& res_mgr() const { return res_mgr_; }

  /** @name Instance observer methods. */
  //@{
  virtual void onChat(Player& pl, const std::string& msg);
  virtual void onPlayerJoined(Player& pl);
  virtual void onPlayerChangeNick(Player& pl, const std::string& nick);
  virtual void onPlayerStateChange(Player& pl);
  virtual void onPlayerChangeFieldConf(Player& pl);
  virtual void onStateChange();
  virtual void onServerChangeFieldConfs();
  virtual void onPlayerStep(Player& pl);
  virtual void onPlayerRanked(Player& pl);
  virtual void onNotification(GameInstance::Severity, const std::string& );
  virtual void onServerConnect(bool success);
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

  InputMappings input_mappings_;

  bool initDisplay();
  void endDisplay();
  void updateView(unsigned int width, unsigned int height);
  void enterFirstScreen();
  void onRedrawTick(const boost::system::error_code& ec);

  sf::RenderWindow window_;
  bool focused_;
  ResourceManager res_mgr_;
  InputHandler input_handler_;
  std::unique_ptr<Screen> screen_;
  boost::asio::monotone_timer redraw_timer_;

  std::unique_ptr<GameInstance> instance_;
  ServerInstance* server_instance_;
  ClientInstance* client_instance_;

  StyleGlobal style_;
};


}

#endif
