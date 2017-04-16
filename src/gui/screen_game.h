#ifndef GUI_SCREEN_GAME_H_
#define GUI_SCREEN_GAME_H_

#include <memory>
#include <list>
#include <map>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Audio/Sound.hpp>
#include "screen.h"
#include "resources.h"
#include "input.h"

namespace gui {

class StyleGlobal;
class GuiInterface;
class FieldDisplay;


/** @brief Style for a field display
 *
 * Style entries:
 *  - FrameOrigin: origin of field blocks in frame
 */
struct StyleField
{
  static constexpr unsigned int RANK_MAX = 10;

  const StyleGlobal* global;

  /// Block pixel size
  unsigned int bk_size = 0;

  /// Tile group for block of a given color
  struct TilesBkColor {
    ImageTile normal, bg, face, flash, mutate;
  };
  /// Color blocks
  std::vector<TilesBkColor> tiles_bk_color;

  /// Garbages
  struct TilesGb {
    ImageTile tiles[4][4];
    ImageTile center[2][2];
    ImageTile mutate, flash;
  } tiles_gb;

  /// Field frame
  ImageFrame::Style field_frame_style;
  /// Origin of field blocks in frame (top left corner)
  sf::Vector2f frame_origin;

  /// Cursor (two positions)
  ImageTile tiles_cursor[2];

  /// Signs (combo and chain)
  struct TilesSigns {
    ImageTile combo, chain;
  } tiles_signs;
  StyleText sign_style;

  /// Hanging garbages
  struct TilesGbHanging {
    ImageTile blocks[FIELD_WIDTH];
    ImageTile line;
  } tiles_gb_hanging;
  StyleText gb_hanging_style;

  /// Start countdown
  StyleText start_countdown_style;
  /// Rank sign
  struct {
    StyleText win;
    StyleText lose;
    StyleText draw;
    std::vector<StyleText> rank;
  } rank_sign_style;

  void load(const StyleLoader& loader, const StyleGlobal& global);
};


class ScreenGame: public Screen, public GameInputScheduler::InputProvider
{
 public:
  ScreenGame(GuiInterface& intf);
  virtual void enter();
  virtual void exit();
  virtual void redraw();
  virtual bool onInputEvent(const sf::Event& ev);
  virtual void onServerDisconnect();
  virtual void onPlayerStep(Player& pl);
  virtual void onPlayerRanked(Player& pl);
  virtual void onStateChange();

  /// InputProvider interface.
  virtual KeyState getNextInput(const Player& pl);

  void setPlayerMapping(const Player& pl, const InputMapping& mapping);

 private:
  GameInputScheduler input_scheduler_;
  StyleField style_field_;
  typedef std::map<FldId, std::unique_ptr<FieldDisplay>> FieldDisplayContainer;
  FieldDisplayContainer field_displays_;
  typedef std::map<PlId, InputMapping> InputMappingContainer;
  InputMappingContainer input_mappings_;
};


/// Drawable class to display fields
class FieldDisplay: public sf::Drawable, public sf::Transformable
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
  FieldDisplay(const GuiInterface& intf, const Field& fld, const StyleField& style);
  virtual ~FieldDisplay() {}

  /// Update internal display after a step
  void step();
  /// Update internal display when player is ranked
  void doRank();

 protected:
  virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const;

 private:
  void renderBlock(sf::RenderTarget& target, sf::RenderStates states, int x, int y) const;

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
  void renderBouncingBlock(sf::RenderTarget& target, sf::RenderStates states, int x, int y, float bounce, unsigned int color) const;

  const GuiInterface& intf_;
  const Field& field_;
  const StyleField& style_;
  ImageFrame field_frame_;
  sf::Sprite spr_cursor_;
  std::unique_ptr<sf::Text> text_start_countdown_;
  std::unique_ptr<sf::Text> text_rank_sign_;

  /// Current lift offset (from 0 to 1)
  float lift_offset_;
  /// Per-block crouching state
  unsigned int crouch_dt_[FIELD_WIDTH][FIELD_HEIGHT+1];

  /** @name Signs */
  //@{

  class Sign: public sf::Drawable, public sf::Transformable
  {
   public:
    Sign(const StyleField& style, const FieldPos& pos, bool chain, unsigned int val);
    virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const;
    /// Step and update the sign.
    void step();
    unsigned int dt() const { return dt_; }
   private:
    static const unsigned int DURATION;
    const StyleField& style_;
    sf::Sprite bg_;    ///< Background sprite.
    sf::Text txt_;     ///< Sign text.
    unsigned int dt_;  ///< Remaining display time.
  };

  typedef std::deque<Sign> SignContainer;
  SignContainer signs_;

  /// Return top-left match position
  FieldPos matchSignPos();

  //@}

  /** @name Hanging garbages */
  //@{

  /// Drawable for hanging garbages
  class GbHanging: public sf::Drawable, public sf::Transformable
  {
   public:
    GbHanging(const StyleField& res, const Garbage& gb);
    virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const;
    /// Step and update the garbage.
    void step();
    /// Set position from drop order above the field.
    void setPosition(int i);
    GbId gbid() const { return gb_.gbid; }
   private:
    /// Update the text, if needed.
    void updateText();

    const StyleField& style_;
    const Garbage& gb_;
    sf::Sprite bg_;  ///< Background sprite.
    sf::Text txt_;   ///< Sign text, not used if size_ is 0.
    /// Chain size written in text, 0 if no text.
    int txt_size_;
  };

  /// List of hanging garbage drawables
  typedef std::list<std::unique_ptr<GbHanging>> GbHangingList;
  GbHangingList gbw_drbs_;

  //@}

  struct {
    sf::Sound move;
    struct {
      sf::Sound both;
      sf::Sound left;
      sf::Sound right;
    } swap;
    SoundPool fall;
    std::vector<std::vector<SoundPool>> pops;
  } sounds_;

};


}

#endif
