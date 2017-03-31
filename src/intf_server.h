#ifndef INTF_SERVER_H_
#define INTF_SERVER_H_

#include "server.h"


class BasicServerInterface: public ServerInstance::Observer
{
 public:
  BasicServerInterface();
  bool run(IniFile& cfg);

  /** @name ServerInstance::Observer methods. */
  //@{
  virtual void onChat(Player& pl, const std::string& msg);
  virtual void onPlayerJoined(Player& pl);
  virtual void onPlayerChangeNick(Player& pl, const std::string& nick);
  virtual void onPlayerStateChange(Player& pl);
  virtual void onPlayerChangeFieldConf(Player& pl);
  virtual void onStateChange();
  virtual void onServerChangeFieldConfs();
  virtual void onPlayerStep(Player& pl);
  virtual void onPlayerRanked(Player& pl);
  //@}

 private:
  ServerInstance* instance_;
};

#endif
