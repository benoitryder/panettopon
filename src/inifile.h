#ifndef INIFILE_H_
#define INIFILE_H_

/** @file
 * @brief Parser for INI configuration files.
 *
 * This is a very simple implementation:
 *  - string quoting (with simple or double quotes)
 *  - escape sequences in strings: <tt>\\</tt>, <tt>\"</tt>, <tt>\'</tt>, <tt>\n</tt>
 *  - strip whitespaces after keys and around values
 *  - comments begin with <tt>#</tt> or <tt>;</tt> at the beginning of the line
 *  - break lines with trailing <tt>\</tt>
 *
 * Section and values names are internally stored as dotted strings. They are
 * both joined by a dot to obtain the actual entry key.
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
  class NotSetError: public std::runtime_error {
   public:
    NotSetError(const std::string& key):
        std::runtime_error("value not set: "+key) {}
  };
  class ParseError: public std::runtime_error {
   public:
    ParseError(const std::string& key):
        std::runtime_error("failed to parse value: "+key) {}
  };
  class ConvertError: public std::runtime_error {
   public:
    ConvertError(const std::string& key):
        std::runtime_error("failed to convert value: "+key) {}
  };

  /// Create an empty INI file content.
  IniFile() {}
  /// Load config from a file, add it to current content.
  bool load(const std::string& fname);

  /// Return true if the value exists.
  bool has(const std::string& key) const;
  /// Retrieve a value, throw a NotSetError if not set
  template <typename T> T get(const std::string& key) const;
  /// Set a value
  template <typename T> void set(const std::string& key, const T& val);
  /// Unset a value
  void unset(const std::string& key);

  /** @name Aliases for access by section and key */
  //@{
  inline bool has(const std::string& section, const std::string& key) const;
  template <typename T> T get(const std::string& section, const std::string& key) const;
  template <typename T> void set(const std::string& section, const std::string& key, const T& val);
  //@}

  /// Retrieve a value, use default if not set
  template <typename T> T get(const std::string& section, const std::string& key, const T& def) const;
  /// Convenient alias to retrieve a string value
  inline std::string get(const std::string& section, const std::string& key, const char* def) const;


 private:
  typedef std::map<std::string, std::string> entries_type;
  entries_type entries_;
};


/** @brief Convert a configuration string to an object
 *
 * The default is to use boost::lexical_cast.
 *
 * Since partial function specialization is not permitted, we have to use a
 * template class to allow specialization for vectors, etc.
 */
template <typename T> struct IniFileConverter
{
  static T parse(const std::string& value)
  {
    return boost::lexical_cast<T>(value);
  }
};


template <typename T> T IniFile::get(const std::string& key) const
{
  entries_type::const_iterator it = entries_.find(key);
  if( it != entries_.end() ) {
    try {
      return IniFileConverter<T>::parse(it->second);
    } catch(const std::exception&) {
      throw ParseError(key);
    }
  }
  throw NotSetError(key);
}

template <typename T> void IniFile::set(const std::string& key, const T& val)
{
  try {
    const std::string sval = boost::lexical_cast<std::string>(val);
    if( sval.empty() ) {
      entries_.erase(key);
    } else {
      entries_[key] = sval;
    }
  } catch(const boost::bad_lexical_cast&) {
    throw ConvertError(key);
  }
}

bool IniFile::has(const std::string& section, const std::string& key) const
{
  return has(section+'.'+key);
}

template <typename T> T IniFile::get(const std::string& section, const std::string& key) const
{
  return get<T>(section+'.'+key);
}

template <typename T> void IniFile::set(const std::string& section, const std::string& key, const T& val)
{
  set<T>(section+'.'+key, val);
}

template <typename T> T IniFile::get(const std::string& section, const std::string& key, const T& def) const
{
  try {
    return get<T>(section, key);
  } catch(const NotSetError&) {
    return def;
  }
}

std::string IniFile::get(const std::string& section, const std::string& key, const char* def) const
{
  return get<std::string>(section, key, def);
}


#endif
