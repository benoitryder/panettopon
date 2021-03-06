#ifndef GUI_WIDGET_H_
#define GUI_WIDGET_H_

#include <memory>
#include <vector>
#include <functional>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Shape.hpp>
#include "style.h"

namespace gui {

class Screen;
class InputMapping;


/** @brief Base class for widgets.
 *
 * Style properties:
 *  - Pos: x,y vector
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

  virtual bool onInputEvent(const InputMapping&, const sf::Event&) { return false; }
  bool focused() const { return focused_; }
  virtual void focus(bool focused) { focused_ = focused; }
  WFocusable* neighbor(Neighbor n) const { return neighbors_[n]; }
  void setNeighbor(Neighbor n, WFocusable* w);
  void setNeighbors(WFocusable* up, WFocusable* down, WFocusable* left, WFocusable* right);

  /** @brief Return neighbor to focus following an event
   *
   * @return the new object to focus, \e nullptr if event not handled or widget
   * is currently not focused.
   */
  WFocusable* neighborToFocus(const InputMapping& mapping, const sf::Event& ev);

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
  template <class T, class... Args> T& addWidget(Args&&... args)
  {
    std::unique_ptr<T> unique = std::make_unique<T>(args...);
    T* raw = unique.get();
    widgets.push_back(std::move(unique));
    return *raw;
  }

 protected:
  virtual const std::string& type() const;

 public:
  typedef std::vector<std::unique_ptr<Widget>> Container;
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
  void setColor(const sf::Color& c) { frame_.setColor(c); }

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
 *  - StyleTextAlign properties
 *  - ImageFrameX properties
 *  - Width
 *  - Focus.*: all properties
 */
class WButton: public WFocusable
{
 public:
  struct Style {
    StyleText text;
    StyleTextAlign align;
    ImageFrameX::Style frame;
    float width;

    void load(const StyleLoader& loader);
    void apply(WButton& o);
  };

  WButton(const Screen& screen, const std::string& name);
  void setCaption(const std::string& caption);
  virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const;
  virtual bool onInputEvent(const InputMapping&, const sf::Event&);
  virtual void focus(bool focused);
  typedef std::function<void()> Callback;
  void setCallback(Callback cb) { callback_ = cb; }

 protected:
  virtual const std::string& type() const;

 private:
  sf::Text caption_;  ///< Button caption
  ImageFrameX frame_;
  Style style_;
  Style style_focus_;
  Style* current_style_;
  Callback callback_;
};


/** @brief Simple text widget
 *
 * Style properties:
 *  - StyleText properties
 *  - StyleTextAlign properties
 */
class WLabel: public Widget
{
 public:
  WLabel(const Screen& screen, const std::string& name);
  void setText(const std::string& caption);
  virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const;

 protected:
  virtual const std::string& type() const;

 private:
  sf::Text text_;
  StyleTextAlign style_align_;
};


/** @brief Text field
 *
 * Style properties:
 *  - StyleText properties
 *  - ImageFrameX properties
 *  - XAlign
 *  - Width
 *  - TextMarginsX: left and right margin for text
 *  - Focus.*
 *  - Active.*
 */
class WEntry: public WFocusable
{
 public:
  struct Style {
    StyleText text;
    ImageFrameX::Style frame;
    XAlign xalign;
    float width;
    float text_margin_left;
    float text_margin_right;

    void load(const StyleLoader& loader);
    void apply(WEntry& o);
  };

  WEntry(const Screen& screen, const std::string& name, bool auto_active=true);
  void setText(const std::string& caption);
  virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const;
  virtual bool onInputEvent(const InputMapping&, const sf::Event&);
  virtual void focus(bool focused);
  std::string text() const { return text_.getString(); }
  typedef std::function<void(bool)> Callback;
  void setCallback(Callback cb) { callback_ = cb; }

 protected:
  virtual const std::string& type() const;

 private:
  /// Update text, text image and cursor position for display
  void updateTextDisplay(bool force=false);
  void activate();
  void deactivate(bool validate);

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
  bool active_;
  bool auto_active_;
  Style style_;
  Style style_focus_;
  Style style_active_;
  Style* current_style_;
  Callback callback_;
};


/** @brief Choice among an option list
 *
 * Similar to a drop-down list, unless it does not drop-down.
 *
 * Style properties:
 *  - StyleText properties
 *  - StyleTextAlign properties
 *  - ImageFrameX properties
 *  - Width
 *  - Focus.*
 */
class WChoice: public WFocusable
{
 public:
  struct Style {
    StyleText text;
    StyleTextAlign align;
    ImageFrameX::Style frame;
    float width;

    void load(const StyleLoader& loader);
    void apply(WChoice& o);
  };

  typedef std::vector<std::string> ItemContainer;
  typedef std::function<void()> Callback;

  WChoice(const Screen& screen, const std::string& name);
  const ItemContainer& items() const { return items_; }
  unsigned int index() const { return index_; }
  const ItemContainer::value_type& value() const { return items_[index_]; }
  void setItems(const ItemContainer& items);
  void select(unsigned int i);
  /// Select given value, return \e true if found, \e false otherwise
  bool selectValue(const ItemContainer::value_type& v);
  /// Add an item, return its index
  unsigned int addItem(const ItemContainer::value_type& v);
  virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const;
  virtual bool onInputEvent(const InputMapping&, const sf::Event&);
  virtual void focus(bool focused);

  /// Set a callback to call on value change
  void setCallback(Callback cb) { callback_ = cb; }

 protected:
  virtual const std::string& type() const;

 private:
  ItemContainer items_;
  unsigned int index_;
  sf::Text text_;
  ImageFrameX frame_;
  Style style_;
  Style style_focus_;
  Style* current_style_;
  Callback callback_;
};


}

#endif
