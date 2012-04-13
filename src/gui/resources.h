#ifndef GUI_RESOURCES_H_
#define GUI_RESOURCES_H_

#include <memory>
#include <map>
#include <string>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <istream>
#include "../util.h"
#include "../inifile.h"

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
  /// Load language strings.
  void setLang(const std::string &lang);

  /// Get filename to use for a given resource filename
  std::string getResourceFilename(const std::string& filename) const;
  /// Get an image from its name.
  const sf::Texture *getImage(const std::string &name);
  /// Get a font from its name
  const sf::Font *getFont(const std::string &name);
  /// Style accessors
  const IniFile &style() const { return style_; }
  /// Get a language string from its section and name
  std::string getLang(const std::string &section, const std::string &key) const;

 private:
  std::string res_path_;  ///< Path to resources.

  typedef std::map<std::string, std::shared_ptr<sf::Texture> > ImageContainer;
  ImageContainer images_;  ///< Loaded images.
  typedef std::map<std::string, std::shared_ptr<sf::Font> > FontContainer;
  FontContainer fonts_;  ///< Loaded fonts
  IniFile style_;  ///< Style configuration
  IniFile lang_;  ///< Language strings
};


/** @brief Image subpart with rendering capabilities
 */
class ImageTile
{
 public:
  ImageTile();

  /// Initialize the tile using image subrect
  void create(const sf::Texture *img, const sf::IntRect &rect);
  /// Initialize the tile (x,y) from a (sx,sy) tilemap
  void create(const sf::Texture *img, int sx, int sy, int x, int y);
  /// Draw the tile at given position, with given scaling
  void render(sf::RenderTarget &target, sf::RenderStates states, float x, float y, float kx, float ky) const;
  /// Draw the tile at given position
  void render(sf::RenderTarget &target, sf::RenderStates states, float x, float y) const;
  /** @brief Set the tile on a sprite
   *
   * If \e center is \e true, sprite's origin is set to be centred on the tile.
   */
  void setToSprite(sf::Sprite *spr, bool center=false) const;

 private:
  const sf::Texture *image_;
  sf::IntRect rect_;
};


/** @brief Display a frame using a single image.
 *
 * Sides and inside are stretched to fill the drawn area while preserving the
 * original aspect size.
 * The inside subrect is the inner part rect in the whole frame image.
 */
class ImageFrame
{
 public:
  ImageFrame();
  /// Initialize the frame using image subrect and frame inside subrect
  void create(const sf::Texture *img, const sf::IntRect& rect, const sf::IntRect& inside);
  /// Draw the frame at given position, with given size
  void render(sf::RenderTarget &target, sf::RenderStates states, const sf::FloatRect& rect) const;
  /// Draw the frame centered on 0,0 with given size
  void render(sf::RenderTarget &target, sf::RenderStates states, const sf::Vector2f& size) const;

 private:
  const sf::Texture *image_;
  sf::IntRect rect_;
  sf::IntRect inside_;
};


/// Like ImageFrame, but with only 3 parts (left/middle/right)
class ImageFrameX
{
 public:
  ImageFrameX();
  /// Initialize the frame using image subrect and margin
  void create(const sf::Texture *img, const sf::IntRect& rect, unsigned int inside_left, unsigned int inside_width);
  /// Draw the frame at given position, with given size
  void render(sf::RenderTarget &target, sf::RenderStates states, const sf::FloatRect& rect) const;
  /// Draw the frame centered on 0,0 with given width
  void render(sf::RenderTarget &target, sf::RenderStates states, float w) const;

 private:
  const sf::Texture *image_;
  sf::IntRect rect_;
  unsigned int inside_left_;
  unsigned int inside_width_;
};


/** @brief Container for field display resources
 *
 * Style entries:
 *  - ColorNb: number of colors in block map
 *  - FrameOrigin: origin of field blocks in frame
 */
class StyleField
{
 public:
  StyleField();
  void load(ResourceManager *res_mgr, const std::string& section);

  /// Number of colors
  unsigned int color_nb;
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
  const sf::Texture *img_field_frame;
  /// Origin of field blocks in frame (top left corner)
  sf::Vector2f frame_origin;

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


/** @name Conversion helpers for style.ini */
//@{
std::istream& operator>>(std::istream& in, sf::Color& color);

template <typename T> std::istream& operator>>(std::istream& in, sf::Rect<T>& rect)
{
  T left, top, width, height;
  char c1, c2, c3;
  in >> left >> c1 >> top >> c2 >> width >> c3 >> height;
  if( in && c1 == ',' && c2 == ',' && c3 == ',' && width >= 0 && height >= 0 ) {
    rect.left = left;
    rect.top = top;
    rect.width = width;
    rect.height = height;
    return in;
  }
  in.clear( in.rdstate() | std::istream::failbit );
  return in;
}

template <typename T> std::istream& operator>>(std::istream& in, sf::Vector2<T>& vect)
{
  T x, y;
  char c;
  in >> x >> c >> y;
  if( in && c == ',' ) {
    vect.x = x;
    vect.y = y;
    return in;
  }
  in.clear( in.rdstate() | std::istream::failbit );
  return in;
}

template <typename T1, typename T2> std::istream& operator>>(std::istream& in, std::pair<T1, T2>& pair)
{
  T1 v1;
  T2 v2;
  char c;
  in >> v1 >> c >> v2;
  if( in && c == ',' ) {
    pair.first = v1;
    pair.second = v2;
    return in;
  }
  in.clear( in.rdstate() | std::istream::failbit );
  return in;
}

//@}

#endif
