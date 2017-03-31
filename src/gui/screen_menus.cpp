#include <functional>
#include "screen_menus.h"
#include "screen_game.h"
#include "interface.h"
#include "../server.h"
#include "../log.h"

namespace gui {


ScreenStart::ScreenStart(GuiInterface& intf):
    Screen(intf, "ScreenStart")
{
}

void ScreenStart::enter()
{
  intf_.stopInstance();
  const ResourceManager& res_mgr = intf_.res_mgr();

  WButton* button_join = &container_.addWidget<WButton>(*this, "JoinServer");
  button_join->setCaption(res_mgr.getLang({name_, "JoinServer"}));

  WButton* button_create = &container_.addWidget<WButton>(*this, "CreateServer");
  button_create->setCaption(res_mgr.getLang({name_, "CreateServer"}));

  button_exit_ = &container_.addWidget<WButton>(*this, "Exit");
  button_exit_->setCaption(res_mgr.getLang({name_, "Exit"}));

  button_join->setNeighbors(button_exit_, button_create, NULL, NULL);
  button_create->setNeighbors(button_join, button_exit_, NULL, NULL);
  button_exit_->setNeighbors(button_create, button_join, NULL, NULL);

  button_join->setCallback(std::bind(&ScreenStart::onJoinServer, this));
  button_create->setCallback(std::bind(&ScreenStart::onCreateServer, this));
  button_exit_->setCallback(std::bind(&GuiInterface::swapScreen, &intf_, nullptr));

  this->focus(button_join);

  // "Debug start" button, only when debug mode is enabled
  if(intf_.cfg().get<bool>("Global.Debug", false)) {
    WButton* button_debugstart = &container_.addWidget<WButton>(*this, "DebugStart");
    button_debugstart->setCaption(res_mgr.getLang({name_, "DebugStart"}));

    button_debugstart->setNeighbors(button_exit_, button_join, NULL, NULL);
    button_debugstart->setCallback(std::bind(&ScreenStart::onDebugStart, this));
    button_join->setNeighbors(button_debugstart, button_create, NULL, NULL);
    button_exit_->setNeighbors(button_create, button_debugstart, NULL, NULL);

    this->focus(button_debugstart);
  }
}

bool ScreenStart::onInputEvent(const sf::Event& ev)
{
  if(Screen::onInputEvent(ev)) {
    return true;
  }
  if(InputMapping::Global.cancel.match(ev)) {
    if( focused_ == button_exit_ ) {
      intf_.swapScreen(nullptr);
    } else {
      this->focus(button_exit_);
    }
    return true;
  }
  return false;
}

void ScreenStart::onDebugStart()
{
  // create server and players
  intf_.startServer(intf_.cfg().get<uint16_t>("Global.Port"));
  Player& pl1 = intf_.server()->newLocalPlayer("Player 1");
  Player& pl2 = intf_.server()->newLocalPlayer("Player 2");
  // swap to screen game
  auto new_screen = std::make_unique<ScreenGame>(intf_);
  new_screen->setPlayerMapping(pl1, InputMapping::DefaultKeyboardMapping);
  new_screen->setPlayerMapping(pl2, InputMapping::DefaultKeyboardMapping);
  intf_.swapScreen(new_screen.release());
  // lobby: players are ready
  GameInstance* instance = intf_.instance();
  instance->playerSetState(pl1, Player::State::LOBBY_READY);
  instance->playerSetState(pl2, Player::State::LOBBY_READY);
}

void ScreenStart::onJoinServer()
{
  intf_.swapScreen(new ScreenJoinServer(intf_));
}

void ScreenStart::onCreateServer()
{
  intf_.swapScreen(new ScreenCreateServer(intf_));
}


ScreenJoinServer::ScreenJoinServer(GuiInterface& intf):
    Screen(intf, "ScreenJoinServer"),
    submitting_event_{sf::Event::Count, {}}
{
}

void ScreenJoinServer::enter()
{
  const ResourceManager& res_mgr = intf_.res_mgr();
  const IniFile& cfg = intf_.cfg();

  WLabel* label = &container_.addWidget<WLabel>(*this, "HostPortLabel");
  label->setText(res_mgr.getLang({name_, "HostPort"}));

  entry_host_ = &container_.addWidget<WEntry>(*this, "HostEntry");
  entry_host_->setText(cfg.get("Client.Hostname", ""));

  entry_port_ = &container_.addWidget<WEntry>(*this, "PortEntry");
  entry_port_->setText(cfg.get("Global.Port", ""));

  label = &container_.addWidget<WLabel>(*this, "NickLabel");
  label->setText(res_mgr.getLang({name_, "PlayerName"}));

  entry_nick_ = &container_.addWidget<WEntry>(*this, "NickEntry");
  entry_nick_->setText(cfg.get("Client.Nick", "Player"));

  WButton* button = &container_.addWidget<WButton>(*this, "JoinButton");
  button->setCaption(res_mgr.getLang({name_, "Join"}));

  //XXX neighbors should be defined in style.ini too
  entry_host_->setNeighbors(button, entry_port_, NULL, NULL);
  entry_port_->setNeighbors(entry_host_, entry_nick_, NULL, NULL);
  entry_nick_->setNeighbors(entry_port_, button, NULL, NULL);
  button->setNeighbors(entry_nick_, entry_host_, NULL, NULL);

  this->focus(entry_host_);
}

bool ScreenJoinServer::onInputEvent(const sf::Event& ev)
{
  if(submitting_event_.type != sf::Event::Count) {
    return true;  // ignore input
  }
  if(Screen::onInputEvent(ev)) {
    return true;
  }
  if(InputMapping::Global.cancel.match(ev)) {
    intf_.swapScreen(new ScreenStart(intf_));
    return true;
  } else if(InputMapping::Global.confirm.match(ev)) {
    this->submit(ev);
    return true;
  }
  return false;
}

void ScreenJoinServer::onPlayerJoined(Player& pl)
{
  if(pl.local()) {
    assert(submitting_event_.type != sf::Event::Count);
    auto new_screen = std::make_unique<ScreenLobby>(intf_);
    InputMapping mapping = new_screen->getUnusedInputMapping(submitting_event_);
    assert(mapping.type() != InputType::NONE);
    new_screen->addLocalPlayer(pl, mapping);
    intf_.swapScreen(new_screen.release());
  }
}

void ScreenJoinServer::onServerConnect(bool success)
{
  assert(submitting_event_.type != sf::Event::Count);
  if(success) {
    intf_.client()->newLocalPlayer(entry_nick_->text());
  }
}

void ScreenJoinServer::onServerDisconnect()
{
  submitting_event_.type = sf::Event::Count;
  intf_.stopInstance();
}

void ScreenJoinServer::submit(const sf::Event& ev)
{
  assert(submitting_event_.type == sf::Event::Count);
  //TODO display errors to the user
  if( entry_nick_->text().empty() ) {
    // TODO check nick is valid, not only not empty
    LOG("empty nick");
    return;
  } else if( entry_host_->text().empty() ) {
    LOG("empty hostname");
    return;
  }
  uint16_t port;
  try {
    port = boost::lexical_cast<uint16_t>(entry_port_->text());
  } catch(const boost::bad_lexical_cast&) {
    LOG("invalid port value: %s", entry_port_->text().c_str());
    return;
  }

  intf_.cfg().set("Client.Hostname", entry_host_->text());
  intf_.cfg().set("Global.Port", port);
  intf_.cfg().set("Client.Nick", entry_nick_->text());
  intf_.startClient(entry_host_->text(), port);
  submitting_event_ = ev;
}


ScreenCreateServer::ScreenCreateServer(GuiInterface& intf):
    Screen(intf, "ScreenCreateServer")
{
}

void ScreenCreateServer::enter()
{
  const ResourceManager& res_mgr = intf_.res_mgr();
  const IniFile& cfg = intf_.cfg();
  // reload configuration from values

  WLabel* label = &container_.addWidget<WLabel>(*this, "PortLabel");
  label->setText(res_mgr.getLang({name_, "Port"}));

  entry_port_ = &container_.addWidget<WEntry>(*this, "PortEntry");
  entry_port_->setText(cfg.get("Global.Port", ""));

  label = &container_.addWidget<WLabel>(*this, "NickLabel");
  label->setText(res_mgr.getLang({name_, "PlayerName"}));

  entry_nick_ = &container_.addWidget<WEntry>(*this, "NickEntry");
  entry_nick_->setText(cfg.get("Client.Nick", ""));

  label = &container_.addWidget<WLabel>(*this, "PlayerNbLabel");
  label->setText(res_mgr.getLang({name_, "PlayerNumber"}));

  entry_player_nb_ = &container_.addWidget<WEntry>(*this, "PlayerNbEntry");
  entry_player_nb_->setText(cfg.get("Server.PlayerNumber", ""));

  WButton* button = &container_.addWidget<WButton>(*this, "CreateButton");
  button->setCaption(res_mgr.getLang({name_, "Create"}));

  entry_port_->setNeighbors(button, entry_player_nb_, NULL, NULL);
  entry_player_nb_->setNeighbors(entry_port_, entry_nick_, NULL, NULL);
  entry_nick_->setNeighbors(entry_player_nb_, button, NULL, NULL);
  button->setNeighbors(entry_nick_, entry_port_, NULL, NULL);

  this->focus(entry_port_);
}

bool ScreenCreateServer::onInputEvent(const sf::Event& ev)
{
  if(Screen::onInputEvent(ev)) {
    return true;
  }
  if(InputMapping::Global.cancel.match(ev)) {
    intf_.swapScreen(new ScreenStart(intf_));
    return true;
  } else if(InputMapping::Global.confirm.match(ev)) {
    this->submit(ev);
    return true;
  }
  return false;
}

void ScreenCreateServer::submit(const sf::Event& ev)
{
  //TODO display errors to the user
  if( entry_nick_->text().empty() ) {
    // TODO check nick is valid, not only not empty
    LOG("empty nick");
    return;
  }
  uint16_t port;
  try {
    port = boost::lexical_cast<uint16_t>(entry_port_->text());
  } catch(const boost::bad_lexical_cast&) {
    LOG("invalid port value: %s", entry_port_->text().c_str());
    return;
  }
  unsigned int player_nb;
  try {
    player_nb = boost::lexical_cast<unsigned int>(entry_player_nb_->text());
  } catch(const boost::bad_lexical_cast&) {
    LOG("invalid player number: %s", entry_player_nb_->text().c_str());
    return;
  }

  intf_.cfg().set("Global.Port", port);
  intf_.cfg().set("Client.Nick", entry_nick_->text());
  intf_.cfg().set("Server.PlayerNumber", player_nb);
  intf_.startServer(port);

  auto new_screen = std::make_unique<ScreenLobby>(intf_);
  InputMapping mapping = new_screen->getUnusedInputMapping(ev);
  assert(mapping.type() != InputType::NONE);
  Player& pl = intf_.server()->newLocalPlayer(entry_nick_->text());
  new_screen->addLocalPlayer(pl, mapping);
  intf_.swapScreen(new_screen.release());
}


ScreenLobby::ScreenLobby(GuiInterface& intf):
    Screen(intf, "ScreenLobby")
{
}

void ScreenLobby::enter()
{
  assert( intf_.instance() );

  const IniFile& style = this->style();
  player_frames_pos_ = style.get<sf::Vector2f>({name_, "PlayerFramesPos"});
  player_frames_dpos_ = style.get<sf::Vector2f>({name_, "PlayerFramesDPos"});

  // add non-local connected players
  // (local players are added separately, with their mapping)
  for(auto& pair : intf_.instance()->players()) {
    Player& pl = *pair.second;
    if(!pl.local()) {
      this->addRemotePlayer(pl);
    }
  }
  assert(player_frames_.size() == intf_.instance()->players().size());

  // needed if local player added between screen creation and enter() call
  this->updatePlayerFramesLayout();
}

void ScreenLobby::redraw()
{
  Screen::redraw();

  sf::RenderWindow& w = intf_.window();
  for(auto& kv : player_frames_) {
    w.draw(*kv.second);
  }
}


bool ScreenLobby::onInputEvent(const sf::Event& ev)
{
  // events for existing local players
  for(auto const& kv : player_frames_) {
    if(kv.second->onInputEvent(ev)) {
      return true;
    }
  }

  // global keyboard cancel: exit
  if(ev.type == sf::Event::KeyPressed && InputMapping::Global.cancel.match(ev)) {
    intf_.swapScreen(new ScreenStart(intf_));
    return true;
  }

  // global confirm: add local player
  if(InputMapping::Global.confirm.match(ev)) {
    InputMapping mapping = this->getUnusedInputMapping(ev);
    if(mapping.type() != InputType::NONE) {
      auto nick = intf_.cfg().get<std::string>("Client.Nick");
      Player& pl = intf_.server()->newLocalPlayer(nick);
      this->addLocalPlayer(pl, mapping);
    }
    return true;
  }

  return false;
}

void ScreenLobby::onStateChange()
{
  auto state = intf_.instance()->state();
  if(state == GameInstance::State::GAME_INIT) {
    auto new_screen = std::make_unique<ScreenGame>(intf_);
    for(auto& pair : player_frames_) {
      WPlayerFrame& frame = *pair.second;
      if(frame.player().local()) {
        new_screen->setPlayerMapping(frame.player(), frame.mapping());
      }
    }
    intf_.swapScreen(new_screen.release());
  }
}

void ScreenLobby::onServerChangeFieldConfs()
{
  for(auto const& kv : player_frames_) {
    kv.second->updateConfItems();
  }
}

void ScreenLobby::onPlayerJoined(Player& pl)
{
  if(!pl.local()) {
    this->addRemotePlayer(pl);
  }
}

void ScreenLobby::onPlayerChangeNick(Player& pl, const std::string& )
{
  player_frames_.find(pl.plid())->second->update();
}

void ScreenLobby::onPlayerStateChange(Player& pl)
{
  if(pl.state() == Player::State::QUIT) {
    player_frames_.erase(pl.plid());
    this->updatePlayerFramesLayout();
  } else {
    player_frames_.find(pl.plid())->second->update();
  }
}

void ScreenLobby::onPlayerChangeFieldConf(Player& pl)
{
  player_frames_.find(pl.plid())->second->update();
}


void ScreenLobby::updatePlayerFramesLayout()
{
  auto pos = player_frames_pos_;
  unsigned int frame_color = 1;
  for(auto& kv : player_frames_) {
    auto& frame = *kv.second;
    frame.setPosition(pos);
    frame.frame().setColor(intf_.style().colors[frame_color]);
    pos += player_frames_dpos_;
    frame_color++;
  }
}


void ScreenLobby::addLocalPlayer(Player& pl, const InputMapping& mapping)
{
  LOG("adding local player %u to lobby", pl.plid());
  assert(pl.local());
  auto frame_ptr = std::make_unique<WPlayerFrame>(*this, pl);
  WPlayerFrame& frame = *frame_ptr;
  frame.setMapping(mapping);
  player_frames_.emplace(pl.plid(), std::move(frame_ptr));
  frame.updateConfItems();
  intf_.instance()->playerSetState(pl, Player::State::LOBBY);
  this->updatePlayerFramesLayout();
}

void ScreenLobby::addRemotePlayer(Player& pl)
{
  LOG("adding remote player %u to lobby", pl.plid());
  assert(!pl.local());
  auto frame_ptr = std::make_unique<WPlayerFrame>(*this, pl);
  WPlayerFrame& frame = *frame_ptr;
  player_frames_.emplace(pl.plid(), std::move(frame_ptr));
  frame.updateConfItems();
  this->updatePlayerFramesLayout();
}

InputMapping ScreenLobby::getUnusedInputMapping(const sf::Event& event)
{
  if(event.type == sf::Event::KeyPressed) {
    for(auto& mapping : intf_.inputMappings().keyboard) {
      auto it = std::find_if(player_frames_.begin(), player_frames_.end(),
                             [&mapping](auto& kv) { return mapping.isEquivalent(kv.second->mapping()); });
      if(it == player_frames_.end()) {
        return mapping;
      }
    }
    return {};  // no unused keyboard mapping
  } else if(event.type == sf::Event::JoystickButtonPressed) {
    unsigned int joystick_id = event.joystickButton.joystickId;
    for(auto& kv : player_frames_) {
      const InputMapping& player_mapping = kv.second->mapping();
      if(player_mapping.type() == InputType::JOYSTICK) {
        // compare only the first binding
        if(player_mapping.up.joystick.id == joystick_id) {
          return {};  // joystick already in use
        }
      }
    }
    InputMapping mapping = intf_.inputMappings().joystick[0];
    mapping.setJoystickId(joystick_id);
    return mapping;
  }
  return {};
}


const std::string& ScreenLobby::WPlayerFrame::type() const {
  static const std::string type("PlayerFrame");
  return type;
}

ScreenLobby::WPlayerFrame::WPlayerFrame(const Screen& screen, Player& pl):
    WContainer(screen, ""), player_(pl), mapping_(), focused_(nullptr)
{
  frame_ = &addWidget<WFrame>(screen, IniFile::join(type(), "Border"));

  nick_ = &addWidget<WLabel>(screen, IniFile::join(type(), "Nick"));
  choice_conf_ = &addWidget<WChoice>(screen, IniFile::join(type(), "Conf"));

  this->applyStyle(ready_, "Ready");
  ready_.setOrigin(ready_.getLocalBounds().width/2.f, ready_.getLocalBounds().height/2.f);
  ready_.setPosition(getStyle<sf::Vector2f>("Ready.Pos"));

  this->updateConfItems();
  this->update();
  this->focus(choice_conf_);
}

void ScreenLobby::WPlayerFrame::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
  WContainer::draw(target, states);
  states.transform *= getTransform();
  if(player_.state() == Player::State::LOBBY_READY) {
    target.draw(ready_, states);
  }
}

