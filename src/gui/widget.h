#ifndef GUI_WIDGET_H_
#define GUI_WIDGET_H_

#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/function.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Graphics/RenderImage.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Shape.hpp>
#include "resources.h"

namespace gui {

class Screen;


/** @brief Base class for widgets.
 *
 * Style properties:
 *  - Position: x,y vector
 *
 * @note Name does not have to be unique and may be empty.
 */
class Widget: public sf::Drawable
{
 public:
  struct StyleError: public std::runtime_error {
    StyleError(const Widget& w, const std::string& prop, const std::string& msg);
  };

  enum Neighbor {
    NEIGHBOR_NONE = -1,
    NEIGHBOR_UP = 0,
    NEIGHBOR_DOWN,
    NEIGHBOR_LEFT,
    NEIGHBOR_RIGHT,
    NEIGHBOR_COUNT
  };

  Widget(const Screen& screen, const std::string& name);
  virtual ~Widget();
  virtual bool onInputEvent(const sf::Event &) { return false; }
  bool focused() const { return focused_; }
  virtual void focus(bool focused) { focused_ = focused; }
  Widget *neighbor(Neighbor n) const { return neighbors_[n]; }
  void setNeighbors(Widget *up, Widget *down, Widget *left, Widget *right);

  /// Return the widget type as a string.
  virtual const std::string& type() const = 0;

  /** @brief Get style entry key for a given property.
   *
   * The requested property is searched in the following sections (in order):
   *  - Screen.WidgetName
   *  - Screen.Type
   *  - Type
   *
   * @return true if found, false otherwise.
   */
  bool searchStyle(const std::string& prop, std::string *key) const;

 private:
  const Screen& screen_;
  const std::string& name_;
  bool focused_;
  Widget *neighbors_[NEIGHBOR_COUNT];
};


/** @brief Widget container.
 *
 * Contained widgets are owned by the container and deleted when it is
 * destroyed.
 *
 * Style properties: none
 */
class WContainer: public Widget
{
 public:
  WContainer(const Screen& screen, const std::string& name);
  virtual void Render(sf::RenderTarget &target, sf::Renderer &renderer) const;

 private:
  static const std::string type_;
 protected:
  virtual const std::string& type() const;

 public:
  typedef boost::ptr_vector<Widget> Container;
  Container widgets;
};


/** @brief Text button with image background
 *
 * Style properties:
 *  - Font, FontSize
 *  - Color, FocusColor
 *  - Width
 *  - BgImage, BgImageRect, BgImageMarginX
 */
class WButton: public Widget
{
 public:
  WButton(const Screen& screen, const std::string& name);
  void setCaption(const std::string &caption);
  virtual void Render(sf::RenderTarget &target, sf::Renderer &renderer) const;
  virtual bool onInputEvent(const sf::Event &);
  typedef boost::function<void()> Callback;
  void setCallback(Callback cb) { callback_ = cb; }

 private:
  static const std::string type_;
 protected:
  virtual const std::string& type() const;

 private:
  sf::Text caption_;  ///< Button caption
  ImageFrameX frame_;
  sf::Color color_;
  sf::Color focus_color_;
  float width_;
  Callback callback_;
};


/** @brief Simple text widget
 *
 * Style properties:
 *  - Font, FontSize
 *  - Color
 *  - TextAlign: left, center, right
 */
class WLabel: public Widget
{
 public:
  WLabel(const Screen& screen, const std::string& name);
  void setText(const std::string &caption);
  virtual void Render(sf::RenderTarget &target, sf::Renderer &renderer) const;
  void setTextAlign(int align);

 private:
  static const std::string type_;
 protected:
  virtual const std::string& type() const;

 private:
  sf::Text text_;
  int align_;  ///< Text alignement (-1, 0, 1).
};


/** @brief Text field
 *
 * Style properties:
 *  - Font, FontSize
 *  - Color, FocusColor
 *  - Width
 *  - BgImage, BgImageRect, BgImageMarginX
 *  - TextMarginX: left and right margin for text
 */
class WEntry: public Widget
{
 public:
  WEntry(const Screen& screen, const std::string& name);
  void setText(const std::string &caption);
  virtual void Render(sf::RenderTarget &target, sf::Renderer &renderer) const;
  virtual bool onInputEvent(const sf::Event &);
  std::string text() const { return text_.GetString(); }

 private:
  static const std::string type_;
 protected:
  virtual const std::string& type() const;

 private:
  /// Update text image and cursor position after text input or cursor move
  void updateTextDisplay(bool force=false);

 private:
  sf::RenderImage text_img_;
  sf::Text text_;
  sf::Sprite text_sprite_;
  sf::Shape cursor_;
  ImageFrameX frame_;
  sf::Color color_;
  sf::Color focus_color_;
  float width_;
  unsigned int cursor_pos_;  ///< cursor position, in the complete string
};


}

#endif
