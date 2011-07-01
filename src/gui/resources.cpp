#include <cassert>
#include <stdexcept>
#include <SFML/Graphics/Renderer.hpp>
#include <SFML/Graphics/Sprite.hpp>
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



ImageTile::ImageTile():
    image_(NULL)
{
}

void ImageTile::create(const sf::Image *img, const sf::IntRect &rect)
{
  image_ = img;
  rect_ = rect;
}

void ImageTile::create(const sf::Image *img, int sx, int sy, int x, int y)
{
  assert( img->GetWidth() % sx == 0 );
  assert( img->GetHeight() % sy == 0 );
  const int kx = img->GetWidth()  / sx;
  const int ky = img->GetHeight() / sy;
  this->create(img, sf::IntRect(kx*x, ky*y, kx, ky));
}

void ImageTile::render(sf::Renderer &renderer, float x, float y, float w, float h) const
{
  sf::FloatRect coords = image_->GetTexCoords(rect_);
  const float left   = coords.Left;
  const float top    = coords.Top;
  const float right  = coords.Left + coords.Width;
  const float bottom = coords.Top  + coords.Height;

  renderer.SetTexture(image_);
  renderer.Begin(sf::Renderer::TriangleStrip);
    renderer.AddVertex(x,   y,   left,  bottom);
    renderer.AddVertex(x+w, y,   right, bottom);
    renderer.AddVertex(x,   y+h, left,  top);
    renderer.AddVertex(x+w, y+h, right, top);
  renderer.End();
}

void ImageTile::setToSprite(sf::Sprite *spr, bool center) const
{
  spr->SetImage(*image_);
  spr->SetSubRect(rect_);
  if( center ) {
    spr->SetOrigin( rect_.Width/2., rect_.Height/2. );
  }
}



ResField::ResField():
    bk_size(0), img_field_frame(NULL)
{
}

void ResField::load(ResourceManager *res_mgr)
{
  const sf::Image *img;

  // Block tiles (and block size)
  img = res_mgr->getImage("BkColor-map");
  bk_size = img->GetHeight()/5;
  int nb_colors = img->GetWidth()/bk_size;
  tiles_bk_color.resize(nb_colors); // create sprites, uninitialized
  for(int i=0; i<nb_colors; i++ ) {
    TilesBkColor &tiles = tiles_bk_color[i];
    tiles.normal.create(img, nb_colors, 5, i, 0);
    tiles.bg    .create(img, nb_colors, 5, i, 1);
    tiles.face  .create(img, nb_colors, 5, i, 2);
    tiles.flash .create(img, nb_colors, 5, i, 3);
    tiles.mutate.create(img, nb_colors, 5, i, 4);
  }

  // Garbages
  img = res_mgr->getImage("BkGarbage-map");
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
  img_field_frame = res_mgr->getImage("Field-Frame");

  // Cursor
  img = res_mgr->getImage("SwapCursor");
  tiles_cursor[0].create(img, 1, 2, 0, 0);
  tiles_cursor[1].create(img, 1, 2, 0, 1);

  // Labels
  img = res_mgr->getImage("Labels");
  tiles_labels.combo.create(img, 2, 1, 0, 0);
  tiles_labels.chain.create(img, 2, 1, 1, 0);

  // Hanging garbages
  img = res_mgr->getImage("GbHanging-map");
  const size_t gb_hanging_sx = FIELD_WIDTH/2; // on 2 rows
  for(int i=0; i<FIELD_WIDTH; i++) {
    tiles_gb_hanging.blocks[i].create(img, gb_hanging_sx+1, 2, i%gb_hanging_sx, i/gb_hanging_sx);
  }
  tiles_gb_hanging.line.create(img, gb_hanging_sx+1, 2, gb_hanging_sx, 0);
}


}

