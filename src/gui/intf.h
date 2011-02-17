#ifndef GUI_INTF_H_
#define GUI_INTF_H_

#include "../monotone_timer.hpp"
#include "../client.h"
#include "resources.h"


class Config;

namespace gui {

class FieldDisplay;


class GuiInterface: public ClientInstance::Observer,
    public GameInputScheduler::InputProvider
{
 protected:
  static const std::string CONF_SECTION;

 public:
  GuiInterface();
  virtual ~GuiInterface();
  bool run(const Config &cfg);

  /** @name ClientInstance::Observer methods. */
  //@{
  virtual void onChat(Player *pl, const std::string &msg);
  virtual void onPlayerJoined(Player *pl);
  virtual void onPlayerChangeNick(Player *pl, const std::string &nick);
  virtual void onPlayerReady(Player *pl);
  virtual void onPlayerQuit(Player *pl);
  virtual void onStateChange();
  virtual void onPlayerStep(Player *pl);
  virtual void onNotification(GameInstance::Severity, const std::string &);
  virtual void onServerDisconnect();
  //@}
  virtual KeyState getNextInput(Player *pl);

  /// Add a message in given color.
  void addMessage(int color, const char *fmt, ...);

 private:

  boost::asio::io_service io_service_;
  ClientInstance instance_;
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

  const DisplayRes &res() const { return res_; }

 protected:
  virtual void Render(sf::RenderTarget &target, sf::Renderer &renderer) const;

 private:
  void renderBlock(sf::Renderer &renderer, int x, int y) const;

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
  void renderBouncingBlock(sf::Renderer &renderer, int x, int y, float bounce, unsigned int color) const;

  const Field &field_;
  const DisplayRes &res_;

  sf::Sprite spr_frame_;
  sf::Sprite spr_cursor_;

  /// Current lift offset (from 0 to 1).
  float lift_offset_;
  /// Per-block crouching state.
  unsigned int crouch_dt_[FIELD_WIDTH][FIELD_HEIGHT+1];

  /** @name Labels. */
  //@{

  class Label: public sf::Drawable
  {
   public:
    Label(const DisplayRes &res, const FieldPos &pos, bool chain, unsigned int val);
    virtual void Render(sf::RenderTarget &target, sf::Renderer &renderer) const;
    /// Step and update the label.
    void step();
    unsigned int dt() const { return dt_; }
   private:
    static const unsigned int DURATION;
    const DisplayRes &res_;
    sf::Sprite bg_;    ///< Background sprite.
    sf::Text txt_;     ///< Label text.
    unsigned int dt_;  ///< Remaining display time.
  };

  typedef std::deque<Label> LabelContainer;
  LabelContainer labels_;

  /// Return top-left match position.
  FieldPos matchLabelPos();

  //@}

  /** @name Waiting garbages. */
  //@{

  /// Drawable for waiting garbages.
  class GbWaitingDrb: public sf::Drawable
  {
   public:
    GbWaitingDrb(const DisplayRes &res, const Garbage &gb);
    virtual void Render(sf::RenderTarget &target, sf::Renderer &renderer) const;
    /// Step and update the garbage.
    void step();
    /// Set position from drop order above the field.
    void setPosition(int i);
    GbId gbid() const { return gb_.gbid; }
   private:
    /// Update the text, if needed.
    void updateText();

    const DisplayRes &res_;
    const Garbage &gb_;
    sf::Sprite bg_;  ///< Background sprite.
    sf::Text txt_;   ///< Label text, not used if size_ is 0.
    /// Chain size written in text, 0 if no text.
    int txt_size_;
  };

  /// List of waiting garbage drawables.
  typedef boost::ptr_list<GbWaitingDrb> GbWaitingDrbList;
  GbWaitingDrbList gbw_drbs_;

  //@}
};


}

#endif
