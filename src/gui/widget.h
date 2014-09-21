#ifndef GUI_WIDGET_H_
#define GUI_WIDGET_H_

#include <functional>
#include <boost/ptr_container/ptr_vector.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Shape.hpp>
#include "style.h"

namespace gui {

class Screen;


/** @brief Base class for widgets.
 *
 * Style properties:
 *  - Position: x,y vector
 *
 * @note Name does not have to be unique and may be empty.
 */
class Widget: public sf::Drawable, public sf::Transformable, public StyleLoader
{
 public:
  Widget(const Screen& screen, const std::string& name);
  virtual ~Widget();

  /// Return the widget type as a string.
  virtual const std::string& type() const = 0;

  virtual const ResourceManager& res_mgr() const;
  /** @brief Get style entry key for a given property.
   *
   * The requested property is searched in the following sections (in order):
   *  - Screen.WidgetName (with fallback)
   *  - Screen.Type (with fallback)
   *  - Type
   *
   * @return true if found, false otherwise.
   */
  virtual bool searchStyle(const std::string& prop, std::string& key) const;
  virtual std::string styleErrorSection() const;

 protected:
  const Screen& screen_;
  const std::string name_;
};


/** @brief Focusable widget.
 */
class WFocusable: public Widget
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

  WFocusable(const Screen& screen, const std::string& name);

  virtual bool onInputEvent(const sf::Event&) { return false; }
  bool focused() const { return focused_; }
  virtual void focus(bool focused) { focused_ = focused; }
  WFocusable* neighbor(Neighbor n) const { return neighbors_[n]; }
  void setNeighbors(WFocusable* up, WFocusable* down, WFocusable* left, WFocusable* right);

 private:
  bool focused_;
  WFocusable* neighbors_[NEIGHBOR_COUNT];
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
  virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const;

 protected:
  virtual const std::string& type() const;

 public:
  typedef boost::ptr_vector<Widget> Container;
  Container widgets;
};


/** @brief Widget wrapping an ImageFrame.
 *
 * Style properties:
 *  - ImageFrame properties
 *  - Size
 */
class WFrame: public Widget
{
 public:
  WFrame(const Screen& screen, const std::string& name);
  virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const;

 protected:
  virtual const std::string& type() const;

 private:
  ImageFrame frame_;
  sf::Vector2f size_;
};


/** @brief Text button with image background
 *
 * Style properties:
 *  - StyleText properties
 *  - ImageFrameX properties
 *  - Width
 *  - Focus.*: all properties
 */
class WButton: public WFocusable
{
 public:
  struct Style {
    StyleText text;
    ImageFrameX::Style frame;
    sf::Color color;
    float width;

    void load(const StyleLoader& loader);
    void apply(WButton& o);
  };

  WButton(const Screen& screen, const std::string& name);
  void setCaption(const std::string& caption);
  virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const;
  virtual bool onInputEvent(const sf::Event&);
  virtual void focus(bool focused);
  typedef std::function<void()> Callback;
  void setCallback(Callback cb) { callback_ = cb; }

 protected:
  virtual const std::string& type() const;

 private:
  sf::Text caption_;  ///< Button caption
  ImageFrameX frame_;
  float width_;
  Style style_;
  Style style_focus_;
  Callback callback_;
};


/** @brief Simple text widget
 *
 * Style properties:
 *  - StyleText properties
 *  - TextAlign: left, center, right
 */
class WLabel: public Widget
{
 public:
  WLabel(const Screen& screen, const std::string& name);
  void setText(const std::string& caption);
  virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const;
  void setTextAlign(int align);

 protected:
  virtual const std::string& type() const;

 private:
  sf::Text text_;
  int align_;  ///< Text alignement (-1, 0, 1).
};


/** @brief Text field
 *
 * Style properties:
 *  - StyleText properties
 *  - ImageFrameX properties
 *  - Width
 *  - TextMarginsX: left and right margin for text
 *  - Focus.*
 */
class WEntry: public WFocusable
{
 public:
  struct Style {
    StyleText text;
    ImageFrameX::Style frame;
    sf::Color color;
    float width;
    float text_margin_left;
    float text_margin_right;

    void load(const StyleLoader& loader);
    void apply(WEntry& o);
  };

  WEntry(const Screen& screen, const std::string& name);
  void setText(const std::string& caption);
  virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const;
  virtual bool onInputEvent(const sf::Event&);
  virtual void focus(bool focused);
  std::string text() const { return text_.getString(); }

 protected:
  virtual const std::string& type() const;

 private:
  /// Update text image and cursor position after text input or cursor move
  void updateTextDisplay(bool force=false);

 private:
  class Cursor: public sf::Drawable {
   public:
    Cursor();
    void setHeight(float h);
    virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const;
    void setColor(const sf::Color& c);
    float x;
   private:
    sf::Vertex vertices_[2];
    sf::Color color_;
  };

  sf::RenderTexture text_img_;
  sf::Text text_;
  sf::Sprite text_sprite_;
  Cursor cursor_;
  ImageFrameX frame_;
  unsigned int cursor_pos_;  ///< cursor position, in the complete string
  float width_;
  Style style_;
  Style style_focus_;
};


/** @brief Choice among an option list
 *
 * Similar to a drop-down list, unless it does not drop-down.
 *
 * Style properties:
 *  - StyleText properties
 *  - ImageFrameX properties
 *  - Width
 *  - Focus.*
 */
class WChoice: public WFocusable
{
 public:
  struct Style {
    StyleText text;
    ImageFrameX::Style frame;
    sf::Color color;
    float width;

    void load(const StyleLoader& loader);
    void apply(WChoice& o);
  };

  typedef std::vector<std::string> ItemContainer;

  WChoice(const Screen& screen, const std::string& name);
  const ItemContainer& items() const { return items_; }
  unsigned int index() const { return index_; }
  void setItems(const ItemContainer& items);
  void select(unsigned int i);
  virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const;
  virtual bool onInputEvent(const sf::Event&);
  virtual void focus(bool focused);

 protected:
  virtual const std::string& type() const;

 private:
  ItemContainer items_;
  unsigned int index_;
  sf::Text text_;
  ImageFrameX frame_;
  float width_;
  Style style_;
  Style style_focus_;
};


}

#endif
