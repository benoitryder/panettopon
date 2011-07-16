#ifndef GUI_WIDGET_H_
#define GUI_WIDGET_H_

#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/function.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/Shape.hpp>

namespace gui {

class StyleButton;


class Widget: public sf::Drawable
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
  virtual void Render(sf::RenderTarget &target, sf::Renderer &renderer) const;

 public:
  typedef boost::ptr_vector<Widget> Container;
  Container widgets;
};


class WButton: public Widget
{
 public:
  WButton(const StyleButton &style, float width);
  void setCaption(const std::string &caption);
  virtual void Render(sf::RenderTarget &target, sf::Renderer &renderer) const;
  virtual bool onInputEvent(const sf::Event &);
  typedef boost::function<void()> Callback;
  void setCallback(Callback cb) { callback_ = cb; }

 private:
  const StyleButton &style_;
  sf::Text caption_;  ///< Button caption
  Callback callback_;
  float width_;
};


class WLabel: public Widget
{
 public:
  WLabel();
  void setText(const std::string &caption);
  virtual void Render(sf::RenderTarget &target, sf::Renderer &renderer) const;
  void setTextAlign(int align);

 private:
  sf::Text text_;
  int align_;  ///< Text alignement (-1, 0, 1).
};


class WEntry: public Widget
{
 public:
  WEntry(float width, float height); //XXX:temp
  void setText(const std::string &caption);
  virtual void Render(sf::RenderTarget &target, sf::Renderer &renderer) const;
  virtual bool onInputEvent(const sf::Event &);
  std::string text() const { return text_.GetString(); }

 private:
  sf::Text text_;
  sf::Shape bg_;
  sf::Shape cursor_;
  unsigned int cursor_pos_;
};


}

#endif
