#include <cstdio>
#include <boost/bind.hpp>
#include <boost/asio/placeholders.hpp>
#include "config.h"
#include "log.h"
#include "gui/intf.h"
#include "gui/icon.h"

namespace asio = boost::asio;

namespace gui {

const std::string GuiInterface::CONF_SECTION("GUI");


GuiInterface::GuiInterface(): client_(*this, io_service_),
    redraw_timer_(io_service_)
{
  keys_.up    = sf::Key::Up;
  keys_.down  = sf::Key::Down;
  keys_.left  = sf::Key::Left;
  keys_.right = sf::Key::Right;
  keys_.swap  = sf::Key::Code('d');
  keys_.raise = sf::Key::Code('f');
  keys_.quit  = sf::Key::Code('q');

  conf_.redraw_dt = (1000.0/60.0);
  conf_.fullscreen = false;
  conf_.screen_width = 800;
  conf_.screen_height = 600;
  conf_.zoom = 0.3;
}

GuiInterface::~GuiInterface()
{
  this->endDisplay();
}


bool GuiInterface::run(const Config &cfg)
{
#define CONF_LOAD_KEY(n,ini) do{ \
  const std::string s = cfg.get(CONF_SECTION, #ini, ""); \
  if( s.empty() ) break; \
  sf::Key::Code key = str2key(s); \
  if( key == 0 ) \
    LOG("invalid conf. value for %s: %s", #ini, s.c_str()); \
  else \
    keys_.n = key; \
}while(0)
  CONF_LOAD_KEY(up,    KeyUp   );
  CONF_LOAD_KEY(down,  KeyDown );
  CONF_LOAD_KEY(left,  KeyLeft );
  CONF_LOAD_KEY(right, KeyRight);
  CONF_LOAD_KEY(swap,  KeySwap );
  CONF_LOAD_KEY(raise, KeyRaise);
  CONF_LOAD_KEY(quit,  KeyQuit );
#undef CONF_LOAD_KEY

#define CONF_LOAD(n,ini,t) \
  conf_.n = cfg.get##t(CONF_SECTION, #ini, conf_.n);
  CONF_LOAD(fullscreen,    Fullscreen,   Bool);
  CONF_LOAD(screen_width,  ScreenWidth,  Int);
  CONF_LOAD(screen_height, ScreenHeight, Int);
  CONF_LOAD(zoom,          Zoom,         Double);
#undef CONF_LOAD
  float f = cfg.getDouble(CONF_SECTION, "FPS", 60.0);
  //XXX default not based on the cfg value set in constructor
  if( f <= 0 ) {
    LOG("invalid conf. value for FPS: %f", f);
  } else {
    conf_.redraw_dt = (1000.0/f);
  }


  int port = cfg.getInt("Global", "Port", 20102);
  const std::string host = cfg.get("Client", "Hostname", "localhost");

  conf_.res_path = cfg.get(CONF_SECTION, "ResPath", "./res");
  // strip trailing '/' (or '\' on Windows)
#ifdef WIN32
  size_t p = conf_.res_path.find_last_not_of("/\\");
#else
  size_t p = conf_.res_path.find_last_not_of('/');
#endif
  if( p != std::string::npos ) {
    conf_.res_path = conf_.res_path.substr(0, p+1);
  }

  // start display loop
  if( !this->initDisplay() ) {
    LOG("display initialization failed");
    this->endDisplay();
    return false;
  }
  boost::posix_time::time_duration dt = boost::posix_time::milliseconds(conf_.redraw_dt);
  redraw_timer_.expires_from_now(dt);
  redraw_timer_.async_wait(boost::bind(&GuiInterface::onRedrawTick, this,
                                       asio::placeholders::error));

  client_.connect(host.c_str(), port, 3000);
  io_service_.run();
  return true;
}


bool GuiInterface::initDisplay()
{
  //TODO handle errors/exceptions
  window_.Create(
      sf::VideoMode(conf_.screen_width, conf_.screen_height, 32),
      "Panettopon",
      conf_.fullscreen ? sf::Style::Fullscreen : sf::Style::Resize|sf::Style::Close
      );
  window_.SetIcon(icon.width, icon.height, icon.data);
  window_.EnableKeyRepeat(false);
  //window_.PreserveOpenGLStates();

  window_.GetDefaultView().Zoom(conf_.zoom);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  /*TODO:check
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, window_.GetWidth(), 0, window_.GetHeight(), -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  //glTranslatef(0.5, 0.5, 0);
  */

  // Load resources
  {
    try {
      res_.load(conf_.res_path);
    } catch(const std::exception &e) {
      LOG("failed to load resources: %s", e.what());
      return false;
    }
  }

  return true;
}

void GuiInterface::endDisplay()
{
  window_.Close();
}

void GuiInterface::redraw()
{
  /* TODO:check
  glClear(GL_COLOR_BUFFER_BIT);
  glEnable(GL_TEXTURE_2D);
  */
  //TODO draw fields only during match

  window_.Clear(sf::Color(48,48,48));

  glPushMatrix();

  FieldDisplayMap::iterator it;
  for( it=fdisplays_.begin(); it!=fdisplays_.end(); ++it ) {
    window_.Draw(*(*it).second);
  }

  glPopMatrix();

  window_.Display();
}


void GuiInterface::onRedrawTick(const boost::system::error_code &ec)
{
  if( ec == asio::error::operation_aborted )
    return;

  // process SDL events
  if( ! window_.IsOpened() ) {
    //TODO
    io_service_.stop();
    return;
  }

  sf::Event event;
  while(window_.GetEvent(event)) {
    if( event.Type == sf::Event::Closed ||
       (event.Type == sf::Event::KeyPressed && event.Key.Code == keys_.quit)
      ) {
      window_.Close();
      io_service_.stop(); //XXX
    }
  }

  this->redraw();

  //TODO use a thread and SFML timing methods
  boost::posix_time::time_duration dt = boost::posix_time::milliseconds(conf_.redraw_dt);
  redraw_timer_.expires_from_now(dt);
  redraw_timer_.async_wait(boost::bind(&GuiInterface::onRedrawTick, this,
                                       asio::placeholders::error));
}


void GuiInterface::addMessage(int color, const char *fmt, ...)
{
  //TODO
  (void)color; (void)fmt;
}

sf::Key::Code GuiInterface::str2key(const std::string &s)
{
  if( s.empty() )
    return sf::Key::Code(0);
  if( s.size() == 1 ) {
    int val = s[0];
    if( val < ' ' || val > '~' )
      return sf::Key::Code(0);
    return sf::Key::Code(val | 0x20); // lowercase
  }
  if( s == "up" )
    return sf::Key::Up;
  if( s == "down" )
    return sf::Key::Down;
  if( s == "left" )
    return sf::Key::Left;
  if( s == "right" )
    return sf::Key::Right;
  if( s == "escape" )
    return sf::Key::Escape;
  return sf::Key::Code(0);
}


void GuiInterface::onChat(const Player *pl, const std::string &msg)
{
  this->addMessage(0, "%s(%u): %s", pl->nick().c_str(), pl->plid(), msg.c_str());
}

void GuiInterface::onNotification(Severity sev, const std::string &msg)
{
  int c = 0;
  switch( sev ) {
    case SEVERITY_MESSAGE: c = 2; break;
    case SEVERITY_NOTICE:  c = 3; break;
    case SEVERITY_WARNING: c = 7; break;
    case SEVERITY_ERROR:   c = 8; break;
  }
  this->addMessage(c, ">> %s", msg.c_str());
}

void GuiInterface::onPlayerJoined(const Player *pl)
{
  this->addMessage(2, "%s(%u) joined", pl->nick().c_str(), pl->plid());
}

void GuiInterface::onPlayerSetNick(const Player *pl, const std::string &old_nick)
{
  this->addMessage(2, "%s(%u) is now known as %s", old_nick.c_str(),
                   pl->plid(), pl->nick().c_str());
}

void GuiInterface::onPlayerReady(const Player *pl)
{
  if( pl->ready() )
    this->addMessage(2, "%s(%u) is ready", pl->nick().c_str(), pl->plid());
  else
    this->addMessage(2, "%s(%u) is not ready anymore", pl->nick().c_str(), pl->plid());
}

void GuiInterface::onPlayerQuit(const Player *pl)
{
  this->addMessage(2, "%s(%u) has quit", pl->nick().c_str(), pl->plid());
}

void GuiInterface::onMatchInit(const Match *)
{
  this->addMessage(2, "match init");
}

void GuiInterface::onMatchReady(const Match *m)
{
  assert( fdisplays_.empty() );

  Match::FieldContainer::const_iterator it;
  for( it=m->fields().begin(); it!=m->fields().end(); ++it ) {
    FieldDisplay *fdp = new FieldDisplay(*it, res_, fdisplays_.size());
    const Field *fld = &(*it);
    fdisplays_.insert(fld, fdp);
  }
}

void GuiInterface::onMatchStart(const Match *)
{
  this->addMessage(2, "START");
}

void GuiInterface::onMatchEnd(const Match *)
{
  this->addMessage(2, "END");
  fdisplays_.clear();
}

void GuiInterface::onFieldStep(const Field *fld)
{
  FieldDisplayMap::iterator it = fdisplays_.find(fld);
  assert( it != fdisplays_.end() );
  (*it).second->step();
}

void GuiInterface::onFieldLost(const Field *fld)
{
  const Player *pl = fld->player();
  if( pl != NULL ) {
    this->addMessage(2, "%s(%u) lost", pl->nick().c_str(), pl->plid());
  }
}


KeyState GuiInterface::getNextInput()
{
  const sf::Input &input = window_.GetInput();
  int key = GAME_KEY_NONE;
  if( input.IsKeyDown(keys_.up   ) ) key |= GAME_KEY_UP;
  if( input.IsKeyDown(keys_.down ) ) key |= GAME_KEY_DOWN;
  if( input.IsKeyDown(keys_.left ) ) key |= GAME_KEY_LEFT;
  if( input.IsKeyDown(keys_.right) ) key |= GAME_KEY_RIGHT;
  if( input.IsKeyDown(keys_.swap ) ) key |= GAME_KEY_SWAP;
  if( input.IsKeyDown(keys_.raise) ) key |= GAME_KEY_RAISE;
  return key;
}



const unsigned int FieldDisplay::CROUCH_DURATION = 8;
const float FieldDisplay::BOUNCE_SYMBOL_SIZE =  80/128.;
const float FieldDisplay::BOUNCE_WIDTH_MIN   =  72/128.;
const float FieldDisplay::BOUNCE_WIDTH_MAX   = 104/128.;
const float FieldDisplay::BOUNCE_HEIGHT_MIN  =  50/128.;
const float FieldDisplay::BOUNCE_HEIGHT_MAX  =  84/128.;
const float FieldDisplay::BOUNCE_Y_MIN       = -48/128.;
const float FieldDisplay::BOUNCE_Y_MAX       =  60/128.;


FieldDisplay::FieldDisplay(const Field &fld, const DisplayRes &res, int slot):
    sf::Drawable(), field_(fld), res_(res)
{
  assert( slot >= 0 );

  ::memset(crouch_dt_, 0, sizeof(crouch_dt_));

  this->SetPosition( slot * 2*res.img_field_frame.GetWidth() + 100, 100 );
  this->SetScale(0.5, 0.5); //XXX

  spr_frame_.SetImage(res_.img_field_frame);
  spr_frame_.SetCenter(
      (res_.img_field_frame.GetWidth()-res_.bk_size*FIELD_WIDTH/2)/2,
      (res_.img_field_frame.GetHeight()-res_.bk_size*FIELD_HEIGHT/2)/2
      );
  spr_frame_.SetScale(2,2);

  spr_cursor_.SetImage(res_.img_cursor);
  spr_cursor_.SetCenter(
      res_.img_cursor.GetWidth()/2,
      res_.img_cursor.GetHeight()/4
      );

  this->step();  // not a step, but do the work
}

FieldDisplay::~FieldDisplay() {}


void FieldDisplay::Render(sf::RenderTarget &target) const
{
  // grid content
  glPushMatrix();
    // scale blocks, reverse Y axis
    glScalef( res_.bk_size, -(float)res_.bk_size, 1 );
    glTranslatef(0, lift_offset_-FIELD_HEIGHT-1, 0);
    int x, y;
    for( x=0; x<FIELD_WIDTH; x++ ) {
      for( y=1; y<=FIELD_HEIGHT; y++ )
        this->renderBlock(target, x, y);
      // raising line: darker
      glColor3f(0.4, 0.4, 0.4);
      this->renderBlock(target, x, 0);
      glColor3f(1,1,1);
    }
  glPopMatrix();

  target.Draw(spr_frame_);

#if 0
  // waiting garbages
  glPushMatrix();
  glColor3f(0.8, 0.4, 0.1); //XXX:temp
  glTranslatef(0.75, FIELD_HEIGHT+0.5-raise_y, 0);

  size_t gb_nb = field_.waitingGarbageCount();
  if( gb_nb > FIELD_WIDTH*2/3 ) {
    gb_nb = FIELD_WIDTH*2/3; // avoid display "overflow"
  }
  size_t gb_i=0;
  for( gb_i=0; gb_i<gb_nb; gb_i++ ) {
    const Garbage &gb = field_.waitingGarbage(gb_i);
    if( gb.type == Garbage::TYPE_CHAIN ) {
      res_.spr_waiting_gb.line.draw();
      if( gb.size.y > 1 ) {
        char buf[5];
        ::snprintf(buf, sizeof(buf), "x%u", gb.size.y);
        buf[sizeof(buf)-1] = '\0';
        const std::string txt(buf);
        glColor3f(0.8,0.8,0.8); //XXX:temp
        res_.font.render(txt, 1.5*0.6, 0.6, Font::STRETCH_Y);
        glColor3f(0.8,0.4,0.1); //XXX:temp (cf. above)
      }
    } else if( gb.type == Garbage::TYPE_COMBO ) {
      res_.spr_waiting_gb.blocks[gb.size.x-1].draw();
    }
    glTranslatef(1.5, 0, 0);
  }

  glColor3f(1,1,1); //XXX:temp (cf. above)
  glPopMatrix();
#endif

  // cursor
  target.Draw(spr_cursor_);

#if 0
  // display labels, older first / newer above
  BasicLabelHolder::iterator it;
  for( it=labels_.begin(); it!=labels_.end(); ++it ) {
    glPushMatrix();
    glTranslatef(it->pos.x+0.5, it->pos.y-0.5
                 //XXX:hack space between combo and chain labels for a same match
                 +(it->chain?0.1:0)
                 +1-0.5*(float)it->dt/BasicLabelHolder::DURATION, 0);

    if( it->chain ) {
      res_.spr_label.chain.draw();
    } else {
      res_.spr_label.combo.draw();
    }

    char buf[5];
    if( it->chain ) {
      ::snprintf(buf, sizeof(buf), "x%u", it->val);
    } else {
      ::snprintf(buf, sizeof(buf), "%u", it->val);
    }
    buf[sizeof(buf)-1] = '\0';
    const std::string txt(buf);

    res_.font.render(txt, 0.6, 0.6, Font::STRETCH_Y);

    glPopMatrix();
  }
#endif
}


void FieldDisplay::step()
{
  // Note: called at init, even if init does not actually steps the field.

  lift_offset_ = 1-(float)field_.raiseStep()/field_.conf().raise_steps;

  // cursor
  if( field_.tick() % 15 == 0 ) {
    if( (field_.tick()/15) % 2 == 0 ) {
      spr_cursor_.SetSubRect( sf::IntRect(
              0, 0, res_.img_cursor.GetWidth(), res_.img_cursor.GetHeight()/2
              ) );
    } else {
      spr_cursor_.SetSubRect( sf::IntRect(
              0, res_.img_cursor.GetHeight()/2,
              res_.img_cursor.GetWidth(), res_.img_cursor.GetHeight()
              ) );
    }
  }
  spr_cursor_.SetPosition(
      res_.bk_size * (field_.cursor().x + 1),
      res_.bk_size * (FIELD_HEIGHT-field_.cursor().y + 0.5 - lift_offset_)
      );

  // field raised: update crouch_dt_
  if( field_.stepInfo().raised ) {
    for(int x=0; x<FIELD_WIDTH; x++) {
      for(int y=FIELD_HEIGHT; y>0; y--) {
        crouch_dt_[x][y] = crouch_dt_[x][y-1];
      }
      crouch_dt_[x][0] = 0;
    }
  }

  // block bouncing after fall
  //TODO update when a block fall?
  for(int x=0; x<FIELD_WIDTH; x++) {
    for(int y=1; y<=FIELD_WIDTH; y++) {
      const Block &bk = field_.block(x,y);
      if( bk.isState(BkColor::LAID) ) {
        crouch_dt_[x][y] = CROUCH_DURATION;
      } else if( bk.isState(BkColor::REST) && crouch_dt_[x][y] != 0 ) {
        crouch_dt_[x][y]--;
      } else {
        crouch_dt_[x][y] = 0;
      }
    }
  }

  labels_.step(&field_);
}


void FieldDisplay::renderBlock(sf::RenderTarget &target, int x, int y) const
{
  const Block &bk = field_.block(x,y);
  if( bk.isNone() || bk.isState(BkColor::CLEARED) ) {
    return;  // nothing to draw
  }

  sf::Vector2f center( x+0.5, y+0.5 );

  if( bk.isColor() ) {
    const DisplayRes::TilesBkColor &tiles = res_.tiles_bk_color[bk.bk_color.color];

    const FieldTile *tile = &tiles.normal; // default
    if( bk.bk_color.state == BkColor::FLASH ) {
      if( (bk.ntick - field_.tick()) % 2 == 0 )
        tile = &tiles.flash;
    } else if( bk.bk_color.state == BkColor::MUTATE ) {
      tile = &tiles.mutate;
    } else if( bk.swapped ) {
      center.x += field_.swapPos().x ? 1 : -1;
      center.y += field_.swapDelay()/(field_.conf().swap_tk+1);
    }

    unsigned int crouch_dt = crouch_dt_[x][y];
    if( crouch_dt == 0 ) {
      tile->render(target, x, y);
    } else {
      // bounce positions: -1 -> +1 (quick) -> 0 (slow)
      float bounce = ( crouch_dt > CROUCH_DURATION/2 )
        ? 4*(float)(CROUCH_DURATION-crouch_dt)/CROUCH_DURATION-1.0
        : 2*(float)crouch_dt/CROUCH_DURATION;
      this->renderBouncingBlock(target, x, y, bounce, bk.bk_color.color);
    }

  } else if( bk.isGarbage() ) {
    const DisplayRes::TilesGb &tiles = res_.tiles_gb;
    glColor3f(0.8, 0.4, 0.1); //XXX:temp

    if( bk.bk_garbage.state == BkGarbage::FLASH ) {
      if( (bk.ntick - field_.tick()) % 2 == 0 ) {
       tiles.mutate.render(target, x, y);
      } else {
       tiles.flash.render(target, x, y);
      }
    } else if( bk.bk_garbage.state == BkGarbage::MUTATE ) {
      tiles.mutate.render(target, x, y);
    } else {
      const Garbage *gb = bk.bk_garbage.garbage;
      bool center_mark = gb->size.x > 2 && gb->size.y > 1;
      const int rel_x = 2*(x-gb->pos.x);
      const int rel_y = 2*(y-gb->pos.y);

      // draw 4 sub-tiles
      const FieldTile *tile;

      // top left
      if( center_mark &&
         (rel_x == gb->size.x || rel_x == gb->size.x-1) &&
         (rel_y+1 == gb->size.y || rel_y+1 == gb->size.y-1)
        ) {
        int tx = ( rel_x   == gb->size.x ) ? 1 : 0;
        int ty = ( rel_y+1 == gb->size.y ) ? 0 : 1;
        tile = &tiles.center[tx][ty];
      } else {
        int tx = ( x == gb->pos.x              ) ? 0 : 2;
        int ty = ( y == gb->pos.y+gb->size.y-1 ) ? 0 : 2;
        tile = &tiles.tiles[tx][ty];
      }
      tile->render(target, sf::FloatRect(x, y+0.5, x+0.5, y+1));

      // top right
      if( center_mark &&
         (rel_x+1 == gb->size.x || rel_x+1 == gb->size.x-1) &&
         (rel_y+1 == gb->size.y || rel_y+1 == gb->size.y-1)
        ) {
        int tx = ( rel_x+1 == gb->size.x ) ? 1 : 0;
        int ty = ( rel_y+1 == gb->size.y ) ? 0 : 1;
        tile = &tiles.center[tx][ty];
      } else {
        int tx = ( x == gb->pos.x+gb->size.x-1 ) ? 3 : 1;
        int ty = ( y == gb->pos.y+gb->size.y-1 ) ? 0 : 2;
        tile = &tiles.tiles[tx][ty];
      }
      tile->render(target, sf::FloatRect(x+0.5, y+0.5, x+1, y+1));

      // bottom left
      if( center_mark &&
          (rel_x == gb->size.x || rel_x == gb->size.x-1) &&
          (rel_y == gb->size.y || rel_y == gb->size.y-1)
        ) {
        int tx = ( rel_x == gb->size.x ) ? 1 : 0;
        int ty = ( rel_y == gb->size.y ) ? 0 : 1;
        tile = &tiles.center[tx][ty];
      } else {
        int tx = ( x == gb->pos.x ) ? 0 : 2;
        int ty = ( y == gb->pos.y ) ? 3 : 1;
        tile = &tiles.tiles[tx][ty];
      }
      tile->render(target, sf::FloatRect(x, y, x+0.5, y+0.5));

      // bottom right
      if( center_mark &&
         (rel_x+1 == gb->size.x || rel_x+1 == gb->size.x-1) &&
         (rel_y == gb->size.y || rel_y == gb->size.y-1)
        ) {
        int tx = ( rel_x+1 == gb->size.x ) ? 1 : 0;
        int ty = ( rel_y   == gb->size.y ) ? 0 : 1;
        tile = &tiles.center[tx][ty];
      } else {
        int tx = ( x == gb->pos.x+gb->size.x-1 ) ? 3 : 1;
        int ty = ( y == gb->pos.y              ) ? 3 : 1;
        tile = &tiles.tiles[tx][ty];
      }
      tile->render(target, sf::FloatRect(x+0.5, y, x+1, y+0.5));
    }
    glColor3f(1,1,1); //XXX:temp (cf. above)
  }
}


void FieldDisplay::renderBouncingBlock(sf::RenderTarget &target, int x, int y, float bounce, unsigned int color) const
{
  const DisplayRes::TilesBkColor &tiles = res_.tiles_bk_color[color];
  tiles.bg.render(target, x, y);

  float offy, dx, dy;
  if( bounce < 0 ) {
    offy = bounce*(float)(BOUNCE_Y_MIN+BOUNCE_HEIGHT_MIN/2);
    dx = 0.5 * bounce*((float)BOUNCE_WIDTH_MAX/BOUNCE_SYMBOL_SIZE-1);
    dy = 0.5 * bounce*((float)BOUNCE_HEIGHT_MIN/BOUNCE_SYMBOL_SIZE-1);
  } else {
    offy = -bounce*(float)(BOUNCE_Y_MAX-BOUNCE_HEIGHT_MAX/2);
    dx = -0.5 * bounce*((float)BOUNCE_WIDTH_MIN/BOUNCE_SYMBOL_SIZE-1);
    dy = -0.5 * bounce*((float)BOUNCE_HEIGHT_MAX/BOUNCE_SYMBOL_SIZE-1);
  }

  sf::FloatRect pos( x + dx, y + dy - offy, x+1 - dx, y+1 - dy - offy );
  tiles.face.render(target, pos);
}


}

