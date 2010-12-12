#include <memory>
#include <stdexcept>
#include <png.h>
#include "gui/graphics.h"
#include "log.h"


namespace gui {


Image::Image(const std::string &name)
{
  sfc_ = loadPNG(res_path_ + "/" + name + ".png");
}

Image::~Image()
{
  SDL_FreeSurface(sfc_);
}


SDL_Surface *Image::loadPNG(const std::string &fname)
{
  FILE *fp = ::fopen(fname.c_str(), "rb");
  if( fp == NULL ) {
    throw std::runtime_error("cannot open file "+fname);
  }

  unsigned char header[8];
  ::fread(header, 1, sizeof(header), fp);
  if( !png_check_sig(header, sizeof(header)) ) {
    ::fclose(fp);
    throw std::runtime_error("invalid PNG header");
  }


  // libpng init

  png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if( png_ptr == NULL ) {
    fclose(fp);
    throw std::runtime_error("cannot allocate PNG struct");
  }

  png_infop info_ptr = png_create_info_struct(png_ptr);
  if( png_ptr == NULL ) {
    png_destroy_read_struct(&png_ptr, NULL, NULL);
    ::fclose(fp);
    throw std::runtime_error("cannot allocate PNG info struct");
  }


  SDL_Surface *sfc = NULL;

  if( ::setjmp(png_jmpbuf(png_ptr)) ) {
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    if( sfc != NULL ) {
      SDL_FreeSurface(sfc);
    }
    ::fclose(fp);
    throw std::runtime_error("error while reading PNG file");
  }

  png_init_io(png_ptr, fp);
  png_set_sig_bytes(png_ptr, sizeof(header));


  // set libpng transformations
  png_read_info(png_ptr, info_ptr);

  if( png_get_bit_depth(png_ptr, info_ptr) == 16 )
    png_set_strip_16(png_ptr);
  png_set_packing(png_ptr);
  if( png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_PALETTE )
    png_set_palette_to_rgb(png_ptr);
  if( png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS) )
    png_set_tRNS_to_alpha(png_ptr);

  png_read_update_info(png_ptr, info_ptr);

  const png_uint_32 img_w = png_get_image_width(png_ptr, info_ptr);
  const png_uint_32 img_h = png_get_image_height(png_ptr, info_ptr);

  // allocate the SDL_Surface

  sfc = SDL_CreateRGBSurface(SDL_SWSURFACE|SDL_SRCALPHA, img_w, img_h,
      png_get_bit_depth(png_ptr, info_ptr)*png_get_channels(png_ptr, info_ptr),
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
      0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff
#else
      0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000
#endif
      );
  if( sfc == NULL ) {
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    ::fclose(fp);
    throw std::runtime_error(
        std::string("SDL surface creation failed: ") + SDL_GetError());
  }

  // prepare row buffers
  // use an auto_ptr because of setjump/longjump
  std::auto_ptr<png_bytep> a_row_ptrs(
      new png_bytep[img_h * sizeof(png_bytep)]
      );
  png_bytep *row_ptrs = a_row_ptrs.get();
  unsigned int i;
  for( i=0; i<img_h; i++ )
    row_ptrs[i] = (Uint8 *)sfc->pixels + i * sfc->pitch;

  // everything is ready: read
  png_read_image(png_ptr, row_ptrs);
  png_read_end(png_ptr, NULL);

  // clean and return
  row_ptrs = NULL; a_row_ptrs.reset();
  png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
  ::fclose(fp);

  return sfc;
}



Sprite::Sprite(): tex_id_(0), tw_(0), th_(0), dw_(0), dh_(0)
{
}

Sprite &Sprite::initFromImage(const Image &img)
{
  return initFromImage(img, 0, 0, img.w(), img.h());
}

Sprite &Sprite::initFromImage(const Image &img, int x, int y, int w, int h)
{
  assert( ! this->isInitialized() );

  // Create texture
  glPixelStorei(GL_UNPACK_ROW_LENGTH, img.w());
  glPixelStorei(GL_UNPACK_SKIP_PIXELS, x);
  glPixelStorei(GL_UNPACK_SKIP_ROWS, y);
  glGenTextures(1, &tex_id_);
  glBindTexture(GL_TEXTURE_2D, tex_id_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0,
      GL_RGBA, GL_UNSIGNED_BYTE, img.pixels());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

  tw_ = h;
  th_ = w;

  return *this;
}


Sprite &Sprite::setRepeat(bool b)
{
  assert( this->isInitialized() );
  GLuint wrap = b ? GL_REPEAT : GL_CLAMP;
  glBindTexture(GL_TEXTURE_2D, tex_id_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
  return *this;
}

Sprite &Sprite::setSize(float w, float h)
{
  assert( this->isInitialized() );
  dw_ = w;
  dh_ = h;
  return *this;
}

Sprite &Sprite::setZoom(float r)
{
  assert( this->isInitialized() );
  dw_ = tw_ * r;
  dh_ = th_ * r;
  return *this;
}


Sprite::~Sprite()
{
  glDeleteTextures(1, &tex_id_); // tex_id_==0 silently ignored
  tex_id_ = 0;
}


void Sprite::draw(float x, float y, float w, float h) const
{
  const float half_dw = 0.5*dw_*(w<0?-w:w);
  const float half_dh = 0.5*dh_*(h<0?-h:h);
  glBindTexture(GL_TEXTURE_2D, tex_id_);
  glBegin(GL_QUADS);
  glTexCoord2f(x,   y+h); glVertex2f(-half_dw, -half_dh);
  glTexCoord2f(x,   y  ); glVertex2f(-half_dw,  half_dh);
  glTexCoord2f(x+w, y  ); glVertex2f( half_dw,  half_dh);
  glTexCoord2f(x+w, y+h); glVertex2f( half_dw, -half_dh);
  glEnd();
}


void Font::load(const std::string &name, int size)
{
  this->unload();

  const std::string full_name = res_path_ + "/" + name;
  font_ = new FTTextureFont(full_name.c_str());
  if( font_ == NULL ) {
    throw std::runtime_error("failed to init font "+name);
  }
  font_->FaceSize(size);
  // force font preloading
  this->bbox("0");
}


void Font::unload()
{
  delete font_;
  font_ = NULL;
}


FTBBox Font::bbox(const std::string &txt) const
{
  return font_->BBox(txt.c_str(), -1);
}


void Font::render(const std::string &txt) const
{
  font_->Render(txt.c_str(), -1, FTPoint(), FTPoint(), FTGL::RENDER_FRONT);
}

void Font::render(const std::string &txt, float w, float h, Stretch s) const
{
  FTBBox bb = this->bbox(txt);
  const float bb_h = bb.Upper().Y() - bb.Lower().Y();
  const float bb_w = bb.Upper().X() - bb.Lower().X();
  float scale_w = w/bb_w;
  float scale_h = h/bb_h;
  if( scale_w > scale_h && !(s & STRETCH_X) ) {
    scale_w = scale_h;
  } else if( scale_h > scale_w && !(s & STRETCH_Y) ) {
    scale_h = scale_w;
  }
  glPushMatrix();
  //glTranslatef(-scale_w*(bb_w/2+bb.Lower().X()), -scale_h*(bb_h/2+bb.Lower().Y()), 0);
  glScalef( scale_w, scale_h, 1 );
  glTranslatef(-bb_w/2-bb.Lower().X(), -bb_h/2-bb.Lower().Y(), 0);
  this->render(txt);
  glPopMatrix();
}


}

