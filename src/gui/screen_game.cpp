#include "screen_game.h"
#include "screen_menus.h"
#include "interface.h"
#include "../log.h"

namespace gui {


void StyleField::load(const StyleLoader& loader, const StyleGlobal& global)
{
  const ResourceManager& res_mgr = loader.res_mgr();
  this->global = &global;
  unsigned int color_nb = global.colors.size() - 1;

  // Block tiles (and block size)
  {
    const sf::Texture& img = res_mgr.getImage("BkColor-map");
    if(img.getSize().x % color_nb != 0 || img.getSize().y % 5 != 0) {
      throw std::runtime_error("block map size does not match tile count");
    }
    bk_size = img.getSize().y/5;
    tiles_bk_color.resize(color_nb); // create sprites, uninitialized
    for(unsigned int i=0; i<color_nb; i++) {
      TilesBkColor& tiles = tiles_bk_color[i];
      tiles.normal.create(img, color_nb, 5, i, 0);
      tiles.bg    .create(img, color_nb, 5, i, 1);
      tiles.face  .create(img, color_nb, 5, i, 2);
      tiles.flash .create(img, color_nb, 5, i, 3);
      tiles.mutate.create(img, color_nb, 5, i, 4);
    }
  }

  // Garbages
  {
    const sf::Texture& img = res_mgr.getImage("BkGarbage-map");
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
  }

  // Frame
  field_frame_style.load(StyleLoaderPrefix(loader, "Frame"));

  // Cursor
  {
    const sf::Texture& img = res_mgr.getImage("SwapCursor");
    tiles_cursor[0].create(img, 1, 2, 0, 0);
    tiles_cursor[1].create(img, 1, 2, 0, 1);
  }

  // Signs
  {
    const sf::Texture& img = res_mgr.getImage("Signs");
    tiles_signs.combo.create(img, 2, 1, 0, 0);
    tiles_signs.chain.create(img, 2, 1, 1, 0);
    sign_style.load(StyleLoaderPrefix(loader, "Sign"));
  }

  // Hanging garbages
  {
    const sf::Texture& img = res_mgr.getImage("GbHanging-map");
    const size_t gb_hanging_sx = FIELD_WIDTH/2; // on 2 rows
    for(int i=0; i<FIELD_WIDTH; i++) {
      tiles_gb_hanging.blocks[i].create(img, gb_hanging_sx+1, 2, i%gb_hanging_sx, i/gb_hanging_sx);
    }
    tiles_gb_hanging.line.create(img, gb_hanging_sx+1, 2, gb_hanging_sx, 0);
    gb_hanging_style.load(StyleLoaderPrefix(loader, "Garbage"));
  }

  // Start countdown
  start_countdown_style.load(StyleLoaderPrefix(loader, "StartCountdown"));
  // Rank sign
  {
    StyleLoaderPrefix ranksign_loader = StyleLoaderPrefix(loader, "RankSign");
    rank_sign_style.win.load(StyleLoaderPrefix(ranksign_loader, "Win", true));
    rank_sign_style.lose.load(StyleLoaderPrefix(ranksign_loader, "Lose", true));
    rank_sign_style.draw.load(StyleLoaderPrefix(ranksign_loader, "Draw", true));
    for(unsigned int i=1; i<RANK_MAX; i++) {
      std::string dummy;
      std::string str_i = std::to_string(i);
      // at least the color must be (re)defined
      if(!ranksign_loader.searchStyle(str_i, dummy)) {
        break;
      }
      rank_sign_style.rank[i-1].load(StyleLoaderPrefix(ranksign_loader, str_i, true));
    }
  }
}


ScreenGame::ScreenGame(GuiInterface& intf):
    Screen(intf, "ScreenGame"),
    input_scheduler_(*intf.instance(), *this, intf.io_service())
{
}

void ScreenGame::enter()
{
  style_field_.load(StyleLoaderPrefix(*this, "Field"), intf_.style());
}

void ScreenGame::exit()
{
  input_scheduler_.stop();
  field_displays_.clear();
}

void ScreenGame::redraw()
{
  Screen::redraw();
  sf::RenderWindow& w = intf_.window();
  for(const auto& pair : field_displays_) {
    w.draw(*pair.second);
  }
}

bool ScreenGame::onInputEvent(const sf::Event& ev)
{
  if(Screen::onInputEvent(ev)) {
    return true;
  }

  if(InputMapping::Global.cancel.match(ev)) {
    if(ev.type == sf::Event::KeyPressed) {
      // only for keyboard, because button can be used by joystick mappings to
      // play, and could be pressed by mistake
      intf_.swapScreen(std::make_unique<ScreenStart>(intf_));
      return true;
    }
  } else if(InputMapping::Global.confirm.match(ev)) {
    if(intf_.instance()->state() == GameInstance::State::LOBBY) {
      auto new_screen = std::make_unique<ScreenLobby>(intf_);
      // re-add local players with their mapping
      for(auto& pair : intf_.instance()->players()) {
        Player& pl = *pair.second;
        if(pl.local()) {
          new_screen->addLocalPlayer(pl, input_mappings_[pl.plid()]);
        }
      }
      intf_.swapScreen(std::move(new_screen));
      return true;
    }
  }

  return false;
}

void ScreenGame::onServerDisconnect()
{
  auto new_screen = std::make_unique<ScreenStart>(intf_);
  new_screen->addNotification({Notification::Severity::ERROR, "disconnected from server"});
  intf_.swapScreen(std::move(new_screen));
}

void ScreenGame::onPlayerStep(Player& pl)
{
  assert(pl.field());
  if(pl.field()) {
    auto fdp = field_displays_.find(pl.field()->fldid());
    if(fdp != field_displays_.end()) {
      (*fdp).second->step();
    }
  }
}

void ScreenGame::onPlayerRanked(Player& pl)
{
  assert(pl.field());
  if(pl.field()) {
    auto fdp = field_displays_.find(pl.field()->fldid());
    if(fdp != field_displays_.end()) {
      (*fdp).second->doRank();
    }
  }
}

void ScreenGame::onStateChange()
{
  auto state = intf_.instance()->state();
  if(state == GameInstance::State::LOBBY) {
    input_scheduler_.stop();

  } else if(state == GameInstance::State::GAME_READY) {
    const auto& fields = intf_.instance()->match().fields();
    // compute values for field display position and size
    const auto screen_size = intf_.window().getView().getSize();
    const float min_width = fields.size() * GuiInterface::REF_FIELD_SIZE.x;
    float scale = 1;
    if(screen_size.x < min_width) {
      // zoom-out for all fields to fit
      scale = screen_size.x / min_width;
    } else {
      scale = 1;
    }
    // evenly separate the extra space as margins (avoid large gap between
    // fields and smaller margins on screen sides)
    const float space_x = (screen_size.x / scale - GuiInterface::REF_FIELD_SIZE.x * fields.size()) / (fields.size() + 1);

    // once dx has been computed, apply block_size scaling
    scale *= GuiInterface::REF_BLOCK_SIZE / static_cast<float>(style_field_.bk_size);
    // restrict to 0.25 zoom steps to avoid ugly scaling
    scale = std::ceil(4 * scale) / 4.;

    // create a field display for each playing player
    float x = -0.5 * (GuiInterface::REF_FIELD_SIZE.x + space_x) * (fields.size() - 1);
    for(const auto& field : fields) {
      if(intf_.style().colors.size() - 1 < field->conf().color_nb) {
        throw std::runtime_error("not enough configured colors to display fields");
      }
      FldId fldid = field->fldid(); // intermediate variable because a ref is required
      auto fdp = std::make_unique<FieldDisplay>(intf_, *field, style_field_);
      fdp->scale(scale, scale);
      fdp->move(x, 0);
      field_displays_.emplace(fldid, std::move(fdp));
      x += GuiInterface::REF_FIELD_SIZE.x + space_x;
    }

    for(auto& pair : intf_.instance()->players()) {
      Player& pl = *pair.second;
      if(pl.local() && pl.state() == Player::State::GAME_INIT) {
        intf_.instance()->playerSetState(pl, Player::State::GAME_READY);
      }
    }

  } else if(state == GameInstance::State::GAME) {
    LOG("match start");
    input_scheduler_.start();
  }
}


KeyState ScreenGame::getNextInput(const Player& pl)
{
  if(!intf_.focused()) {
    return GAME_KEY_NONE;
  }
  const InputMapping& mapping = input_mappings_[pl.plid()];
  int key = GAME_KEY_NONE;
  if(mapping.up.isActive()) key |= GAME_KEY_UP;
  if(mapping.down.isActive()) key |= GAME_KEY_DOWN;
  if(mapping.left.isActive()) key |= GAME_KEY_LEFT;
  if(mapping.right.isActive()) key |= GAME_KEY_RIGHT;
  if(mapping.swap.isActive()) key |= GAME_KEY_SWAP;
  if(mapping.raise.isActive()) key |= GAME_KEY_RAISE;
  return key;
}


void ScreenGame::setPlayerMapping(const Player& pl, const InputMapping& mapping)
{
  input_mappings_[pl.plid()] = mapping;
}


const unsigned int FieldDisplay::CROUCH_DURATION = 8;
const float FieldDisplay::BOUNCE_SYMBOL_SIZE =  80/128.;
const float FieldDisplay::BOUNCE_WIDTH_MIN   =  72/128.;
const float FieldDisplay::BOUNCE_WIDTH_MAX   = 104/128.;
const float FieldDisplay::BOUNCE_HEIGHT_MIN  =  50/128.;
const float FieldDisplay::BOUNCE_HEIGHT_MAX  =  84/128.;
const float FieldDisplay::BOUNCE_Y_MIN       = -48/128.;
const float FieldDisplay::BOUNCE_Y_MAX       =  60/128.;


FieldDisplay::FieldDisplay(const GuiInterface& intf, const Field& fld, const StyleField& style):
    sf::Drawable(), intf_(intf), field_(fld), style_(style)
{
  ::memset(crouch_dt_, 0, sizeof(crouch_dt_));
  this->setOrigin(style_.bk_size*FIELD_WIDTH/2, style_.bk_size*FIELD_HEIGHT/2);

  style_.field_frame_style.apply(field_frame_);
  field_frame_.setColor(intf_.style().colors[field_.fldid()]);

  // start countdown
  {
    text_start_countdown_ = std::make_unique<sf::Text>();
    style_.start_countdown_style.apply(*text_start_countdown_);
    // use a dummy string to center the text
    text_start_countdown_->setString("0.00");
    sf::FloatRect r = text_start_countdown_->getLocalBounds();
    text_start_countdown_->setOrigin(r.width/2, 0);
    text_start_countdown_->setPosition(style_.bk_size * FIELD_WIDTH/2, style_.bk_size * 2);
  }

  style_.tiles_cursor[0].setToSprite(spr_cursor_, true);

  // load sounds
  sounds_.move.setBuffer(intf_.res_mgr().getSound("move"));
  sounds_.swap.both.setBuffer(intf_.res_mgr().getSound("swap-both"));
  sounds_.swap.left.setBuffer(intf_.res_mgr().getSound("swap-left"));
  sounds_.swap.right.setBuffer(intf_.res_mgr().getSound("swap-right"));
  sounds_.fall.setBuffer(intf_.res_mgr().getSound("fall"));
  for(int i=0;; i++) {
    sounds_.pops.emplace_back();
    auto& sub_pops = sounds_.pops.back();
    for(int j=0;; j++) {
      std::string name = "pop-" + std::to_string(i) + '-' + std::to_string(j);
      try {
        sub_pops.emplace_back(intf_.res_mgr().getSound(name));
      } catch(const ResourceManager::LoadError&) {
        if(i == 0 && j == 0) {
          throw;  // except at least one entry
        }
        break;
      }
    }
    if(sub_pops.empty()) {
      sounds_.pops.pop_back();
      break;
    }
  }

  this->step();  // not a step, but do the work
}


void FieldDisplay::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
  const float field_sx = style_.bk_size * FIELD_WIDTH;
  const float field_sy = style_.bk_size * FIELD_HEIGHT;

