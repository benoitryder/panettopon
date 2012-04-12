#include "screen_game.h"
#include "screen_menus.h"
#include "interface.h"
#include "../log.h"

namespace gui {


ScreenGame::ScreenGame(GuiInterface &intf, Player *pl):
    Screen(intf, "ScreenGame"),
    player_(pl),
    input_scheduler_(*intf.instance(), *this, intf.io_service()),
    fdp_player_(NULL)
{
  keys_.up    = sf::Keyboard::Up;
  keys_.down  = sf::Keyboard::Down;
  keys_.left  = sf::Keyboard::Left;
  keys_.right = sf::Keyboard::Right;
  keys_.swap  = sf::Keyboard::D;
  keys_.raise = sf::Keyboard::F;
}

void ScreenGame::enter()
{
  style_field_.load(&intf_.res_mgr(), "ScreenGame.Field");
  assert( player_ );
}

void ScreenGame::exit()
{
  input_scheduler_.stop();
  fdp_player_.reset();
}

void ScreenGame::redraw()
{
  sf::RenderWindow &w = intf_.window();
  w.clear(sf::Color(48,48,48)); //XXX:tmp
  //TODO
  if( fdp_player_.get() ) {
    w.draw(*fdp_player_);
  }
}

bool ScreenGame::onInputEvent(const sf::Event &ev)
{
  if( ev.type == sf::Event::KeyPressed ) {
    if( ev.key.code == sf::Keyboard::Escape ) {
      intf_.swapScreen(new ScreenStart(intf_));
      return true;
    }
  }

  return false;
}

void ScreenGame::onPlayerStep(Player *pl)
{
  if( pl == player_ ) {
    fdp_player_->step();
  }
}

void ScreenGame::onStateChange(GameInstance::State state)
{
  if( state == GameInstance::STATE_LOBBY ) {
    intf_.swapScreen(new ScreenLobby(intf_, player_));
  } else if( state == GameInstance::STATE_READY ) {
    assert( player_->field() );
    fdp_player_.reset(new FieldDisplay(*player_->field(), style_field_));
    fdp_player_->setScale(0.5, 0.5);
    intf_.instance()->playerSetReady(player_, true);
  } else if( state == GameInstance::STATE_GAME ) {
    LOG("match start");
    input_scheduler_.start();
  }
}


KeyState ScreenGame::getNextInput(Player *pl)
{
  assert( pl == player_ );
  if( !intf_.focused() ) {
    return GAME_KEY_NONE;
  }
  int key = GAME_KEY_NONE;
  if( sf::Keyboard::isKeyPressed(keys_.up   ) ) key |= GAME_KEY_UP;
  if( sf::Keyboard::isKeyPressed(keys_.down ) ) key |= GAME_KEY_DOWN;
  if( sf::Keyboard::isKeyPressed(keys_.left ) ) key |= GAME_KEY_LEFT;
  if( sf::Keyboard::isKeyPressed(keys_.right) ) key |= GAME_KEY_RIGHT;
  if( sf::Keyboard::isKeyPressed(keys_.swap ) ) key |= GAME_KEY_SWAP;
  if( sf::Keyboard::isKeyPressed(keys_.raise) ) key |= GAME_KEY_RAISE;
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


FieldDisplay::FieldDisplay(const Field &fld, const StyleField &style):
    sf::Drawable(), field_(fld), style_(style)
{
  ::memset(crouch_dt_, 0, sizeof(crouch_dt_));
  //TODO:recup
  //this->setPosition( slot * 2*style.img_field_frame.getSize().x, 0 );
  this->setOrigin(style_.bk_size*FIELD_WIDTH/2, style_.bk_size*FIELD_HEIGHT/2);

  spr_frame_.setTexture(*style_.img_field_frame);
  spr_frame_.setOrigin(style_.frame_origin.x, style_.frame_origin.y);
  spr_frame_.setScale(2,2);

  style_.tiles_cursor[0].setToSprite(&spr_cursor_, true);

  this->step();  // not a step, but do the work
}


void FieldDisplay::Render(sf::RenderTarget &target, sf::Renderer &renderer) const
{
  // grid content
  renderer.PushStates();
    // scale blocks, reverse Y axis
    renderer.ApplyModelView(sf::Matrix3::Transformation(
            sf::Vector2f(0,0),
            sf::Vector2f(0, -(float)style_.bk_size*(lift_offset_-FIELD_HEIGHT-1)),
            0, sf::Vector2f(style_.bk_size, -(float)style_.bk_size)
            ));
    int x, y;
    for( x=0; x<FIELD_WIDTH; x++ ) {
      for( y=1; y<=FIELD_HEIGHT; y++ ) {
        this->renderBlock(renderer, x, y);
      }
      // raising line: darker
      renderer.SetColor(sf::Color(96,96,96));
      this->renderBlock(renderer, x, 0);
      renderer.SetColor(sf::Color::White);
    }
  renderer.PopStates();

  target.draw(spr_frame_);

  // hanging garbages
  renderer.SetColor(sf::Color(204,102,25)); //XXX:temp
  GbHangingList::const_iterator gb_it;
  unsigned gb_i;  // avoid display "overflow", max: FIELD_WIDTH*2/3
  for(gb_it=gbw_drbs_.begin(), gb_i=0;
      gb_it != gbw_drbs_.end() && gb_i<FIELD_HEIGHT*2/3;
      ++gb_it, gb_i++) {
    target.draw(*gb_it);
  }
  renderer.SetColor(sf::Color::White); //XXX:temp (cf. above)

  // cursor
  target.draw(spr_cursor_);

  // labels
  LabelContainer::const_iterator it;
  for( it=labels_.begin(); it!=labels_.end(); ++it ) {
    target.draw( (*it) );
  }
}


void FieldDisplay::step()
{
  // Note: called at init, even if init does not actually steps the field.
  const Field::StepInfo &info = field_.stepInfo();

  lift_offset_ = 1-(float)field_.raiseStep()/field_.conf().raise_steps;

  // cursor
  if( field_.tick() % 15 == 0 ) {
    style_.tiles_cursor[ (field_.tick()/15) % 2 ].setToSprite(&spr_cursor_, true);
  }
  spr_cursor_.setPosition(
      style_.bk_size * (field_.cursor().x + 1),
      style_.bk_size * (FIELD_HEIGHT-field_.cursor().y + 0.5 - lift_offset_)
      );

  // field raised: update crouch_dt_
  if( info.raised ) {
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

  // labels

  // update
  LabelContainer::iterator it;
  for( it=labels_.begin(); it!=labels_.end(); ++it ) {
    it->step();
  }
  // remove expired labels
  while( !labels_.empty() && labels_.front().dt() == 0 ) {
    labels_.pop_front();
  }
  // create new labels, if needed
  if( info.combo != 0 ) {
    FieldPos pos = this->matchLabelPos();
    if( pos.y < FIELD_HEIGHT ) {
      pos.y++; // display label above top matching block, if possible
    }
    if( info.chain > 1 ) {
      labels_.push_back( Label(style_, pos, true, info.chain) );
      pos.y--;
    }
    if( info.combo > 3 ) {
      labels_.push_back( Label(style_, pos, false, info.combo) );
    }
  }

  // hanging garbages
  /* XXX
   * Current implementation is a bit rough.
   * It would be better to keep track of garbage changes through events
   * and find a way to do nothing when nothing has changed.
   */

  size_t gb_nb = field_.hangingGarbageCount();
  size_t gb_i;
  GbHangingList::iterator gbd_it;

  // normal case: no changes, only update garbages
  for(gb_i=0, gbd_it=gbw_drbs_.begin();
      gb_i<gb_nb && gbd_it!=gbw_drbs_.end();
      gb_i++, ++gbd_it) {
    const Garbage &gb = field_.hangingGarbage(gb_i);
    if( gb.gbid != gbd_it->gbid() ) {
      break;
    }
    gbd_it->step();
  }
  // move/insert other garbages
  for( ; gb_i<gb_nb; gb_i++ ) {
    const Garbage &gb = field_.hangingGarbage(gb_i);
    // find an already existing drawable
    GbHangingList::iterator gbd_it2;
    for( gbd_it2=gbd_it; gbd_it2!=gbw_drbs_.end() && gb.gbid != gbd_it2->gbid(); ++gbd_it2 ) ;
    if( gbd_it2 == gbw_drbs_.end() ) {
      // not found: create and insert at the new position
      gbd_it = gbw_drbs_.insert(gbd_it, new GbHanging(style_, gb));
    } else if( gbd_it != gbd_it2 ) {
      // found: move it at the right position (swap)
      //note: do the '.release()' "by hand" to help the compiler
      gbd_it = gbw_drbs_.insert(gbd_it, gbw_drbs_.release(gbd_it2).release());
    }
    gbd_it->step();
    gbd_it->setPosition(gb_i);
    gbd_it++;
  }
  // remove remaining garbages (those removed)
  if( gbd_it != gbw_drbs_.end() ) {
    gbw_drbs_.erase(gbd_it, gbw_drbs_.end());
  }
}


void FieldDisplay::renderBlock(sf::Renderer &renderer, int x, int y) const
{
  const Block &bk = field_.block(x,y);
  if( bk.isNone() || bk.isState(BkColor::CLEARED) ) {
    return;  // nothing to draw
  }

  sf::Vector2f center( x+0.5, y+0.5 );

  if( bk.isColor() ) {
    const StyleField::TilesBkColor &tiles = style_.tiles_bk_color[bk.bk_color.color];

    const ImageTile *tile = &tiles.normal; // default
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
      tile->render(renderer, x, y, 1, 1);
    } else {
      // bounce positions: -1 -> +1 (quick) -> 0 (slow)
      float bounce = ( crouch_dt > CROUCH_DURATION/2 )
        ? 4*(float)(CROUCH_DURATION-crouch_dt)/CROUCH_DURATION-1.0
        : 2*(float)crouch_dt/CROUCH_DURATION;
      this->renderBouncingBlock(renderer, x, y, bounce, bk.bk_color.color);
    }

  } else if( bk.isGarbage() ) {
    const StyleField::TilesGb &tiles = style_.tiles_gb;
    renderer.SetColor(sf::Color(204,102,25)); //XXX:temp

    if( bk.bk_garbage.state == BkGarbage::FLASH ) {
      if( (bk.ntick - field_.tick()) % 2 == 0 ) {
       tiles.mutate.render(renderer, x, y, 1, 1);
      } else {
       tiles.flash.render(renderer, x, y, 1, 1);
      }
    } else if( bk.bk_garbage.state == BkGarbage::MUTATE ) {
      tiles.mutate.render(renderer, x, y, 1, 1);
    } else {
      const Garbage *gb = bk.bk_garbage.garbage;
      bool center_mark = gb->size.x > 2 && gb->size.y > 1;
      const int rel_x = 2*(x-gb->pos.x);
      const int rel_y = 2*(y-gb->pos.y);

      // draw 4 sub-tiles
      const ImageTile *tile;

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
      tile->render(renderer, x, y+0.5, 0.5, 0.5);

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
      tile->render(renderer, x+0.5, y+0.5, 0.5, 0.5);

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
      tile->render(renderer, x, y, 0.5, 0.5);

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
      tile->render(renderer, x+0.5, y, 0.5, 0.5);
    }
    renderer.SetColor(sf::Color::White); //XXX:temp (cf. above)
  }
}


void FieldDisplay::renderBouncingBlock(sf::Renderer &renderer, int x, int y, float bounce, unsigned int color) const
{
  const StyleField::TilesBkColor &tiles = style_.tiles_bk_color[color];
  tiles.bg.render(renderer, x, y, 1, 1);

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

  tiles.face.render(renderer, x + dx, y + dy - offy, 1-2*dx, 1-2*dy);
}


const unsigned int FieldDisplay::Label::DURATION = 42;

FieldDisplay::Label::Label(const StyleField &style, const FieldPos &pos, bool chain, unsigned int val):
    style_(style), dt_(DURATION)
{
  this->setPosition((pos.x+0.5)*style_.bk_size, (FIELD_HEIGHT-pos.y+0.5)*style_.bk_size);
  //XXX:hack space between combo and chain labels for a same match
  this->move(0, -0.1*style_.bk_size);

  // prepare label text
  char buf[5];
  if( chain) {
    ::snprintf(buf, sizeof(buf), "x%u", val);
  } else {
    ::snprintf(buf, sizeof(buf), "%u", val);
  }
  buf[sizeof(buf)-1] = '\0';

  // initialize text
  txt_.setString(buf);
  txt_.setColor(sf::Color::White);
  sf::FloatRect txt_rect = txt_.getRect();
  float txt_sx = 0.8*style_.bk_size / txt_rect.width;
  float txt_sy = 0.8*style_.bk_size / txt_rect.height;
  if( txt_sx > txt_sy ) {
    txt_sx = txt_sy; // stretch Y, not X
  }
  txt_.setOrigin(txt_rect.width/2, (txt_.getFont().getLineSpacing(txt_.getCharacterSize())+2)/2);
  txt_.setScale(txt_sx, txt_sy); // scale after computations (needed for GetRect())

  // initialize sprite
  if( chain ) {
    style_.tiles_labels.chain.setToSprite(&bg_, true);
  } else {
    style_.tiles_labels.combo.setToSprite(&bg_, true);
  }
}


void FieldDisplay::Label::draw(sf::RenderTarget &target, sf::RenderStates states) const
{
  target.draw(bg_, states);
  target.draw(txt_, states);
}

void FieldDisplay::Label::step()
{
  dt_--;
  this->move(0, -0.5*style_.bk_size/Label::DURATION);
}


FieldPos FieldDisplay::matchLabelPos()
{
  unsigned int x, y;
  for( y=FIELD_HEIGHT; y>0; y-- ) {
    for( x=0; x<FIELD_WIDTH; x++ ) {
      const Block &bk = field_.block(x,y);
      if( bk.isState(BkColor::FLASH) &&
         bk.ntick - field_.tick() == field_.conf().flash_tk ) {
        return FieldPos(x,y);
      }
    }
  }
  // should not happen, combo != 0 tested before call
  assert( false );
  return FieldPos();
}



FieldDisplay::GbHanging::GbHanging(const StyleField &style, const Garbage &gb):
  style_(style), gb_(gb), txt_size_(0)
{
  // initialize sprite
  if( gb.type == Garbage::TYPE_CHAIN ) {
    style_.tiles_gb_hanging.line.setToSprite(&bg_, true);
  } else if( gb.type == Garbage::TYPE_COMBO ) {
    style_.tiles_gb_hanging.blocks[gb.size.x-1].setToSprite(&bg_, true);
  } else {
    //TODO not handled yet
  }

  this->updateText();
}

void FieldDisplay::GbHanging::Render(sf::RenderTarget &target, sf::Renderer &) const
{
  target.draw(bg_);
  if( txt_size_ != 0 ) {
    target.draw(txt_);
  }
}

void FieldDisplay::GbHanging::setPosition(int i)
{
  this->setPosition((0.75+1.5*i)*style_.bk_size, -0.5*style_.bk_size);
}

void FieldDisplay::GbHanging::step()
{
  this->updateText();
}

void FieldDisplay::GbHanging::updateText()
{
  // text for >x1 chain garbages only
  if( gb_.type != Garbage::TYPE_CHAIN || gb_.size.y < 2 ) {
    // not actually needed: type does not change and size only increases
    txt_size_ = 0;
    return;
  }
  // size did not changed: nothing to do
  if( txt_size_ == gb_.size.y ) {
    return;
  }
  txt_size_ = gb_.size.y;

  // prepare label text
  char buf[5];
  ::snprintf(buf, sizeof(buf), "x%u", txt_size_);
  buf[sizeof(buf)-1] = '\0';

  // reset the whole text to have a "fresh" GetRect()
  txt_ = sf::Text(buf);
  txt_.setColor(sf::Color::White);
  sf::FloatRect txt_rect = txt_.getRect();
  float txt_sx = 0.8*style_.bk_size / txt_rect.width;
  float txt_sy = 0.8*style_.bk_size / txt_rect.height;
  if( txt_sx > txt_sy ) {
    txt_sx = txt_sy; // stretch Y, not X
  }
  txt_.setOrigin(txt_rect.width/2, (txt_.getFont().getLineSpacing(txt_.getCharacterSize())+2)/2);
  txt_.setScale(txt_sx, txt_sy); // scale after computations (needed)
}



}

