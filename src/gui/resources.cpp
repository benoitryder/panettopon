#include <cassert>
#include <stdexcept>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include "resources.h"

namespace gui {


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
    throw std::runtime_error("failed to load style.ini file");
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
    throw std::runtime_error("failed to load language "+lang);
  }
}


std::string ResourceManager::getResourceFilename(const std::string& filename) const
{
  if( res_path_.empty() ) {
    throw std::runtime_error("resource path not set");
  }
  return res_path_+"/"+filename;
}

const sf::Texture* ResourceManager::getImage(const std::string& name)
{
  ImageContainer::iterator it = images_.find(name);
  if( it != images_.end() ) {
    return (*it).second.get();
  }

  sf::Texture* img = new sf::Texture;
  std::shared_ptr<sf::Texture> pimg(img);
  if( ! img->loadFromFile(this->getResourceFilename(name+".png")) ) {
    throw std::runtime_error("failed to load image "+name);
  }
  images_[name] = pimg;

  return img;
}

const sf::Font* ResourceManager::getFont(const std::string& name)
{
  FontContainer::iterator it = fonts_.find(name);
  if( it != fonts_.end() ) {
    return (*it).second.get();
  }

  sf::Font* font = new sf::Font;
  std::shared_ptr<sf::Font> pfont(font);
  if( ! font->loadFromFile(this->getResourceFilename(name+".ttf")) ) {
    throw std::runtime_error("failed to load font "+name);
  }
  fonts_[name] = pfont;

  return font;
}

std::string ResourceManager::getLang(const std::string& section, const std::string& key) const
{
  return lang_.get<std::string>(section, key);
}


ImageTile::ImageTile():
    image_(NULL)
{
}

void ImageTile::create(const sf::Texture* img, const sf::IntRect& rect)
{
  image_ = img;
  rect_ = rect;
}

void ImageTile::create(const sf::Texture* img, int sx, int sy, int x, int y)
{
  assert( img->getSize().x % sx == 0 );
  assert( img->getSize().y % sy == 0 );
  const int kx = img->getSize().x / sx;
  const int ky = img->getSize().y / sy;
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

void ImageTile::setToSprite(sf::Sprite* spr, bool center) const
{
  spr->setTexture(*image_);
  spr->setTextureRect(rect_);
  if( center ) {
    spr->setOrigin( rect_.width/2., rect_.height/2. );
  }
}


void ImageFrame::Style::apply(ImageFrame& o)
{
  o.create(image, rect, inside);
}

void ImageFrameX::Style::apply(ImageFrameX& o)
{
  o.create(image, rect, inside_left, inside_width);
}


ImageFrame::ImageFrame():
    image_(NULL)
{
}

void ImageFrame::create(const sf::Texture* img, const sf::IntRect& rect, const sf::IntRect& inside)
{
  image_ = img;
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
    sf::Vertex(sf::Vector2f(img_x0, img_y0), sf::Vector2f(tex_x0, tex_y3)),
    sf::Vertex(sf::Vector2f(img_x0, img_y1), sf::Vector2f(tex_x0, tex_y2)),
    sf::Vertex(sf::Vector2f(img_x1, img_y0), sf::Vector2f(tex_x1, tex_y3)),
    sf::Vertex(sf::Vector2f(img_x1, img_y1), sf::Vector2f(tex_x1, tex_y2)),
    sf::Vertex(sf::Vector2f(img_x2, img_y0), sf::Vector2f(tex_x2, tex_y3)),
    sf::Vertex(sf::Vector2f(img_x2, img_y1), sf::Vector2f(tex_x2, tex_y2)),
    sf::Vertex(sf::Vector2f(img_x3, img_y0), sf::Vector2f(tex_x3, tex_y3)),
    sf::Vertex(sf::Vector2f(img_x3, img_y1), sf::Vector2f(tex_x3, tex_y2)),
  };
  const sf::Vertex vertices1[] = {
    sf::Vertex(sf::Vector2f(img_x0, img_y1), sf::Vector2f(tex_x0, tex_y2)),
    sf::Vertex(sf::Vector2f(img_x0, img_y2), sf::Vector2f(tex_x0, tex_y1)),
    sf::Vertex(sf::Vector2f(img_x1, img_y1), sf::Vector2f(tex_x1, tex_y2)),
    sf::Vertex(sf::Vector2f(img_x1, img_y2), sf::Vector2f(tex_x1, tex_y1)),
    sf::Vertex(sf::Vector2f(img_x2, img_y1), sf::Vector2f(tex_x2, tex_y2)),
    sf::Vertex(sf::Vector2f(img_x2, img_y2), sf::Vector2f(tex_x2, tex_y1)),
    sf::Vertex(sf::Vector2f(img_x3, img_y1), sf::Vector2f(tex_x3, tex_y2)),
    sf::Vertex(sf::Vector2f(img_x3, img_y2), sf::Vector2f(tex_x3, tex_y1)),
  };
  const sf::Vertex vertices2[] = {
    sf::Vertex(sf::Vector2f(img_x0, img_y2), sf::Vector2f(tex_x0, tex_y1)),
    sf::Vertex(sf::Vector2f(img_x0, img_y3), sf::Vector2f(tex_x0, tex_y0)),
    sf::Vertex(sf::Vector2f(img_x1, img_y2), sf::Vector2f(tex_x1, tex_y1)),
    sf::Vertex(sf::Vector2f(img_x1, img_y3), sf::Vector2f(tex_x1, tex_y0)),
    sf::Vertex(sf::Vector2f(img_x2, img_y2), sf::Vector2f(tex_x2, tex_y1)),
    sf::Vertex(sf::Vector2f(img_x2, img_y3), sf::Vector2f(tex_x2, tex_y0)),
    sf::Vertex(sf::Vector2f(img_x3, img_y2), sf::Vector2f(tex_x3, tex_y1)),
    sf::Vertex(sf::Vector2f(img_x3, img_y3), sf::Vector2f(tex_x3, tex_y0)),
  };

  target.draw(vertices0, sizeof(vertices0)/sizeof(*vertices0), sf::TrianglesStrip, states);
  target.draw(vertices1, sizeof(vertices1)/sizeof(*vertices1), sf::TrianglesStrip, states);
  target.draw(vertices2, sizeof(vertices2)/sizeof(*vertices2), sf::TrianglesStrip, states);
}

