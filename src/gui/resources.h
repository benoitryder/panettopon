#ifndef GUI_RESOURCES_H_
#define GUI_RESOURCES_H_

#include <GL/gl.h>
#include <SFML/Graphics.hpp>


namespace gui {

/** @brief Image subpart with rendering capabilities.
 *
 * Similar to sf::Sprite but without positioning, scaling, rotating, etc.
 *
 * Tiles are drawn using positions in blocks and assuming Y=0 is the bottom of
 * the screen. It used in particular for field blocks.
 */
class ImageTile
{
 public:
  ImageTile() {}
  ~ImageTile() {}

  /// Initialize the tile using image subrect.
  void create(const sf::Image &img, const sf::IntRect &rect);
  /// Initialize the tile (x,y) from a (sx,sy) tilemap.
  void create(const sf::Image &img, int sx, int sy, int x, int y);
  /// Draw the tile at given position.
  void render(sf::Renderer &renderer, float x, float y, float w, float h) const;
  /** @brief Affect the tile to a sprite.
   *
   * If \e center is \e true, sprite origin is changed to be cented on the
   * tile.
   */
  void setToSprite(sf::Sprite &spr, bool center=false) const;

 private:
  sf::ResourcePtr<sf::Image> image_;
  sf::IntRect rect_;
};


/// Container for display resources.
class DisplayRes
{
 public:
  DisplayRes();

  void load(const std::string &res_path);

  /// Block pixel size.
  unsigned int bk_size;

  /// Tile group for block of a given color.
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
  sf::Image img_field_frame;

  /// Cursor (two positions).
  ImageTile tiles_cursor[2];

  /// Labels (combo and chain).
  struct TilesLabels {
    ImageTile combo, chain;
  } tiles_labels;

  /// Waiting garbages.
  struct TilesWaitingGb {
    ImageTile blocks[FIELD_WIDTH];
    ImageTile line;
  } tiles_waiting_gb;

  /// Default font
  const sf::Font &font;

 private:
  sf::Image img_bk_color_;
  sf::Image img_bk_gb_;
  sf::Image img_cursor_;
  sf::Image img_labels_;
  sf::Image img_waiting_gb_;
};

}

#endif
