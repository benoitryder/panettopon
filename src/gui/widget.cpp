#include "widget.h"

namespace gui {


Widget::Widget():
    focused_(false)
{
  this->setNeighbors(NULL, NULL, NULL, NULL);
}

Widget::~Widget() {}

void Widget::setNeighbors(Widget *up, Widget *down, Widget *left, Widget *right)
{
  neighbors_[NEIGHBOR_UP] = up;
  neighbors_[NEIGHBOR_DOWN] = down;
  neighbors_[NEIGHBOR_LEFT] = left;
  neighbors_[NEIGHBOR_RIGHT] = right;
}


}

