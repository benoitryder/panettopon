#ifndef GUI_INTF_H_
#define GUI_INTF_H_

#include <SFML/Graphics/RenderWindow.hpp>
#include "monotone_timer.hpp"
#include "interface.h"
#include "client.h"
#include "gui/resources.h"


class Config;

namespace gui {

class FieldDisplay;


class GuiInterface: public ClientInterface
{
 protected:
  static const std::string CONF_SECTION;

 public:
  GuiInterface();
  virtual ~GuiInterface();
  virtual bool run(const Config &cfg);
  virtual void onChat(const Player *pl, const std::string &msg);
  virtual void onNotification(Severity sev, const std::string &msg);
  virtual void onPlayerJoined(const Player *pl);
  virtual void onPlayerSetNick(const Player *pl, const std::string &old_nick);
  virtual void onPlayerReady(const Player *pl);
  virtual void onPlayerQuit(const Player *pl);
  virtual void onMatchInit(const Match *m);
  virtual void onMatchReady(const Match *m);
  virtual void onMatchStart(const Match *m);
  virtual void onMatchEnd(const Match *m);
  virtual void onFieldStep(const Field *fld);
  virtual void onFieldLost(const Field *fld);
  virtual KeyState getNextInput();

  /// Add a message in given color.
  void addMessage(int color, const char *fmt, ...);

 private:

  boost::asio::io_service io_service_;
  Client client_;
  sf::RenderWindow window_;

  typedef boost::ptr_map<const Field *, FieldDisplay> FieldDisplayMap;
  FieldDisplayMap fdisplays_;

  /// Key bindings.
  struct {
    sf::Key::Code up, down, left, right;
    sf::Key::Code swap, raise;
    sf::Key::Code quit;
  } keys_;
  /// Various configuration values.
  struct {
    unsigned int redraw_dt;
    bool fullscreen;
    unsigned int screen_width, screen_height;
    float zoom;
    std::string res_path;
  } conf_;

  /// Get a key code from a key name, 0 if invalid.
  static sf::Key::Code str2key(const std::string &s);


  /// Init display, load resources.
  bool initDisplay();
  /// Close display (but don't free resources).
  void endDisplay();

  /// Redraw the screen.
  void redraw();
  /// Callback for redraw.
  void onRedrawTick(const boost::system::error_code &ec);

  DisplayRes res_;
  boost::asio::monotone_timer redraw_timer_;
};


/** @brief Drawable class to display fields.
 *
 * The rendering origin is the top left of the field.
 */
class FieldDisplay: public sf::Drawable
{
 protected:
  static const unsigned int CROUCH_DURATION;
  static const float BOUNCE_BLOCK_SIZE;
  static const float BOUNCE_SYMBOL_SIZE;
  static const float BOUNCE_WIDTH_MIN;
  static const float BOUNCE_WIDTH_MAX;
  static const float BOUNCE_HEIGHT_MIN;
  static const float BOUNCE_HEIGHT_MAX;
  static const float BOUNCE_Y_MIN;
  static const float BOUNCE_Y_MAX;

 public:
  FieldDisplay(const Field &fld, const DisplayRes &res, int slot);
  virtual ~FieldDisplay();

  /// Update internal display after a step.
  void step();

 protected:
  virtual void Render(sf::RenderTarget &target) const;

 private:
  void renderBlock(sf::RenderTarget &target, int x, int y) const;

  /** @brief Draw a bouncing block.
   *
   * The block is drawn centered on the current position.
   * \e bounce gives bouncing position of the block's symbol, relative to the
   * normal state (0), from -1 (max crouching) to 1 (max boucing). Rendered
   * position is not necessarily computed using a linear function.
   *
   * Only the symbol part is drawn, to drawing pixels of adjacent tiles because
   * of stretching.
   */
  void renderBouncingBlock(sf::RenderTarget &target, int x, int y, float bounce, unsigned int color) const;

  const Field &field_;
  const DisplayRes &res_;

  sf::Sprite spr_frame_;
  sf::Sprite spr_cursor_;

  /// Current lift offset (from 0 to 1).
  float lift_offset_;
  /// Per-block crouching state.
  unsigned int crouch_dt_[FIELD_WIDTH][FIELD_HEIGHT+1];

  BasicLabelHolder labels_;
};


}

#endif