void ScreenLobby::WPlayerFrame::update()
{
  nick_->setText(player_.nick());
  const std::string& conf_name = player_.fieldConf().name;
  if(!choice_conf_->selectValue(conf_name)) {
    choice_conf_->select(choice_conf_->addItem(conf_name));
  }
}

void ScreenLobby::WPlayerFrame::updateConfItems()
{
  auto conf_name = player_.fieldConf().name;
  if(player_.local()) {
    // add all server configurations
    auto& confs = screen_.intf().instance()->conf().field_confs;
    WChoice::ItemContainer items;
    items.reserve(confs.size());
    for(auto& conf : confs) {
      items.push_back(conf.name);
    }
    choice_conf_->setItems(items);
    // reselect previous value
    if(!choice_conf_->selectValue(conf_name)) {
      choice_conf_->select(choice_conf_->addItem(conf_name));
    }
  } else {
    // single item: the selected configuration
    choice_conf_->setItems({conf_name});
  }
}

void ScreenLobby::WPlayerFrame::focus(WFocusable* w)
{
  if(focused_) {
    focused_->focus(false);
  }
  focused_ = w;
  if(focused_) {
    focused_->focus(true);
  }
}

bool ScreenLobby::WPlayerFrame::onInputEvent(const sf::Event& ev)
{
  if(!player_.local()) {
    return false;
  }

  if(focused_) {
    // detect conf change
    unsigned int conf_index = choice_conf_->index();
    if(focused_->onInputEvent(mapping_, ev)) {
      if(choice_conf_->index() != conf_index) {
        // choice updated
        auto instance = screen_.intf().instance();
        auto* fc = instance->conf().fieldConf(choice_conf_->value());
        if(fc) {  // should always be true
          instance->playerSetFieldConf(player_, *fc);
        }
      }
      return true;
    }
  }

  if(mapping_.confirm.match(ev)) {
    if(player_.state() != Player::State::LOBBY_READY) {
      screen_.intf().instance()->playerSetState(player_, Player::State::LOBBY_READY);
      this->focus(nullptr);
    }
    return true;
  } else if(mapping_.cancel.match(ev)) {
    if(player_.state() == Player::State::LOBBY_READY) {
      screen_.intf().instance()->playerSetState(player_, Player::State::LOBBY);
      this->focus(choice_conf_);
    } else {
      screen_.intf().instance()->playerSetState(player_, Player::State::QUIT);
    }
    return true;
  } else if(focused_) {
    WFocusable* next_focused = focused_->neighborToFocus(mapping_, ev);
    if(next_focused) {
      this->focus(next_focused);
      return true;
    }
  }

  return false;
}


}

