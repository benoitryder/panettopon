#ifndef INTF_SERVER_H_
#define INTF_SERVER_H_

#include "interface.h"

class BasicServerInterface: public ServerInterface
{
 public:
  virtual bool run(const Config &cfg);
  virtual void onChat(const Player *pl, const std::string &msg);
  virtual void onPlayerJoined(const Player *pl);
  virtual void onPlayerSetNick(const Player *pl, const std::string &old_nick);
  virtual void onPlayerReady(const Player *pl);
  virtual void onPlayerQuit(const Player *pl);
  virtual void onMatchInit(const Match *m);
  virtual void onMatchReady(const Match *m);
  virtual void onMatchStart(const Match *m);
  virtual void onMatchEnd(const Match *m);
  virtual void onFieldStep(const Field *fld);
  virtual void onFieldLost(const Field *fld);
};

#endif
