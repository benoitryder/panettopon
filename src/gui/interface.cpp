#include <boost/bind.hpp>
#include <boost/asio/placeholders.hpp>
#include "interface.h"
#include "screen_menus.h"
#include "../config.h"
#include "../log.h"

namespace asio = boost::asio;

namespace gui {


const std::string GuiInterface::CONF_SECTION("GUI");

GuiInterface::GuiInterface():
    redraw_timer_(io_service_)
{
  conf_.redraw_dt = (1000.0/60.0);
  conf_.fullscreen = false;
  conf_.screen_width = 800;
  conf_.screen_height = 600;
}

GuiInterface::~GuiInterface()
{
  this->endDisplay();
}

bool GuiInterface::run(const Config &cfg)
{
#define CONF_LOAD(n,ini,t) \
  conf_.n = cfg.get##t(CONF_SECTION, #ini, conf_.n);
  CONF_LOAD(fullscreen,    Fullscreen,   Bool);
  CONF_LOAD(screen_width,  ScreenWidth,  Int);
  CONF_LOAD(screen_height, ScreenHeight, Int);
#undef CONF_LOAD
  float f = cfg.getDouble(CONF_SECTION, "FPS", 60.0);
  //XXX default not based on the cfg value set in constructor
  if( f <= 0 ) {
    LOG("invalid conf. value for FPS: %f", f);
  } else {
    conf_.redraw_dt = (1000.0/f);
  }

  res_mgr_.init(cfg.get(CONF_SECTION, "ResPath", "./res"));

  // create the first screen
  this->swapScreen(new ScreenStart(*this));

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

  io_service_.run();
  return true;
}


void GuiInterface::swapScreen(Screen *screen)
{
  if( screen_ != NULL ) {
    screen_->exit();
    prev_screen_.swap(screen_);
  }
  screen_.reset(screen);
  if( screen_ == NULL ) {
    this->endDisplay();
  } else {
    screen->enter();
  }
}


bool GuiInterface::initDisplay()
{
  //TODO handle errors/exceptions
  window_.Create(
      sf::VideoMode(conf_.screen_width, conf_.screen_height, 32),
      "Panettopon",
      conf_.fullscreen ? sf::Style::Fullscreen : sf::Style::Resize|sf::Style::Close
      );
  window_.EnableKeyRepeat(false);

  // load icon
  const sf::Image *icon = res_mgr_.getImage("icon-32");
  window_.SetIcon(icon->GetWidth(), icon->GetHeight(), icon->GetPixelsPtr());

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
      + boost::posix_time::milliseconds(conf_.redraw_dt);

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

  prev_screen_.reset();
  if( screen_ ) {  // screen may have been removed by a swapScreen()
    screen_->redraw();
    window_.Display();
  }

  redraw_timer_.expires_from_now(next_redraw - boost::posix_time::microsec_clock::universal_time());
  redraw_timer_.async_wait(boost::bind(&GuiInterface::onRedrawTick, this,
                                       asio::placeholders::error));
}


}

