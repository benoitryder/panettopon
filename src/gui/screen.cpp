#include "screen.h"
#include "interface.h"
#include "../log.h"

namespace gui {


Screen::Screen(GuiInterface &intf):
    intf_(intf)
{
}

Screen::~Screen() {}
void Screen::enter() {}
void Screen::exit() {}
void Screen::redraw() {}


}

