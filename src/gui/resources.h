#ifndef GUI_RESOURCES_H_
#define GUI_RESOURCES_H_

#include <GL/gl.h>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Font.hpp>


namespace gui {

/** @brief Sprite resource for field blocks.
 *
 * Provide features similar to sf::Sprite but more suitable for field blocks
 * and is not a drawable itself.
 *
 * Tiles are drawn using positions in blocks and assuming Y=0 is the bottom of
 * the screen.
 */
class FieldTile
{
 public:
  FieldTile() {}
  ~FieldTile() {}

  /// Initialize the tile using texture coordinates (from 0 to 1).
  void create(const sf::Image &img, const sf::FloatRect &rect);
  /// Initialize the tile using number of horizontal tiles and positions.
  void create(const sf::Image &img, int nx, int x, int y);
  /// Draw the tile using x,y as top left corner.
  void render(sf::RenderTarget &target, int x, int y) const;
  /// Draw the tile using a given quad position.
  void render(sf::RenderTarget &target, const sf::FloatRect &pos) const;

 private:
  sf::ResourcePtr<sf::Image> image_;
  sf::FloatRect rect_;
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
    FieldTile normal, bg, face, flash, mutate;
  };
  /// Color blocks
  std::vector<TilesBkColor> tiles_bk_color;

  /// Garbages
  struct TilesGb {
    FieldTile tiles[4][4];
    FieldTile center[2][2];
    FieldTile mutate, flash;
  } tiles_gb;

  /// Labels
  struct SprsLabels {
    sf::Sprite chain, combo;
  } spr_label;

  // Waiting garbages
  struct SprsWaitingGb {
    sf::Sprite line;
    sf::Sprite blocks[FIELD_WIDTH];
  } spr_waiting_gb;

  /// Field frame
  sf::Image img_field_frame;

  /// Cursor (two positions).
  sf::Image img_cursor;

  /// Default font
  const sf::Font &font;

 private:
  sf::Image img_bk_color_;
  sf::Image img_bk_gb_;
};

}

#endif
