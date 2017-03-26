#ifndef GUI_STYLES_H_
#define GUI_STYLES_H_

#include <string>
#include <SFML/Graphics/Text.hpp>
#include "resources.h"
#include "../inifile.h"

namespace sf {
  class Sprite;
  class Text;
}


namespace gui {

class FieldDisplay;


struct StyleError: public std::runtime_error
{
  StyleError(const std::string& key, const std::string& msg);
  StyleError(const StyleLoader& loader, const std::string& prop, const std::string& msg);
};

/// Retrieve style container from object type
template <class T> struct StyleType { typedef typename T::Style container; };


/** @brief Search for style properties
 *
 * Provide methods to search for style properties.
 * It handles style fallbacks (if needed) and format understandable error messages.
 *
 * This class can be used as a mixin.
 */
class StyleLoader
{
 public:
  StyleLoader();
  virtual ~StyleLoader();

  virtual const ResourceManager& res_mgr() const = 0;

  /** @brief Search style entry key for a given property
   * @return true if found, false otherwise.
   */
  virtual bool searchStyle(const std::string& prop, std::string& key) const = 0;
  bool searchStyle(IniFile::Path path, std::string& key) const {
    return searchStyle(IniFile::join(path), key);
  }

  /** @brief Style section to be used in some error message
   *
   * This string is used as prefix for style error (e.g. missing property).
   * It should use dotted notation and be a section on which style values may
   * be set.
   */
  virtual std::string styleErrorSection() const = 0;

  /// Get a style, throw a StyleError if not found
  template <class T> T getStyle(const std::string& prop) const;
  template <class T> T getStyle(IniFile::Path path) const;
  /// Get a style and its key, throw a StyleError if not found
  template <class T> T getStyle(const std::string& prop, std::string& key) const;
  template <class T> T getStyle(IniFile::Path path, std::string& key) const;

  /** @brief Search and get a style if available
   * @return true if found, false otherwise.
   */
  template <class T> bool fetchStyle(const std::string& prop, T& val) const;
  template <class T> bool fetchStyle(IniFile::Path path, T& val) const;

  /// Helper to load then apply a style
  template <class T> void applyStyle(T& o) const;
  template <class T> void applyStyle(T& o, const std::string& prop) const;

};


/// Basic style loader, loading from a given section
class StyleLoaderResourceManager: public StyleLoader
{
 public:
  StyleLoaderResourceManager(const ResourceManager& res_mgr, const std::string& name);
  virtual ~StyleLoaderResourceManager() {}

  virtual const ResourceManager& res_mgr() const { return res_mgr_; }
  virtual bool searchStyle(const std::string& prop, std::string& key) const;
  virtual std::string styleErrorSection() const { return name_; }

 protected:
  const ResourceManager& res_mgr_;
  std::string name_;
};


/** @brief Style loader that search in a subsection
 *
 * If fallback is true, the loader will search in the parent loader if entry is
 * not found under the given prefix.
 */
class StyleLoaderPrefix: public StyleLoader
{
 public:
  StyleLoaderPrefix(const StyleLoader& loader, const std::string& prefix, bool fallback=false);
  virtual ~StyleLoaderPrefix();

  virtual const ResourceManager& res_mgr() const;
  virtual bool searchStyle(const std::string& prop, std::string& key) const;
  virtual std::string styleErrorSection() const;

 protected:
  const StyleLoader& loader_;
  std::string prefix_;
  bool fallback_;
};


template <class T> T StyleLoader::getStyle(const std::string& prop) const
{
  std::string key;
  if(searchStyle(prop, key)) {
    return res_mgr().style().get<T>(key);
  }
  throw StyleError(*this, prop, "not set");
}

template <class T> T StyleLoader::getStyle(IniFile::Path path) const
{
  return getStyle<T>(IniFile::join(path));
}

template <class T> T StyleLoader::getStyle(const std::string& prop, std::string& key) const
{
  if(searchStyle(prop, key)) {
    return res_mgr().style().get<T>(key);
  }
  throw StyleError(*this, prop, "not set");
}

template <class T> T StyleLoader::getStyle(IniFile::Path path, std::string& key) const
{
  return getStyle<T>(IniFile::join(path), key);
}

template <class T> bool StyleLoader::fetchStyle(const std::string& prop, T& val) const
{
  std::string key;
  if(searchStyle(prop, key)) {
    val = res_mgr().style().get<T>(key);
    return true;
  }
  return false;
}

template <class T> bool StyleLoader::fetchStyle(IniFile::Path path, T& val) const
{
  return fetchStyle<T>(IniFile::join(path), val);
}

template <class T> void StyleLoader::applyStyle(T& o) const
{
  typename StyleType<T>::container style;
  style.load(*this);
  style.apply(o);
}

template <class T> void StyleLoader::applyStyle(T& o, const std::string& prop) const
{
  return StyleLoaderPrefix(*this, prop).applyStyle(o);
}


/** @brief Style for a sf::Text
 *
 * Style properties:
 *  - Font
 *  - FontSize
 *  - FontOutlineThickness
 *  - FontStyle
 *  - FontColor
 *  - FontOutlineColor
 */
struct StyleText
{
  const sf::Font* font = nullptr;
  unsigned int size = 30;
  unsigned int border_width = 0;
  unsigned int text_style = sf::Text::Regular;
  sf::Color color = sf::Color::White;
  sf::Color border_color = sf::Color::Black;

  void load(const StyleLoader& loader);
  void apply(sf::Text& o) const;
};


/** @brief Style for a sf::Sprite
 *
 * Style properties:
 *  - Image
 *  - ImageRect
 */
struct StyleSprite
{
  const sf::Texture* image;
  sf::IntRect rect;

  void load(const StyleLoader& loader);
  void apply(sf::Sprite& o) const;
};


template <> struct StyleType<sf::Text> { typedef StyleText container; };
template <> struct StyleType<sf::Sprite> { typedef StyleSprite container; };


}


#endif
