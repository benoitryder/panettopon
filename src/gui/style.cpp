#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Text.hpp>
#include "style.h"

namespace gui {


StyleLoader::StyleLoader() {}
StyleLoader::~StyleLoader() {}


StyleLoaderPrefix::StyleLoaderPrefix(const StyleLoader& loader, const std::string& prefix, bool fallback):
    loader_(loader), prefix_(prefix), fallback_(fallback)
{
}

StyleLoaderPrefix::~StyleLoaderPrefix() {}

ResourceManager& StyleLoaderPrefix::res_mgr() const
{
  return loader_.res_mgr();
}

bool StyleLoaderPrefix::searchStyle(const std::string& prop, std::string& key) const
{
  if(loader_.searchStyle({prefix_, prop}, key)) {
    return true;
  } else if(fallback_) {
    return loader_.searchStyle(prop, key);
  } else {
    return false;
  }
}

std::string StyleLoaderPrefix::styleErrorSection() const
{
  return IniFile::join(loader_.styleErrorSection(), prefix_);
}



StyleError::StyleError(const std::string& key, const std::string& msg):
  std::runtime_error("style error for "+key+": "+msg) {}

StyleError::StyleError(const StyleLoader& loader, const std::string& prop, const std::string& msg):
  std::runtime_error("style error for "+loader.styleErrorSection()+"."+prop+": "+msg) {}



void StyleText::load(const StyleLoader& loader)
{
  ResourceManager& res_mgr = loader.res_mgr();
  const IniFile& ini = res_mgr.style();
  std::string key;

  font = res_mgr.getFont(loader.getStyle<std::string>("Font"));

  loader.fetchStyle<unsigned int>("FontSize", size);
  loader.fetchStyle<unsigned int>("FontBorderWidth", border_width);

  if(loader.searchStyle("FontStyle", key)) {
    const std::string val = ini.get<std::string>(key);
    unsigned int txt_style;
    if(val == "regular") {
      txt_style = sf::Text::Regular;
    } else if(val == "bold") {
      txt_style = sf::Text::Bold;
    } else if(val == "italic") {
      txt_style = sf::Text::Italic;
    } else if(val == "bold,italic" || val == "italic,bold") {
      txt_style = sf::Text::Bold|sf::Text::Italic;
    } else {
      throw StyleError(key, "invalid value");
    }
    text_style = txt_style;
  }

  loader.fetchStyle<sf::Color>("FontColor", color);
  loader.fetchStyle<sf::Color>("FontBorderColor", border_color);
}


void StyleText::apply(sf::Text& o) const
{
  if(!font) {
    throw std::runtime_error("text style font not set");
  }
  o.setFont(*font);
  o.setCharacterSize(size);
  o.setBorderWidth(border_width);
  o.setStyle(text_style);
  o.setColor(color);
  o.setBorderColor(border_color);
}


void StyleSprite::load(const StyleLoader& loader)
{
  ResourceManager& res_mgr = loader.res_mgr();

  image = res_mgr.getImage(loader.getStyle<std::string>("Image"));
  if(!loader.fetchStyle<decltype(rect)>("ImageRect", rect)) {
    rect = sf::IntRect(0, 0, image->getSize().x, image->getSize().y);
  }
}


void StyleSprite::apply(sf::Sprite& o) const
{
  o.setTexture(*image, true);
  o.setTextureRect(rect);
}


}
