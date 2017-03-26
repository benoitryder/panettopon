#include "interface.h"
#include "screen_menus.h"
#include "../inifile.h"
#include "../server.h"
#include "../client.h"
#include "../deletion_handler.h"
#include "../log.h"

namespace asio = boost::asio;

namespace gui {

void StyleGlobal::load(const StyleLoader& loader)
{
  colors.push_back(loader.getStyle<sf::Color>("Color.Neutral"));
  for(unsigned int i=1; i<=16; ++i) {
    sf::Color color;
    if(!loader.fetchStyle({"Color", std::to_string(i)}, color)) {
      break;
    }
    colors.push_back(color);
  }
  unsigned int color_nb = colors.size() - 1;
  if(color_nb < 4) {
    throw std::runtime_error("color count is too small, must be at least 4");
  }
}



const std::string GuiInterface::CONF_SECTION("GUI");

GuiInterface::GuiInterface():
    cfg_(NULL), focused_(false), redraw_timer_(io_service_),
    instance_(), server_instance_(NULL), client_instance_(NULL)
{
  window_conf_.redraw_dt = (1000.0/60.0);
  window_conf_.fullscreen = false;
  window_conf_.screen_width = 800;
  window_conf_.screen_height = 600;
}

GuiInterface::~GuiInterface()
{
  this->endDisplay();
}

bool GuiInterface::run(IniFile* cfg)
{
#define CONF_LOAD(n,ini) \
  window_conf_.n = cfg->get({CONF_SECTION, #ini}, window_conf_.n);
  CONF_LOAD(fullscreen,    Fullscreen);
  CONF_LOAD(screen_width,  ScreenWidth);
  CONF_LOAD(screen_height, ScreenHeight);
#undef CONF_LOAD
  float f = cfg->get({CONF_SECTION, "FPS"}, 60.0);
  //XXX default not based on the cfg value set in constructor
  if( f <= 0 ) {
    LOG("invalid conf. value for FPS: %f", f);
  } else {
    window_conf_.redraw_dt = (1000.0/f);
  }

  res_mgr_.init(cfg->get({CONF_SECTION, "ResPath"}, "./res"));
  // set some default values
  if(!cfg->has("Global.Port")) {
    cfg->set("Global.Port", DEFAULT_PNP_PORT);
  }
  if(!cfg->has("Client.Hostname")) {
    cfg->set("Client.Hostname", "localhost");
  }
  if(!cfg->has("Client.Nick")) {
    cfg->set("Client.Nick", "Player");
  }

  style_.load(StyleLoaderResourceManager(res_mgr_, "Global"));

  // start display loop
  if( !this->initDisplay() ) {
    LOG("display initialization failed");
    this->endDisplay();
    return false;
  }
  // postpone swap to the first screen to have access to cfg in enter()
  io_service_.post(std::bind(&GuiInterface::enterFirstScreen, this));

  boost::posix_time::time_duration dt = boost::posix_time::milliseconds(window_conf_.redraw_dt);
  redraw_timer_.expires_from_now(dt);
  redraw_timer_.async_wait(std::bind(&GuiInterface::onRedrawTick, this, std::placeholders::_1));

  assert( cfg_ == NULL );
  cfg_ = cfg;
  io_service_.run();
  cfg_ = NULL;
  return true;
}


void GuiInterface::swapScreen(Screen* screen)
{
  if(screen_) {
    screen_->exit();
    io_service_.post(deletion_handler<Screen>(std::move(screen_)));
  }
  screen_.reset(screen);
  if(!screen_) {
    this->endDisplay();
  } else {
    screen_->enter();
  }
}


void GuiInterface::onChat(Player* pl, const std::string& msg)
{
  screen_->onChat(pl, msg);
}

void GuiInterface::onPlayerJoined(Player* pl)
{
  screen_->onPlayerJoined(pl);
}

void GuiInterface::onPlayerChangeNick(Player* pl, const std::string& nick)
{
  screen_->onPlayerChangeNick(pl, nick);
}

void GuiInterface::onPlayerStateChange(Player* pl)
{
  screen_->onPlayerStateChange(pl);
}

void GuiInterface::onPlayerChangeFieldConf(Player* pl)
{
  screen_->onPlayerChangeFieldConf(pl);
}

void GuiInterface::onStateChange()
{
  screen_->onStateChange();
}

void GuiInterface::onServerChangeFieldConfs()
{
  screen_->onServerChangeFieldConfs();
}

void GuiInterface::onPlayerStep(Player* pl)
{
  screen_->onPlayerStep(pl);
}

void GuiInterface::onPlayerRanked(Player* pl)
{
  screen_->onPlayerRanked(pl);
}

void GuiInterface::onNotification(GameInstance::Severity sev, const std::string& msg)
{
  screen_->onNotification(sev, msg);
}

void GuiInterface::onServerConnect(bool success)
{
  if(!success) {
    instance_.reset();
  }
  screen_->onServerConnect(success);
}

void GuiInterface::onServerDisconnect()
{
  screen_->onServerDisconnect();
}


void GuiInterface::startServer(int port)
{
  assert(!instance_);
  server_instance_ = new ServerInstance(*this, io_service_);
  instance_.reset(server_instance_);
  server_instance_->loadConf(*cfg_);
  server_instance_->startServer(port);
}

void GuiInterface::startClient(const std::string& host, int port)
{
  assert(!instance_);
  client_instance_ = new ClientInstance(*this, io_service_);
  instance_.reset(client_instance_);
  client_instance_->connect(host.c_str(), port, 3000);
}

void GuiInterface::stopInstance()
{
  if(!instance_) {
    return; // nothing to do
  }
  if( server_instance_ ) {
    server_instance_->stopServer();
    server_instance_ = NULL;
  } else if( client_instance_ ) {
    client_instance_->disconnect();
    client_instance_ = NULL;
  }
  io_service_.post(deletion_handler<GameInstance>(std::move(instance_)));
}



bool GuiInterface::initDisplay()
{
  window_.create(
      sf::VideoMode(window_conf_.screen_width, window_conf_.screen_height, 32),
      "Panettopon",
      window_conf_.fullscreen ? sf::Style::Fullscreen : sf::Style::Resize|sf::Style::Close
      );
  window_.setKeyRepeatEnabled(true);
  window_.setActive();
  focused_ = true;

  // load icon
  sf::Image icon;
  icon.loadFromFile(res_mgr_.getResourceFilename("icon-32.png"));
  window_.setIcon(icon.getSize().x, icon.getSize().y, icon.getPixelsPtr());

  sf::View view = window_.getDefaultView();
  view.setCenter(0,0);
  window_.setView(view);

  return true;
}

void GuiInterface::endDisplay()
{
  window_.close();
}

void GuiInterface::enterFirstScreen()
{
  this->swapScreen(new ScreenStart(*this));
}

void GuiInterface::onRedrawTick(const boost::system::error_code& ec)
{
  if( ec == asio::error::operation_aborted ) {
    return;
  }

  if( ! window_.isOpen() ) {
    io_service_.stop();
    return;
  }

  // compute time of next redraw now, to not add up redraw processing time to
  // redraw period
  boost::posix_time::ptime next_redraw =
      boost::posix_time::microsec_clock::universal_time()
      + boost::posix_time::milliseconds(window_conf_.redraw_dt);

  sf::Event event;
  while(window_.pollEvent(event)) {
    if( event.type == sf::Event::KeyPressed ||
       event.type == sf::Event::KeyReleased ||
       event.type == sf::Event::TextEntered ||
       event.type == sf::Event::JoystickButtonPressed ||
       event.type == sf::Event::JoystickMoved) {
      if(input_handler_.filterEvent(event)) {
        if(focused_) {
          screen_->onInputEvent(event);
        }
      }
    } else if( event.type == sf::Event::Closed ) {
      this->endDisplay();
    } else if( event.type == sf::Event::GainedFocus ) {
      focused_ = true;
    } else if( event.type == sf::Event::LostFocus ) {
      focused_ = false;
    }
  }

  if(screen_) {  // screen may have been removed by a swapScreen()
    screen_->redraw();
    window_.display();
  }

  redraw_timer_.expires_from_now(next_redraw - boost::posix_time::microsec_clock::universal_time());
  redraw_timer_.async_wait(std::bind(&GuiInterface::onRedrawTick, this, std::placeholders::_1));
}


}

