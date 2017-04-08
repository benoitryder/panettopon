#include <cassert>
#include <stdexcept>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/SoundBuffer.hpp>
#include "resources.h"
#include "style.h"

namespace gui {

static const std::vector<std::string> sound_ext{"wav", "ogg", "flac"};


ResourceManager::ResourceManager()
{
}


void ResourceManager::init(const std::string& path)
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

  const std::string style_path = res_path_+"/style.ini";
  if( !style_.load(style_path.c_str()) ) {
    throw LoadError("failed to load style.ini file");
  }
  // use english as default language
  this->setLang("en");
}


void ResourceManager::setLang(const std::string& lang)
{
  if( lang.empty() || lang.find_first_of("/\\.") != std::string::npos ) {
    throw std::invalid_argument("empty resource path");
  }
  const std::string lang_path = this->getResourceFilename("lang/"+lang+".ini");
  if( !lang_.load(lang_path.c_str()) ) {
    throw LoadError("failed to load language "+lang);
  }
}


std::string ResourceManager::getResourceFilename(const std::string& filename) const
{
  if( res_path_.empty() ) {
    throw LoadError("resource path not set");
  }
  return res_path_+"/"+filename;
}

const sf::Texture& ResourceManager::getImage(const std::string& name) const
{
  const auto it = images_.find(name);
  if( it != images_.end() ) {
    return *(*it).second.get();
  }

  auto img = std::make_shared<sf::Texture>();
  if( ! img->loadFromFile(this->getResourceFilename(name+".png")) ) {
    throw LoadError("failed to load image "+name);
  }
  img->setSmooth(true);
  images_[name] = img;

  return *img;
}

const sf::Font& ResourceManager::getFont(const std::string& name) const
{
  const auto it = fonts_.find(name);
  if( it != fonts_.end() ) {
    return *(*it).second.get();
  }

  auto font = std::make_shared<sf::Font>();
  if( ! font->loadFromFile(this->getResourceFilename(name+".ttf")) ) {
    throw LoadError("failed to load font "+name);
  }
  fonts_[name] = font;

  return *font;
}

const sf::SoundBuffer& ResourceManager::getSound(const std::string& name) const
{
  const auto it = sounds_.find(name);
  if(it != sounds_.end()) {
    return *(*it).second.get();
  }

  auto sound = std::make_shared<sf::SoundBuffer>();
  bool loaded = false;
  for(auto& ext : sound_ext) {
    if(sound->loadFromFile(this->getResourceFilename("sound/"+name+"."+ext))) {
      loaded = true;
      break;
    }
  }
  if(!loaded) {
    throw LoadError("failed to load sound "+name);
  }
  sounds_[name] = sound;

  return *sound;
}

std::string ResourceManager::getLang(const std::string& key) const
{
  return lang_.get<std::string>(key);
}

std::string ResourceManager::getLang(IniFile::Path path) const
{
  return lang_.get<std::string>(path);
}


ImageTile::ImageTile():
    image_(NULL)
{
}

void ImageTile::create(const sf::Texture& img, const sf::IntRect& rect)
{
  image_ = &img;
  rect_ = rect;
}

void ImageTile::create(const sf::Texture& img, int sx, int sy, int x, int y)
{
  assert(img.getSize().x % sx == 0);
  assert(img.getSize().y % sy == 0);
  const int kx = img.getSize().x / sx;
  const int ky = img.getSize().y / sy;
  this->create(img, sf::IntRect(kx*x, ky*y, kx, ky));
}

void ImageTile::render(sf::RenderTarget& target, sf::RenderStates states, float x, float y, float w, float h, const sf::Color& c) const
{
  states.texture = image_;
  const float left   = rect_.left + 0.1;
  const float top    = rect_.top + 0.1;
  const float right  = rect_.left + rect_.width - 0.1;
  const float bottom = rect_.top  + rect_.height - 0.1;

  const sf::Vertex vertices[] = {
    sf::Vertex(sf::Vector2f(x,   y),   c, sf::Vector2f(left,  bottom)),
    sf::Vertex(sf::Vector2f(x+w, y),   c, sf::Vector2f(right, bottom)),
    sf::Vertex(sf::Vector2f(x,   y+h), c, sf::Vector2f(left,  top)),
    sf::Vertex(sf::Vector2f(x+w, y+h), c, sf::Vector2f(right, top)),
  };
  target.draw(vertices, sizeof(vertices)/sizeof(*vertices), sf::TrianglesStrip, states);
}

