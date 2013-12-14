#include "screen_game.h"
#include "screen_menus.h"
#include "interface.h"
#include "../log.h"

namespace gui {


ScreenGame::ScreenGame(GuiInterface& intf, Player* pl):
    Screen(intf, "ScreenGame"),
    player_(pl),
    input_scheduler_(*intf.instance(), *this, intf.io_service()),
    style_field_(intf.res_mgr())
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
  style_field_.load("ScreenGame.Field");
  assert( player_ );
}

void ScreenGame::exit()
{
  input_scheduler_.stop();
  field_displays_.clear();
}

void ScreenGame::redraw()
{
  sf::RenderWindow& w = intf_.window();
  w.clear(sf::Color(48,48,48)); //XXX:tmp
  for(const auto& pair : field_displays_) {
    w.draw(*pair.second);
  }
}

bool ScreenGame::onInputEvent(const sf::Event& ev)
{
  if( ev.type == sf::Event::KeyPressed ) {
    if( ev.key.code == sf::Keyboard::Escape ) {
      intf_.swapScreen(new ScreenStart(intf_));
      return true;
    }
  }

  return false;
}

void ScreenGame::onPlayerStep(Player* pl)
{
  auto fdp = field_displays_.find(pl->plid());
  if(fdp != field_displays_.end()) {
    (*fdp).second->step();
  }
}

void ScreenGame::onStateChange(GameInstance::State state)
{
  if(state == GameInstance::State::LOBBY) {
    intf_.swapScreen(new ScreenLobby(intf_, player_));

  } else if(state == GameInstance::State::GAME_READY) {
    const auto& fields = intf_.instance()->match().fields();
    // compute values for field display position and size
    const float field_width = style_field_.bk_size * (FIELD_WIDTH+2);
    const float field_height = style_field_.bk_size * (FIELD_HEIGHT+4);
    const auto screen_size = intf_.window().getView().getSize();
    const float dx = screen_size.x / fields.size();
    const float scale = std::min(dx / field_width, screen_size.y / field_height);

    // create a field display for each playing player
    float x = (-0.5*fields.size() + 0.5) * dx;
    for(const auto& field : fields) {
      FldId fldid = field.fldid(); // intermediate variable because a ref is required
      FieldDisplay* fdp = new FieldDisplay(field, style_field_);
      field_displays_.insert(fldid, fdp);
      fdp->scale(scale, scale);
      fdp->move(x, 0);
      x += dx;
    }

    intf_.instance()->playerSetState(player_, Player::State::GAME_READY);

  } else if(state == GameInstance::State::GAME) {
    LOG("match start");
    input_scheduler_.start();
  }
}


KeyState ScreenGame::getNextInput(Player* /*pl*/)
{
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


FieldDisplay::FieldDisplay(const Field& fld, const StyleField& style):
    sf::Drawable(), field_(fld), style_(style)
{
  ::memset(crouch_dt_, 0, sizeof(crouch_dt_));
  this->setOrigin(style_.bk_size*FIELD_WIDTH/2, style_.bk_size*FIELD_HEIGHT/2);

  spr_frame_.setTexture(*style_.img_field_frame);
  spr_frame_.setOrigin(style_.frame_origin.x, style_.frame_origin.y);
  spr_frame_.setScale(2,2);
  spr_frame_.setColor(style_.colors[field_.fldid()]);

  style_.tiles_cursor[0].setToSprite(&spr_cursor_, true);

  this->step();  // not a step, but do the work
}


void FieldDisplay::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
  states.transform *= getTransform();
  {
    // change view to clip rendered block to the fied area
    // use field position and size in relative screen coordinates
    sf::View view_orig = target.getView();
    sf::View view(sf::Vector2f(FIELD_WIDTH/2, FIELD_HEIGHT/2), sf::Vector2f(FIELD_WIDTH, FIELD_HEIGHT));
    sf::FloatRect field_rect = states.transform.transformRect(sf::FloatRect(
            0, 0,
            style_.bk_size*FIELD_WIDTH, style_.bk_size*FIELD_HEIGHT));
    view.setViewport(sf::FloatRect(
            (field_rect.left - view_orig.getCenter().x) / view_orig.getSize().x + 0.5,
            (field_rect.top - view_orig.getCenter().y) / view_orig.getSize().y + 0.5,
            field_rect.width / view_orig.getSize().x,
            field_rect.height / view_orig.getSize().y));
    target.setView(view);

    // use a new render state, scaled one block size
    sf::RenderStates states_bk;
    // lift blocks, reverse Y axis
    states_bk.transform.translate(0, -lift_offset_+FIELD_HEIGHT+1).scale(1, -1);
    int x, y;
    for( x=0; x<FIELD_WIDTH; x++ ) {
      for( y=0; y<=FIELD_HEIGHT; y++ ) {
        this->renderBlock(target, states_bk, x, y);
      }
    }

    target.setView(view_orig);
  }

  target.draw(spr_frame_, states);

  // hanging garbages
  GbHangingList::const_iterator gb_it;
  unsigned gb_i;  // avoid display "overflow", max: FIELD_WIDTH*2/3
  for(gb_it=gbw_drbs_.begin(), gb_i=0;
      gb_it != gbw_drbs_.end() && gb_i<FIELD_WIDTH*2/3;
      ++gb_it, gb_i++) {
    target.draw(*gb_it, states);
  }

  // cursor
  target.draw(spr_cursor_, states);

  // signs
  SignContainer::const_iterator it;
  for( it=signs_.begin(); it!=signs_.end(); ++it ) {
    target.draw((*it), states);
  }
}


