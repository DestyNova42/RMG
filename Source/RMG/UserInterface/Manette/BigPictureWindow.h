/*
 * Rosalie's Mupen GUI - mode manette (fork DestyNova42/RMG)
 *  GPL-3.0, see LICENSE
 */

// Controller-friendly fullscreen interface ("Mode manette").

#pragma once

#include <array>
#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <functional>
#include <vector>

#include <QColor>
#include <QElapsedTimer>
#include <QHash>
#include <QPixmap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QWidget>

#include "BigPictureSettings.h"
#include "Manette.hpp"

class BigPictureAudio;

// One game of the library (from RMG's ROM list)
struct ManetteGame
{
  std::string file;           // full path
  std::string good_name;      // from the ROM database
  std::string internal_name;  // from the ROM header
  std::string md5;
  std::string header_id;      // e.g. NSME
  std::string region;
  unsigned int crc1 = 0, crc2 = 0;
  // Key for resume points, play time…: the MD5, or else the CRCs and the file name
  std::string GetGameID() const;
  std::string GetFilePath() const { return file; }
  std::string GetFileName() const;
};

// Play time per game, kept in Manette.ini (same interface as Dolphin's TimePlayed)
class TimePlayed
{
public:
  void Reload() {}
  std::chrono::milliseconds GetTimePlayed(const std::string& game_id) const;
  static void Add(const std::string& game_id, std::chrono::milliseconds time);
};

class BigPictureWindow final : public QWidget
{
  Q_OBJECT

public:
  explicit BigPictureWindow(QWidget* parent = nullptr);
  ~BigPictureWindow() override;

  // Shows the interface fullscreen and starts listening to the controller.
  void Open();
  // Leaves the controller mode entirely.
  void Close();
  bool IsActive() const { return m_active; }

  // Called by the main window
  void OnEmulationStarted();   // first frames of the game
  void OnEmulationFinished();  // the game stopped (or failed to start)
  void OnEmulationPaused(bool paused);
  // RMG's ROM list
  void SetGames(std::vector<ManetteGame> games);

