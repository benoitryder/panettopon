#ifndef INTERFACE_H_
#define INTERFACE_H_

#include <string>
#include "game.h"
#include "util.h"
#undef SEVERITY_ERROR   // defined in windows.h

class Config;
class Player;
class Match;


class Interface
{
 public:
  Interface() {}
  virtual ~Interface() {}
  virtual bool run(const Config &cfg) = 0;

  enum Severity {
    SEVERITY_MESSAGE = 1,
    SEVERITY_NOTICE,
    SEVERITY_WARNING,
    SEVERITY_ERROR,
  };

  virtual void onChat(const Player *pl, const std::string &msg) = 0;
  virtual void onPlayerJoined(const Player *pl) = 0;
  virtual void onPlayerSetNick(const Player *pl, const std::string &old_nick) = 0;
  virtual void onPlayerReady(const Player *pl) = 0;
  virtual void onPlayerQuit(const Player *pl) = 0;
  virtual void onMatchInit(const Match *m) = 0;
  virtual void onMatchReady(const Match *m) = 0;
  virtual void onMatchStart(const Match *m) = 0;
  virtual void onMatchEnd(const Match *m) = 0;
  /// Fields stepped one or several ticks.
  virtual void onFieldStep(const Field *fld) = 0;
  virtual void onFieldLost(const Field *fld) = 0;
};


class ServerInterface: public Interface
{
 public:
  ServerInterface() {}
  virtual ~ServerInterface() {}
};


class ClientInterface: public Interface
{
 public:
  ClientInterface() {}
  virtual ~ClientInterface() {}

  virtual void onNotification(Severity sev, const std::string &msg) = 0;
  virtual KeyState getNextInput() = 0;
};


#endif