void ImageFrame::render(sf::RenderTarget& target, sf::RenderStates states, const sf::Vector2f& size) const
{
  this->render(target, states, sf::FloatRect(-size.x/2, -size.y/2, size.x, size.y));
}


ImageFrameX::ImageFrameX():
    image_(NULL)
{
}

void ImageFrameX::create(const sf::Texture* img, const sf::IntRect& rect, unsigned int inside_left, unsigned int inside_width)
{
  image_ = img;
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
  const float tex_x1 = (float)inside_left_/rect_.width * rect_.width;
  const float tex_x2 = (float)(inside_left_+inside_width_)/rect_.width * rect_.width;

  const sf::Vertex vertices[] = {
    // left
    sf::Vertex(sf::Vector2f(img_x0, img_y0), sf::Vector2f(tex_x0, tex_y1)),
    sf::Vertex(sf::Vector2f(img_x0, img_y1), sf::Vector2f(tex_x0, tex_y0)),
    sf::Vertex(sf::Vector2f(img_x1, img_y0), sf::Vector2f(tex_x1, tex_y1)),
    sf::Vertex(sf::Vector2f(img_x1, img_y1), sf::Vector2f(tex_x1, tex_y0)),
    // middle
    sf::Vertex(sf::Vector2f(img_x2, img_y0), sf::Vector2f(tex_x2, tex_y1)),
    sf::Vertex(sf::Vector2f(img_x2, img_y1), sf::Vector2f(tex_x2, tex_y0)),
    // right
    sf::Vertex(sf::Vector2f(img_x3, img_y0), sf::Vector2f(tex_x3, tex_y1)),
    sf::Vertex(sf::Vector2f(img_x3, img_y1), sf::Vector2f(tex_x3, tex_y0)),
  };
  target.draw(vertices, sizeof(vertices)/sizeof(*vertices), sf::TrianglesStrip, states);
}

void ImageFrameX::render(sf::RenderTarget& target, sf::RenderStates states, float w) const
{
  this->render(target, states, sf::FloatRect(-w/2, -rect_.height/2, w, rect_.height));
}


void StyleText::apply(sf::Text& o)
{
  if(!font) {
    throw std::runtime_error("text style font not set");
  }
  o.setFont(*font);
  o.setCharacterSize(size);
  o.setBorderWidth(border_width);
  o.setStyle(style);
  o.setColor(color);
  o.setBorderColor(border_color);
}

void StyleSprite::apply(sf::Sprite& o)
{
  o.setTexture(*image, true);
  o.setTextureRect(rect);
}