  // Esc / the "Stop" hotkey during a game: quit with a resume point. False if not in a game.
  bool QuitFromHotkey();

signals:
  void LaunchRequested(const QString& path, const QString& savestate);
  void RefreshRequested();
  void ChooseRomDirectoryRequested();
  void ConfigureControllersRequested();
  void ClassicRequested();
  void QuitRequested();
  void StopRequested();    // "Quitter le jeu" in the pause menu
  void ResumeRequested();  // the game gets the focus back

protected:
  void closeEvent(QCloseEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

private:
  enum class Action
  {
    Up,
    Down,
    Left,
    Right,
    Accept,
    Back,
    Menu,
    PageUp,
    PageDown,
    Favorite,  // Y
    Options,   // X
    Sort,      // Z
    Count,
  };

  enum class Mode
  {
    Intro,
    Library,
    Menu,
    ConfirmQuit,
    Setup,
    InGame,  // hidden, a game is running
    Pause,
    Launching,     // loading screen while the game boots
    FadingOut,     // the menu fades away, revealing the game
    Remap,         // waiting for one control
    Calibrate,     // stick calibration
    Memcard,       // memory card manager
    GameSettings,  // per-game settings
    Players,       // multiplayer: who plays on which port
    PadTest,       // live view of a controller
    Closing,       // fading to black before stopping the game
    ConfirmUpdate, // an update is available
    ConfirmDriver, // USB driver of Switch 2 controllers
    ChooseDisc,    // multi-disc game: which disc to start
    WiiTest,       // live view of an emulated Wii Remote
    WiiSaves,      // Wii save manager
    DeviceDiag,    // raw inputs of every controller
    Gallery,       // screenshots
    Stats,         // play time
    WiiPair,       // pairing a real Wii Remote
  };

  enum class Tab
  {
    All,
    Recent,
    Favorites,
    Settings,
  };

  struct Entry
  {
    std::shared_ptr<const ManetteGame> game;
    QString path;
    QString title;
    QStringList discs;  // every disc of the game, in order
  };

  // ---- lifecycle / data
  void Suspend();
  void ScheduleRebuild();
  void RebuildList();
  void ApplyTab();
  void ApplyPreferences();
  const Entry* Selected() const;

  // ---- input
  void PollController();
  void PollInGame();
  void Press(Action action);
  void HandleAction(Action action);
  void HandleLibrary(Action action);
  void HandleSettings(Action action);
  void HandlePause(Action action);
  void HandleMenu(Action action);
  void ToggleFavorite();
  void SetSelected(int index);

  // ---- pause menu
  void OpenPause();
  void Resume();
  std::vector<int> PauseItems() const;

  // ---- controller setup wizard

  // ---- remap / calibration

  // ---- memory cards

  // ---- per-game settings

  // ---- controllers
  void DrawPadTest(QPainter& painter);
  void Notify(const QString& text);  // toast, also over a running game

  // ---- updates

  // ---- launching, resume, sorting
  enum HeroButton
  {
    HERO_PLAY,
    HERO_RESUME,
    HERO_NEW,
    HERO_SETTINGS,
    HERO_FAVORITE,
  };
  std::vector<int> HeroButtons(const Entry& entry) const;
  void ActivateHero(int button);
  void Launch(const Entry& entry, int disc, bool resume);
  void StartNewGame(const Entry& entry);  // asks for the disc if needed
  bool HasResume(const Entry& entry) const;
  QString ResumeStatePath(const std::string& game_id) const;
  QString ResumeShotPath(const std::string& game_id) const;
  const QPixmap& ResumeShot(const Entry& entry);
  void SaveResume();
  void CycleSort();
  void DrawResumeCard(QPainter& painter, const Entry& sel, const QRectF& rect);

  // ---- Wii
  void OpenGallery();
  void HandleGallery(Action action);
  void DrawGallery(QPainter& painter);
  QString ShotGameTitle(const QString& path) const;
  void OpenStats();
  void HandleStats(Action action);
  void DrawStats(QPainter& painter);

  // ---- USB driver (instead of Zadig)

  // ---- themes
  void ApplyTheme();
  void DrawIntroN64(QPainter& painter);
  std::vector<BigPictureSettingCategory>& Cats();

  // ---- pause screenshots
  QString PauseShotPath() const;
  QString SlotThumbPath(int slot) const;
  void LoadSlotThumbs();

  // ---- animation
  void Tick();
  void BeginTransition(int direction);
  void SetMode(Mode mode);
  void Toast(const QString& text);

  // ---- drawing
  QFont Font(bool display, int px, int weight) const;
  void DrawIconRaw(QPainter& painter, const QString& name, const QRectF& rect);
  void DrawIcon(QPainter& painter, const QString& name, const QRectF& rect);
  void DrawHints(QPainter& painter, const std::vector<std::pair<QString, QString>>& left,
                 const std::vector<std::pair<QString, QString>>& right);
  qreal DrawPillButton(QPainter& painter, qreal x, qreal y, const QString& icon,
                       const QString& label, bool primary, qreal alpha = 1.0, qreal forced_width = 0);
  void DrawScene(QPainter& painter);
  void DrawBackground(QPainter& painter);
  void DrawIntro(QPainter& painter);
  void DrawTopBar(QPainter& painter);
  void DrawHero(QPainter& painter, const Entry& sel, qreal left, qreal top, qreal max_width);
  void DrawCoverShelf(QPainter& painter);
  void DrawList(QPainter& painter);
  void DrawEmpty(QPainter& painter);
  void DrawSettings(QPainter& painter, qreal top);
  void DrawMenu(QPainter& painter);
  void DrawDialog(QPainter& painter, const QString& title,
                  const std::vector<std::pair<QString, QString>>& hints);
  void DrawPause(QPainter& painter);
  void DrawToast(QPainter& painter);

  QPixmap CoverFor(const Entry& entry, const QSize& size);
  QColor AmbientFor(const Entry& entry);
  QString PlayInfo(const Entry& entry) const;
  QString ControllerName() const;

  std::vector<Entry> m_all;      // every game
  std::vector<Entry> m_entries;  // games of the current tab

  std::unique_ptr<BigPictureAudio> m_audio;

  bool m_active = false;
  bool m_intro_done = false;
  Mode m_mode = Mode::Library;
  Mode m_mode_before_setup = Mode::Library;
  Tab m_tab = Tab::All;
  int m_selected = 0;
  int m_menu_index = 0;
  QSet<QString> m_favorites;

  // settings screen
  std::vector<BigPictureSettingCategory> m_settings;
  int m_settings_cat = 0;
  int m_settings_item = 0;
  bool m_settings_in_items = false;
  bool m_pause_settings = false;  // settings opened from the pause menu
  qreal m_settings_scroll = 0;

  // pause menu
  const Entry* m_running = nullptr;
  Entry m_running_entry;
  int m_pause_index = 0;
  int m_slot = 1;
  int m_disc = 0;
  bool m_home_was_down = true;

  // preferences
  BigPicturePrefs::View m_view = BigPicturePrefs::View::Covers;
  BigPicturePrefs::Effect m_effect = BigPicturePrefs::Effect::Halo;
  BigPicturePrefs::Transition m_transition = BigPicturePrefs::Transition::Fade;

  // animation state
  QElapsedTimer m_clock;
  qint64 m_last_tick = 0;
  qreal m_time = 0;              // seconds, for ambient effects
  qreal m_scroll = 0;            // cover shelf (px at 1080p)
  qreal m_list_scroll = 0;       // list view (rows)
  qreal m_hero_t = 1;            // hero fade-in after a selection change
  qreal m_overlay_t = 0;         // menu / dialog / pause opening
  qreal m_intro_t = 0;
  qreal m_trans_t = 1;           // screen transition
  int m_trans_dir = 0;
  QPixmap m_frame;               // last composed frame
  QPixmap m_trans_from;          // frame before the transition
  std::vector<qreal> m_focus;    // per entry: 0 = normal, 1 = selected
  QColor m_ambient;
  QString m_toast;
  qreal m_toast_t = 0;

  QTimer* m_poll_timer = nullptr;
  QTimer* m_game_timer = nullptr;
  QTimer* m_anim_timer = nullptr;
  QTimer* m_rebuild_timer = nullptr;

  // Controller edge detection / auto-repeat
  u32 m_prev_held = 0;
  qint64 m_held_since[16] = {};
  qint64 m_last_repeat[16] = {};

  // setup wizard
  int m_setup_port = 0;
  int m_setup_step = 0;
  bool m_setup_device_chosen = false;
  bool m_setup_wii = false;
  QTimer* m_setup_timer = nullptr;
  QTimer* m_setup_check_timer = nullptr;

  QString m_display_family;
  QString m_text_family;

  QHash<QString, QPixmap> m_cover_cache;
  QHash<QString, QPixmap> m_banner_cache;
  QHash<QString, QColor> m_ambient_cache;
  QHash<QString, QPixmap> m_icons;

  // remap
  int m_remap_port = 0, m_remap_group = 0, m_remap_index = 0, m_remap_mirror = -1;
  bool m_remap_flip = false;
  QString m_remap_prompt;
  Mode m_mode_before_remap = Mode::Library;
  qint64 m_remap_started = 0;
  int m_remap_wait = 0;  // 0 browsing, 1 replace, 2 add another button
  int m_remap_scroll = 0;
  void OpenRemap(int port);
  void HandleRemap(Action action);
  void PollRemap();
  void DrawRemap(QPainter& painter);

  // calibration
  int m_cal_port = 0, m_cal_group = 0;
  qreal m_cal_x = 0, m_cal_y = 0;

  // memory cards
  int m_mc_card = 0;
  int m_mc_index = 0;
  bool m_mc_actions_open = false;
  bool m_mc_confirm_erase = false;
  int m_mc_action = 0;
  Mode m_mode_before_memcard = Mode::Library;

  // per-game settings
  std::vector<BigPictureSettingCategory> m_game_settings;
  QString m_game_settings_title;

  // loading / fading
  qreal m_fade_out = 0;  // FadingOut progress
  qreal m_fade_in = 1;   // black veil after returning from a game (1 = done)
  qreal m_window_opacity = 1;
  std::array<QPixmap, 9> m_slot_thumbs;
  qreal m_game_aspect = 4.0 / 3.0;

  // quick settings / cheats (pause)
  std::vector<BigPictureSettingCategory> m_quick_settings;
  bool m_quick = false;
  Mode m_mode_before_game_settings = Mode::Library;

  // controllers
  QTimer* m_device_timer = nullptr;
  QString m_bt_status;
  QStringList m_known_devices;
  int m_test_port = 0;
  qint64 m_test_start_since = -1;
  Mode m_mode_before_players = Mode::Library;
  qint64 m_last_switch = 0;

  // in-game notifications (separate, click-through window)
  QWidget* m_notifier = nullptr;
  QTimer* m_notify_timer = nullptr;
  QString m_notify_text;
  qreal m_notify_t = 0;

  // play time of the running game
  qint64 m_play_started = -1;  // m_clock time when the game (re)started running
  void FlushPlayTime();
  QString m_pending_state;      // resume point to load once the game runs
  // ROM list from RMG
  std::vector<ManetteGame> m_games;
  QHash<QString, bool> m_cover_tried;
  void DownloadCover(const Entry& entry);
  QString CoverFile(const Entry& entry) const;
  // closing
  qreal m_close_t = 0;
  bool m_stop_sent = false;

  // updates
  QString m_update_url;
  QString m_update_version;
  bool m_update_busy = false;

  // hero buttons / resume / discs
  bool m_hero_focus = false;
  mutable QHash<QString, bool> m_resume_known;
  int m_hero_button = 0;
  QString m_resume_shot_path;
  QPixmap m_resume_shot;
  int m_choose_disc = 0;
  Entry m_choose_entry;

  // Screenshot gallery
  std::vector<QString> m_shots;  // newest first
  int m_shot_index = 0;
  bool m_shot_view = false;      // one screenshot on the whole screen
  bool m_shot_delete_armed = false;
  std::map<QString, QPixmap> m_shot_thumbs;
  QString m_shot_full_path;
  QPixmap m_shot_full;
  // Statistics
  std::vector<std::pair<Entry, qint64>> m_stats_top;     // minutes played
  std::vector<std::pair<Entry, qint64>> m_stats_recent;  // last played (epoch s)
  qint64 m_stats_total_min = 0;
  int m_stats_played = 0;
  // Real Wii Remote pairing
  int m_pair_port = 0;
  qint64 m_pair_last_scan = 0;
  qint64 m_home_down_since = -1;
  bool m_slept_in_game = false;
  bool m_wii_icons = false;
  bool m_swap_menu_buttons = false;
  void QuitGame();
  void DrawDeviceDiag(QPainter& painter);
  int m_diag_index = 0;
  // Mouse: clickable areas, rebuilt with each frame
  struct Hit
  {
    QRectF rect;
    std::function<void(QPointF)> click;
  };
  std::vector<Hit> m_hits;
  void AddHit(const QRectF& rect, std::function<void()> click);
  void AddHitAt(const QRectF& rect, std::function<void(QPointF)> click);
  void ClickEntry(int index);
  QTimer* m_cursor_timer = nullptr;
  int m_wheel_accum = 0;
  // C-stick: jump by letter
  QChar InitialOf(const Entry& e) const;
  void JumpLetter(int dir);
  void DrawJumpLetter(QPainter& painter);
  int m_jump_dir = 0;
  qint64 m_jump_last = 0;
  QString m_jump_letter;
  qint64 m_jump_shown = 0;
  void DrawDpadDot(QPainter& painter, QPointF p, qreal r);
  QHash<QString, bool> m_hd_known;

  // USB driver
  QStringList m_driver_asked;
  QString m_driver_devices;
  bool m_driver_busy = false;
  Mode m_mode_before_driver = Mode::Library;
};
