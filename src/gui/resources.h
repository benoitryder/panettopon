#ifndef GUI_RESOURCES_H_
#define GUI_RESOURCES_H_

#include <map>
#include <string>
#include <SFML/Graphics/Image.hpp>
#include "../util.h"

namespace sf {
  class Sprite;
}

namespace gui {


class ResourceManager
{
 public:
  ResourceManager();
  /// Initialize resources, set the resource path.
  void init(const std::string &path);

  /// Get an image from its name.
  const sf::Image *getImage(const std::string &name);

 private:
  std::string res_path_;  ///< Path to resources.

  typedef std::map<std::string, sf::ResourcePtr<sf::Image> > ImageContainer;
  ImageContainer images_;  ///< Loaded images.
};


/** @brief Image subpart with rendering capabilities
 *
 * Similar to sf::Sprite but without positioning, scaling, rotating, etc.
 *
 * Tiles are drawn using positions in blocks and assuming Y=0 is the bottom of
 * the screen. It used in particular for field blocks.
 */
class ImageTile
{
 public:
  ImageTile();

  /// Initialize the tile using image subrect
  void create(const sf::Image *img, const sf::IntRect &rect);
  /// Initialize the tile (x,y) from a (sx,sy) tilemap
  void create(const sf::Image *img, int sx, int sy, int x, int y);
  /// Draw the tile at given position
  void render(sf::Renderer &renderer, float x, float y, float w, float h) const;
  /** @brief Set the tile on a sprite
   *
   * If \e center is \e true, sprite's origin is set to be centred on the tile.
   */
  void setToSprite(sf::Sprite *spr, bool center=false) const;

 private:
  const sf::Image *image_;
  sf::IntRect rect_;
};


/// Container for field display resources
class ResField
{
 public:
  ResField();
  void load(ResourceManager *res_mgr);

  /// Block pixel size
  unsigned int bk_size;

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
  const sf::Image *img_field_frame;

  /// Cursor (two positions)
  ImageTile tiles_cursor[2];

  /// Labels (combo and chain)
  struct TilesLabels {
    ImageTile combo, chain;
  } tiles_labels;

  /// Hanging garbages
  struct TilesGbHanging {
    ImageTile blocks[FIELD_WIDTH];
    ImageTile line;
  } tiles_gb_hanging;
};


}

#endif
