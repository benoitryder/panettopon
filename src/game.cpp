#include <memory>
#include "game.h"
#include "netplay.h"
#include "inifile.h"
#include "log.h"


bool FieldConf::isValid() const
{
  // raise_change_speed values must be increasing
  uint16_t last_tk = 0;
  for(uint16_t tk : raise_speed_changes) {
    if(tk <= last_tk) {
      return false;
    }
    last_tk = tk;
  }
  return swap_tk > 0
      && manual_raise_speed > 0
      && raise_speeds.size() == raise_speed_changes.size() + 1
      && (stop_combo_0 > 0 || stop_combo_k > 0)
      && (stop_chain_0 > 0 || stop_chain_k > 0)
      && gb_hang_tk > 0
      && flash_tk > 0
      && levitate_tk > 0
      && pop_tk > 0
      && pop0_tk > 0
      && transform_tk > 0
      && color_nb > 3 && color_nb < 16;
}

void FieldConf::fromPacket(const netplay::FieldConf& pkt)
{
  name = pkt.name();
#define FIELD_CONF_EXPR_INIT(n,ini) \
  n = pkt.n();
  FIELD_CONF_APPLY(FIELD_CONF_EXPR_INIT);
#undef FIELD_CONF_EXPR_INIT
  raise_speeds.clear();
  std::copy(pkt.raise_speeds().begin(), pkt.raise_speeds().end(), std::back_inserter(raise_speeds));
  raise_speed_changes.clear();
  std::copy(pkt.raise_speed_changes().begin(), pkt.raise_speed_changes().end(), std::back_inserter(raise_speed_changes));
  raise_adjacent = static_cast<RaiseAdjacent>(pkt.raise_adjacent());
  if( !this->isValid() ) {
    throw netplay::CallbackError("invalid configuration");
  }
}

void FieldConf::toPacket(netplay::FieldConf* pkt) const
{
  pkt->set_name(name);
#define FIELD_CONF_EXPR_INIT(n,ini) \
  pkt->set_##n(n);
  FIELD_CONF_APPLY(FIELD_CONF_EXPR_INIT);
#undef FIELD_CONF_EXPR_INIT
  std::copy(raise_speeds.begin(), raise_speeds.end(), google::protobuf::RepeatedFieldBackInserter(pkt->mutable_raise_speeds()));
  std::copy(raise_speed_changes.begin(), raise_speed_changes.end(), google::protobuf::RepeatedFieldBackInserter(pkt->mutable_raise_speed_changes()));
  pkt->set_raise_adjacent(static_cast<netplay::FieldConf::RaiseAdjacent>(raise_adjacent));
}

