#include "util.h"
#include "gui/resources.h"

namespace gui {


void FieldTile::create(const sf::Image &img, const sf::FloatRect &rect)
{
  image_ = &img;
  rect_ = rect;
}

void FieldTile::create(const sf::Image &img, int nx, int x, int y)
{
  int ny = img.GetHeight() / (img.GetWidth() / nx);
  float kx = 1./nx;
  float ky = 1./ny;
  this->create(img, sf::FloatRect(kx*x, ky*y, kx, ky));
}

void FieldTile::render(sf::Renderer &renderer, int x, int y) const
{
  this->render(renderer, sf::FloatRect(x, y, 1, 1));
}

void FieldTile::render(sf::Renderer &renderer, const sf::FloatRect &pos) const
{
  renderer.SetTexture(image_);
  renderer.Begin(sf::Renderer::TriangleStrip);
    renderer.AddVertex(pos.Left,           pos.Top,            rect_.Left,             rect_.Top+rect_.Height);
    renderer.AddVertex(pos.Left+pos.Width, pos.Top,            rect_.Left+rect_.Width, rect_.Top+rect_.Height);
    renderer.AddVertex(pos.Left,           pos.Top+pos.Height, rect_.Left,             rect_.Top);
    renderer.AddVertex(pos.Left+pos.Width, pos.Top+pos.Height, rect_.Left+rect_.Width, rect_.Top);
  renderer.End();
  /*XXX:temp
  glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(rect_.Left,             rect_.Top+rect_.Height); glVertex2f(pos.Left,           pos.Top);
    glTexCoord2f(rect_.Left+rect_.Width, rect_.Top+rect_.Height); glVertex2f(pos.Left+pos.Width, pos.Top);
    glTexCoord2f(rect_.Left,             rect_.Top);              glVertex2f(pos.Left,           pos.Top+pos.Height);
    glTexCoord2f(rect_.Left+rect_.Width, rect_.Top);              glVertex2f(pos.Left+pos.Width, pos.Top+pos.Height);
  glEnd();
  */
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
    tiles.normal.create(img_bk_color_, nb_colors, i, 0);
    tiles.bg    .create(img_bk_color_, nb_colors, i, 1);
    tiles.face  .create(img_bk_color_, nb_colors, i, 2);
    tiles.flash .create(img_bk_color_, nb_colors, i, 3);
    tiles.mutate.create(img_bk_color_, nb_colors, i, 4);
  }

  // Garbages
  img_bk_gb_.LoadFromFile(res_path+"/BkGarbage-map.png");
  for(int x=0; x<4; x++) {
    for(int y=0; y<4; y++) {
      tiles_gb.tiles[x][y].create(img_bk_gb_, 8, x, y);
    }
  }
  //XXX center: setRepeat(true)
  for(int x=0; x<2; x++) {
    for(int y=0; y<2; y++) {
      tiles_gb.center[x][y].create(img_bk_gb_, 8, 4+x, y);
    }
  }
  tiles_gb.mutate.create(img_bk_gb_, 4, 3, 0);
  tiles_gb.flash .create(img_bk_gb_, 4, 3, 1);

  img_field_frame.LoadFromFile(res_path+"/Field-Frame.png");
  img_cursor.LoadFromFile(res_path+"/SwapCursor.png");
  img_labels.LoadFromFile(res_path+"/Labels.png");

#if 0
  {
    Image img("WaitingGb-map");
    const size_t nb_w = FIELD_WIDTH/2;
    const int tw = img.w()/(nb_w+1);
    const int th = img.h()/2;

    for( unsigned int i=0; i<FIELD_WIDTH; i++) {
      spr_waiting_gb.blocks[i].initFromImage(img, (i%nb_w)*tw, (i/nb_w)*th, tw, th).setSize(1.5,1);
    }
    spr_waiting_gb.line.initFromImage(img, nb_w*tw, 0, tw, th).setSize(1.5,1);
  }
#endif
}


}

