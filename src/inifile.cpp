#include <cstdio>
#include <cstdlib>
#include "inifile.h"


const unsigned int IniFile::MAX_LINE_SIZE = 4096;


bool IniFile::load(const char *fname)
{
  FILE *fp = fopen(fname, "r");
  if( fp == NULL )
    return false;

  char buf[MAX_LINE_SIZE];
  content_type::iterator section_it = content_.end();
  for(;;) {

    // read next line
    std::string line;
    for(;;) {
      if( fgets(buf, sizeof(buf), fp) == NULL ) {
        if( feof(fp) )
          goto ok;
        else
          goto error;
      }
      line += buf;

      // strip CR/LF
      size_t pos = line.find_first_of("\r\n");
      if( pos != std::string::npos ) // should always be true
        line.erase(pos);

      if( line.empty() || line[line.size()-1] != '\\' )
        break;
      // trailing \: join lines
      line.erase(line.size()-1);
    }

    // blank ligne: ignore
    if( line.find_first_not_of(" \t") == std::string::npos )
      continue;
    // comment: ignore
    if( line[0] == '#' || line[0] == ';' )
      continue;

    if( line[0] == '[' ) {

      // section
      size_t pos = line.find(']');
      if( pos < 2 || pos == std::string::npos )
        goto error;
      std::string name = line.substr(1, pos-1);
      content_type::value_type section(name, section_type());
      section_it = content_.insert( section ).first;

    } else {

      if( section_it == content_.end() )
        goto error; // no current section

      // value

      // read the key
      size_t pos_eq = line.find('=');
      if( pos_eq == 0 || pos_eq == std::string::npos )
        goto error;
      size_t pos_key = line.find_last_not_of(" \t", pos_eq-1);
      if( pos_key == std::string::npos )
        goto error;
      std::string key = line.substr(0, pos_key+1);

      // read the value
      std::string val;
      size_t pos_val = line.find_first_not_of(" \t", pos_eq+1);
      if( pos_val != std::string::npos ) {
        // non-empty value
        if( line[pos_val] == '\"' || line[pos_val] == '\'' ) {
          // string: process escape sequences and find ending delimiter
          char delim = line[pos_val];
          size_t p = pos_val+1;
          for(;;) {
            if( p >= line.size() )
              goto error; // no ending delimiter
            if( line[p] == delim )
              break; // end of string
            if( line[p] == '\\' ) {
              if( line[p+1]=='\\' || line[p+1]=='\"' || line[p+1] == '\'' )
                line.erase(p,1); // escape
              else if( line[p+1] == '\n' ) {
                line.erase(p,1);
                line[p] = '\n';
              }
            }
            p++;
          }
          val = line.substr(pos_val+1, p-(pos_val+1));
        } else {
          // simple value: strip comments
          size_t p = line.find_first_of("#;", pos_val);
          val = line.substr(pos_val, p-pos_val-1); // ok if p == npos
        }
      }
      if( !val.empty() )
        (*section_it).second[key] = val;
    }
  }

ok:
  fclose(fp);
  return true;
error:
  fclose(fp);
  return false;
}


bool IniFile::has(const std::string &section, const std::string &key) const
{
  content_type::const_iterator sec_it = content_.find(section);
  if( sec_it == content_.end() )
    return false;
  section_type::const_iterator val_it = sec_it->second.find(key);
  if( val_it == sec_it->second.end() )
    return false;
  return true;
}

std::string IniFile::get(const std::string &section, const std::string &key, const std::string def) const
{
  content_type::const_iterator sec_it = content_.find(section);
  if( sec_it == content_.end() )
    return def;
  section_type::const_iterator val_it = sec_it->second.find(key);
  if( val_it == sec_it->second.end() )
    return def;
  return val_it->second;
}

int IniFile::getInt(const std::string &section, const std::string &key, int def) const
{
  content_type::const_iterator sec_it = content_.find(section);
  if( sec_it == content_.end() )
    return def;
  section_type::const_iterator val_it = sec_it->second.find(key);
  if( val_it == sec_it->second.end() )
    return def;
  const std::string &val = val_it->second;
  const char *s = val.c_str();
  char *ptr = NULL;
  int ret = strtol(s, &ptr, 0);
  if( ptr != s + val.size() )
    return def;
  return ret;
}

double IniFile::getDouble(const std::string &section, const std::string &key, double def) const
{
  content_type::const_iterator sec_it = content_.find(section);
  if( sec_it == content_.end() )
    return def;
  section_type::const_iterator val_it = sec_it->second.find(key);
  if( val_it == sec_it->second.end() )
    return def;
  const std::string &val = val_it->second;
  const char *s = val.c_str();
  char *ptr = NULL;
  double ret = strtod(s, &ptr);
  if( ptr != s + val.size() )
    return def;
  return ret;
}

bool IniFile::getBool(const std::string &section, const std::string &key, bool def) const
{
  content_type::const_iterator sec_it = content_.find(section);
  if( sec_it == content_.end() )
    return def;
  section_type::const_iterator val_it = sec_it->second.find(key);
  if( val_it == sec_it->second.end() )
    return def;
  const std::string &val = val_it->second;
  switch( val[0] ) {
    case '1':
    case 'y': case 'Y':
    case 't': case 'T':
    case 'o': case 'O':
      return true;
    case '0':
    case 'n': case 'N':
    case 'f': case 'F':
      return false;
    default:
      return def;
  }
}

void IniFile::set(const std::string &section, const std::string &key, std::string val)
{
  if( val.empty() )
    content_[section].erase(key);
  else
    content_[section][key] = val;;
}

