#ifndef GUI_WIDGET_H_
#define GUI_WIDGET_H_

#include <boost/ptr_container/ptr_vector.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/Shape.hpp>

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


/** @brief Widget container.
 *
 * Contained widgets are owned by the container and deleted when it is
 * destroyed.
 */
class WContainer: public Widget
{
 public:
  WContainer();
  virtual void draw(sf::RenderTarget &target);
  virtual void setPosition(float x, float y);

 public:
  typedef boost::ptr_vector<Widget> Container;
  Container widgets;

 protected:
  class Drawable: public sf::Drawable {
   public:
    virtual void Render(sf::RenderTarget &target, sf::Renderer &) const;
    WContainer *container;
  } drawable_;
};


class WButton: public Widget
{
 public:
  WButton(float width, float height);
  void setCaption(const std::string &caption);
  void setColor(const sf::Color &color);
  virtual void draw(sf::RenderTarget &target);
  virtual void setPosition(float x, float y);
  virtual void focus(bool focused);

 private:
  sf::Text caption_;  ///< Button caption
  sf::Shape bg_;  ///< Background shape
};


}

#endif