void ImageTile::render(sf::RenderTarget& target, sf::RenderStates states, float x, float y, const sf::Color& c) const
{
  this->render(target, states, x, y, rect_.width, rect_.height, c);
}

void ImageTile::setToSprite(sf::Sprite& spr, bool center) const
{
  spr.setTexture(*image_);
  spr.setTextureRect(rect_);
  if(center) {
    spr.setOrigin(rect_.width/2., rect_.height/2.);
  }
}


ImageFrame::ImageFrame():
    image_(NULL), color_(sf::Color::White)
{
}

void ImageFrame::create(const sf::Texture& img, const sf::IntRect& rect, const sf::IntRect& inside)
{
  image_ = &img;
  rect_ = rect;
  inside_ = inside;
}

void ImageFrame::render(sf::RenderTarget& target, sf::RenderStates states, const sf::FloatRect& rect) const
{
  states.texture = image_;

  const float img_x0 = rect.left;
  const float img_y0 = rect.top;
  const float img_x3 = rect.left + rect.width;
  const float img_y3 = rect.top  + rect.height;

  const float tex_x0 = rect_.left;
  const float tex_y0 = rect_.top;
  const float tex_x3 = rect_.left + rect_.width;
  const float tex_y3 = rect_.top  + rect_.height;

  const float img_x1 = rect.left + inside_.left;
  const float img_y1 = rect.top  + inside_.top;
  const float img_x2 = rect.left + rect.width  - (rect_.width  - inside_.left - inside_.width);
  const float img_y2 = rect.top  + rect.height - (rect_.height - inside_.top  - inside_.height);

  const float tex_x1 = rect_.left + inside_.left;
  const float tex_y1 = rect_.top + inside_.top;
  const float tex_x2 = rect_.left + inside_.left + inside_.width;
  const float tex_y2 = rect_.top + inside_.top  + inside_.height;

  const sf::Vertex vertices0[] = {
    sf::Vertex(sf::Vector2f(img_x0, img_y0), color_, sf::Vector2f(tex_x0, tex_y3)),
    sf::Vertex(sf::Vector2f(img_x0, img_y1), color_, sf::Vector2f(tex_x0, tex_y2)),
    sf::Vertex(sf::Vector2f(img_x1, img_y0), color_, sf::Vector2f(tex_x1, tex_y3)),
    sf::Vertex(sf::Vector2f(img_x1, img_y1), color_, sf::Vector2f(tex_x1, tex_y2)),
    sf::Vertex(sf::Vector2f(img_x2, img_y0), color_, sf::Vector2f(tex_x2, tex_y3)),
    sf::Vertex(sf::Vector2f(img_x2, img_y1), color_, sf::Vector2f(tex_x2, tex_y2)),
    sf::Vertex(sf::Vector2f(img_x3, img_y0), color_, sf::Vector2f(tex_x3, tex_y3)),
    sf::Vertex(sf::Vector2f(img_x3, img_y1), color_, sf::Vector2f(tex_x3, tex_y2)),
  };
  const sf::Vertex vertices1[] = {
    sf::Vertex(sf::Vector2f(img_x0, img_y1), color_, sf::Vector2f(tex_x0, tex_y2)),
    sf::Vertex(sf::Vector2f(img_x0, img_y2), color_, sf::Vector2f(tex_x0, tex_y1)),
    sf::Vertex(sf::Vector2f(img_x1, img_y1), color_, sf::Vector2f(tex_x1, tex_y2)),
    sf::Vertex(sf::Vector2f(img_x1, img_y2), color_, sf::Vector2f(tex_x1, tex_y1)),
    sf::Vertex(sf::Vector2f(img_x2, img_y1), color_, sf::Vector2f(tex_x2, tex_y2)),
    sf::Vertex(sf::Vector2f(img_x2, img_y2), color_, sf::Vector2f(tex_x2, tex_y1)),
    sf::Vertex(sf::Vector2f(img_x3, img_y1), color_, sf::Vector2f(tex_x3, tex_y2)),
    sf::Vertex(sf::Vector2f(img_x3, img_y2), color_, sf::Vector2f(tex_x3, tex_y1)),
  };
  const sf::Vertex vertices2[] = {
    sf::Vertex(sf::Vector2f(img_x0, img_y2), color_, sf::Vector2f(tex_x0, tex_y1)),
    sf::Vertex(sf::Vector2f(img_x0, img_y3), color_, sf::Vector2f(tex_x0, tex_y0)),
    sf::Vertex(sf::Vector2f(img_x1, img_y2), color_, sf::Vector2f(tex_x1, tex_y1)),
    sf::Vertex(sf::Vector2f(img_x1, img_y3), color_, sf::Vector2f(tex_x1, tex_y0)),
    sf::Vertex(sf::Vector2f(img_x2, img_y2), color_, sf::Vector2f(tex_x2, tex_y1)),
    sf::Vertex(sf::Vector2f(img_x2, img_y3), color_, sf::Vector2f(tex_x2, tex_y0)),
    sf::Vertex(sf::Vector2f(img_x3, img_y2), color_, sf::Vector2f(tex_x3, tex_y1)),
    sf::Vertex(sf::Vector2f(img_x3, img_y3), color_, sf::Vector2f(tex_x3, tex_y0)),
  };

  target.draw(vertices0, sizeof(vertices0)/sizeof(*vertices0), sf::TrianglesStrip, states);
  target.draw(vertices1, sizeof(vertices1)/sizeof(*vertices1), sf::TrianglesStrip, states);
  target.draw(vertices2, sizeof(vertices2)/sizeof(*vertices2), sf::TrianglesStrip, states);
}

