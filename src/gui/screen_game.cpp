#include "screen_game.h"
#include "screen_menus.h"
#include "interface.h"
#include "../log.h"

namespace gui {


ScreenGame::ScreenGame(GuiInterface &intf):
    Screen(intf),
    input_scheduler_(*intf.instance(), *this, intf.io_service())
{
  keys_.up    = sf::Key::Up;
  keys_.down  = sf::Key::Down;
  keys_.left  = sf::Key::Left;
  keys_.right = sf::Key::Right;
  keys_.swap  = sf::Key::Code('d');
  keys_.raise = sf::Key::Code('f');
}

void ScreenGame::enter()
{
  GameInstance *instance = intf_.instance();
  //XXX:temp
  // retrieve the first local player as the controlled one
  GameInstance::PlayerContainer::iterator it;
  for( it=instance->players().begin(); it!=instance->players().end(); ++it ) {
    if( (*it).second->local() ) {
      player_ = (*it).second;
      break;
    }
  }
  assert( player_ );
}

void ScreenGame::redraw()
{
  sf::RenderWindow &w = intf_.window();
  w.Clear(sf::Color(48,48,48)); //XXX:tmp
  //TODO
}

bool ScreenGame::onInputEvent(const sf::Event &ev)
{
  if( ev.Type == sf::Event::KeyPressed ) {
    if( ev.Key.Code == sf::Key::Escape ) {
      intf_.swapScreen(new ScreenStart(intf_));
      return true;
    }
  }

  return false;
}


KeyState ScreenGame::getNextInput(Player *pl)
{
  assert( pl == player_ );
  const sf::Input &input = intf_.window().GetInput();
  int key = GAME_KEY_NONE;
  if( input.IsKeyDown(keys_.up   ) ) key |= GAME_KEY_UP;
  if( input.IsKeyDown(keys_.down ) ) key |= GAME_KEY_DOWN;
  if( input.IsKeyDown(keys_.left ) ) key |= GAME_KEY_LEFT;
  if( input.IsKeyDown(keys_.right) ) key |= GAME_KEY_RIGHT;
  if( input.IsKeyDown(keys_.swap ) ) key |= GAME_KEY_SWAP;
  if( input.IsKeyDown(keys_.raise) ) key |= GAME_KEY_RAISE;
  return key;
}



}

