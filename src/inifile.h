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

#include <vector>
#include <map>
#include <string>
#include <stdexcept>
#include <initializer_list>
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

  typedef std::initializer_list<const std::string> Path;

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
  /// Retrieve a value, use default if not set
  template <typename T> T get(const std::string& key, const T& def) const;
  /// Convenient alias to retrieve a string value
  inline std::string get(const std::string& key, const char* def) const;

  /** @name Aliases for access by split key path */
  //@{
  inline bool has(Path path) const;
  template <typename T> T get(Path path) const;
  template <typename T> void set(Path path, const T& val);
  inline void unset(Path path);
  template <typename T> T get(Path path, const T& def) const;
  inline std::string get(Path path, const char* def) const;
  //@}

  /// Build a key from a split key path
  static std::string join(Path path);
  /// Alternate join to avoid the need of brackets
  template <typename ...T> static std::string join(T... t) { return join({t...}); }

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

template <typename T> struct IniFileConverter<std::vector<T>>
{
  static std::vector<T> parse(const std::string& value)
  {
    std::vector<T> vec;
    if(value.empty()) {
      return vec;
    }
    T val;
    char c = ',';
    std::istringstream in(value);
    in >> val;
    while(in && c == ',') {
      vec.push_back(val);
      in >> c >> val;
    }
    return vec;
  }
};


template <typename T> T IniFile::get(const std::string& key) const
{
  entries_type::const_iterator it = entries_.find(key);
  if( it != entries_.end() ) {
    try {
      return IniFileConverter<T>::parse(it->second);
    } catch(const std::exception& e) {
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

template <typename T> T IniFile::get(const std::string& key, const T& def) const
{
  try {
    return get<T>(key);
  } catch(const NotSetError&) {
    return def;
  }
}

std::string IniFile::get(const std::string& key, const char* def) const
{
  return get<std::string>(key, def);
}


bool IniFile::has(Path path) const
{
  return has(join(path));
}

template <typename T> T IniFile::get(Path path) const
{
  return get<T>(join(path));
}

template <typename T> void IniFile::set(Path path, const T& val)
{
  set<T>(join(path), val);
}

void IniFile::unset(Path path)
{
  unset(join(path));
}

template <typename T> T IniFile::get(Path path, const T& def) const
{
  return get<T>(join(path), def);
}

inline std::string IniFile::get(Path path, const char* def) const
{
  return get<std::string>(join(path), def);
}


/** @brief Parsing helpers
 */
namespace parsing {

/** @brief Parse a value from a string, ending at a given delimiter.
 *
 * Parse value from \a source substring starting at \a pos and ending at the
 * first delimiter found after \a pos.
 * If delimiter is not found, parse the whole remaining string.
 *
 * Return the position after the delimiter (or std::string::npos if not found).
 */
template <typename T> size_t castUntil(const std::string& source, size_t pos, char delim, T& value)
{
  if(pos > source.size()) {
    throw std::out_of_range("cannot parse value, missing data");
  }
  size_t sep = source.find(delim, pos);
  value = boost::lexical_cast<T>(source.substr(pos, sep - pos));
  if(sep != std::string::npos) {
    sep++;
  }
  return sep;
}

/// Same as parseUntil(), but using IniFileConverter::parse
template <typename T> size_t convertUntil(const std::string& source, size_t pos, char delim, T& value)
{
  if(pos > source.size()) {
    throw std::out_of_range("cannot parse value, missing data");
  }
  size_t sep = source.find(delim, pos);
  value = IniFileConverter<T>::parse(source.substr(pos, sep - pos));
  if(sep != std::string::npos) {
    sep++;
  }
  return sep;
}


}


#endif