void FieldDisplay::step()
{
  // Note: called at init, even if init does not actually steps the field.
  const Field::StepInfo& info = field_.stepInfo();

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
      const Block& bk = field_.block(x,y);
      if( bk.isState(BkColor::LAID) ) {
        crouch_dt_[x][y] = CROUCH_DURATION;
      } else if( bk.isState(BkColor::REST) && crouch_dt_[x][y] != 0 ) {
        crouch_dt_[x][y]--;
      } else {
        crouch_dt_[x][y] = 0;
      }
    }
  }

  // signs

  // update
  SignContainer::iterator it;
  for( it=signs_.begin(); it!=signs_.end(); ++it ) {
    it->step();
  }
  // remove expired signs
  while( !signs_.empty() && signs_.front().dt() == 0 ) {
    signs_.pop_front();
  }
  // create new signs, if needed
  if( info.combo != 0 ) {
    FieldPos pos = this->matchSignPos();
    if( pos.y < FIELD_HEIGHT ) {
      pos.y++; // display sign above top matching block, if possible
    }
    if( info.chain > 1 ) {
      signs_.push_back( Sign(style_, pos, true, info.chain) );
      pos.y--;
    }
    if( info.combo > 3 ) {
      signs_.push_back( Sign(style_, pos, false, info.combo) );
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
    const Garbage& gb = field_.hangingGarbage(gb_i);
    if( gb.gbid != gbd_it->gbid() ) {
      break;
    }
    gbd_it->step();
  }
  // move/insert other garbages
  for( ; gb_i<gb_nb; gb_i++ ) {
    const Garbage& gb = field_.hangingGarbage(gb_i);
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


void FieldDisplay::renderBlock(sf::RenderTarget& target, sf::RenderStates states, int x, int y) const
{
  const Block& bk = field_.block(x,y);
  if( bk.isNone() || bk.isState(BkColor::CLEARED) ) {
    return;  // nothing to draw
  }

  sf::Vector2f center( x+0.5, y+0.5 );

  if( bk.isColor() ) {
    const StyleField::TilesBkColor& tiles = style_.tiles_bk_color[bk.bk_color.color];

    const ImageTile* tile = &tiles.normal; // default
    if( bk.bk_color.state == BkColor::FLASH ) {
      if( (bk.ntick - field_.tick()) % 2 == 0 ) {
        tile = &tiles.flash;
      }
    } else if( bk.bk_color.state == BkColor::MUTATE ) {
      tile = &tiles.mutate;
    } else if( bk.swapped ) {
      center.x += field_.swapPos().x ? 1 : -1;
      center.y += field_.swapDelay()/(field_.conf().swap_tk+1);
    }

    unsigned int crouch_dt = crouch_dt_[x][y];
    if( crouch_dt == 0 ) {
      tile->render(target, states, x, y, 1, 1, y ? sf::Color::White : sf::Color(96,96,96));
    } else {
      // bounce positions: -1 -> +1 (quick) -> 0 (slow)
      float bounce = ( crouch_dt > CROUCH_DURATION/2 )
        ? 4*(float)(CROUCH_DURATION-crouch_dt)/CROUCH_DURATION-1.0
        : 2*(float)crouch_dt/CROUCH_DURATION;
      this->renderBouncingBlock(target, states, x, y, bounce, bk.bk_color.color);
    }

  } else if( bk.isGarbage() ) {
    const StyleField::TilesGb& tiles = style_.tiles_gb;
    const Garbage* gb = bk.bk_garbage.garbage;
    //XXX gb is reset before being transformed
    const sf::Color& color = style_.colors[(gb && gb->from) ? gb->from->fldid() : 0];

    if( bk.bk_garbage.state == BkGarbage::FLASH ) {
      if( (bk.ntick - field_.tick()) % 2 == 0 ) {
       tiles.mutate.render(target, states, x, y, 1, 1, color);
      } else {
       tiles.flash.render(target, states, x, y, 1, 1, color);
      }
    } else if( bk.bk_garbage.state == BkGarbage::MUTATE ) {
      tiles.mutate.render(target, states, x, y, 1, 1, color);
    } else {
      bool center_mark = gb->size.x > 2 && gb->size.y > 1;
      const int rel_x = 2*(x-gb->pos.x);
      const int rel_y = 2*(y-gb->pos.y);

      // draw 4 sub-tiles
      const ImageTile* tile;

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
      tile->render(target, states, x, y+0.5, 0.5, 0.5, color);

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
      tile->render(target, states, x+0.5, y+0.5, 0.5, 0.5, color);

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
      tile->render(target, states, x, y, 0.5, 0.5, color);

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
      tile->render(target, states, x+0.5, y, 0.5, 0.5, color);
    }
  }
}


void FieldDisplay::renderBouncingBlock(sf::RenderTarget& target, sf::RenderStates states, int x, int y, float bounce, unsigned int color) const
{
  const StyleField::TilesBkColor& tiles = style_.tiles_bk_color[color];
  tiles.bg.render(target, states, x, y, 1, 1);

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

  tiles.face.render(target, states, x + dx, y + dy - offy, 1-2*dx, 1-2*dy);
}


const unsigned int FieldDisplay::Sign::DURATION = 42;

FieldDisplay::Sign::Sign(const StyleField& style, const FieldPos& pos, bool chain, unsigned int val):
    style_(style), dt_(DURATION)
{
  this->setPosition((pos.x+0.5)*style_.bk_size, (FIELD_HEIGHT-pos.y+0.5)*style_.bk_size);
  //XXX:hack space between combo and chain signs for a same match
  this->move(0, -0.1*style_.bk_size);

  // initialize text
  //XXX store/cache text style information on StyleField
  style_.applyStyle(&txt_, "Sign");
  txt_.setString(chain ? 'x'+std::to_string(val) : std::to_string(val));
  txt_.setColor(sf::Color::White);
  sf::FloatRect txt_rect = txt_.getLocalBounds();
  float txt_sx = std::min(1.0, 0.8*style_.bk_size / txt_rect.width);
  txt_.setOrigin(txt_rect.left + txt_rect.width/2, txt_rect.top + txt_rect.height/2);
  txt_.setScale(txt_sx, 1);

  // initialize sprite
  if( chain ) {
    style_.tiles_signs.chain.setToSprite(&bg_, true);
  } else {
    style_.tiles_signs.combo.setToSprite(&bg_, true);
  }
}


void FieldDisplay::Sign::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
  states.transform *= getTransform();
  target.draw(bg_, states);
  target.draw(txt_, states);
}

void FieldDisplay::Sign::step()
{
  dt_--;
  this->move(0, -0.5*style_.bk_size/Sign::DURATION);
}


FieldPos FieldDisplay::matchSignPos()
{
  unsigned int x, y;
  for( y=FIELD_HEIGHT; y>0; y-- ) {
    for( x=0; x<FIELD_WIDTH; x++ ) {
      const Block& bk = field_.block(x,y);
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



FieldDisplay::GbHanging::GbHanging(const StyleField& style, const Garbage& gb):
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
  bg_.setColor(style_.colors[gb_.from ? gb_.from->fldid() : 0]);
  //XXX store/cache text style information on StyleField
  style_.applyStyle(&txt_, "Garbage");
  txt_.setColor(sf::Color::White);

  this->updateText();
}

void FieldDisplay::GbHanging::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
  states.transform *= getTransform();
  target.draw(bg_, states);
  if( txt_size_ != 0 ) {
    target.draw(txt_, states);
  }
}

void FieldDisplay::GbHanging::setPosition(int i)
{
  this->sf::Transformable::setPosition((0.75+1.5*i)*style_.bk_size, -0.5*style_.bk_size);
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

  txt_.setString('x'+std::to_string(txt_size_));
  txt_.setScale(1, 1); // reset scale before getting rect
  sf::FloatRect txt_rect = txt_.getLocalBounds();
  float txt_sx = std::min(1.0, 0.8*style_.bk_size / txt_rect.width);
  txt_.setOrigin(txt_rect.left + txt_rect.width/2, txt_rect.top + txt_rect.height/2);
  txt_.setScale(txt_sx, 1); // scale after computations (needed)
}



}

