#include <cstdio>
#include <cstdlib>
#include "inifile.h"


const unsigned int IniFile::MAX_LINE_SIZE = 4096;


bool IniFile::load(const std::string& fname)
{
  FILE *fp = fopen(fname.c_str(), "r");
  if( fp == NULL ) {
    return false;
  }

  char buf[MAX_LINE_SIZE];
  std::string section;
  for(;;) {

    // read next line
    std::string line;
    for(;;) {
      if( fgets(buf, sizeof(buf), fp) == NULL ) {
        if( feof(fp) ) {
          goto ok;
        } else {
          goto error;
        }
      }
      line += buf;

      // strip CR/LF
      size_t pos = line.find_first_of("\r\n");
      if( pos != std::string::npos ) { // should always be true
        line.erase(pos);
      }

      if( line.empty() || line[line.size()-1] != '\\' ) {
        break;
      }
      // trailing \: join lines
      line.erase(line.size()-1);
    }

    // blank ligne: ignore
    if( line.find_first_not_of(" \t") == std::string::npos ) {
      continue;
    }
    // comment: ignore
    if( line[0] == '#' || line[0] == ';' ) {
      continue;
    }

    if( line[0] == '[' ) {

      // section
      size_t pos = line.find(']');
      if( pos < 2 || pos == std::string::npos ) {
        goto error;
      }
      section = line.substr(1, pos-1);

    } else {

      if( section.empty() ) {
        goto error; // no current section
      }

      // value

      // read the key
      size_t pos_eq = line.find('=');
      if( pos_eq == 0 || pos_eq == std::string::npos ) {
        goto error;
      }
      size_t pos_key = line.find_last_not_of(" \t", pos_eq-1);
      if( pos_key == std::string::npos ) {
        goto error;
      }
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
            if( p >= line.size() ) {
              goto error; // no ending delimiter
            } else if( line[p] == delim ) {
              break; // end of string
            } else if( line[p] == '\\' ) {
              if( line[p+1]=='\\' || line[p+1]=='\"' || line[p+1] == '\'' ) {
                line.erase(p,1); // escape
              } else if( line[p+1] == '\n' ) {
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
      if( !val.empty() ) {
        entries_[section+'.'+key] = val;
      }
    }
  }

ok:
  fclose(fp);
  return true;
error:
  fclose(fp);
  return false;
}


bool IniFile::has(const std::string& key) const
{
  return entries_.find(key) != entries_.end();
}

void IniFile::set(const std::string& key, const std::string& val)
{
  if( val.empty() ) {
    entries_.erase(key);
  } else {
    entries_[key] = val;
  }
}

