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



/// Parse and store key/values of an INI file.
class IniFile
{
 private:
  static const unsigned int MAX_LINE_SIZE;

 public:
  /// Create an empty INI file content.
  IniFile() {}
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
