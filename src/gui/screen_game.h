#ifndef GUI_SCREEN_GAME_H_
#define GUI_SCREEN_GAME_H_

#include <SFML/Graphics/Sprite.hpp>
#include "screen.h"

namespace gui {

class GuiInterface;
class ResField;


class ScreenGame: public Screen, public GameInputScheduler::InputProvider
{
 public:
  ScreenGame(GuiInterface &intf, Player *pl);
  virtual void enter();
  virtual void exit();
  virtual void redraw();
  virtual bool onInputEvent(const sf::Event &ev);
  virtual void onStateChange(GameInstance::State state);

  /// InputProvider interface.
  virtual KeyState getNextInput(Player *pl);

 private:
  Player *player_;  ///< Local controlled player
  GameInputScheduler input_scheduler_;

  /// Key bindings.
  struct {
    sf::Key::Code up, down, left, right;
    sf::Key::Code swap, raise;
  } keys_;
};


/// Drawable class to display fields
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
  FieldDisplay(const Field &fld, const ResField &res);
  virtual ~FieldDisplay() {}

  /// Update internal display after a step
  void step();

 protected:
  virtual void Render(sf::RenderTarget &target, sf::Renderer &renderer) const;

 private:
  void renderBlock(sf::Renderer &renderer, int x, int y) const;

  /** @brief Draw a bouncing block
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
  const ResField &res_;
  sf::Sprite spr_frame_;
  sf::Sprite spr_cursor_;

  /// Current lift offset (from 0 to 1)
  float lift_offset_;
  /// Per-block crouching state
  unsigned int crouch_dt_[FIELD_WIDTH][FIELD_HEIGHT+1];

  /** @name Labels */
  //@{

  class Label: public sf::Drawable
  {
   public:
    Label(const ResField &res, const FieldPos &pos, bool chain, unsigned int val);
    virtual void Render(sf::RenderTarget &target, sf::Renderer &renderer) const;
    /// Step and update the label.
    void step();
    unsigned int dt() const { return dt_; }
   private:
    static const unsigned int DURATION;
    const ResField &res_;
    sf::Sprite bg_;    ///< Background sprite.
    sf::Text txt_;     ///< Label text.
    unsigned int dt_;  ///< Remaining display time.
  };

  typedef std::deque<Label> LabelContainer;
  LabelContainer labels_;

  /// Return top-left match position
  FieldPos matchLabelPos();

  //@}

  /** @name Waiting garbages */
  //@{

  /// Drawable for waiting garbages
  class GbWaiting: public sf::Drawable
  {
   public:
    GbWaiting(const ResField &res, const Garbage &gb);
    virtual void Render(sf::RenderTarget &target, sf::Renderer &renderer) const;
    /// Step and update the garbage.
    void step();
    /// Set position from drop order above the field.
    void setPosition(int i);
    GbId gbid() const { return gb_.gbid; }
   private:
    /// Update the text, if needed.
    void updateText();

    const ResField &res_;
    const Garbage &gb_;
    sf::Sprite bg_;  ///< Background sprite.
    sf::Text txt_;   ///< Label text, not used if size_ is 0.
    /// Chain size written in text, 0 if no text.
    int txt_size_;
  };

  /// List of waiting garbage drawables
  typedef boost::ptr_list<GbWaiting> GbWaitingList;
  GbWaitingList gbw_drbs_;

  //@}
};


}

#endif
