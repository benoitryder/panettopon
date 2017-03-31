#include "inifile.h"
#include "log.h"
#include "intf_curses.h"


namespace curses {


const std::string CursesInterface::CONF_SECTION("Curses");


CursesInterface::CursesInterface():
    cfg_(0), instance_(*this, io_service_),
    input_scheduler_(instance_, *this, io_service_),
    player_(NULL), wmsg_(NULL)
{
  keys_.up    = KEY_UP;
  keys_.down  = KEY_DOWN;
  keys_.left  = KEY_LEFT;
  keys_.right = KEY_RIGHT;
  keys_.swap  = 'd';
  keys_.raise  = 'f';
  keys_.quit  = 'q';
}

CursesInterface::~CursesInterface()
{
  this->endCurses();
}


bool CursesInterface::run(IniFile& cfg)
{
#define CONF_LOAD_KEY(n,ini) do{ \
  const std::string s = cfg.get({CONF_SECTION, #ini}, ""); \
  if( s.empty() ) break; \
  int key = str2key(s); \
  if( key == 0 ) { \
    LOG("invalid conf. value for %s: %s", #ini, s.c_str()); \
  } else { \
    keys_.n = key; \
  } \
}while(0)
  CONF_LOAD_KEY(up,    KeyUp   );
  CONF_LOAD_KEY(down,  KeyDown );
  CONF_LOAD_KEY(left,  KeyLeft );
  CONF_LOAD_KEY(right, KeyRight);
  CONF_LOAD_KEY(swap,  KeySwap );
  CONF_LOAD_KEY(raise, KeyRaise);
  CONF_LOAD_KEY(quit,  KeyQuit );
#undef CONF_LOAD_KEY

  int port = cfg.get<int>("Global.Port", DEFAULT_PNP_PORT);
  const std::string host = cfg.get("Client.Hostname", "localhost");

  if( !this->initCurses() ) {
    LOG("terminal initialization failed");
    return false;
  }
  instance_.connect(host.c_str(), port, 3000);

  assert(!cfg_);
  cfg_ = &cfg;
  io_service_.run();
  cfg_ = 0;
  return true;
}


bool CursesInterface::initCurses()
{
  assert( wmsg_ == NULL );
  ::initscr();

  if( !::has_colors() ) {
    LOG("terminal does not support colors");
    return false;
  }
  //TODO check number of available colors

  ::noecho();
  ::cbreak();
  ::curs_set(0);
  ::timeout(0);
  ::keypad(stdscr, TRUE);

  ::start_color();
  // Implementations doesn't allow to define pair 0
  // Anyway, default values are those we need.
  //::init_pair(0, COLOR_WHITE,   COLOR_BLACK);
  ::init_pair(1, COLOR_RED,     COLOR_BLACK);
  ::init_pair(2, COLOR_GREEN,   COLOR_BLACK);
  ::init_pair(3, COLOR_CYAN,    COLOR_BLACK);
  ::init_pair(4, COLOR_YELLOW,  COLOR_BLACK);
  ::init_pair(5, COLOR_MAGENTA, COLOR_BLACK);
  ::init_pair(6, COLOR_BLUE,    COLOR_BLACK);
  ::init_pair(7, COLOR_RED,   COLOR_BLACK);
  ::init_pair(8, COLOR_WHITE, COLOR_RED);

  // first log window: whole screen
  wmsg_ = ::subwin(stdscr, 0, 0, 0, 0);
  if( wmsg_ == NULL ) {
    return false;
  }
  ::scrollok(wmsg_, TRUE);

  ::touchwin(stdscr);
  ::touchwin(wmsg_);
  ::refresh();

  return true;
}

void CursesInterface::endCurses()
{
  //TODO clean internal display structures
  if( wmsg_ != NULL ) {
    ::delwin(wmsg_);
  }
  wmsg_ = NULL;
  ::endwin();
  ::delwin(stdscr);
}


void CursesInterface::addMessage(int color, const char* fmt, ...)
{
  if( wmsg_ == NULL ) {
    return;
  }

  int x, y;
  y = x = 0; // initialize x and y to avoid a warning
  getyx(wmsg_, y, x);
  (void)y;

  // For unknown reasons, with the curses implementation, using wcolor_set
  // for color 0 does not work. We have to switch off a color to reset the
  // color attribute.
  ::wattroff(wmsg_, COLOR_PAIR(1));
  ::wcolor_set(wmsg_, color, NULL);

  ::wprintw(wmsg_, "\n\r");
  va_list ap;
  va_start(ap,fmt);
  ::vw_printw(wmsg_, fmt, ap);
  va_end(ap);

  ::wnoutrefresh(wmsg_);
  ::doupdate();
}

int CursesInterface::str2key(const std::string& s)
{
  if( s.empty() ) {
    return 0;
  }
  if( s.size() == 1 ) {
    int val = s[0];
    if( val < ' ' || val > '~' ) {
      return 0;
    }
    return val | 0x20; // lowercase
  }
  if(s == "up") return KEY_UP;
  if(s == "down") return KEY_DOWN;
  if(s == "left") return KEY_LEFT;
  if(s == "right") return KEY_RIGHT;
  return 0;
}


void CursesInterface::onChat(Player& pl, const std::string& msg)
{
  this->addMessage(0, "%s(%u): %s", pl.nick().c_str(), pl.plid(), msg.c_str());
}

void CursesInterface::onNotification(GameInstance::Severity sev, const std::string& msg)
{
  int c = 0;
  switch( sev ) {
    case GameInstance::SEVERITY_MESSAGE: c = 2; break;
    case GameInstance::SEVERITY_NOTICE:  c = 3; break;
    case GameInstance::SEVERITY_WARNING: c = 7; break;
    case GameInstance::SEVERITY_ERROR:   c = 8; break;
  }
  this->addMessage(c, ">> %s", msg.c_str());
}

void CursesInterface::onServerConnect(bool success)
{
  if(success) {
    instance_.newLocalPlayer(cfg_->get("Client.Nick", "Player"));
  } else {
    io_service_.stop();
  }
}

void CursesInterface::onServerDisconnect()
{
  io_service_.stop();
}

void CursesInterface::onPlayerJoined(Player& pl)
{
  this->addMessage(2, "%s(%u) joined", pl.nick().c_str(), pl.plid());
  if(pl.local()) {
    assert(player_ == nullptr);
    player_ = &pl;
    instance_.playerSetState(*player_, Player::State::LOBBY_READY);
  }
}

void CursesInterface::onPlayerChangeNick(Player& pl, const std::string& nick)
{
  this->addMessage(2, "%s(%u) is now known as %s",
                   nick.c_str(), pl.plid(), pl.nick().c_str());
}

void CursesInterface::onPlayerStateChange(Player& pl)
{
  Player::State state = pl.state();
  if(state == Player::State::QUIT) {
    this->addMessage(2, "%s(%u) has quit", pl.nick().c_str(), pl.plid());
    if(&pl == player_) {
      player_ = nullptr;
      io_service_.stop();
    }
  } else if(state == Player::State::LOBBY_READY || state == Player::State::GAME_READY) {
    this->addMessage(2, "%s(%u) is ready", pl.nick().c_str(), pl.plid());
  } else if(state == Player::State::LOBBY && instance_.state() == GameInstance::State::LOBBY) {
    this->addMessage(2, "%s(%u) is not ready", pl.nick().c_str(), pl.plid());
  }
}

void CursesInterface::onPlayerChangeFieldConf(Player& pl)
{
  this->addMessage(2, "%s(%u) changed configuration", pl.nick().c_str(), pl.plid());
}

void CursesInterface::onStateChange()
{
  auto state = instance_.state();
  if(state == GameInstance::State::LOBBY) {
    input_scheduler_.stop();
    this->addMessage(2, "match end");
    fdisplays_.clear();
    if( player_ != NULL ) {
      instance_.playerSetState(*player_, Player::State::LOBBY_READY);
    }

  } else if(state == GameInstance::State::GAME_INIT) {
    this->addMessage(2, "match init");

  } else if(state == GameInstance::State::GAME_READY) {
    this->addMessage(2, "match ready");
    assert( fdisplays_.empty() );
    ::clear();
    assert( wmsg_ != NULL );
    ::delwin(wmsg_);
    wmsg_ = NULL;
    wmsg_ = ::subwin(stdscr, LINES-FIELD_HEIGHT-5, 0, FIELD_HEIGHT+5, 0);
    assert( wmsg_ != NULL ); //XXX error
    ::scrollok(wmsg_, TRUE);

    for(const auto& fld : instance_.match().fields()) {
      auto fdp = std::make_unique<FieldDisplay>(*this, *fld, fdisplays_.size());
      auto it = fdisplays_.emplace(fld.get(), std::move(fdp)).first;
      it->second->draw();
    }
    ::redrawwin(stdscr);
    ::refresh();
    if( player_ != NULL ) {
      instance_.playerSetState(*player_, Player::State::GAME_READY);
    }

  } else if(state == GameInstance::State::GAME) {
    LOG("START");
    input_scheduler_.start();
  }
}

void CursesInterface::onServerChangeFieldConfs()
{
}

void CursesInterface::onPlayerStep(Player& pl)
{
  FieldDisplayMap::iterator it = fdisplays_.find(pl.field());
  assert( it != fdisplays_.end() );
  (*it).second->step();
  (*it).second->draw();
  ::refresh();
  if(pl.field()->lost()) {
    this->addMessage(2, "%s(%u) lost", pl.nick().c_str(), pl.plid());
  }
}

void CursesInterface::onPlayerRanked(Player& pl)
{
  this->addMessage(2, "%s(%u) ranked %d", pl.nick().c_str(), pl.plid(), pl.field()->rank());
}

KeyState CursesInterface::getNextInput(const Player&)
{
  GameKey key = GAME_KEY_NONE;
  for(;;) {
    int c = ::getch();
    if( c == keys_.quit ) {
      io_service_.stop();
    }
    if( c == ERR ) {
      break;
    } else if( c == keys_.up ) {
      key = GAME_KEY_UP;
    } else if( c == keys_.down ) {
      key = GAME_KEY_DOWN;
    } else if( c == keys_.left ) {
      key = GAME_KEY_LEFT;
    } else if( c == keys_.right) {
      key = GAME_KEY_RIGHT;
    } else if( c == keys_.swap ) {
      key = GAME_KEY_SWAP;
    } else if( c == keys_.raise ) {
      key = GAME_KEY_RAISE;
    }
  }
  return key;
}


FieldDisplay::FieldDisplay(CursesInterface& intf, const Field& fld, int slot):
    intf_(intf), field_(fld)
{
  assert( slot >= 0 );

  wfield_ = ::derwin(stdscr, FIELD_HEIGHT+5, 2*FIELD_WIDTH+2, 0, slot*(2*FIELD_WIDTH+4) );
  assert( wfield_ != NULL ); //XXX error
  wgrid_ = ::derwin(wfield_, FIELD_HEIGHT+2, 2*FIELD_WIDTH+2, 1, 0);
  assert( wgrid_ != NULL ); //XXX error

  ::wclear(wfield_);
  ::touchwin(wfield_);
  ::touchwin(wgrid_);
}

FieldDisplay::~FieldDisplay()
{
  ::wclear(wfield_);
  ::delwin(wgrid_);
  ::delwin(wfield_);
  ::redrawwin(stdscr); //XXX
}


void FieldDisplay::draw()
{
  int x, y;

  // grid borders, red if danger (or garbage falls)
  short color = 0;
  for( x=0; x<FIELD_WIDTH; x++ ) {
    if( !field_.block(x,FIELD_HEIGHT-1).isNone() ) {
      color = 7;
      break;
    }
  }
  ::wcolor_set(wgrid_, color, NULL);
  ::box(wgrid_, 0, 0);

  // grid content
  for( x=0; x<FIELD_WIDTH; x++ ) {
    for( y=1; y<=FIELD_HEIGHT; y++ ) {
      this->drawBlock(x, y);
    }
  }

  // player nick (if available)
  ::wcolor_set(wfield_, field_.lost() ? 1 : 2, NULL);
  Player* pl = intf_.instance_.player(&field_);
  const char* nick = "???";
  if( pl != NULL ) {
    nick = pl->nick().c_str();
  }
  mvwaddnstr(wfield_, FIELD_HEIGHT+3, 0, nick, FIELD_WIDTH+2);

  // hanging garbages

  wcolor_set(wfield_, 0, NULL); // required, don't know why
  char buf[2*FIELD_WIDTH]; // unformatted string
  const size_t gb_nb = field_.hangingGarbageCount();
  size_t gb_i=0;
  for( gb_i=0, x=0; gb_i<gb_nb && x<FIELD_WIDTH; gb_i++ ) {
    const Garbage& gb = field_.hangingGarbage(gb_i);
    if( gb.type == Garbage::TYPE_CHAIN ) {
      x += snprintf(buf+x, sizeof(buf-x), "x%u", gb.size.y);
    } else if( gb.type == Garbage::TYPE_COMBO ) {
      x += snprintf(buf+x, sizeof(buf-x), "%u", gb.size.x);
    }
    if( x < FIELD_WIDTH ) {
      buf[x] = ' ';
    }
    x++;
  }
  while( x<FIELD_WIDTH ) {
    buf[x++]= ' '; // pad with spaces
  }

  // print formatted output
  ::wmove(wfield_, 0, 1);
  for( x=0; x<FIELD_WIDTH; x++ ) {
    chtype ch = buf[x];
    if( ch != ' ' ) {
      ch |= A_REVERSE;
    }
    ch |= COLOR_PAIR(0);
    ::waddch(wfield_, ch);
  }

  // signs
  SignContainer::iterator it;
  for( it=signs_.begin(); it!=signs_.end(); ++it ) {
    char buf[5];
    chtype ch = A_REVERSE|A_BLINK;
    if( it->chain ) {
      ::snprintf(buf, sizeof(buf), "x%u", it->val);
      ch |= COLOR_PAIR(2);
    } else {
      ::snprintf(buf, sizeof(buf), "%2u", it->val);
      ch |= COLOR_PAIR(1);
    }
    buf[sizeof(buf)-1] = '\0';
    int i;
    wmove(wgrid_, FIELD_HEIGHT-it->pos.y+1, it->pos.x*2+2);
    for( i=0; buf[i]!='\0'; i++ ) {
      waddch(wgrid_, ch|buf[i]);
    }
  }

  ::wnoutrefresh(wgrid_);
  ::wnoutrefresh(wfield_);
}


void FieldDisplay::step()
{
  const Field::StepInfo& info = field_.stepInfo();

  // signs

  // update display time
  SignContainer::iterator it;
  for( it=signs_.begin(); it!=signs_.end(); ++it ) {
    it->dt--;
  }
  // remove expired signs
  while( !signs_.empty() && signs_.front().dt == 0 ) {
    signs_.pop_front();
  }
  // create new signs, if needed
  if( info.combo != 0 ) {
    FieldPos pos = this->matchSignPos();
    if( pos.y < FIELD_HEIGHT ) {
      pos.y++; // display sign above top matching block, if possible
    }
    if( info.chain > 1 ) {
      signs_.push_back( Sign(pos, true, info.chain) );
      pos.y--;
    }
    if( info.combo > 3 ) {
      signs_.push_back( Sign(pos, false, info.combo) );
    }
  }
}


void FieldDisplay::drawBlock(int x, int y)
{
  const Block& bk = field_.block(x,y);

  chtype chs[2] = {0,0};
  if( bk.isColor() ) {
    chs[0] = COLOR_PAIR(bk.bk_color.color+1);
    if( bk.bk_color.state == BkColor::FLASH ) {
      if( ((bk.ntick - field_.tick())/4) % 2 == 0 ) {
        chs[0] = ':'|A_REVERSE|COLOR_PAIR(0);
      } else {
        chs[0] |= ':'|A_REVERSE;
      }
    } else if( bk.bk_color.state == BkColor::MUTATE ) {
      chs[0] |= ':';
    } else if( bk.bk_color.state == BkColor::CLEARED ) {
      chs[0] = ' '|COLOR_PAIR(0);
    } else {
      chs[0] |= ' '|A_REVERSE;
    }
  } else if( bk.isGarbage() ) {
    chs[0] = COLOR_PAIR(0)|A_REVERSE;
    if( bk.bk_garbage.state == BkGarbage::FLASH ) {
      if( ((bk.ntick - field_.tick())/4) % 2 == 0 ) {
        chs[0] |= ':';
      } else {
        chs[0] = ':'|COLOR_PAIR(0);
      }
    } else if( bk.bk_garbage.state == BkGarbage::MUTATE ) {
      chs[0] |= ':';
    } else {
      chs[1] = chs[0];
      const Garbage& gb = *bk.bk_garbage.garbage;
      int c1, c2;
      if(gb.size.y == 1) {
        c1 = c2 = ACS_HLINE;
        if(x == gb.pos.x) {
          c1 =' ';
        } else if(x == gb.pos.x + gb.size.x-1) {
          c2 = ' ';
        }
      } else {
        c1 = c2 = ' ';
        if(y == gb.pos.y) {
          c1 = c2 = ACS_HLINE;
          if(x == gb.pos.x) {
            c1 = ACS_LLCORNER;
          } else if(x == gb.pos.x + gb.size.x-1) {
            c2 = ACS_LRCORNER;
          }
        } else if(y == gb.pos.y + gb.size.y-1) {
          c1 = c2 = ACS_HLINE;
          if(x == gb.pos.x) {
            c1 = ACS_ULCORNER;
          } else if(x == gb.pos.x + gb.size.x-1) {
            c2 = ACS_URCORNER;
          }
        } else if(x == gb.pos.x) {
          c1 = ACS_VLINE;
        } else if(x == gb.pos.x + gb.size.x-1) {
          c2 = ACS_VLINE;
        }
      }
      chs[0] |= c1;
      chs[1] |= c2;
    }
  } else {
    chs[0] = ' '|COLOR_PAIR(0);
  }
  if( chs[1] == 0 ) {
    chs[1] = chs[0];
  }

  // swap
  if( bk.swapped ) {
    if( x == field_.swapPos().x ) {
      chs[0] = ' '|COLOR_PAIR(0);
      const Block& bk2 = field_.block(x+1,y);
      if( bk2.isColor() &&
         (bk.isNone() || field_.swapDelay() > field_.conf().swap_tk*2/3) ) {
        chs[1] = ' '|A_REVERSE|COLOR_PAIR(bk2.bk_color.color+1);
      }
    } else if( x == field_.swapPos().x+1 ) {
      chs[1] = ' '|COLOR_PAIR(0);
      const Block& bk2 = field_.block(x-1,y);
      if( bk2.isColor() &&
         (bk.isNone() || field_.swapDelay() > field_.conf().swap_tk*1/3) ) {
        chs[0] = ' '|A_REVERSE|COLOR_PAIR(bk2.bk_color.color+1);
      }
    }
  }

  // cursor
  if( y == field_.cursor().y ) {
    if( x == field_.cursor().x ) {
      chs[0] &= ~0xff;
      chs[0] |= '[';
    } else if( x == field_.cursor().x+1 ) {
      chs[1] &= ~0xff;
      chs[1] |= ']';
    }
  }

  mvwaddchnstr(wgrid_, FIELD_HEIGHT-y+1, 2*x+1, chs, 2);
}


const unsigned int FieldDisplay::Sign::DURATION = 42;

FieldDisplay::Sign::Sign(const FieldPos& pos, bool chain, unsigned int val):
    pos(pos), chain(chain), val(val), dt(DURATION)
{
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


}

