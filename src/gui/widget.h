#ifndef GUI_WIDGET_H_
#define GUI_WIDGET_H_

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Window/Event.hpp>

namespace gui {


class Widget
{
 public:
  enum Neighbor {
    NEIGHBOR_NONE = -1,
    NEIGHBOR_UP = 0,
    NEIGHBOR_DOWN,
    NEIGHBOR_LEFT,
    NEIGHBOR_RIGHT,
    NEIGHBOR_COUNT
  };

  Widget();
  virtual ~Widget();
  virtual void draw(sf::RenderTarget &target) = 0;
  virtual void setPosition(float x, float y) = 0;
  virtual bool onInputEvent(const sf::Event &) { return false; }
  bool focused() const { return focused_; }
  virtual void focus(bool focused) { focused_ = focused; }
  Widget *neighbor(Neighbor n) const { return neighbors_[n]; }
  void setNeighbors(Widget *up, Widget *down, Widget *left, Widget *right);

 private:
  bool focused_;
  Widget *neighbors_[NEIGHBOR_COUNT];
};


}

#endif
