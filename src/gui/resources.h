#ifndef GUI_RESOURCES_H_
#define GUI_RESOURCES_H_

#include <map>
#include <string>
#include <SFML/Graphics/Image.hpp>

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


}

#endif
