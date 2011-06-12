#include "wcontainer.h"

namespace gui {


WContainer::WContainer()
{
  drawable_.container = this;
}


void WContainer::draw(sf::RenderTarget &target)
{
  Container::iterator it;
  for( it=widgets.begin(); it!=widgets.end(); ++it ) {
    it->draw(target);
  }
}

void WContainer::setPosition(float x, float y)
{
  drawable_.SetPosition(x, y);
}

void WContainer::Drawable::Render(sf::RenderTarget &target, sf::Renderer &) const
{
  container->draw(target);
}


}

