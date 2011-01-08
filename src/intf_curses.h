#ifndef INTF_CURSES_H_
#define INTF_CURSES_H_

#include "interface.h"
#include "client.h"

// curses last to avoid naming conflicts due to bloody curses macros
#include <curses.h>

class Config;

namespace curses {

class FieldDisplay;

class CursesInterface: public ClientInterface
{
 protected:
  static const std::string CONF_SECTION;

 public:
  CursesInterface();
  virtual ~CursesInterface();
  virtual bool run(const Config &cfg);
  virtual void onChat(const Player *pl, const std::string &msg);
  virtual void onNotification(Severity sev, const std::string &msg);
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
  virtual KeyState getNextInput();

  /// Add a message in given color.
  void addMessage(int color, const char *fmt, ...);

 private:

  bool initCurses();
  void endCurses();

  boost::asio::io_service io_service_;
  ClientInstance client_;
  /// Window for messages.
  WINDOW *wmsg_;

  typedef boost::ptr_map<const Field *, FieldDisplay> FieldDisplayMap;
  FieldDisplayMap fdisplays_;

  /// Key bindings.
  struct {
    int up, down, left, right;
    int swap, raise;
    int quit;
  } keys_;

  /// Get a key code from a key name, 0 if invalid.
  static int str2key(const std::string &s);
};


class FieldDisplay
{
 public:
  FieldDisplay(const Field &fld, int slot);
  ~FieldDisplay();

  void draw();
  /// Next game tick.
  void step();

 private:
  void drawBlock(int x, int y);

  const Field &field_;
  WINDOW *wfield_;
  WINDOW *wgrid_;

  /** @name Labels. */
  //@{

  struct Label {
    static const unsigned int DURATION;
    Label(const FieldPos &pos, bool chain, unsigned int val);
    FieldPos pos;      ///< position, in field coordinates
    bool chain;        ///< true for chain, false for combo
    unsigned int val;  ///< combo or chain value
    unsigned int dt;   ///< remaining display time
  };

  typedef std::deque<Label> LabelContainer;
  LabelContainer labels_;

  /// Return top-left match position.
  FieldPos matchLabelPos();

  //@}
};

}

#endif