Stylable::StyleError::StyleError(const std::string& prop, const std::string& msg):
  std::runtime_error("style error for "+prop+": "+msg) {}

Stylable::StyleError::StyleError(const Stylable& stylable, const std::string& prop, const std::string& msg):
  std::runtime_error("style error for "+stylable.styleErrorSection()+"."+prop+": "+msg) {}


Stylable::Stylable(ResourceManager& res_mgr):
  res_mgr_(res_mgr)
{
}

Stylable::~Stylable() {}

void Stylable::loadStyle(StyleText& style, const std::string prefix) const
{
  const IniFile& ini = res_mgr_.style();
  std::string key;

  if(searchStyle(prefix+"Font", &key)) {
    style.font = res_mgr_.getFont(ini.get<std::string>(key));
  } else if(!style.font) {
    throw StyleError(*this, prefix+"Font", "not set");
  }

  if(searchStyle(prefix+"FontSize", &key)) {
    style.size = ini.get<unsigned int>(key);
  }
  if(searchStyle(prefix+"FontBorderWidth", &key)) {
    style.border_width = ini.get<unsigned int>(key);
  }

  if(searchStyle(prefix+"FontStyle", &key)) {
    const std::string val = ini.get<std::string>(key);
    unsigned int txt_style;
    if( val == "regular" ) {
      txt_style = sf::Text::Regular;
    } else if( val == "bold" ) {
      txt_style = sf::Text::Bold;
    } else if( val == "italic" ) {
      txt_style = sf::Text::Italic;
    } else if( val == "bold,italic" || val == "italic,bold" ) {
      txt_style = sf::Text::Bold|sf::Text::Italic;
    } else {
      throw StyleError(key, "invalid value");
    }
    style.style = txt_style;
  }

  if(searchStyle(prefix+"FontColor", &key)) {
    style.color = ini.get<sf::Color>(key);
  }
  if(searchStyle(prefix+"FontBorderColor", &key)) {
    style.border_color = ini.get<sf::Color>(key);
  }
}

void Stylable::loadStyle(ImageFrame::Style& style, const std::string prefix) const
{
  const IniFile& ini = res_mgr_.style();
  std::string key;

  if(searchStyle(prefix+"Image", &key)) {
    style.image = res_mgr_.getImage(ini.get<std::string>(key));
    if(searchStyle(prefix+"ImageRect", &key)) {
      style.rect = ini.get<sf::IntRect>(key);
    } else {
      style.rect = sf::IntRect(0, 0, style.image->getSize().x, style.image->getSize().y);
    }
    if(searchStyle(prefix+"ImageInside", &key)) {
      sf::IntRect inside = ini.get<sf::IntRect>(key);
      if(inside.left < 0 || inside.left+inside.width > style.rect.width ||
         inside.top < 0 || inside.top+inside.height > style.rect.height) {
        throw StyleError(key, "image inside not contained in image size");
      }
      style.inside = inside;
    } else {
      throw StyleError(*this, prefix+"ImageInside", "not set");
    }
  } else if(!style.image) {
    throw StyleError(*this, prefix+"Image", "not set");
  }
}

void Stylable::loadStyle(ImageFrameX::Style& style, const std::string prefix) const
{
  const IniFile& ini = res_mgr_.style();
  std::string key;

  if(searchStyle(prefix+"Image", &key)) {
    style.image = res_mgr_.getImage(ini.get<std::string>(key));
    if(searchStyle(prefix+"ImageRect", &key)) {
      style.rect = ini.get<sf::IntRect>(key);
    } else {
      style.rect = sf::IntRect(0, 0, style.image->getSize().x, style.image->getSize().y);
    }
    if(searchStyle(prefix+"ImageInside", &key)) {
      std::pair<int, int> inside(0,0);
      inside = ini.get<decltype(inside)>(key);
      if(inside.first < 0 || inside.first+inside.second > style.rect.width) {
        throw StyleError(key, "image inside not contained in image size");
      }
      style.inside_left = inside.first;
      style.inside_width = inside.second;
    } else {
      throw StyleError(*this, prefix+"ImageInside", "not set");
    }
  } else if(!style.image) {
    throw StyleError(*this, prefix+"Image", "not set");
  }
}

void Stylable::loadStyle(StyleSprite& style, const std::string prefix) const
{
  const IniFile& ini = res_mgr_.style();
  std::string key;

  if(searchStyle(prefix+"Image", &key)) {
    style.image = res_mgr_.getImage(ini.get<std::string>(key));
    if(searchStyle(prefix+"ImageRect", &key)) {
      style.rect = ini.get<sf::IntRect>(key);
    } else {
      style.rect = sf::IntRect(0, 0, style.image->getSize().x, style.image->getSize().y);
    }
  } else {
    throw StyleError(*this, prefix+"Image", "not set");
  }
}


