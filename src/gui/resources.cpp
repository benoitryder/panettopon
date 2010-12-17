#include "util.h"
#include "gui/resources.h"

namespace gui {


void ImageTile::create(const sf::Image &img, const sf::IntRect &rect)
{
  image_ = &img;
  rect_ = rect;
}

void ImageTile::create(const sf::Image &img, int sx, int sy, int x, int y)
{
  //XXX check 'img.GetWidth() % sx == 0' and 'img.GetHeight() % sy == 0'
  const int kx = img.GetWidth()  / sx;
  const int ky = img.GetHeight() / sy;
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

void ImageTile::setToSprite(sf::Sprite &spr, bool center) const
{
  spr.SetImage(*image_);
  spr.SetSubRect(rect_);
  if( center ) {
    spr.SetOrigin( rect_.Width/2., rect_.Height/2. );
  }
}


DisplayRes::DisplayRes():
  font(sf::Font::GetDefaultFont())
{
}

void DisplayRes::load(const std::string &res_path)
{
  //TODO check number of available colors

  // Block tiles (and block size)
  img_bk_color_.LoadFromFile(res_path+"/BkColor-map.png");
  bk_size = img_bk_color_.GetHeight()/5;
  int nb_colors = img_bk_color_.GetWidth()/bk_size;
  tiles_bk_color.resize(nb_colors); // create sprites, uninitialized
  for(int i=0; i<nb_colors; i++ ) {
    TilesBkColor &tiles = tiles_bk_color[i];
    tiles.normal.create(img_bk_color_, nb_colors, 5, i, 0);
    tiles.bg    .create(img_bk_color_, nb_colors, 5, i, 1);
    tiles.face  .create(img_bk_color_, nb_colors, 5, i, 2);
    tiles.flash .create(img_bk_color_, nb_colors, 5, i, 3);
    tiles.mutate.create(img_bk_color_, nb_colors, 5, i, 4);
  }

  // Garbages
  img_bk_gb_.LoadFromFile(res_path+"/BkGarbage-map.png");
  for(int x=0; x<4; x++) {
    for(int y=0; y<4; y++) {
      tiles_gb.tiles[x][y].create(img_bk_gb_, 8, 4, x, y);
    }
  }
  //XXX center: setRepeat(true)
  for(int x=0; x<2; x++) {
    for(int y=0; y<2; y++) {
      tiles_gb.center[x][y].create(img_bk_gb_, 8, 4, 4+x, y);
    }
  }
  tiles_gb.mutate.create(img_bk_gb_, 4, 2, 3, 0);
  tiles_gb.flash .create(img_bk_gb_, 4, 2, 3, 1);

  // Frame
  img_field_frame.LoadFromFile(res_path+"/Field-Frame.png");

  // Cursor
  img_cursor_.LoadFromFile(res_path+"/SwapCursor.png");
  tiles_cursor[0].create(img_cursor_, 1, 2, 0, 0);
  tiles_cursor[1].create(img_cursor_, 1, 2, 0, 1);

  // Labels
  img_labels_.LoadFromFile(res_path+"/Labels.png");
  tiles_labels.combo.create(img_labels_, 2, 1, 0, 0);
  tiles_labels.chain.create(img_labels_, 2, 1, 1, 0);

  // Waiting garbages
  img_waiting_gb_.LoadFromFile(res_path+"/WaitingGb-map.png");
  const size_t waiting_gb_sx = FIELD_WIDTH/2; // on 2 rows
  for(int i=0; i<FIELD_WIDTH; i++) {
    tiles_waiting_gb.blocks[i].create(img_waiting_gb_, waiting_gb_sx+1, 2, i%waiting_gb_sx, i/waiting_gb_sx);
  }
  tiles_waiting_gb.line.create(img_waiting_gb_, waiting_gb_sx+1, 2, waiting_gb_sx, 0);
}


}