void ImageFrame::render(sf::RenderTarget& target, sf::RenderStates states, const sf::Vector2f& size) const
{
  this->render(target, states, sf::FloatRect(-size.x/2, -size.y/2, size.x, size.y));
}


void ImageFrame::Style::load(const StyleLoader& loader)
{
  const ResourceManager& res_mgr = loader.res_mgr();
  std::string key;

  image = &res_mgr.getImage(loader.getStyle<std::string>("Image"));
  if(!loader.fetchStyle("ImageRect", rect)) {
    rect = sf::IntRect(0, 0, image->getSize().x, image->getSize().y);
  }

  inside = loader.getStyle<decltype(inside)>("ImageInside", key);
  if(inside.left < 0 || inside.left+inside.width > rect.width ||
     inside.top < 0 || inside.top+inside.height > rect.height) {
    throw StyleError(key, "image inside not contained in image size");
  }

  loader.fetchStyle<sf::Color>("Color", color);
}

void ImageFrame::Style::apply(ImageFrame& o) const
{
  o.create(*image, rect, inside);
  o.setColor(color);
}


ImageFrameX::ImageFrameX():
    image_(NULL)
{
}

void ImageFrameX::create(const sf::Texture& img, const sf::IntRect& rect, unsigned int inside_left, unsigned int inside_width)
{
  image_ = &img;
  rect_ = rect;
  inside_left_ = inside_left;
  inside_width_ = inside_width;
}

