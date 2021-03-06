#ifndef GUI_RESOURCES_H_
#define GUI_RESOURCES_H_

#include <memory>
#include <map>
#include <vector>
#include <string>
#include <istream>
#include <stdexcept>
#include "../util.h"
#include "../inifile.h"

namespace sf {
  class Font;
  class Text;
  class Texture;
  class Sprite;
  class Sound;
  class SoundBuffer;
}

namespace gui {

class StyleLoader;


class ResourceManager
{
 public:
  struct LoadError: public std::runtime_error
  {
    LoadError(const std::string& what): std::runtime_error(what) {}
  };

  ResourceManager();
  /// Initialize resources, set the resource path.
  void init(const std::string& path);
  /// Load language strings.
  void setLang(const std::string& lang);

  /// Get filename to use for a given resource filename
  std::string getResourceFilename(const std::string& filename) const;
  /// Get an image from its name.
  const sf::Texture& getImage(const std::string& name) const;
  /// Get a font from its name
  const sf::Font& getFont(const std::string& name) const;
  /// Get a sound buffer from its name
  const sf::SoundBuffer& getSound(const std::string& name) const;
  /// Style accessors
  const IniFile& style() const { return style_; }
  /// Get a language string from its section and name
  std::string getLang(const std::string& key) const;
  std::string getLang(IniFile::Path path) const;

 private:
  std::string res_path_;  ///< Path to resources.

  typedef std::map<std::string, std::shared_ptr<sf::Texture> > ImageContainer;
  mutable ImageContainer images_;  ///< Loaded images.
  typedef std::map<std::string, std::shared_ptr<sf::Font> > FontContainer;
  mutable FontContainer fonts_;  ///< Loaded fonts
  typedef std::map<std::string, std::shared_ptr<sf::SoundBuffer> > SoundContainer;
  mutable SoundContainer sounds_;  ///< Loaded sound buffers
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
  void create(const sf::Texture& img, const sf::IntRect& rect);
  /// Initialize the tile (x,y) from a (sx,sy) tilemap
  void create(const sf::Texture& img, int sx, int sy, int x, int y);
  /// Draw the tile at given position, with given scaling
  void render(sf::RenderTarget& target, sf::RenderStates states, float x, float y, float kx, float ky, const sf::Color& c=sf::Color::White) const;
  /// Draw the tile at given position
  void render(sf::RenderTarget& target, sf::RenderStates states, float x, float y, const sf::Color& c=sf::Color::White) const;
  /** @brief Set the tile on a sprite
   *
   * If \e center is \e true, sprite's origin is set to be centred on the tile.
   */
  void setToSprite(sf::Sprite& spr, bool center=false) const;

 private:
  const sf::Texture* image_;
  sf::IntRect rect_;
};


/** @brief Display a frame using a single image.
 *
 * Sides and inside are stretched to fill the drawn area while preserving the
 * original aspect size.
 * The inside subrect is the inner part rect in the whole frame image.
 * The outside border can be drawn as part of the rendering size, or outside of
 * it.
 */
class ImageFrame
{
 public:
  enum class Border
  {
    INSIDE,
    OUTSIDE
  };

  struct Style {
    const sf::Texture* image = nullptr;
    sf::IntRect rect;
    sf::IntRect inside;
    Border border;
    sf::Color color = sf::Color::White;
    void load(const StyleLoader& loader);
    void apply(ImageFrame& o) const;
  };

  ImageFrame();
  /// Initialize the frame using image subrect and frame inside subrect
  void create(const sf::Texture& img, const sf::IntRect& rect, const sf::IntRect& inside, Border border);
  /// Set drawing color
  void setColor(const sf::Color& color) { color_ = color; }
  /// Draw the frame at given position, with given size
  void render(sf::RenderTarget& target, sf::RenderStates states, const sf::FloatRect& rect) const;
  /// Draw the frame centered on 0,0 with given size
  void render(sf::RenderTarget& target, sf::RenderStates states, const sf::Vector2f& size) const;

 private:
  const sf::Texture* image_;
  sf::IntRect rect_;
  sf::IntRect inside_;
  Border border_;
  sf::Color color_;
};


/// Like ImageFrame, but with only 3 parts (left/middle/right)
class ImageFrameX
{
 public:
  typedef ImageFrame::Border Border;

  struct Style {
    const sf::Texture* image = nullptr;
    sf::IntRect rect;
    unsigned int inside_left;
    unsigned int inside_width;
    Border border;
    sf::Color color = sf::Color::White;
    void load(const StyleLoader& loader);
    void apply(ImageFrameX& o) const;
  };

  ImageFrameX();
  /// Initialize the frame using image subrect and margin
  void create(const sf::Texture& img, const sf::IntRect& rect, unsigned int inside_left, unsigned int inside_width, Border border);
  /// Set drawing color
  void setColor(const sf::Color& color) { color_ = color; }
  /// Draw the frame at given position, with given size
  void render(sf::RenderTarget& target, sf::RenderStates states, const sf::FloatRect& rect) const;
  /// Draw the frame centered on 0,0 with given width
  void render(sf::RenderTarget& target, sf::RenderStates states, float w) const;

 private:
  const sf::Texture* image_;
  sf::IntRect rect_;
  unsigned int inside_left_;
  unsigned int inside_width_;
  Border border_;
  sf::Color color_;
};


/** @brief Sound pool for concurrent playing
 *
 * A single sf::Sound object is not sufficient if the sound can be played
 * several times simultaneously.
 */
class SoundPool
{
 public:
  SoundPool();
  SoundPool(const sf::SoundBuffer& buffer);

  /// Set sound's source buffer
  void setBuffer(const sf::SoundBuffer& buffer);
  /// Get an available sound object from the pool
  sf::Sound& getSound();
  /** @brief Get an available sound object and play it
   *
   * This is a shortcut for getSound().play().
   */
  void play();

 private:
  std::vector<sf::Sound> pool_;
  const sf::SoundBuffer* buffer_;
};


}


/** @name Conversions for style.ini */
//@{

template <> struct IniFileConverter<sf::Color>
{
  static sf::Color parse(const std::string& value);
};

template <typename T> struct IniFileConverter<sf::Rect<T>>
{
  static sf::Rect<T> parse(const std::string& value)
  {
    sf::Rect<T> rect;
    size_t pos = 0;
    pos = parsing::castUntil(value, pos, ',', rect.left);
    pos = parsing::castUntil(value, pos, ',', rect.top);
    pos = parsing::castUntil(value, pos, ',', rect.width);
    pos = parsing::castUntil(value, pos, 0, rect.height);
    return rect;
  }
};

template <typename T> struct IniFileConverter<sf::Vector2<T>>
{
  static sf::Vector2<T> parse(const std::string& value)
  {
    sf::Vector2<T> vect;
    size_t pos = 0;
    pos = parsing::castUntil(value, pos, ',', vect.x);
    pos = parsing::castUntil(value, pos, 0, vect.y);
    return vect;
  }
};

template <typename T1, typename T2> struct IniFileConverter<std::pair<T1, T2>>
{
  static std::pair<T1, T2> parse(const std::string& value)
  {
    std::pair<T1, T2> pair;
    size_t pos = 0;
    pos = parsing::castUntil(value, pos, ',', pair.first);
    pos = parsing::castUntil(value, pos, 0, pair.second);
    return pair;
  }
};

template <> struct IniFileConverter<gui::ImageFrame::Border>
{
  static gui::ImageFrame::Border parse(const std::string& value);
};

//@}

#endif
