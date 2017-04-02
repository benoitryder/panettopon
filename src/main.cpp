#ifdef WIN32
#include <winsock2.h>  // workaround for boost bug
#include <windows.h>
#else
#include <sys/time.h>
#endif
#include <unistd.h>
#include <cstdio>
#include "inifile.h"
#include "log.h"
#include "optget.h"
#include "util.h"

#ifndef WITHOUT_INTF_SERVER
#include "intf_server.h"
#endif
#ifndef WITHOUT_INTF_CURSES
#include "intf_curses.h"
#endif
#ifndef WITHOUT_INTF_GUI
#include "gui/interface.h"
#endif

/// Default config file.
#define CONF_FILE_DEFAULT  "panettopon.ini"


/// Return current time (used to initialize randomness)
inline int64_t current_time(void)
{
#ifdef WIN32
  LARGE_INTEGER freq;
  LARGE_INTEGER t;
  QueryPerformanceFrequency(&freq);
  QueryPerformanceCounter(&t);
  return 1000000*t.QuadPart/freq.QuadPart;
#else
  struct timeval tv;
  gettimeofday(&tv, 0);
  return tv.tv_sec*1000000 + tv.tv_usec;
#endif
}


#ifdef USE_WINMAIN
#include <shellapi.h>

int main(int /*argc*/, char** argv);

/// Wrapper for the standard main function
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
  return main(__argc, __argv);
}

#endif


/// Print program usage.
void usage(void)
{
  printf(
      "PaNettoPon - a network multiplayer Panel de Pon\n"
      "\n"
      "usage: panettopon [OPTIONS] [host] port\n"
      "\n"
      " -c  --conf       configuration file\n"
      " -i  --interface  interface type, from the following\n"
#ifndef WITHOUT_INTF_SERVER
      "                      server  simple server runner\n"
#endif
#ifndef WITHOUT_INTF_GUI
      "                      gui     graphic interface\n"
#endif
#ifndef WITHOUT_INTF_CURSES
      "                      curses  text-based interface\n"
#endif
      " -n  --nick       nickname\n"
      " -h, --help       display this help\n"
      " -o, --log-file   log messages to the given file (messages are still\n"
      "                  displayed), use \"-\" to write to stderr\n"
    );
}


/** @brief Entry point.
 *
 * Parse program arguments, start interfaces.
 *
 * @retval  0  program lived normally
 * @retval  1  fatal error
 * @retval  2  invalid arguments
 * @retval  3  configuration file error
 */
int main(int /*argc*/, char** argv)
{
  try {
    OptGetItem opts[] = {
      { 'c', "conf", OPTGET_STR, {} },
      { 'i', "interface", OPTGET_STR, {} },
      { 'n', "nick", OPTGET_STR, {} },
      { 'o', "log-file", OPTGET_STR, {} },
      { 'h', "help", OPTGET_FLAG, {} },
      { 0, 0, OPTGET_NONE, {} }
    };

    auto ptr_logger = std::make_unique<FileLogger>();
    FileLogger& file_logger = *ptr_logger;
    Logger::setLogger(std::move(ptr_logger));

    // Program arguments
    const char* conf_file = NULL;
    char* port = NULL;
    char* host = NULL;
    const char* nick = NULL;
    const char* intfarg = NULL;

    char* const* opt_args = argv+1;
    OptGetItem* opt;
    int ret;
    for(;;) {
      ret = optget_parse(opts, &opt_args, &opt);
      if( ret != OPTGET_OK ) {
        break;
      }

      // extra arguments
      if( opt->type == OPTGET_NONE ) {
        if( port == NULL ) {
          port = opt->value.str;
        } else if( host == NULL ) {
          host = port;
          port = opt->value.str;
        }
        else {
          LOG("unexpected extra argument: %s", opt->value.str);
        }
      } else {
        // options
        switch( opt->short_name ) {
          case 'c':
            conf_file = opt->value.str;
            break;
          case 'i':
            intfarg = opt->value.str;
            break;
          case 'n':
            nick = opt->value.str;
            break;
          case 'o':
            file_logger.setFile( opt->value.str );
            break;
          case 'h':
            usage();
            return 0;
            break;
          default:
            break;
        }
      }
    }

    // standard parsing errors
    if( ret != OPTGET_LAST ) {
      switch( ret ) {
        case OPTGET_ERR_SHORT_NAME:
        case OPTGET_ERR_LONG_NAME:
          LOG("unknown option: %s", *opt_args);
          break;
        case OPTGET_ERR_VAL_FMT:
          LOG("invalid option value: %s", *opt_args);
          break;
        case OPTGET_ERR_VAL_MISSING:
          LOG("missing option value: -%c", opt->short_name);
          break;
        case OPTGET_ERR_VAL_UNEXP:
          LOG("unexpected value for option: %s", *opt_args);
          break;
        default:
          // nothing to do
          break;
      }
      return 2;
    }

    // load configuration
    IniFile cfg;
    if( conf_file == NULL && ::access(CONF_FILE_DEFAULT, R_OK) == 0 ) {
      conf_file = CONF_FILE_DEFAULT; // default config file
    }
    if( conf_file != NULL ) {
      if( !cfg.load(conf_file) ) {
        LOG("failed to load configuration file");
        return 3;
      }
    }

    if( intfarg != NULL ) {
      cfg.set("Global.Interface", intfarg);
    }
    if( port != NULL ) {
      cfg.set("Global.Port", port);
    }
    if( host != NULL ) {
      cfg.set("Client.Hostname", host);
    }
    if( nick != NULL ) {
      cfg.set("Client.Nick", nick);
    }

    // init randomness
    ::srand( current_time() );

    const std::string intfstr = cfg.get("Global.Interface", "server");
#ifndef WITHOUT_INTF_SERVER
    if( intfstr == "server" ) {
      BasicServerInterface intf;
      ret = intf.run(cfg);
    } else
#endif
#ifndef WITHOUT_INTF_CURSES
    if( intfstr == "curses" ) {
      curses::CursesInterface intf;
      ret = intf.run(cfg);
    } else
#endif
#ifndef WITHOUT_INTF_GUI
    if( intfstr == "gui" ) {
      gui::GuiInterface intf;
      ret = intf.run(cfg);
    } else
#endif
    {
      LOG("invalid interface: '%s'", intfstr.c_str());
      return 2;
    }
    return ret ? 0 : 1;
  } catch(const std::exception& e) {
    fprintf(stderr, "fatal error: %s\n", e.what());
    return 1;
  }
}

