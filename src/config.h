#ifndef CONFIG_H_
#define CONFIG_H_

/** @file
 * @brief Configuration file.
 *
 * Configuration files use the INI file format.
 * This is a very simple implementation:
 *  - string quoting (with simple or double quotes)
 *  - escape sequences in strings: <tt>\\</tt>, <tt>\"</tt>, <tt>\'</tt>
 *  - strip whitespaces after keys and around values
 *  - comments begin with <tt>#</tt> or <tt>;</tt>
 *  - break lines with trailing <tt>\</tt>
 *
 * Empty values are considered as unset.
 */

#include <map>
#include <string>



class Config
{
 private:
  static const unsigned int MAX_LINE_SIZE;

 public:
  /// Create an empty config.
  Config() {}
  /// Load config from a file, add it to current content.
  bool load(const char *fname);

  /// Return true if the value exists.
  bool has(const std::string &section, const std::string &key) const;
  /// Retrieve a value as a string.
  std::string get(const std::string &section, const std::string &key, const std::string def) const;
  /// Retrieve a value as an integer.
  int getInt(const std::string &section, const std::string &key, int def) const;
  /// Retrieve a value as an double.
  double getDouble(const std::string &section, const std::string &key, double def) const;
  /// Retrieve a value as an boolean.
  bool getBool(const std::string &section, const std::string &key, bool def) const;
  /// Set a value.
  void set(const std::string &section, const std::string &key, std::string val);

 private:
  typedef std::map<std::string, std::string> section_type;
  typedef std::map<std::string, section_type> content_type;
  content_type content_;
};


#endif