  states.transform *= getTransform();
  {
    // change view to clip rendered block to the fied area
    // use field position and size in relative screen coordinates
    sf::View view_orig = target.getView();
    sf::View view(sf::Vector2f(FIELD_WIDTH/2, FIELD_HEIGHT/2), sf::Vector2f(FIELD_WIDTH, FIELD_HEIGHT));
    sf::FloatRect field_rect = states.transform.transformRect({0, 0, field_sx, field_sy});
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

  field_frame_.render(target, states, {0, 0, field_sx, field_sy});

  // hanging garbages
  GbHangingList::const_iterator gb_it;
  unsigned gb_i;  // avoid display "overflow", max: FIELD_WIDTH*2/3
  for(gb_it=gbw_drbs_.begin(), gb_i=0;
      gb_it != gbw_drbs_.end() && gb_i<FIELD_WIDTH*2/3;
      ++gb_it, gb_i++) {
    target.draw(**gb_it, states);
  }

  // cursor
  target.draw(spr_cursor_, states);

  // signs
  SignContainer::const_iterator it;
  for( it=signs_.begin(); it!=signs_.end(); ++it ) {
    target.draw((*it), states);
  }

  // if game is over, darken the field
  if(field_.rank()) {
    sf::RenderStates states_dark(sf::BlendMultiply, states.transform, nullptr, nullptr);
    const sf::Color dark(64, 64, 64);
    const sf::Vertex vertices[] = {
      {{0, 0}, dark},
      {{field_sx, 0}, dark},
      {{field_sx, field_sy}, dark},
      {{0, field_sy}, dark},
    };
    target.draw(vertices, sizeof(vertices)/sizeof(*vertices), sf::Quads, states_dark);
  }

  // temporary elements
  if(text_start_countdown_) {
    target.draw(*text_start_countdown_, states);
  }
  if(text_rank_sign_) {
    target.draw(*text_rank_sign_, states);
  }
}


void FieldDisplay::step()
{
  // Note: called at init, even if init does not actually steps the field.
  const Field::StepInfo& info = field_.stepInfo();

  lift_offset_ = (float)field_.raiseProgress()/Field::RAISE_PROGRESS_MAX;

  // cursor
  if( field_.tick() % 15 == 0 ) {
    style_.tiles_cursor[ (field_.tick()/15) % 2 ].setToSprite(spr_cursor_, true);
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

  for(int x=0; x<FIELD_WIDTH; x++) {
    for(int y=1; y<=FIELD_WIDTH; y++) {
      const Block& bk = field_.block(x,y);
      // block bouncing
      if( bk.isState(BkColor::LAID) ) {
        crouch_dt_[x][y] = CROUCH_DURATION;
      } else if( bk.isState(BkColor::REST) && crouch_dt_[x][y] != 0 ) {
        crouch_dt_[x][y]--;
      } else {
        crouch_dt_[x][y] = 0;
      }
    }
  }

  // only play sounds for local players
  Player* pl = intf_.instance()->player(&field_);
  if(pl && pl->local()) {
    if(info.move) {
      sounds_.move.play();
    }
    if(info.swap) {
      sounds_.swap.both.stop();
      sounds_.swap.left.stop();
      sounds_.swap.right.stop();
      auto& swap_pos = field_.swapPos();
      if(field_.block(swap_pos.x, swap_pos.y).isNone()) {
        sounds_.swap.right.play();  // no left block, play 'right' sound
      } else if(field_.block(swap_pos.x + 1, swap_pos.y).isNone()) {
        sounds_.swap.left.play();  // no right block, play 'left' sound
      } else {
        sounds_.swap.both.play();  // both blocks, play 'both' sound
      }
    }
    if(info.blocks.laid) {
      sounds_.fall.play();
    }
    if(!info.blocks.popped.empty()) {
      unsigned int max_chain = sounds_.pops.size();
      for(auto& combo : info.blocks.popped) {
        auto& pop_sounds = sounds_.pops[std::min(combo.chain, max_chain) - 1];
        unsigned int max_pos = pop_sounds.size() - 1;
        pop_sounds[std::min(combo.pos, max_pos)].play();
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
    if( gb.gbid != (*gbd_it)->gbid() ) {
      break;
    }
    (*gbd_it)->step();
  }
  // move/insert other garbages
  for( ; gb_i<gb_nb; gb_i++ ) {
    const Garbage& gb = field_.hangingGarbage(gb_i);
    // find an already existing drawable
    GbHangingList::iterator gbd_it2;
    for( gbd_it2=gbd_it; gbd_it2!=gbw_drbs_.end() && gb.gbid != (*gbd_it2)->gbid(); ++gbd_it2 ) ;
    if( gbd_it2 == gbw_drbs_.end() ) {
      // not found: create and insert at the new position
      gbd_it = gbw_drbs_.insert(gbd_it, std::make_unique<GbHanging>(style_, gb));
    } else if( gbd_it != gbd_it2 ) {
      // found: move it at the right position (swap)
      gbd_it = gbw_drbs_.insert(gbd_it, std::move(*gbd_it2));
      gbw_drbs_.erase(gbd_it2);
    }
    (*gbd_it)->step();
    (*gbd_it)->setPosition(gb_i);
    gbd_it++;
  }
  // remove remaining garbages (those removed)
  if( gbd_it != gbw_drbs_.end() ) {
    gbw_drbs_.erase(gbd_it, gbw_drbs_.end());
  }

  // start countdown
  const ServerConf& server_conf = intf_.instance()->conf();
  if(field_.tick() < server_conf.tk_start_countdown) {
    unsigned int ticks = server_conf.tk_start_countdown - field_.tick();
    char buf[5];
    ::snprintf(buf, sizeof(buf), "%3.2f", (ticks * server_conf.tk_usec)/1000000.);
    text_start_countdown_->setString(buf);
  } else if(field_.tick() == server_conf.tk_start_countdown) {
    text_start_countdown_.reset();
  }
}


void FieldDisplay::doRank()
{
  assert(!text_rank_sign_);
  assert(field_.rank());
  const ResourceManager& res_mgr = intf_.res_mgr();

  text_rank_sign_ = std::make_unique<sf::Text>();

  // use different text/style depending on player count
  // default to "Lose"
  std::string lang_key = "Lose";
  const StyleText* style = &style_.rank_sign_style.lose;
  unsigned int field_count = intf_.instance()->match().fields().size();
  unsigned int rank = field_.rank();
  if(field_count == 1) {
    // lose, this is the default
  } else if(field_count == 2) {
    if(rank == 1) {
      // detect draws (all players are first)
      bool draw = true;
      for(const auto& field : intf_.instance()->match().fields()) {
        LOG("field %d: %d", field->fldid(), field->rank());
        draw = draw && field->rank() == 1;
      }
      if(draw) {
        lang_key = "Draw";
        style = &style_.rank_sign_style.draw;
      } else {
        lang_key = "Win";
        style = &style_.rank_sign_style.win;
      }
    } else {
      // lose, this is the default
    }
  } else if(rank < StyleField::RANK_MAX) {
    lang_key = std::to_string(rank);
    if(rank-1 < style_.rank_sign_style.rank.size()) {
      style = &style_.rank_sign_style.rank[rank-1];
    }
  }

  text_rank_sign_->setString(res_mgr.getLang({"Rank", lang_key}));
  style->apply(*text_rank_sign_);
  sf::FloatRect r = text_rank_sign_->getLocalBounds();
  text_rank_sign_->setOrigin(r.width/2, 0);
  text_rank_sign_->setPosition(style_.bk_size * FIELD_WIDTH/2, style_.bk_size * 2);
}


void FieldDisplay::renderBlock(sf::RenderTarget& target, sf::RenderStates states, int x, int y) const
{
  const Block& bk = field_.block(x,y);
  if( bk.isNone() || bk.isState(BkColor::CLEARED) ) {
    return;  // nothing to draw
  }

  if( bk.isColor() ) {
    const StyleField::TilesBkColor& tiles = style_.tiles_bk_color[bk.bk_color.color];

    const ImageTile* tile = &tiles.normal; // default
    float dx = 0;
    if(field_.lost()) {
      tile = &tiles.mutate;
    } else if( bk.bk_color.state == BkColor::FLASH ) {
      if( (bk.ntick - field_.tick()) % 2 == 0 ) {
        tile = &tiles.flash;
      }
    } else if( bk.bk_color.state == BkColor::MUTATE ) {
      tile = &tiles.mutate;
    } else if( bk.swapped ) {
      dx = (field_.swapPos().x == x ? 1 : -1) * (float)field_.swapDelay()/(field_.conf().swap_tk+1);
    }

    unsigned int crouch_dt = crouch_dt_[x][y];
    if(crouch_dt == 0 || field_.lost()) {
      tile->render(target, states, x+dx, y, 1, 1, y ? sf::Color::White : sf::Color(96,96,96));
    } else {
      // bounce positions: -1 -> +1 (quick) -> 0 (slow)
      float bounce = ( crouch_dt > CROUCH_DURATION/2 )
        ? 4*(float)(CROUCH_DURATION-crouch_dt)/CROUCH_DURATION-1.0
        : 2*(float)crouch_dt/CROUCH_DURATION;
      this->renderBouncingBlock(target, states, x+dx, y, bounce, bk.bk_color.color);
    }

  } else if( bk.isGarbage() ) {
    const StyleField::TilesGb& tiles = style_.tiles_gb;
    const Garbage& gb = *bk.bk_garbage.garbage;
    const sf::Color& color = intf_.style().colors[gb.from ? gb.from->fldid() : 0];

    if( bk.bk_garbage.state == BkGarbage::FLASH ) {
      if( (bk.ntick - field_.tick()) % 2 == 0 ) {
       tiles.mutate.render(target, states, x, y, 1, 1, color);
      } else {
       tiles.flash.render(target, states, x, y, 1, 1, color);
      }
    } else if( bk.bk_garbage.state == BkGarbage::MUTATE ) {
      tiles.mutate.render(target, states, x, y, 1, 1, color);
    } else {
      bool center_mark = gb.size.x > 2 && gb.size.y > 1;
      const int rel_x = 2*(x-gb.pos.x);
      const int rel_y = 2*(y-gb.pos.y);

      // draw 4 sub-tiles
      const ImageTile* tile;

      // top left
      if( center_mark &&
         (rel_x == gb.size.x || rel_x == gb.size.x-1) &&
         (rel_y+1 == gb.size.y || rel_y+1 == gb.size.y-1)
        ) {
        int tx = (rel_x   == gb.size.x) ? 1 : 0;
        int ty = (rel_y+1 == gb.size.y) ? 0 : 1;
        tile = &tiles.center[tx][ty];
      } else {
        int tx = (x == gb.pos.x            ) ? 0 : 2;
        int ty = (y == gb.pos.y+gb.size.y-1) ? 0 : 2;
        tile = &tiles.tiles[tx][ty];
      }
      tile->render(target, states, x, y+0.5, 0.5, 0.5, color);

      // top right
      if( center_mark &&
         (rel_x+1 == gb.size.x || rel_x+1 == gb.size.x-1) &&
         (rel_y+1 == gb.size.y || rel_y+1 == gb.size.y-1)
        ) {
        int tx = (rel_x+1 == gb.size.x) ? 1 : 0;
        int ty = (rel_y+1 == gb.size.y) ? 0 : 1;
        tile = &tiles.center[tx][ty];
      } else {
        int tx = (x == gb.pos.x+gb.size.x-1) ? 3 : 1;
        int ty = (y == gb.pos.y+gb.size.y-1) ? 0 : 2;
        tile = &tiles.tiles[tx][ty];
      }
      tile->render(target, states, x+0.5, y+0.5, 0.5, 0.5, color);

      // bottom left
      if( center_mark &&
          (rel_x == gb.size.x || rel_x == gb.size.x-1) &&
          (rel_y == gb.size.y || rel_y == gb.size.y-1)
        ) {
        int tx = (rel_x == gb.size.x) ? 1 : 0;
        int ty = (rel_y == gb.size.y) ? 0 : 1;
        tile = &tiles.center[tx][ty];
      } else {
        int tx = (x == gb.pos.x) ? 0 : 2;
        int ty = (y == gb.pos.y) ? 3 : 1;
        tile = &tiles.tiles[tx][ty];
      }
      tile->render(target, states, x, y, 0.5, 0.5, color);

      // bottom right
      if( center_mark &&
         (rel_x+1 == gb.size.x || rel_x+1 == gb.size.x-1) &&
         (rel_y == gb.size.y || rel_y == gb.size.y-1)
        ) {
        int tx = (rel_x+1 == gb.size.x) ? 1 : 0;
        int ty = (rel_y   == gb.size.y) ? 0 : 1;
        tile = &tiles.center[tx][ty];
      } else {
        int tx = (x == gb.pos.x+gb.size.x-1) ? 3 : 1;
        int ty = (y == gb.pos.y            ) ? 3 : 1;
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
  style_.sign_style.apply(txt_);
  txt_.setString(chain ? 'x'+std::to_string(val) : std::to_string(val));
  txt_.setFillColor(sf::Color::White);
  sf::FloatRect txt_rect = txt_.getLocalBounds();
  float txt_sx = std::min(1.0, 0.8*style_.bk_size / txt_rect.width);
  txt_.setOrigin(txt_rect.left + txt_rect.width/2, txt_rect.top + txt_rect.height/2);
  txt_.setScale(txt_sx, 1);

  // initialize sprite
  if( chain ) {
    style_.tiles_signs.chain.setToSprite(bg_, true);
  } else {
    style_.tiles_signs.combo.setToSprite(bg_, true);
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
  if( gb.type == Garbage::Type::CHAIN ) {
    style_.tiles_gb_hanging.line.setToSprite(bg_, true);
  } else if( gb.type == Garbage::Type::COMBO ) {
    style_.tiles_gb_hanging.blocks[gb.size.x-1].setToSprite(bg_, true);
  } else {
    //TODO not handled yet
  }
  bg_.setColor(style_.global->colors[gb_.from ? gb_.from->fldid() : 0]);
  //XXX store/cache text style information on StyleField
  style_.gb_hanging_style.apply(txt_);
  txt_.setFillColor(sf::Color::White);

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
  if( gb_.type != Garbage::Type::CHAIN || gb_.size.y < 2 ) {
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