void FieldConf::fromIniFile(const IniFile& cfg, const std::string& section)
{
  // name is set by the caller
#define FIELD_CONF_EXPR_INIT(n,ini) \
  this->n = cfg.get<decltype(this->n)>({section, #ini});
  FIELD_CONF_APPLY(FIELD_CONF_EXPR_INIT);
#undef FIELD_CONF_EXPR_INIT
  raise_speeds = cfg.get<std::vector<uint16_t>>({section, "RaiseSpeeds"});
  raise_speed_changes = cfg.get<std::vector<uint16_t>>({section, "RaiseSpeedChanges"});

  const std::string s_raise_adjacent = cfg.get<std::string>({section, "RaiseAdjacent"});
  if(s_raise_adjacent == "never") {
    raise_adjacent = ADJACENT_NEVER;
  } else if(s_raise_adjacent == "always") {
    raise_adjacent = ADJACENT_ALWAYS;
  } else if(s_raise_adjacent == "alternate") {
    raise_adjacent = ADJACENT_ALTERNATE;
  } else {
    throw std::runtime_error("invalid RaiseAdjacent value: "+s_raise_adjacent);
  }
  if(!this->isValid()) {
    throw std::runtime_error("invalid configuration: "+section);
  }
}


Field::Field(FldId fldid, const FieldConf& conf, uint32_t seed):
    fldid_(fldid), seed_(seed), rank_(0),
    enable_swap_(true), enable_raise_(true),
    conf_(conf)
{
  ::memset(grid_, 0, sizeof(grid_));
}

Field::~Field()
{
}


void Field::initMatch()
{
  cursor_ = FieldPos(2,6);
  swap_ = FieldPos();
  swap_dt_ = 0;
  chain_ = 1;
  tick_ = 0;
  lost_ = false;
  lost_dt_ = 0;
  key_state_ = GAME_KEY_NONE;
  key_repeat_ = 0;
  raise_progress_ = 0;
  raise_speed_index_ = 0;
  manual_raise_ = false;
  stop_dt_ = 0;
  transformed_nb_ = 0;
  raised_lines_ = 0;
  ::memset(gb_drop_pos_, 0, sizeof(gb_drop_pos_));

  step_info_ = StepInfo();
  enable_swap_ = false;
  enable_raise_ = false;
}



Field::StepInfo::StepInfo():
    combo(0), chain(1), raised(false), swap(false), move(false)
{
}


void Field::step(KeyState keys)
{
  assert( !lost_ );

  step_info_ = StepInfo();

  tick_++;

  int x, y;
  bool full = false; // is field full? outdated after block falling
  for( x=0; x<FIELD_WIDTH; x++ ) {
    if( block(x, FIELD_HEIGHT).type ) {
      full = true;
      break;
    }
  }

  bool raise = enable_raise_ && !this->isSwapping();
  bool stop_dec = true;

  unsigned int color_pop = 0; // a group will pop if != 0
  unsigned int garbage_pop = 0; // garbages will pop if != 0

  // evolution of blocks
  //TODO swapping a falling chaining block
  // above block still chains, further above blocks don't

  for( y=1; y<=FIELD_HEIGHT; y++ ) {
    for( x=0; x<FIELD_WIDTH; x++ ) { // order matters, for garbage processing
      Block* bk = &grid_[x][y];
      if( bk->isNone() ) {
        continue;
      }

      // TODO check condition and moment for auto-raise
      if( stop_dec ) {
        if( bk->isState(BkColor::FLASH) ||
            bk->isState(BkGarbage::FLASH) ) {
          raise = stop_dec = false; //TODO:check
        } else if(
            raise && !bk->isState(BkColor::REST) &&
            !bk->isState(BkGarbage::REST) ) {
          raise = false;
        }
      }

      // swapped block, don't "evolve"
      if( bk->swapped ) {
        continue;
      }

      Block* bk2 = &grid_[x][y-1]; // above block

      // color blocks
      if( bk->isColor() ) {
        BkColor* bkc = &bk->bk_color;
        if( bkc->state == BkColor::REST ) {
          if( bk2->swapped ) {
            // do nothing
            //XXX not on graph?
          } else if( bk2->isNone() ) {
            bkc->state = BkColor::LEVITATE;
            bk->chaining = false;
            bk->ntick = tick_ + conf_.levitate_tk;
          } else if( bk2->isState(BkColor::LEVITATE) ) {
            bkc->state = BkColor::LEVITATE;
            bk->chaining = bk2->chaining;
            bk->ntick = bk2->ntick;
          } else if( bk->chaining ) {
            // remove chain flag (kept after laid state)
            bkc->state = BkColor::REST;
            bk->chaining = false;
          }
        } else if( bkc->state == BkColor::LEVITATE ) {
          if( tick_ >= bk->ntick ) {
            if( bk2->isNone() ) {
              bk2->type = Block::COLOR;
              bk2->bk_color.state = BkColor::FALL;
              bk2->bk_color.color = bkc->color;
              bk2->chaining = bk->chaining;
              bk2->ntick = 0;
              *bk = Block();
            } else {
              bkc->state = BkColor::LAID;
              bk->ntick = 0;
            }
          } else if( bk2->isState(BkColor::LEVITATE) ) {
            // swapping blocks below chaining falling block don't cancel chain
            bkc->state = BkColor::LEVITATE;
            bk->chaining = bk->chaining || bk2->chaining;
            bk->ntick = bk2->ntick;
          }
        } else if( bkc->state == BkColor::FALL ) {
          if( bk2->isNone() ) {
            *bk2 = *bk;
            bk->type = Block::NONE;
            bk->chaining = false;
            bk->ntick = 0;
          } else if( bk2->isState(BkColor::LEVITATE) ) {
            // swapping blocks below chaining falling block don't cancel chain
            bkc->state = BkColor::LEVITATE;
            bk->ntick = bk2->ntick;
          } else {
            bkc->state = BkColor::LAID;
            bk->ntick = 0;
          }
        } else if( bkc->state == BkColor::LAID ) {
          if( bk2->isNone() ) {
            bkc->state = BkColor::LEVITATE;
            bk->ntick = tick_ + conf_.levitate_tk;
          } else if( bk2->isState(BkColor::LEVITATE) ) {
            bkc->state = BkColor::LEVITATE;
            bk->chaining = bk2->chaining;
            bk->ntick = bk2->ntick;
          } else {
            bkc->state = BkColor::REST;
          }
        }
        else if( bk->ntick != 0 && tick_ >= bk->ntick ) {
          // matching, tick events
          if( bkc->state == BkColor::FLASH ) {
            bkc->state = BkColor::MUTATE;
            bk->ntick = 0;
            // ntick and group_pos are set below
            color_pop++;
          } else if( bkc->state == BkColor::MUTATE ) {
            bkc->state = BkColor::CLEARED;
            bk->ntick = tick_ + bk->group_pos * conf_.pop_tk + 1;
          } else if( bkc->state == BkColor::CLEARED ) {
            bk->type = Block::NONE;
            bk->chaining = false;
            bk->ntick = 0;
            // above blocks: levitate + chain
            int yy;
            for( yy=y+1; yy<FIELD_HEIGHT; yy++ ) {
              Block* bk3 = &grid_[x][yy];
              if( !bk3->isState(BkColor::REST) && !bk3->isState(BkColor::LAID) ) {
                break;
              }
              bk3->bk_color.state = BkColor::LEVITATE;
              bk3->chaining = true;
              bk3->ntick = tick_ + conf_.levitate_tk;
            }
          } else if( bkc->state == BkColor::TRANSFORMED ) {
            bkc->state = BkColor::LEVITATE;
            bk->chaining = true;
            bk->ntick = tick_ + conf_.transform_tk;
          }
        }
      }

      // garbages
      else if( bk->isGarbage() ) {
        BkGarbage* bkg = &bk->bk_garbage;
        Garbage* gb = bkg->garbage;
        if( bkg->state == BkGarbage::REST ) {
          if( bk2->isNone() || bk2->isState(BkGarbage::FALL) ) {
            // all below blocks are identical
            // Block::NONE and BkGarbage::FALL should not be mixed
            int xx;
            for( xx=gb->pos.x+1; xx<gb->pos.x+gb->size.x; xx++ ) {
              const Block& bk_it = block(xx,y-1);
              if( bk_it.type != bk2->type ||
                 (bk_it.type == Block::GARBAGE &&
                  bk_it.bk_garbage.state != bk2->bk_garbage.state) ) {
                break;
              }
            }
            if( xx == gb->pos.x+gb->size.x ) {
              this->setGarbageState(gb, BkGarbage::FALL);
            }
          }
          // skip processed blocks (before pos.y update)
          //note: does not work if size.y>1 && size.x<FIELD_WIDTH
          x = gb->pos.x + gb->size.x-1;
          y = gb->pos.y + gb->size.y-1; // for combos value does not change
        } else if( bkg->state == BkGarbage::FALL ) {
          int xx;
          for( xx=gb->pos.x; xx<gb->pos.x+gb->size.x; xx++ ) {
            if( !block(xx,y-1).isNone() ) {
              break;
            }
          }
          if( xx == gb->pos.x+gb->size.x ) {
            this->fallGarbage(gb);
          } else {
            this->setGarbageState(gb, BkGarbage::REST);
          }

          // skip processed blocks (before pos.y update)
          //note: does not work if size.y>1 && size.x<FIELD_WIDTH
          x = gb->pos.x + gb->size.x-1;
          y = gb->pos.y + gb->size.y-1; // for combos value does not change
        }
        else if( bk->ntick != 0 && tick_ >= bk->ntick ) {
          // matching, tick events
          if( bkg->state == BkGarbage::FLASH ) {
            bkg->state = BkGarbage::MUTATE;
            bk->chaining = bk->chaining;
            bk->ntick = 0;
            // ntick and group_pos are set below
            garbage_pop++;
          } else if( bkg->state == BkGarbage::MUTATE ) {
            if( y < gb->pos.y ) {
              this->transformGarbage(x, y);
            } else {
              bkg->state = BkGarbage::TRANSFORMED;
              bk->ntick = tick_ + bk->group_pos * conf_.pop_tk + 1;
            }
          } else if( bkg->state == BkGarbage::TRANSFORMED ) {
            bkg->state = BkGarbage::REST;
            bk->ntick = 0;
          }
        }
      }
    }
  }

  // matching

  // color, 0x80 (nothing), 0x40 mask if already matched
  uint8_t match[FIELD_WIDTH][FIELD_HEIGHT+1];
  for( x=0; x<FIELD_WIDTH; x++ ) {
    for( y=1; y<=FIELD_HEIGHT; y++ ) {
      const Block& bk = block(x,y);
      match[x][y] = bk.isState(BkColor::REST) && !bk.swapped
          ? bk.bk_color.color : 0x80;
    }
  }

  uint8_t color = -1;
  int p0;
  // vertical
  for( x=0; x<FIELD_WIDTH; x++ ) {
    p0 = -1;
    for( y=1; y<=FIELD_HEIGHT; y++ ) {
      uint8_t c = match[x][y] & 0x8f;
      if( c != 0x80 && (p0 != -1 && c == color) ) {
        continue;
      }
      // end of group
      if( p0 != -1 && y-p0 >= 3 ) {
        while( p0 < y ) {
          match[x][p0++] |= 0x40;
        }
      }

      if( c == 0x80 ) {
        p0 = -1;
      } else {
        p0 = y;
        color = c;
      }
    }
    // end of group
    if( p0 != -1 && y-p0 >= 3 ) {
      while( p0 < y ) {
        match[x][p0++] |= 0x40;
      }
    }
  }
  // horizontal
  for( y=1; y<=FIELD_HEIGHT; y++ ) {
    p0 = -1;
    for( x=0; x<FIELD_WIDTH; x++ ) {
      uint8_t c = match[x][y] & 0x8f;
      if( c != 0x80 && (p0 != -1 && c == color) ) {
        continue;
      }
      // end of group
      if( p0 != -1 && x-p0 >= 3 ) {
        while( p0 < x ) {
          match[p0++][y] |= 0x40;
        }
      }

      if( c == 0x80 ) {
        p0 = -1;
      } else {
        p0 = x;
        color = c;
      }
    }
    // end of group
    if( p0 != -1 && x-p0 >= 3 ) {
      while( p0 < x ) {
        match[p0++][y] |= 0x40;
      }
    }
  }

  // chains, combos, garbages
  bool chained = false;
  for( x=0; x<FIELD_WIDTH; x++ ) {
    for( y=1; y<=FIELD_HEIGHT; y++ ) {
      if( (match[x][y] & 0x40) == 0 ) {
        continue;
      }
      step_info_.combo++;
      chained = chained || block(x,y).chaining;
    }
  }

  if( step_info_.combo > 0 ) {
    for( x=0; x<FIELD_WIDTH; x++ ) {
      for( y=1; y<=FIELD_HEIGHT; y++ ) {
        if( (match[x][y] & 0x40) == 0 ) {
          continue;
        }
        Block* bk = &grid_[x][y];
        assert( bk->isColor() ); // not sure...
        bk->bk_color.state = BkColor::FLASH;
        bk->chaining = chained;
        bk->ntick = tick_ + conf_.flash_tk;

        // garbages
        if( x > 0 ) this->matchGarbage(&grid_[x-1][y], chained);
        if( x < FIELD_WIDTH-1 ) this->matchGarbage(&grid_[x+1][y], chained);
        if( y > 1 ) this->matchGarbage(&grid_[x][y-1], chained);
        if( y < FIELD_HEIGHT ) this->matchGarbage(&grid_[x][y+1], chained);
      }
    }

    if( chained ) {
      step_info_.chain = ++chain_;
    }
    LOG("[%u|%u] match +%d x%d", fldid_, tick_, step_info_.combo, step_info_.chain);
  }


  // Process dropping garbages
  if( !gbs_drop_.empty() && !full && raise ) {
    LOG("[%u|%u] gb: dropping", fldid_, tick_);
    //TODO drop condition: no drop when flashing/chain
    Garbage* gb = gbs_drop_.pop_front().release();
    gbs_field_.push_back(gb);
    gb->pos.y = FIELD_HEIGHT;

    Block bkgb;
    bkgb.type = Block::GARBAGE;
    bkgb.bk_garbage = (BkGarbage){ BkGarbage::REST, gb };
    if( gb->type == Garbage::TYPE_CHAIN ) {
      // new chain
      gb->pos.x = 0;
      for( x=0; x<FIELD_WIDTH; x++ ) {
        grid_[x][FIELD_HEIGHT] = bkgb;
      }
    } else if( gb->type == Garbage::TYPE_COMBO ) {
      // new combo
      int xx = gb_drop_pos_[gb->size.x];
      gb->pos.x = xx;
      for( x=0; x<gb->size.x; x++ ) {
        grid_[x+xx][FIELD_HEIGHT] = bkgb;
      }

      // iterate drop pos
      if( 2*gb->size.x > FIELD_WIDTH ) {
        xx++;
      } else {
        xx += gb->size.x;
      }
      if( xx + gb->size.x > FIELD_WIDTH ) {
        xx = 0;
      }
      gb_drop_pos_[gb->size.x] = xx;
    }
    raise = 0;
  }


  // Swap steps
  if( this->isSwapping() ) {
    if( --swap_dt_ == 0 ) {
      grid_[swap_.x  ][swap_.y].swapped = false;
      grid_[swap_.x+1][swap_.y].swapped = false;
      swap_ = FieldPos();
    }
  }


  // Input keys, one action per frame

  if(!enable_swap_) {
    keys &= ~GAME_KEY_SWAP;
  }
  if(!enable_raise_) {
    keys &= ~GAME_KEY_RAISE;
  }

  KeyState keys_input = keys; // candidate processed keys
  if( keys == key_state_ ) {
    //XXX:minor overflow check
    key_repeat_++;
  } else {
    key_repeat_ = 0;
    // keys pushed since last frame
    keys_input = (key_state_^keys)&keys;
    // update key state
    key_state_ = keys;
  }

  // process key
  if( (keys_input&GAME_KEY_MOVE) && (key_repeat_ == 0 || key_repeat_ >= 10) ) {
    if( keys_input & GAME_KEY_UP ) {
      if( cursor_.y+1 < FIELD_HEIGHT ) {
        step_info_.move = true;
        cursor_.y++;
      }
    } else if( keys_input & GAME_KEY_DOWN ) {
      if( cursor_.y > 1 ) {
        step_info_.move = true;
        cursor_.y--;
      }
    } else if( keys_input & GAME_KEY_LEFT ) {
      if( cursor_.x > 0 ) {
        step_info_.move = true;
        cursor_.x--;
      }
    } else if( keys_input & GAME_KEY_RIGHT ) {
      if( cursor_.x+1 < FIELD_WIDTH-1 ) {
        step_info_.move = true;
        cursor_.x++;
      }
    }
  } else if( (keys_input & GAME_KEY_SWAP) && key_repeat_ == 0 ) {
    const FieldPos& p = cursor_;
    Block* bk1 = &grid_[p.x  ][p.y];
    Block* bk2 = &grid_[p.x+1][p.y];

    if( // swappable
        ( bk1->isNone() || bk1->isState(BkColor::REST) || bk1->isState(BkColor::FALL) ) &&
        ( bk2->isNone() || bk2->isState(BkColor::REST) || bk2->isState(BkColor::FALL) ) &&
        // don't swap two empty blocks
        ( !bk1->isNone() || !bk2->isNone() ) &&
        // not under a levitating block
        !( p.y < FIELD_HEIGHT && (
                grid_[p.x  ][p.y+1].isState(BkColor::LEVITATE) ||
                grid_[p.x+1][p.y+1].isState(BkColor::LEVITATE) )
         ) ) {
      // cancel previous swap, if any
      if( this->isSwapping() ) {
        grid_[swap_.x  ][swap_.y].swapped = false;
        grid_[swap_.x+1][swap_.y].swapped = false;
      }
      // new swap
      Block bk = *bk1;
      *bk1 = *bk2;
      *bk2 = bk;
      swap_ = cursor_;
      swap_dt_ = conf_.swap_tk;
      bk1->swapped = true;
      bk2->swapped = true;
      step_info_.swap = true;
    }
  } else if( keys & GAME_KEY_RAISE ) {
    // key-up not required, fallback action
    manual_raise_ = true;
    stop_dt_ = 0;
  }


  // set timing on the new popping group
  // after swapping, on purpose
  if( color_pop || garbage_pop ) {
    // color blocks: from top-left to bottom-right
    Tick tick_pop = tick_ + conf_.pop0_tk;
    for( y=FIELD_HEIGHT; y>0; y-- ) {
      for( x=0; x<FIELD_WIDTH; x++ ) {
        Block* bk = &grid_[x][y];
        if( !bk->isState(BkColor::MUTATE) || bk->ntick != 0 ) {
          continue;
        }
        bk->ntick = tick_pop;
        bk->group_pos = --color_pop;
        tick_pop += conf_.pop_tk;
      }
    }

    // garbages: from bottom-right to top-left
    // ntick already incremented
    for( y=1; y<=FIELD_HEIGHT; y++ ) {
      for( x=FIELD_WIDTH-1; x>=0; x-- ) {
        Block* bk = &grid_[x][y];
        if( !bk->isState(BkGarbage::MUTATE) || bk->ntick != 0 ) {
          continue;
        }
        bk->ntick = tick_pop;
        bk->group_pos = --garbage_pop;
        tick_pop += conf_.pop_tk;
      }
    }
  }


  // reset chain count
  // after adding chain flag above removed blocks
  if( chain_ > 1 && step_info_.combo == 0 ) {
    // reset chain count
    bool cancel = true;
    for( x=0; cancel && x<FIELD_WIDTH; x++ ) {
      for( y=1; y<=FIELD_HEIGHT; y++ ) {
        // garbages keep chain if matched with a chain
        // transformed color blocks always keep chain
        if( grid_[x][y].chaining ) {
          cancel = false;
          break;
        }
      }
    }
    if( cancel ) {
      LOG("[%u|%u] end of chain", fldid_, tick_);
      chain_ = 1;
    }
  }

  if( step_info_.combo > 0 ) {
    // cancel manual raise on match
    manual_raise_ = false;
    // update stop ticks
    if( step_info_.combo > 3 ) {
      unsigned int tk = conf_.stop_combo_0+conf_.stop_combo_k*(step_info_.combo-4);
      if( tk > stop_dt_ ) {
        stop_dt_ = tk;
      }
    }
    if( step_info_.chain > 1 ) {
      unsigned int tk = conf_.stop_chain_0+conf_.stop_chain_k*(step_info_.chain-2);
      if( tk > stop_dt_ ) {
        stop_dt_ = tk;
      }
    }
  } else if(stop_dec && stop_dt_ > 0) {
    --stop_dt_;
  } else if(stop_dec && full && raise) {
    // field lost?
    if(lost_dt_ == 0) {
      lost_dt_ = conf_.lost_tk;
    } else {
      lost_dt_--;
    }
    // not in the "else" above, to support lost_tk == 0
    if(lost_dt_ == 0) {
      lost_ = true;
      chain_ = 1; // just in case
      return;
    }
  } else if(!full && raise && stop_dt_ == 0) {
    lost_dt_ = 0;
    raise_progress_ += manual_raise_ ? conf_.manual_raise_speed : conf_.raise_speeds[raise_speed_index_];
    while(raise_progress_ > RAISE_PROGRESS_MAX) {
      this->raise();
    }
  }

  // update raise speed if needed
  if(raise_speed_index_ < conf_.raise_speed_changes.size() &&
     tick_ >= conf_.raise_speed_changes[raise_speed_index_]) {
    raise_speed_index_++;
    LOG("[%u|%u] speed up", fldid_, tick_);
  }
}


void Field::waitGarbageDrop(Garbage* gb)
{
  LOG("[%u|%u] waitGarbageDrop(%u)", fldid_, tick_, gb->gbid);
  this->removeHangingGarbage(gb);
  gbs_wait_.push_back(gb);
}

void Field::dropNextGarbage()
{
  LOG("[%u|%u] dropNextGarbage()", fldid_, tick_);
  Garbage* gb = gbs_wait_.pop_front().release();
  gb->gbid = 0;
  gbs_drop_.push_back(gb);
}

void Field::insertHangingGarbage(Garbage* gb, unsigned int pos)
{
  LOG("[%u|%u] insertHangingGarbage(%u, %u)", fldid_, tick_, gb->gbid, pos);
  gbs_hang_.insert( gbs_hang_.begin()+pos, gb );
}

void Field::removeHangingGarbage(Garbage* gb)
{
  LOG("[%u|%u] removeHangingGarbage(%u)", fldid_, tick_, gb->gbid);
  GarbageList::iterator it;
  for( it=gbs_hang_.begin(); it!=gbs_hang_.end(); ++it ) {
    if( &(*it) == gb ) {
      gbs_hang_.release(it).release();
      return;
    }
  }
  assert( !"garbage not found" );
}


void Field::fillRandom(int n)
{
  // default content, including raising line
  // raising line last, for above-block constraint
  int x, y;
  for( y=n; y>=0; y-- ) {
    for( x=0; x<FIELD_WIDTH; x++ ) {
      this->setRaiseColor(x, y);
    }
  }
}

void Field::abort()
{
  lost_ = true;
}


void Field::setGridContentToPacket(google::protobuf::RepeatedPtrField<netplay::PktPlayerField_Block>* grid)
{
  grid->Clear();
  grid->Reserve(FIELD_WIDTH*(FIELD_HEIGHT+1));
  int x, y;
  for( y=0; y<=FIELD_HEIGHT; y++ ) {
    for( x=0; x<FIELD_WIDTH; x++ ) {
      netplay::PktPlayerField_Block* np_bk = grid->Add();
      const Block& bk = block(x,y);
      np_bk->set_swapped( bk.swapped );
      np_bk->set_chaining( bk.chaining );
      np_bk->set_ntick( bk.ntick );
      if( bk.type == Block::COLOR ) {
        netplay::PktPlayerField_BkColor* np_bk_color = np_bk->mutable_bk_color();
        const BkColor& bk_color = bk.bk_color;
        np_bk_color->set_state( static_cast<netplay::PktPlayerField_BkColor::State>(bk_color.state) );
        np_bk_color->set_color( bk_color.color );
      } else if( bk.type == Block::GARBAGE ) {
        netplay::PktPlayerField_BkGarbage* np_bk_garbage = np_bk->mutable_bk_garbage();
        const BkGarbage& bk_garbage = bk.bk_garbage;
        np_bk_garbage->set_state( static_cast<netplay::PktPlayerField_BkGarbage::State>(bk_garbage.state) );
      } else {
        // NONE, do nothing
      }
    }
  }
}

bool Field::setGridContentFromPacket(const google::protobuf::RepeatedPtrField<netplay::PktPlayerField_Block>& grid)
{
  if( grid.size() != FIELD_WIDTH*(FIELD_HEIGHT+1) ) {
    return false;
  }
  int x, y;
  google::protobuf::RepeatedPtrField<netplay::PktPlayerField_Block>::const_iterator it = grid.begin();
  for( y=0; y<=FIELD_HEIGHT; y++ ) {
    for( x=0; x<FIELD_WIDTH; x++ ) {
      Block* bk = &grid_[x][y];
      const netplay::PktPlayerField_Block& np_bk = (*it++);
      if( np_bk.has_bk_color() ) {
        if( np_bk.has_bk_garbage() ) {
          return false; // mutually exclusive fields
        }
        const netplay::PktPlayerField_BkColor& np_bk_color = np_bk.bk_color();
        bk->type = Block::COLOR;
        BkColor* bk_color = &bk->bk_color;
        bk_color->state = static_cast<BkColor::State>(np_bk_color.state());
        bk_color->color = np_bk_color.color();
      } else if ( np_bk.has_bk_garbage() ) {
        assert( !"not supported yet" );
      } else {
        bk->type = Block::NONE;
      }
      // invalid states may be set (eg. type=GARBAGE, swapped=true)
      bk->swapped = np_bk.swapped();
      bk->chaining = np_bk.chaining();
      bk->ntick = np_bk.ntick();
    }
  }
  return true;
}


void Field::raise()
{
  LOG("[%u|%u] raise", fldid_, tick_);

  int x;
  for( x=0; x<FIELD_WIDTH; x++ ) {
    int y;
    for( y=FIELD_HEIGHT; y>0; y-- ) {
      grid_[x][y] = grid_[x][y-1];
    }
    this->setRaiseColor(x);
  }
  if( cursor_.y+1 < FIELD_HEIGHT ) {
    cursor_.y++;
  }

  // raise while swapping should not happen, we support it though
  if( this->isSwapping() ) {
    if( swap_.y == FIELD_HEIGHT ) {
      // should not happen
      swap_.x = swap_.y = -1;
      swap_dt_ = 0;
    }
    else {
      swap_.y++;
    }
  }

  boost::ptr_list<Garbage>::iterator gb_it;
  for( gb_it=gbs_field_.begin(); gb_it!=gbs_field_.end(); ++gb_it ) {
    gb_it->pos.y++;
  }

  step_info_.raised = true;
  raise_progress_ = 0;
  manual_raise_ = false;
  raised_lines_++;
}


void Field::setRaiseColor(int x, int y)
{
  // Depending on the mode, bad block is the 1st or 2nd left one.
  //XXX if y>0, the method is used to fill the screen (at init) and mode
  // 'never' is always used
  const int bad_dx = ( y == 0 &&
      ( conf_.raise_adjacent == FieldConf::ADJACENT_ALWAYS ||
       ( conf_.raise_adjacent == FieldConf::ADJACENT_ALTERNATE &&
        raised_lines_ % 2 == 0 ) ) ) ? 2 : 1;
  int bad_color1 = -1;
  if( x >= bad_dx ) {
    const Block& bk = block(x-bad_dx,y);
    if( bk.type == Block::COLOR ) {
      bad_color1 = bk.bk_color.color;
    }
  }
  int bad_color2 = -1;
  if( y < FIELD_HEIGHT ) {
    const Block& bk = block(x,y+1);
    if( bk.type == Block::COLOR ) {
      bad_color2 = bk.bk_color.color;
    }
  }

  Block* bk = &grid_[x][y];
  bk->type = Block::COLOR;
  for(;;) {
    int color = this->rand() % conf_.color_nb;
    if( color == bad_color1 || color == bad_color2 ) {
      continue;
    }
    bk->bk_color.color = color;
    break;
  }
  bk->bk_color.state = BkColor::REST;
  bk->ntick = 0;
}


void Field::setGarbageState(const Garbage* gb, BkGarbage::State st)
{
  int x, y;
  for( x=gb->pos.x; x<gb->pos.x+gb->size.x; x++ ) {
    for( y=gb->pos.y; y<gb->pos.y+gb->size.y && y<=FIELD_HEIGHT; y++ ) {
      grid_[x][y].bk_garbage.state = st;
    }
  }
}

void Field::fallGarbage(Garbage* gb)
{
  int x;
  // for chains: no need to modify "middle" lines
  // bottom line: copy and set ntick
  // (ntick is only used on the bottom line)
  for( x=gb->pos.x; x<gb->pos.x+gb->size.x; x++ ) {
    grid_[x][gb->pos.y-1] = grid_[x][gb->pos.y];
    grid_[x][gb->pos.y-1].ntick = tick_+1;
  }

  if( gb->pos.y+gb->size.y-1 <= FIELD_HEIGHT ) {
    // top line: empty
    for( x=gb->pos.x; x<gb->pos.x+gb->size.x; x++ ) {
      grid_[x][gb->pos.y+gb->size.y-1] = Block();
    }
  } else {
    // add new line (at most 1 per frame)
    Block* bk = &grid_[gb->pos.x][gb->pos.y];
    for( x=gb->pos.x; x<gb->pos.x+gb->size.x; x++ ) {
      grid_[x][FIELD_HEIGHT] = *bk;
    }
  }

  gb->pos.y--;
}

void Field::matchGarbage(Block* bk, bool chained)
{
  if( !bk->isState(BkGarbage::REST) ) {
    return;
  }
  Garbage* gb = bk->bk_garbage.garbage;

  // update block states
  Block bk_match;
  bk_match.type = Block::GARBAGE;
  bk_match.bk_garbage.state = BkGarbage::FLASH;
  bk_match.bk_garbage.garbage = gb;
  bk_match.chaining = chained;
  bk_match.ntick = tick_ + conf_.flash_tk;

  int x, y;
  for( x=0; x<gb->size.x; x++ ) {
    for( y=0; y<gb->size.y && gb->pos.y+y<=FIELD_HEIGHT; y++ ) {
      grid_[gb->pos.x+x][gb->pos.y+y] = bk_match;
    }
  }

  // match adjacent garbages
  if( gb->pos.x > 0 ) {
    for( y=0; y<gb->size.y && gb->pos.y+y<=FIELD_HEIGHT; y++ ) {
      this->matchGarbage(&grid_[gb->pos.x-1][gb->pos.y+y], chained);
    }
  }
  if( gb->pos.x+gb->size.x < FIELD_WIDTH ) {
    for( y=0; y<gb->size.y && gb->pos.y+y<=FIELD_HEIGHT; y++ ) {
      this->matchGarbage(&grid_[gb->pos.x+gb->size.x][gb->pos.y+y], chained);
    }
  }
  if( gb->pos.y > 0 ) {
    for( x=0; x<gb->size.x; x++ ) {
      this->matchGarbage(&grid_[gb->pos.x+x][gb->pos.y-1], chained);
    }
  }
  if( gb->pos.y+gb->size.y <= FIELD_HEIGHT ) {
    for( x=0; x<gb->size.x; x++ ) {
      this->matchGarbage(&grid_[gb->pos.x+x][gb->pos.y+gb->size.y], chained);
    }
  }

  gb->size.y--;
  gb->pos.y++;
}

void Field::transformGarbage(int x, int y)
{
  Block* bk = &grid_[x][y];
  int color = -1;
  if( ++transformed_nb_ == FIELD_WIDTH-1 ) {
    transformed_nb_ = 0;
    // get below color
    int yy;
    for( yy=y-1; yy>=0; yy-- ) {
      const Block& bk2 = block(x,yy);
      if( bk2.isNone() ) {
        continue;
      }
      if( !bk2.isState(BkColor::MUTATE) && !bk2.isState(BkColor::FLASH) ) {
        color = bk2.bk_color.color;
        break;
      }
    }
  }

  // no forced color, draw a random one
  if( color == -1 ) {
    for(;;) {
      color = this->rand() % conf_.color_nb;
      if( x < FIELD_WIDTH-1 ) {
        const Block& bk2 = block(x+1,y);
        if( bk2.type == Block::COLOR && color == bk2.bk_color.color ) {
          continue;
        }
      }
      if( y > 0 ) { // always true
        const Block& bk2 = block(x,y-1);
        if( bk2.type == Block::COLOR && color == bk2.bk_color.color ) {
          continue;
        }
      }
      break;
    }
  }

  // remove garbage if it was its last block
  Garbage* gb = bk->bk_garbage.garbage;
  if( gb->size.y == 0 && x == gb->pos.x ) {
    // ptr_list uses objects but we need to compare pointers
    boost::ptr_list<Garbage>::iterator it;
    for( it=gbs_field_.begin(); it!=gbs_field_.end(); ++it ) {
      if( &(*it) == gb ) {
        gbs_field_.erase(it);
        break;
      }
    }
  }

  bk->type = Block::COLOR;
  bk->bk_color.state = BkColor::TRANSFORMED;
  bk->bk_color.color = color;
  // chaining: unchanged
  bk->ntick = tick_ + bk->group_pos * conf_.pop_tk + 2;
}


uint32_t Field::rand()
{
  seed_ = 1103515245*seed_ + 12345;
  return (uint32_t)(seed_/65536) % 32768;
}


Match::Match():
    started_(false), tick_(0)
{
}

void Match::start()
{
  assert( !started_ );
  started_ = true;
  tick_ = 0;

  FieldContainer::iterator it;
  for( it=fields_.begin(); it!=fields_.end(); ++it ) {
    it->initMatch();
  }
}

void Match::stop()
{
  assert( started_ );
  started_ = false;
}

void Match::clear()
{
  assert(!started_);
  gbs_hang_.clear();
  gbs_wait_.clear();
  fields_.clear();
}

Field* Match::addField(const FieldConf& conf, uint32_t seed)
{
  assert( !started_ );
  Field* fld = new Field(fields_.size()+1, conf, seed);
  fields_.push_back(fld);
  return fld;
}

void Match::updateTick()
{
  // get the minimum tick value
  Tick ret = 0;
  FieldContainer::const_iterator it;
  for( it=fields_.begin(); it!=fields_.end(); ++it ) {
    if( it->lost() ) {
      continue;
    }
    if( ret == 0 || it->tick() < ret ) {
      ret = it->tick();
    }
  }
  // all fields lost, match tick is the max tick
  // it MUST be set to handle draw games
  if( ret == 0 ) {
    for( it=fields_.begin(); it!=fields_.end(); ++it ) {
      if( it->tick() > ret ) {
        ret = it->tick();
      }
    }
  }

  tick_ = ret;
}

bool Match::updateRanks(std::vector<const Field*>& ranked)
{
  // common case: no draw, no simultaneous death, 1 tick at a time
  // ranking algorithm does not have to be optimized for special cases
  std::vector<Field*> to_rank;
  to_rank.reserve(fields_.size());
  unsigned int no_rank_nb = 0;
  FieldContainer::iterator it;
  for( it=fields_.begin(); it!=fields_.end(); ++it ) {
    if( it->rank() != 0 ) {
      continue;
    }
    no_rank_nb++;
    if( it->lost() && it->tick() <= tick_ ) {
      to_rank.push_back(&(*it));
    }
  }

  if( !to_rank.empty() ) {
    // best player first (lowest tick)
    std::sort(to_rank.begin(), to_rank.end(), [](const Field* a, const Field* b) { return a->tick() < b->tick(); });
    unsigned int rank = no_rank_nb - to_rank.size() + 1;
    std::vector<Field*>::iterator it;
    for( it=to_rank.begin(); it!=to_rank.end(); ++it ) {
      if( it!=to_rank.begin() && (*it)->tick() == (*(it-1))->tick() ) {
        (*it)->setRank( (*(it-1))->rank() );
      } else {
        (*it)->setRank( rank );
      }
      ranked.push_back(*it);
      rank++;
      no_rank_nb--;
    }
  }

  // one (or no) remaining player: end of match
  if( no_rank_nb < 2 ) {
    FieldContainer::iterator it;
    for( it=fields_.begin(); it!=fields_.end(); ++it ) {
      if( (*it).rank() != 0 ) {
        continue;
      }
      it->setRank(1);
      ranked.push_back(&(*it));
      break;
    }
    return true;
  }

  return false;
}


void Match::addGarbage(Garbage* gb, unsigned int pos)
{
  assert( gb->to != NULL );

  gb->to->insertHangingGarbage(gb, pos);
  gbs_hang_[gb->gbid] = gb;
}

void Match::waitGarbageDrop(const Garbage* gb)
{
  assert( gb->to != NULL );

  auto it = gbs_hang_.find(gb->gbid);
  assert( it != gbs_hang_.end() );
  Garbage* gb2 = it->second; // same as gb, but not const
  gbs_hang_.erase(it);
  gb->to->waitGarbageDrop(gb2);
  gbs_wait_[gb->gbid] = gb2;
}


GarbageDistributor::GarbageDistributor(Match& match, Observer& obs):
    match_(match), observer_(obs), current_gbid_(0)
{
}

void GarbageDistributor::reset()
{
  gbs_chain_.clear();
  targets_chain_.clear();
  targets_combo_.clear();
  drop_ticks_.clear();
}

void GarbageDistributor::updateGarbages(Field* fld)
{
  // cancel chain garbage
  if( fld->chain() < 2 ) {
    gbs_chain_.erase(fld);
  }

  // check whether a garbage should be dropped (at most one per step)
  if( fld->hangingGarbageCount() > 0 ) {
    const Garbage& gb = fld->hangingGarbage(0);
    GbChainMap::iterator it = gbs_chain_.find(gb.from);
    // don't drop garbage of an active chain
    if( it == gbs_chain_.end() && (*it).second != &gb ) {
      GbDropTickMap::iterator it2 = drop_ticks_.find(&gb);
      assert( it2 != drop_ticks_.end() );
      if( (*it2).second <= fld->tick() ) {
        drop_ticks_.erase(it2);
        observer_.onGarbageDrop(&gb);
      }
    }
  }

  const Field::StepInfo info = fld->stepInfo();
  if( info.combo == 0 ) {
    return; // no match, no new garbages
  }
  const FieldContainer& fields = match_.fields();

  // check if there is an opponent
  // if there is only one, keep it (faster target choice for 2-player game)
  Field* single_opponent = NULL;
  FieldContainer::const_iterator it;
  for( it=fields.begin(); it!=fields.end(); ++it ) {
    //XXX:hack
    // const_iterator return a pointer to a const
    // but we only need the container to be constant, not values
    // so we use ptr_container's base which returns a void** from an iterator
    Field* it_fld = static_cast<Field*>(*(it.base()));
    if( it_fld == fld || it->lost() ) {
      continue;
    }
    if( single_opponent == NULL ) {
      single_opponent = it_fld;
    } else {
      single_opponent = NULL;
      break;
    }
  }
  if( single_opponent == NULL && it==fields.end() ) {
    return; // no opponent, no target
  }

  // note: opponent count will never increase, we don't have to update targets
  // maps it if there is only one opponent

  if( info.chain == 2 ) {
    Field* target_fld = single_opponent;
    if( target_fld == NULL ) {
      // get player with the least chain garbages
      GbTargetMap::iterator targets_it = targets_chain_.find(fld);
      assert( targets_it != targets_chain_.end() );
      FieldContainer::const_iterator it = (*targets_it).second;
      unsigned int min = -1; // overflow: max value
      FieldContainer::const_iterator it_min = fields.end();
      do {
        if( it == fields.end() ) {
          it = fields.begin();
        }
        //XXX:hack see static_cast above
        Field* fld2 = static_cast<Field*>(*(it.base()));
        if( fld2 == fld || fld2->lost() ) {
          continue;
        }
        size_t nb_chain = 0;
        const size_t nb_gb = fld2->hangingGarbageCount();
        for( nb_chain=0; nb_chain<nb_gb; nb_chain++ ) {
          // chain garbages are put at the beginning
          if( fld2->hangingGarbage(nb_chain).type != Garbage::TYPE_CHAIN ) {
            break;
          }
        }
        if( nb_chain < min ) {
          min = nb_chain;
          target_fld = fld2;
          it_min = it;
          if( min == 0 ) {
            break; // 0 is the least we can found
          }
        }
      } while( it != (*targets_it).second );
      assert( target_fld != NULL );
      (*targets_it).second = it_min;
    }
    this->newGarbage(fld, target_fld, Garbage::TYPE_CHAIN, 1);

  } else if( info.chain > 2 ) {
    // increase chain garbage
    GbChainMap::iterator it = gbs_chain_.find(fld);
    assert( it != gbs_chain_.end() );
    Garbage* gb = (*it).second;
    assert( gb->type == Garbage::TYPE_CHAIN );
    gb->size.y++;
    drop_ticks_[gb] = fld->tick() + fld->conf().gb_hang_tk;
    observer_.onGarbageUpdateSize(gb);
}

  // combo garbage
  // with a width of 6, values match the original PdP rules
  if( info.combo > 3 ) {
    Field* target_fld = single_opponent;
    if( target_fld == NULL ) {
      // get the next target player
      GbTargetMap::iterator targets_it = targets_combo_.find(fld);
      assert( targets_it != targets_combo_.end() );
      FieldContainer::const_iterator it;
      for( it=++(*targets_it).second; ; ++it ) {
        if( it == fields.end() ) {
          it = fields.begin();
        }
        //XXX:hack see static_cast above
        target_fld = static_cast<Field*>(*(it.base()));
        if( target_fld == fld || target_fld->lost() ) {
          continue;
        }
        (*targets_it).second = it;
        break;
      }
      assert( target_fld != NULL );
    }

    if( info.combo-1 <= FIELD_WIDTH ) {
      // one block
      this->newGarbage(fld, target_fld, Garbage::TYPE_COMBO, info.combo-1);
    } else if( info.combo <= 2*FIELD_WIDTH ) {
      // two blocks
      unsigned int n = (info.combo > FIELD_WIDTH*3/2) ? info.combo : info.combo-1;
      this->newGarbage(fld, target_fld, Garbage::TYPE_COMBO, n/2);
      this->newGarbage(fld, target_fld, Garbage::TYPE_COMBO, n/2+n%2);
    } else {
      // n blocks
      unsigned int n;
      if( info.combo == 2*FIELD_WIDTH+1 ) {
        n = 3;
      } else if( info.combo <= 3*FIELD_WIDTH+1 ) {
        n = 4;
      } else if( info.combo <= 4*FIELD_WIDTH+2 ) {
        n = 6;
      } else {
        n = 8;
      }
      while( n-- > 0 ) {
        this->newGarbage(fld, target_fld, Garbage::TYPE_COMBO, FIELD_WIDTH);
      }
    }
  }
}


void GarbageDistributor::newGarbage(Field* from, Field* to, Garbage::Type type, int size)
{
  assert( to != NULL );

  std::unique_ptr<Garbage> gb_unique(new Garbage);
  Garbage* gb = gb_unique.get();
  gb->gbid = this->nextGarbageId();
  gb->from = from;
  gb->to = to;
  gb->type = type;

  unsigned int pos;
  if( type == Garbage::TYPE_CHAIN ) {
    gb->size = FieldPos(FIELD_WIDTH, size);
    const unsigned int n = to->hangingGarbageCount();
    for( pos=0; pos<n; pos++ ) {
      if( to->hangingGarbage(pos).type == Garbage::TYPE_CHAIN ) {
        break;
      }
    }
  } else if( type == Garbage::TYPE_COMBO ) {
    gb->size = FieldPos(size, 1);
    pos = to->hangingGarbageCount(); // push back
  } else {
    assert( !"not supported yet" );
    return;
  }
  drop_ticks_[gb] = to->tick() + to->conf().gb_hang_tk;

  match_.addGarbage(gb_unique.release(), pos);
  if( type == Garbage::TYPE_CHAIN ) {
    gbs_chain_[from] = gb;
  }

  observer_.onGarbageAdd(gb, pos);
}


GbId GarbageDistributor::nextGarbageId()
{
  const Match::GarbageMap gbs_wait = match_.waitingGarbages();
  const Match::GarbageMap gbs_hang = match_.hangingGarbages();
  for(;;) {
    current_gbid_++;
    if( current_gbid_ <= 0 ) { // overflow
      current_gbid_ = 1;
    }
    if( gbs_hang.find(current_gbid_) == gbs_hang.end() &&
       gbs_wait.find(current_gbid_) == gbs_wait.end() ) {
      break; // not already in use
    }
  }
  return current_gbid_;
}