StyleField::StyleField(ResourceManager& res_mgr):
    Stylable(res_mgr),
    bk_size(0), img_field_frame(NULL)
{
}

void StyleField::load(const std::string& section)
{
  style_section_ = section;
  const IniFile& style = res_mgr_.style();

  colors.push_back(style.get<sf::Color>(section, "Color.Neutral"));
  for(unsigned int i=1; i<=16; ++i) {
    const std::string key = "Color." + std::to_string(i);
    if(!style.has(section, key)) {
      break;
    }
    colors.push_back(style.get<sf::Color>(section, key));
  }
  unsigned int color_nb = colors.size() - 1;
  if( color_nb < 4 ) {
    throw std::runtime_error("ColorNb is too small, must be at least 4");
  }

  const sf::Texture* img;

  // Block tiles (and block size)
  img = res_mgr_.getImage("BkColor-map");
  if( img->getSize().x % color_nb != 0 || img->getSize().y % 5 != 0 ) {
    throw std::runtime_error("block map size does not match tile count");
  }
  bk_size = img->getSize().y/5;
  tiles_bk_color.resize(color_nb); // create sprites, uninitialized
  for(unsigned int i=0; i<color_nb; i++ ) {
    TilesBkColor& tiles = tiles_bk_color[i];
    tiles.normal.create(img, color_nb, 5, i, 0);
    tiles.bg    .create(img, color_nb, 5, i, 1);
    tiles.face  .create(img, color_nb, 5, i, 2);
    tiles.flash .create(img, color_nb, 5, i, 3);
    tiles.mutate.create(img, color_nb, 5, i, 4);
  }

  // Garbages
  img = res_mgr_.getImage("BkGarbage-map");
  for(int x=0; x<4; x++) {
    for(int y=0; y<4; y++) {
      tiles_gb.tiles[x][y].create(img, 8, 4, x, y);
    }
  }
  //XXX center: setRepeat(true)
  for(int x=0; x<2; x++) {
    for(int y=0; y<2; y++) {
      tiles_gb.center[x][y].create(img, 8, 4, 4+x, y);
    }
  }
  tiles_gb.mutate.create(img, 4, 2, 3, 0);
  tiles_gb.flash .create(img, 4, 2, 3, 1);

  // Frame
  img_field_frame = res_mgr_.getImage("Field-Frame");
  frame_origin = style.get<sf::Vector2f>(section, "FrameOrigin");

  // Cursor
  img = res_mgr_.getImage("SwapCursor");
  tiles_cursor[0].create(img, 1, 2, 0, 0);
  tiles_cursor[1].create(img, 1, 2, 0, 1);

  // Signs
  img = res_mgr_.getImage("Signs");
  tiles_signs.combo.create(img, 2, 1, 0, 0);
  tiles_signs.chain.create(img, 2, 1, 1, 0);

  // Hanging garbages
  img = res_mgr_.getImage("GbHanging-map");
  const size_t gb_hanging_sx = FIELD_WIDTH/2; // on 2 rows
  for(int i=0; i<FIELD_WIDTH; i++) {
    tiles_gb_hanging.blocks[i].create(img, gb_hanging_sx+1, 2, i%gb_hanging_sx, i/gb_hanging_sx);
  }
  tiles_gb_hanging.line.create(img, gb_hanging_sx+1, 2, gb_hanging_sx, 0);
}


bool StyleField::searchStyle(const std::string& prop, std::string* key) const
{
  if( res_mgr_.style().has(style_section_, prop) ) {
    *key = style_section_+'.'+prop;
    return true;
  }
  return false;
}


}


sf::Color IniFileConverter<sf::Color>::parse(const std::string& value)
{
  sf::Color color;
  char c;
  std::string s;
  std::istringstream in(value);
  in >> c >> s;
  if( in && c == '#' && (s.size() == 6 || s.size() == 8) ) {
    uint32_t argb;
    std::istringstream iss(s);
    iss >> std::hex >> argb;
    if( iss ) {
      color.r = (argb>>16) & 0xff;
      color.g = (argb>>8) & 0xff;
      color.b = argb & 0xff;
      color.a = s.size() == 6 ? 0xff : (argb>>24) & 0xff;
    }
  }
  return color;
}

