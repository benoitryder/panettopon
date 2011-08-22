#include <boost/bind.hpp>
#include <boost/asio/placeholders.hpp>
#include "interface.h"
#include "screen_menus.h"
#include "../inifile.h"
#include "../server.h"
#include "../client.h"
#include "../deletion_handler.h"
#include "../log.h"

namespace asio = boost::asio;

namespace gui {


const std::string GuiInterface::CONF_SECTION("GUI");

GuiInterface::GuiInterface():
    cfg_(NULL), redraw_timer_(io_service_),
    instance_(NULL), server_instance_(NULL), client_instance_(NULL)
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

bool GuiInterface::run(IniFile *cfg)
{
#define CONF_LOAD(n,ini) \
  window_conf_.n = cfg->get(CONF_SECTION, #ini, window_conf_.n);
  CONF_LOAD(fullscreen,    Fullscreen);
  CONF_LOAD(screen_width,  ScreenWidth);
  CONF_LOAD(screen_height, ScreenHeight);
#undef CONF_LOAD
  float f = cfg->get(CONF_SECTION, "FPS", 60.0);
  //XXX default not based on the cfg value set in constructor
  if( f <= 0 ) {
    LOG("invalid conf. value for FPS: %f", f);
  } else {
    window_conf_.redraw_dt = (1000.0/f);
  }

  res_mgr_.init(cfg->get(CONF_SECTION, "ResPath", "./res"));
  // set some default values
  if( !cfg->has("Global", "Port") ) {
    cfg->set("Global", "Port", DEFAULT_PNP_PORT);
  }
  if( !cfg->has("Client", "Hostname") ) {
    cfg->set("Client", "Hostname", "localhost");
  }
  if( !cfg->has("Client", "Nick") ) {
    cfg->set("Client", "Nick", "Player");
  }

  // start display loop
  if( !this->initDisplay() ) {
    LOG("display initialization failed");
    this->endDisplay();
    return false;
  }
  // create the first screen
  this->swapScreen(new ScreenStart(*this));

  boost::posix_time::time_duration dt = boost::posix_time::milliseconds(window_conf_.redraw_dt);
  redraw_timer_.expires_from_now(dt);
  redraw_timer_.async_wait(boost::bind(&GuiInterface::onRedrawTick, this,
                                       asio::placeholders::error));

  assert( cfg_ == NULL );
  cfg_ = cfg;
  io_service_.run();
  cfg_ = NULL;
  return true;
}


void GuiInterface::swapScreen(Screen *screen)
{
  if( screen_.get() ) {
    screen_->exit();
    io_service_.post(deletion_handler<Screen>(screen_.release()));
  }
  screen_.reset(screen);
  if( ! screen_.get() ) {
    this->endDisplay();
  } else {
    screen_->enter();
  }
}


void GuiInterface::onChat(Player *pl, const std::string &msg)
{
  screen_->onChat(pl, msg);
}

void GuiInterface::onPlayerJoined(Player *pl)
{
  screen_->onPlayerJoined(pl);
}

void GuiInterface::onPlayerChangeNick(Player *pl, const std::string &nick)
{
  screen_->onPlayerChangeNick(pl, nick);
}

void GuiInterface::onPlayerReady(Player *pl)
{
  screen_->onPlayerReady(pl);
}

void GuiInterface::onPlayerChangeFieldConf(Player *pl)
{
  screen_->onPlayerChangeFieldConf(pl);
}

void GuiInterface::onPlayerQuit(Player *pl)
{
  screen_->onPlayerQuit(pl);
}

void GuiInterface::onStateChange(GameInstance::State state)
{
  screen_->onStateChange(state);
}

void GuiInterface::onPlayerStep(Player *pl)
{
  screen_->onPlayerStep(pl);
}

void GuiInterface::onNotification(GameInstance::Severity sev, const std::string &msg)
{
  screen_->onNotification(sev, msg);
}

void GuiInterface::onServerDisconnect()
{
  screen_->onServerDisconnect();
}


void GuiInterface::startServer(int port)
{
  assert( instance_.get() == NULL );
  server_instance_ = new ServerInstance(*this, io_service_);
  instance_.reset(server_instance_);
  server_instance_->loadConf(*cfg_);
  server_instance_->startServer(port);
}

void GuiInterface::startClient(const std::string &host, int port)
{
  assert( instance_.get() == NULL );
  client_instance_ = new ClientInstance(*this, io_service_);
  instance_.reset(client_instance_);
  client_instance_->connect(host.c_str(), port, 3000);
}

void GuiInterface::stopInstance()
{
  if( instance_.get() == NULL ) {
    return; // nothing to do
  }
  if( server_instance_ ) {
    server_instance_->stopServer();
    server_instance_ = NULL;
  } else if( client_instance_ ) {
    client_instance_->disconnect();
    client_instance_ = NULL;
  }
  io_service_.post(deletion_handler<GameInstance>(instance_.release()));
}



bool GuiInterface::initDisplay()
{
  //TODO handle errors/exceptions
  window_.Create(
      sf::VideoMode(window_conf_.screen_width, window_conf_.screen_height, 32),
      "Panettopon",
      window_conf_.fullscreen ? sf::Style::Fullscreen : sf::Style::Resize|sf::Style::Close
      );
  window_.EnableKeyRepeat(true);
  window_.SetActive();

  // load icon
  sf::Image icon;
  icon.LoadFromFile(res_mgr_.getResourceFilename("icon-32.png"));
  window_.SetIcon(icon.GetWidth(), icon.GetHeight(), icon.GetPixelsPtr());

  sf::View view = window_.GetDefaultView();
  view.SetCenter(0,0);
  window_.SetView(view);

  return true;
}

void GuiInterface::endDisplay()
{
  window_.Close();
}


void GuiInterface::onRedrawTick(const boost::system::error_code &ec)
{
  if( ec == asio::error::operation_aborted )
    return;

  if( ! window_.IsOpened() ) {
    //TODO
    io_service_.stop();
    return;
  }

  // compute time of next redraw now, to not add up redraw processing time to
  // redraw period
  boost::posix_time::ptime next_redraw =
      boost::posix_time::microsec_clock::universal_time()
      + boost::posix_time::milliseconds(window_conf_.redraw_dt);

  sf::Event event;
  while(window_.PollEvent(event)) {
    if( event.Type == sf::Event::Closed ) {
      this->endDisplay();
    } else if( event.Type == sf::Event::KeyPressed ||
               event.Type == sf::Event::KeyReleased ||
               event.Type == sf::Event::TextEntered ) {
      screen_->onInputEvent(event);
    }
  }

  if( screen_.get() ) {  // screen may have been removed by a swapScreen()
    screen_->redraw();
    window_.Display();
  }

  redraw_timer_.expires_from_now(next_redraw - boost::posix_time::microsec_clock::universal_time());
  redraw_timer_.async_wait(boost::bind(&GuiInterface::onRedrawTick, this,
                                       asio::placeholders::error));
}


}