void ImageFrameX::render(sf::RenderTarget& target, sf::RenderStates states, const sf::FloatRect& rect) const
{
  states.texture = image_;

  const float img_x0 = rect.left;
  const float img_y0 = rect.top;
  const float img_x3 = rect.left + rect.width;
  const float img_y1 = rect.top + rect.height;

  const float tex_x0 = rect_.left;
  const float tex_y0 = rect_.top;
  const float tex_x3 = rect_.left + rect_.width;
  const float tex_y1 = rect_.top  + rect_.height;

  const float img_x1 = rect.left + inside_left_;
  const float img_x2 = rect.left + rect.width  - (rect_.width  - inside_left_ - inside_width_);
  const float tex_x1 = rect_.left + inside_left_;
  const float tex_x2 = rect_.left + inside_left_ + inside_width_;

  const sf::Vertex vertices[] = {
    // left
    sf::Vertex(sf::Vector2f(img_x0, img_y0), color_, sf::Vector2f(tex_x0, tex_y1)),
    sf::Vertex(sf::Vector2f(img_x0, img_y1), color_, sf::Vector2f(tex_x0, tex_y0)),
    sf::Vertex(sf::Vector2f(img_x1, img_y0), color_, sf::Vector2f(tex_x1, tex_y1)),
    sf::Vertex(sf::Vector2f(img_x1, img_y1), color_, sf::Vector2f(tex_x1, tex_y0)),
    // middle
    sf::Vertex(sf::Vector2f(img_x2, img_y0), color_, sf::Vector2f(tex_x2, tex_y1)),
    sf::Vertex(sf::Vector2f(img_x2, img_y1), color_, sf::Vector2f(tex_x2, tex_y0)),
    // right
    sf::Vertex(sf::Vector2f(img_x3, img_y0), color_, sf::Vector2f(tex_x3, tex_y1)),
    sf::Vertex(sf::Vector2f(img_x3, img_y1), color_, sf::Vector2f(tex_x3, tex_y0)),
  };
  target.draw(vertices, sizeof(vertices)/sizeof(*vertices), sf::TrianglesStrip, states);
}

void ImageFrameX::render(sf::RenderTarget& target, sf::RenderStates states, float w) const
{
  this->render(target, states, sf::FloatRect(-w/2, -rect_.height/2, w, rect_.height));
}


void ImageFrameX::Style::load(const StyleLoader& loader)
{
  const ResourceManager& res_mgr = loader.res_mgr();
  std::string key;

  image = &res_mgr.getImage(loader.getStyle<std::string>("Image"));
  if(!loader.fetchStyle("ImageRect", rect)) {
    rect = sf::IntRect(0, 0, image->getSize().x, image->getSize().y);
  }

  std::pair<int, int> inside = loader.getStyle<decltype(inside)>("ImageInside", key);
  if(inside.first < 0 || inside.first+inside.second > rect.width) {
    throw StyleError(key, "image inside not contained in image size");
  }
  inside_left = inside.first;
  inside_width = inside.second;

  loader.fetchStyle<sf::Color>("Color", color);
}

void ImageFrameX::Style::apply(ImageFrameX& o) const
{
  o.create(*image, rect, inside_left, inside_width);
  o.setColor(color);
}


SoundPool::SoundPool(): buffer_(nullptr)
{
}

SoundPool::SoundPool(const sf::SoundBuffer& buffer): buffer_(nullptr)
{
  setBuffer(buffer);
}

void SoundPool::setBuffer(const sf::SoundBuffer& buffer)
{
  buffer_ = &buffer;
  for(auto& sound : pool_) {
    sound.setBuffer(buffer);
  }
}

sf::Sound& SoundPool::getSound()
{
  for(auto& sound : pool_) {
    if(sound.getStatus() == sf::SoundSource::Stopped) {
      return sound;
    }
  }
  pool_.emplace_back(*buffer_);
  return pool_.back();
}

void SoundPool::play()
{
  getSound().play();
}


}


sf::Color IniFileConverter<sf::Color>::parse(const std::string& value)
{
  if((value.size() != 7 && value.size() != 9) || value[0] != '#') {
    throw std::invalid_argument("invalid color value");
  }
  char* end = nullptr;
  uint32_t argb = std::strtoul(value.c_str() + 1, &end, 16);
  if(end == 0 || *end != 0) {
    throw std::invalid_argument("invalid color value");
  }
  sf::Color color;
  color.r = (argb >> 16) & 0xff;
  color.g = (argb >> 8) & 0xff;
  color.b = argb & 0xff;
  color.a = value.size() == 7 ? 0xff : (argb >> 24) & 0xff;
  return color;
}

