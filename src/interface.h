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


/** @brief Provide label features.
 *
 * Labels are used to display combos and chains.
 * step() must be called for each game tick.
 * Then, labels can be iterated using begin() and end().
 *
 * They are stored in a FIFO, pushed back and popped front.
 *
 */
template <unsigned int DT>
class LabelHolder
{
 public:
  static const unsigned int DURATION = DT;

  struct Label {
    Label(const FieldPos &pos, bool chain, unsigned int val):
        pos(pos), chain(chain), val(val), dt(DURATION) {}
    FieldPos pos;      ///< position, in field coordinates
    bool chain;        ///< true for chain, false for combo
    unsigned int val;  ///< combo or chain value
    unsigned int dt;   ///< remaining display time
  };
 private:
  typedef std::deque<Label> container_type;
 public:
  typedef typename container_type::const_iterator iterator;

  iterator begin() const { return labels_.begin(); }
  iterator end() const { return labels_.end(); }

  /// Update labels after a game tick.
  void step(const Field *fld)
  {
    // update display time
    typename container_type::iterator it;
    for( it=labels_.begin(); it!=labels_.end(); ++it ) {
      it->dt--;
    }
    // remove expired labels
    while( !labels_.empty() && labels_.front().dt == 0 ) {
      labels_.pop_front();
    }

    // create new labels, if needed
    if( fld->fcombo() ) {
      FieldPos pos = this->frameLabelPos(fld);
      if( pos.y < FIELD_HEIGHT ) {
        pos.y++; // display label above top matching block, if possible
      }
      if( fld->fchain() > 1 ) {
        this->addLabel(pos, true, fld->fchain());
        pos.y--;
      }
      if( fld->fcombo() > 3 ) {
        this->addLabel(pos, false, fld->fcombo());
      }
    }
  }

 protected:

  FieldPos frameLabelPos(const Field *fld)
  {
    unsigned int x, y;
    // get match position
    for( y=FIELD_HEIGHT; y>0; y-- ) {
      for( x=0; x<FIELD_WIDTH; x++ ) {
        const Block &bk = fld->block(x,y);
        if( bk.isState(BkColor::FLASH) &&
           bk.ntick - fld->tick() == fld->conf().flash_tk ) {
          return FieldPos(x,y);
        }
      }
    }
    // should not happen, fcombo_ != 0 tested before call
    assert( false );
    return FieldPos();
  }

  void addLabel(const FieldPos &pos, bool chain, unsigned int val)
  {
    labels_.push_back( Label(pos, chain, val) );
  }
  container_type labels_;
};


/// Default LabelHolder instanciation.
typedef LabelHolder<42> BasicLabelHolder;


#endif
