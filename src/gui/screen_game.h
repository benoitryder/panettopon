#ifndef GUI_SCREEN_GAME_H_
#define GUI_SCREEN_GAME_H_

#include "screen.h"

namespace gui {

class GuiInterface;


class ScreenGame: public Screen, public GameInputScheduler::InputProvider
{
 public:
  ScreenGame(GuiInterface &intf, Player *pl);
  virtual void enter();
  virtual void exit();
  virtual void redraw();
  virtual bool onInputEvent(const sf::Event &ev);
  virtual void onStateChange(GameInstance::State state);

  /// InputProvider interface.
  virtual KeyState getNextInput(Player *pl);

 private:
  Player *player_;  ///< Local controlled player
  GameInputScheduler input_scheduler_;

  /// Key bindings.
  struct {
    sf::Key::Code up, down, left, right;
    sf::Key::Code swap, raise;
  } keys_;
};


}

#endif
