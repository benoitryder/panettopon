#ifndef INIFILE_H_
#define INIFILE_H_

/** @file
 * @brief Parser for INI configuration files.
 *
 * This is a very simple implementation:
 *  - string quoting (with simple or double quotes)
 *  - escape sequences in strings: <tt>\\</tt>, <tt>\"</tt>, <tt>\'</tt>, <tt>\n</tt>
 *  - strip whitespaces after keys and around values
 *  - comments begin with <tt>#</tt> or <tt>;</tt>
 *  - break lines with trailing <tt>\</tt>
 *
 * Empty values are considered as unset.
 */

#include <map>
#include <string>
#include <stdexcept>
#include <boost/lexical_cast.hpp>



/// Parse and store key/values of an INI file.
class IniFile
{
 private:
  static const unsigned int MAX_LINE_SIZE;

 public:
  /// Create an empty INI file content.
  IniFile() {}
  /// Load config from a file, add it to current content.
  bool load(const std::string& fname);

  /// Return true if the value exists.
  bool has(const std::string& section, const std::string& key) const;
  /// Retrieve a value
  template <typename T> T get(const std::string& section, const std::string& key, const T& def) const;
  /// Convenient alias to retrieve a string value.
  inline std::string get(const std::string& section, const std::string& key, const char *def) const;
  /// Set a value.
  void set(const std::string& section, const std::string& key, const std::string& val);

 private:
  typedef std::map<std::string, std::string> section_type;
  typedef std::map<std::string, section_type> content_type;
  content_type content_;
};


template <typename T> T IniFile::get(const std::string& section, const std::string& key, const T& def) const
{
  content_type::const_iterator sec_it = content_.find(section);
  if( sec_it == content_.end() ) {
    return def;
  }
  section_type::const_iterator val_it = sec_it->second.find(key);
  if( val_it == sec_it->second.end() ) {
    return def;
  }
  try {
    return boost::lexical_cast<T>(val_it->second);
  } catch(const boost::bad_lexical_cast &) {
    throw std::runtime_error("failed to parse "+section+" / "+key+" value");
  }
}

std::string IniFile::get(const std::string& section, const std::string& key, const char *def) const
{
  return get<std::string>(section, key, def);
}


#endif
