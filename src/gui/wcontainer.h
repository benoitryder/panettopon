#ifndef GUI_WCONTAINER_H_
#define GUI_WCONTAINER_H_

#include <boost/ptr_container/ptr_vector.hpp>
#include "widget.h"

namespace gui {


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


}

#endif
