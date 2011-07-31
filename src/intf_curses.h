#ifndef INTF_CURSES_H_
#define INTF_CURSES_H_

#include "client.h"

// curses last to avoid naming conflicts due to bloody curses macros
#include <curses.h>

class IniFile;

namespace curses {

class FieldDisplay;

class CursesInterface: public ClientInstance::Observer,
    public GameInputScheduler::InputProvider
{
  friend class FieldDisplay;
 protected:
  static const std::string CONF_SECTION;

 public:
  CursesInterface();
  virtual ~CursesInterface();
  bool run(IniFile *cfg);

  /** @name ClientInstance::Observer methods. */
  //@{
  virtual void onChat(Player *pl, const std::string &msg);
  virtual void onPlayerJoined(Player *pl);
  virtual void onPlayerChangeNick(Player *pl, const std::string &nick);
  virtual void onPlayerReady(Player *pl);
  virtual void onPlayerQuit(Player *pl);
  virtual void onStateChange(GameInstance::State state);
  virtual void onPlayerStep(Player *pl);
  virtual void onNotification(GameInstance::Severity, const std::string &);
  virtual void onServerDisconnect();
  //@}
  virtual KeyState getNextInput(Player *pl);

  /// Add a message in given color.
  void addMessage(int color, const char *fmt, ...);

 private:

  bool initCurses();
  void endCurses();

  boost::asio::io_service io_service_;
  ClientInstance instance_;
  GameInputScheduler input_scheduler_;
  Player *player_;
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
  FieldDisplay(CursesInterface &intf, const Field &fld, int slot);
  ~FieldDisplay();

  void draw();
  /// Next game tick.
  void step();

 private:
  void drawBlock(int x, int y);

  CursesInterface &intf_;
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
