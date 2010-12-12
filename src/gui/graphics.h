#ifndef GUI_GRAPHICS_H_
#define GUI_GRAPHICS_H_

#include <string>
#include <SDL/SDL.h>
#include <GL/gl.h>
#include <FTGL/ftgl.h>


namespace gui {

/// Set path to resource directory.
void setResPath(const std::string &s);


/// Image resource.
class Image
{
 public:
  Image(const std::string &name);
  ~Image();

  int w() const { return sfc_->w; }
  int h() const { return sfc_->h; }
  void *pixels() const { return sfc_->pixels; }

 protected:
  /** @brief Load a PNG file into an SDL surface.
   * 
   * This method never returns a null pointer.
   * On error, an exception is thrown.
   */
  static SDL_Surface *loadPNG(const std::string &fname);

 private:
  SDL_Surface *sfc_;
};


class Sprite
{
  //XXX must be non-copyable
 public:
  /** @brief Create an empty sprite.
   *
   * After creation, the sprite should be initialized by an init method.
   */
  Sprite();

  bool isInitialized() const { return tex_id_ != 0; }
  /// Initialize a sprite from an entire image.
  Sprite &initFromImage(const Image &img);
  /// Initialize a sprite from part of an image.
  Sprite &initFromImage(const Image &img, int x, int y, int w, int h);

  /** @brief Set texture wrapping.
   * Default behavior is to clamp (false).
   */
  Sprite &setRepeat(bool b);
  /// Set display size.
  Sprite &setSize(float w, float h);
  /// Set display size from a ratio.
  Sprite &setZoom(float r);

 public:
  ~Sprite();

  /// Draw a sprite.
  void draw() const { draw(0,0,1,1); }
  /// Draw a part of a sprite.
  void draw(float x, float y, float w, float h) const;

 private:
  GLuint tex_id_;
  unsigned int tw_, th_; ///< Texture size.
  float dw_, dh_; ///< Display size.
};


/** @brief Font wrapper.
 */
class Font
{
 public:
  enum Stretch{
    STRETCH_NONE = 0,
    STRETCH_X    = 1,
    STRETCH_Y    = 2,
    STRETCH_BOTH = STRETCH_X | STRETCH_Y,
  };

  Font(): font_(NULL) {}
  ~Font() { this->unload(); }

  /// Initialize the font.
  void load(const std::string &name, int size);
  /// Free the font, do nothing if not loaded.
  void unload();

  /// Get bounding box of a text.
  FTBBox bbox(const std::string &txt) const;
  /// Render a text.
  void render(const std::string &txt) const;
  /// Render a text centered, with a given size.
  void render(const std::string &txt, float w, float h, Stretch s) const;

 private:
  FTFont *font_;
};


}

#endif
