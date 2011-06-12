#include <cassert>
#include <stdexcept>
#include "resources.h"

namespace gui {


ResourceManager::ResourceManager()
{
}


void ResourceManager::init(const std::string &path)
{
  if( !res_path_.empty() ) {
    throw std::runtime_error("resource path already set");
  }
  if( path.empty() ) {
    throw std::invalid_argument("empty resource path");
  }

  res_path_ = path;
  // strip trailing '/' (or '\' on Windows)
#ifdef WIN32
  size_t p = res_path_.find_last_not_of("/\\");
#else
  size_t p = res_path_.find_last_not_of('/');
#endif
  if( p != std::string::npos ) {
    res_path_ = res_path_.substr(0, p+1);
  }
}


const sf::Image *ResourceManager::getImage(const std::string &name)
{
  assert( ! res_path_.empty() );

  ImageContainer::iterator it = images_.find(name);
  if( it != images_.end() ) {
    return (*it).second;
  }

  sf::Image *img = new sf::Image;
  sf::ResourcePtr<sf::Image> pimg(img);
  if( ! img->LoadFromFile(res_path_+"/"+name+".png") ) {
    throw std::runtime_error("Failed to load image: "+name);
  }
  images_[name] = pimg;

  return pimg;
}


}

