/*
 * Rosalie's Mupen GUI - mode manette (fork DestyNova42/RMG)
 *  GPL-3.0, see LICENSE
 *
 * Controller-friendly fullscreen interface, adapted from the "Mode manette" of the
 * DestyNova42/dolphin fork.
 */

#include "BigPictureWindow.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <functional>
#include <map>
#include <numbers>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHash>
#include <QImage>
#include <QImageReader>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QLocale>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QRegularExpression>
#include <QScreen>
#include <QStandardPaths>
#include <QTime>
#include <QTimer>
#include <QTransform>
#include <QUrl>
#include <QWheelEvent>
#include <QWindow>

#include <RMG-Core/Directories.hpp>
#include <RMG-Core/Emulation.hpp>
#include <RMG-Core/SaveState.hpp>
#include <RMG-Core/Screenshot.hpp>
#include <RMG-Core/Settings.hpp>
#include <RMG-Core/Video.hpp>

#include "BigPictureAudio.h"
#include "ManettePad.hpp"

using Sound = BigPictureAudio::Sound;
namespace Prefs = BigPicturePrefs;

// ================================================================== small shims

std::string ManetteGame::GetFileName() const
{
  return QFileInfo(QString::fromStdString(file)).completeBaseName().toStdString();
}

std::chrono::milliseconds TimePlayed::GetTimePlayed(const std::string& game_id) const
{
  return std::chrono::milliseconds(
      ManetteQSettings()
          .value(QStringLiteral("playtime/") + QString::fromStdString(game_id), 0)
          .toLongLong());
}

void TimePlayed::Add(const std::string& game_id, std::chrono::milliseconds time)
{
  if (game_id.empty() || time.count() <= 0)
    return;
  const QString key = QStringLiteral("playtime/") + QString::fromStdString(game_id);
  QSettings& q = ManetteQSettings();
  q.setValue(key, q.value(key, 0).toLongLong() + time.count());
}

namespace
{
// Theme colours (changed by ApplyTheme)
QColor BG(13, 11, 26);
QColor SURFACE(23, 20, 43);
QColor SURFACE_DEEP(18, 15, 36);
QColor RAISED(34, 29, 61);
QColor LINE(46, 40, 80);
QColor TEXT(244, 241, 255);
QColor TEXT_SOFT(185, 178, 214);
QColor TEXT_DIM(141, 133, 179);
QColor ACCENT(139, 108, 255);
QColor ACCENT_SOFT(201, 185, 255);

struct Palette
{
  QColor bg, surface, deep, raised, line, text, soft, dim, accent, accent_soft;
};

Palette PaletteFor(BigPicturePrefs::Theme theme)
{
  using T = BigPicturePrefs::Theme;
  switch (theme)
  {
  case T::Charcoal:  // the classic grey console, blue like its A button
    return {{34, 34, 38},     {48, 48, 54},     {41, 41, 46},     {62, 62, 70},
            {82, 82, 92},     {242, 242, 246},  {192, 192, 202},  {142, 142, 154},
            {92, 142, 234},   {182, 206, 250}};
  case T::JungleGreen:
    return {{10, 38, 25},     {16, 56, 37},     {13, 47, 31},     {24, 76, 51},
            {36, 98, 67},     {236, 255, 244},  {172, 222, 194},  {122, 172, 144},
            {84, 226, 136},   {186, 242, 206}};
  case T::FireOrange:
    return {{58, 21, 8},      {84, 33, 12},     {71, 27, 10},     {108, 45, 18},
            {138, 61, 26},    {255, 244, 236},  {255, 208, 178},  {214, 160, 128},
            {255, 142, 50},   {255, 198, 152}};
  case T::GrapePurple:
    return {{34, 15, 56},     {50, 25, 80},     {42, 20, 68},     {66, 35, 102},
            {88, 51, 130},    {248, 240, 255},  {214, 192, 240},  {168, 142, 202},
            {190, 124, 255},  {226, 202, 255}};
  case T::IceBlue:
    return {{11, 38, 60},     {17, 56, 86},     {14, 47, 73},     {25, 74, 110},
            {39, 96, 136},    {236, 248, 255},  {178, 216, 240},  {128, 170, 200},
            {112, 202, 255},  {192, 232, 255}};
  case T::SmokeBlack:
    return {{8, 8, 10},       {22, 22, 26},     {14, 14, 17},     {36, 36, 42},
            {54, 54, 62},     {242, 242, 246},  {174, 174, 184},  {124, 124, 136},
            {150, 150, 170},  {214, 214, 226}};
  case T::WatermelonRed:
    return {{58, 10, 18},     {84, 18, 28},     {70, 14, 22},     {108, 26, 38},
            {138, 40, 54},    {255, 240, 242},  {255, 192, 200},  {210, 142, 152},
            {255, 92, 112},   {255, 182, 192}};
  case T::Gold:
    return {{52, 41, 10},     {76, 60, 16},     {64, 50, 13},     {98, 78, 24},
            {126, 102, 36},   {255, 250, 232},  {242, 222, 162},  {198, 178, 118},
            {244, 198, 50},   {255, 230, 142}};
  default:
    return {{13, 11, 26},     {23, 20, 43},     {18, 15, 36},     {34, 29, 61},
            {46, 40, 80},     {244, 241, 255},  {185, 178, 214},  {141, 133, 179},
            {139, 108, 255},  {201, 185, 255}};
  }
}
const QColor OK_GREEN(91, 214, 154);
const QColor WARN(255, 180, 71);
const QColor DANGER(255, 180, 163);

constexpr int POLL_INTERVAL_MS = 16;
constexpr qint64 REPEAT_DELAY_MS = 350;
constexpr qint64 REPEAT_RATE_MS = 85;
constexpr int SLOT_COUNT = 8;

constexpr u32 Bit(int action)
{
  return 1u << action;
}

qreal EaseOut(qreal t)
{
  t = std::clamp<qreal>(t, 0, 1);
  return 1 - (1 - t) * (1 - t) * (1 - t);
}

qreal EaseInOut(qreal t)
{
  t = std::clamp<qreal>(t, 0, 1);
  return t < 0.5 ? 4 * t * t * t : 1 - std::pow(-2 * t + 2, 3) / 2;
}

// Deterministic pseudo-random in [0, 1) for the background effects.
qreal Hash01(int i, int salt)
{
  u32 x = static_cast<u32>(i) * 747796405u + static_cast<u32>(salt) * 2891336453u;
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  x ^= x >> 16;
  return (x & 0xFFFFFF) / static_cast<qreal>(0x1000000);
}

QPainterPath CubePath(qreal s)
{
  // Isometric cube outline centred on (0, 0), "size" s.
  const qreal h = s * 0.5;
  QPainterPath p;
  p.moveTo(0, -h);
  p.lineTo(h * 0.87, -h * 0.5);
  p.lineTo(h * 0.87, h * 0.5);
  p.lineTo(0, h);
  p.lineTo(-h * 0.87, h * 0.5);
  p.lineTo(-h * 0.87, -h * 0.5);
  p.closeSubpath();
  p.moveTo(0, 0);
  p.lineTo(h * 0.87, -h * 0.5);
  p.moveTo(0, 0);
  p.lineTo(-h * 0.87, -h * 0.5);
  p.moveTo(0, 0);
  p.lineTo(0, h);
  return p;
}

// Region from the ROM database name ("(E)", "(U)"…) or the header
QString RegionName(const std::string& region)
{
  const QString r = QString::fromStdString(region).toLower();
  if (r.contains(QStringLiteral("japan")) || r == QStringLiteral("j"))
    return QStringLiteral("Japon");
  if (r.contains(QStringLiteral("usa")) || r.contains(QStringLiteral("america")) ||
      r.contains(QStringLiteral("ntsc")))
    return QStringLiteral("Amérique");
  if (r.contains(QStringLiteral("europe")) || r.contains(QStringLiteral("pal")) ||
      r.contains(QStringLiteral("germany")) || r.contains(QStringLiteral("france")) ||
      r.contains(QStringLiteral("spain")) || r.contains(QStringLiteral("italy")) ||
      r.contains(QStringLiteral("australia")))
    return QStringLiteral("Europe");
  return QString();
}

// Second line under the title: the full name of the ROM database, or the file name
QString GameDescription(const ManetteGame& g)
{
  const QString good = QString::fromStdString(g.good_name).trimmed();
  if (!good.isEmpty() && !good.contains(QStringLiteral("unknown rom"), Qt::CaseInsensitive))
    return good;
  return QFileInfo(QString::fromStdString(g.file)).fileName();
}

// HD texture pack for GLideN64: <txPath>/<name in the ROM header>
bool HasTexturePack(const ManetteGame& g)
{
  if (g.internal_name.empty())
    return false;
  std::string dir = CoreSettingsGetStringValue(std::string("Video-GLideN64"), std::string("txPath"));
  if (dir.empty())
    dir = (CoreGetUserDataDirectory() / "hires_texture").string();
  const QString name = QString::fromStdString(g.internal_name).trimmed();
  const QDir d(QString::fromStdString(dir));
  return d.exists(name) || d.exists(name.toUpper()) ||
         QFile::exists(d.filePath(name + QStringLiteral("_HIRESTEXTURES.hts"))) ||
         QFile::exists(d.filePath(name.toUpper() + QStringLiteral("_HIRESTEXTURES.hts")));
}

QString LastPlayedKey(const QString& path)
{
  return QStringLiteral("lastplayed/") + QString::number(qHash(path), 16);
}

// Pause menu entries
enum PauseItem
{
  PAUSE_RESUME,
  PAUSE_SAVE,
  PAUSE_LOAD,
  PAUSE_QUICK,
  PAUSE_SETTINGS,
  PAUSE_RESET,
  PAUSE_SCREENSHOT,
  PAUSE_QUIT,
};

// Clean title from the ROM database name: "Super Mario 64 (U) [!]" -> "Super Mario 64"
QString CleanTitle(const QString& name)
{
  QString t = name;
  t.remove(QRegularExpression(QStringLiteral("\\s*[\\(\\[][^\\)\\]]*[\\)\\]]")));
  return t.trimmed();
}
}  // namespace

// Small click-through window used to show notifications over a running game.
class NotifierWidget final : public QWidget
{
public:
  explicit NotifierWidget(std::function<void(QPainter&)> paint)
      : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                             Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus),
        m_paint(std::move(paint))
  {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
  }

protected:
  void paintEvent(QPaintEvent*) override
  {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    m_paint(painter);
  }

private:
  std::function<void(QPainter&)> m_paint;
};

// ================================================================== lifecycle

BigPictureWindow::BigPictureWindow(QWidget* parent) : QWidget(parent)
{
  setWindowTitle(tr("RMG - Mode manette"));
  setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
  setAttribute(Qt::WA_TranslucentBackground);  // the pause menu shows the game behind it
  setFocusPolicy(Qt::StrongFocus);
  setCursor(Qt::BlankCursor);
  setMouseTracking(true);
  m_cursor_timer = new QTimer(this);
  m_cursor_timer->setSingleShot(true);
  m_cursor_timer->setInterval(2500);
  connect(m_cursor_timer, &QTimer::timeout, this, [this] { setCursor(Qt::BlankCursor); });

  ManettePad::Init();

  // Nintendo 64 button icons (rendered from the SVG files, see Resource/LICENSES.txt)
  const QDir icon_dir(QStringLiteral(":/Manette/Icons"));
  for (const QString& file : icon_dir.entryList({QStringLiteral("*.png")}, QDir::Files))
  {
    QPixmap pixmap(icon_dir.filePath(file));
    if (!pixmap.isNull())
      m_icons.insert(QFileInfo(file).completeBaseName(), pixmap);
  }

  const auto load_font = [&](const QString& file) {
    const int id = QFontDatabase::addApplicationFont(QStringLiteral(":/Manette/Fonts/") + file);
    const QStringList families =
        id >= 0 ? QFontDatabase::applicationFontFamilies(id) : QStringList{};
    return families.isEmpty() ? QStringLiteral("Segoe UI") : families.front();
  };
  m_display_family = load_font(QStringLiteral("SpaceGrotesk.ttf"));
  m_text_family = load_font(QStringLiteral("Inter.ttf"));

  const QStringList favorites =
      ManetteQSettings().value(QStringLiteral("favorites")).toStringList();
  m_favorites = QSet<QString>(favorites.begin(), favorites.end());

  m_audio = std::make_unique<BigPictureAudio>();

  BigPictureSettingsHost host;
  host.open_classic = [this] { QTimer::singleShot(0, this, [this] { emit ClassicRequested(); }); };
  host.test_controller = [this] {
    QTimer::singleShot(0, this, [this] {
      m_test_port = 0;
      m_test_start_since = -1;
      m_mode_before_players = m_mode;
      BeginTransition(0);
      m_mode = Mode::PadTest;
    });
  };
  host.device_diagnostic = [this] {
    QTimer::singleShot(0, this, [this] {
      m_test_port = 0;
      m_test_start_since = -1;
      m_mode_before_players = m_mode;
      BeginTransition(0);
      m_mode = Mode::DeviceDiag;
    });
  };
  host.notify = [this](const QString& text) { Toast(text); };
  host.interface_changed = [this] { ApplyPreferences(); };
  host.refresh_library = [this] {
    m_cover_tried.clear();
    emit RefreshRequested();
    Toast(tr("Bibliothèque en cours de rafraîchissement…"));
  };
  host.choose_rom_directory = [this] {
    QTimer::singleShot(0, this, [this] { emit ChooseRomDirectoryRequested(); });
  };
  host.configure_controllers = [this] {
    QTimer::singleShot(0, this, [this] { emit ConfigureControllersRequested(); });
  };
  host.in_game = [] { return CoreIsEmulationRunning(); };
  m_settings = BuildBigPictureSettings(host);
  m_quick_settings = BuildBigPictureQuickSettings();

  m_notifier = new NotifierWidget([this](QPainter& p) {
    const qreal a = std::min<qreal>(1, std::min(m_notify_t / 0.3, (3.0 - m_notify_t) / 0.25));
    const QRectF r = QRectF(m_notifier->rect()).adjusted(8, 8, -8, -8);
    p.setOpacity(std::clamp<qreal>(a, 0, 1));
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(SURFACE.red(), SURFACE.green(), SURFACE.blue(), 235));
    p.drawRoundedRect(r, r.height() / 2, r.height() / 2);
    p.setBrush(ACCENT);
    p.drawEllipse(QPointF(r.left() + r.height() / 2, r.center().y()), 7, 7);
    QFont font(m_text_family);
    font.setPixelSize(int(r.height() * 0.34));
    font.setWeight(QFont::DemiBold);
    p.setFont(font);
    p.setPen(TEXT);
    p.drawText(r.adjusted(r.height(), 0, -20, 0), Qt::AlignVCenter, m_notify_text);
  });

  m_clock.start();

  m_poll_timer = new QTimer(this);
  m_poll_timer->setInterval(POLL_INTERVAL_MS);
  connect(m_poll_timer, &QTimer::timeout, this, &BigPictureWindow::PollController);

  m_game_timer = new QTimer(this);
  m_game_timer->setInterval(30);
  connect(m_game_timer, &QTimer::timeout, this, &BigPictureWindow::PollInGame);

  m_anim_timer = new QTimer(this);
  m_anim_timer->setInterval(16);
  m_anim_timer->setTimerType(Qt::PreciseTimer);
  connect(m_anim_timer, &QTimer::timeout, this, &BigPictureWindow::Tick);

  m_rebuild_timer = new QTimer(this);
  m_rebuild_timer->setSingleShot(true);
  m_rebuild_timer->setInterval(150);
  connect(m_rebuild_timer, &QTimer::timeout, this, &BigPictureWindow::RebuildList);

  ApplyPreferences();
  RebuildList();
}

BigPictureWindow::~BigPictureWindow()
{
  FlushPlayTime();
  delete m_notifier;
}

void BigPictureWindow::ApplyTheme()
{
  const Palette pal = PaletteFor(Prefs::GetTheme());
  BG = pal.bg;
  SURFACE = pal.surface;
  SURFACE_DEEP = pal.deep;
  RAISED = pal.raised;
  LINE = pal.line;
  TEXT = pal.text;
  TEXT_SOFT = pal.soft;
  TEXT_DIM = pal.dim;
  ACCENT = pal.accent;
  ACCENT_SOFT = pal.accent_soft;
  if (Prefs::GetTheme() == Prefs::Theme::GameColor && m_ambient.isValid())
  {
    // Accent follows the selected game's cover colour.
    ACCENT = m_ambient;
    ACCENT_SOFT = m_ambient.lighter(150);
  }
}

void BigPictureWindow::ApplyPreferences()
{
  m_view = Prefs::GetView();
  m_effect = Prefs::GetEffect();
  m_transition = Prefs::GetTransition();
  m_swap_menu_buttons = Prefs::SwapMenuButtons();
  ApplyTheme();
  m_audio->SetVolumes(Prefs::MusicEnabled() ? Prefs::MusicVolume() / 100.0f : 0.0f,
                      Prefs::SfxVolume() / 100.0f);
  ApplyTab();
  update();
}

void BigPictureWindow::Open()
{
  setCursor(Qt::BlankCursor);  // shown again as soon as the mouse moves
  m_resume_known.clear();
  m_resume_shot_path.clear();
  m_active = true;
  m_game_timer->stop();
  // Ignore buttons that are still held (e.g. the one used to quit the game).
  m_prev_held = ~0u;
  m_last_tick = m_clock.elapsed();

  if (!m_intro_done && Prefs::IntroEnabled())
  {
    m_mode = Mode::Intro;
    m_intro_t = 0;
    QTimer::singleShot(250, this, [this] { m_audio->Play(Sound::Startup); });
  }
  else
  {
    m_intro_done = true;
    m_mode = Mode::Library;
    m_overlay_t = 0;
    m_trans_t = 1;
    m_audio->StartMusic();
  }

  if (const QScreen* scr = screen())
    setGeometry(scr->geometry());
  showFullScreen();
  raise();
  activateWindow();
  setFocus();
  m_poll_timer->start();
  m_anim_timer->start();
}

void BigPictureWindow::Suspend()
{
  m_poll_timer->stop();
  m_anim_timer->stop();
  m_audio->StopMusic();
  hide();
}

void BigPictureWindow::Close()
{
  m_active = false;
  m_game_timer->stop();
  Suspend();
}

void BigPictureWindow::FlushPlayTime()
{
  if (m_play_started < 0 || !m_running_entry.game)
    return;
  TimePlayed::Add(m_running_entry.game->GetGameID(),
                  std::chrono::milliseconds(m_clock.elapsed() - m_play_started));
  m_play_started = -1;
}

void BigPictureWindow::OnEmulationStarted()
{
  if (!m_active)
    return;
  m_play_started = m_clock.elapsed();
  if (!m_pending_state.isEmpty())
  {
    // Resume point: loaded once the game runs
    const QString state = m_pending_state;
    m_pending_state.clear();
    QTimer::singleShot(400, this, [state] {
      if (CoreIsEmulationRunning())
        CoreLoadSaveState(std::filesystem::path(state.toStdString()));
    });
  }
  if (m_mode == Mode::Launching)
  {
    // Reveal the game smoothly: the menu fades away over the running game.
    m_mode = Mode::FadingOut;
    m_fade_out = 0;
    m_home_was_down = true;
    return;
  }
  if (m_mode != Mode::InGame && m_mode != Mode::FadingOut)
  {
    m_mode = Mode::InGame;
    Suspend();
  }
  m_home_was_down = true;
  m_game_timer->start();
}

void BigPictureWindow::OnEmulationPaused(bool paused)
{
  if (!m_active)
    return;
  if (paused)
  {
    FlushPlayTime();
    return;
  }
  m_play_started = m_clock.elapsed();
  if (m_mode == Mode::Pause)
  {
    // Resumed from elsewhere (hotkey): close the pause menu.
    m_mode = Mode::InGame;
    Suspend();
    m_home_was_down = true;
    m_game_timer->start();
  }
}

void BigPictureWindow::OnEmulationFinished()
{
  FlushPlayTime();
  m_pending_state.clear();
  if (!m_active)
    return;
  m_game_timer->stop();
  if (m_mode == Mode::InGame || m_mode == Mode::Pause || m_mode == Mode::Launching ||
      m_mode == Mode::FadingOut || m_mode == Mode::Closing)
  {
    m_pause_settings = false;
    QTimer::singleShot(0, this, [this] {
      if (m_active)
      {
        setWindowOpacity(1.0);
        Open();
        m_fade_in = 0;  // fade in from black: no flash of the desktop
      }
    });
  }
}

void BigPictureWindow::SetGames(std::vector<ManetteGame> games)
{
  m_games = std::move(games);
  m_cover_cache.clear();
  m_ambient_cache.clear();
  ScheduleRebuild();
}

void BigPictureWindow::ScheduleRebuild()
{
  m_rebuild_timer->start();
}

void BigPictureWindow::RebuildList()
{
  m_all.clear();
  for (const ManetteGame& g : m_games)
  {
    Entry entry;
    entry.game = std::make_shared<const ManetteGame>(g);
    entry.path = QString::fromStdString(g.file);
    entry.discs << entry.path;
    // Name from the ROM database; unknown ROMs: the file name (often the No-Intro name)
    const QString good = QString::fromStdString(g.good_name);
    if (!good.contains(QStringLiteral("unknown rom"), Qt::CaseInsensitive))
      entry.title = CleanTitle(good);
    if (entry.title.isEmpty())
      entry.title = CleanTitle(QString::fromStdString(g.GetFileName()));
    if (entry.title.isEmpty())
      entry.title = QString::fromStdString(g.internal_name).trimmed();
    if (entry.title.isEmpty())
      entry.title = QString::fromStdString(g.GetFileName());
    m_all.push_back(std::move(entry));
  }
  std::sort(m_all.begin(), m_all.end(), [](const Entry& a, const Entry& b) {
    return QString::localeAwareCompare(a.title, b.title) < 0;
  });
  ApplyTab();
}

void BigPictureWindow::ApplyTab()
{
  QString selected_path;
  if (const Entry* sel = Selected())
    selected_path = sel->path;

  m_entries.clear();
  const QSettings& settings = ManetteQSettings();
  switch (m_tab)
  {
  case Tab::All:
    m_entries = m_all;
    break;
  case Tab::Favorites:
    for (const Entry& e : m_all)
    {
      if (m_favorites.contains(e.path))
        m_entries.push_back(e);
    }
    break;
  case Tab::Recent:
  {
    std::vector<std::pair<qint64, Entry>> recent;
    for (const Entry& e : m_all)
    {
      const qint64 when = settings.value(LastPlayedKey(e.path), 0).toLongLong();
      if (when > 0)
        recent.emplace_back(when, e);
    }
    std::sort(recent.begin(), recent.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
    for (auto& [when, e] : recent)
      m_entries.push_back(std::move(e));
    break;
  }
  case Tab::Settings:
    break;
  }

  // Sorting (not the "Recent" tab, which is already by date)
  const int sort = settings.value(QStringLiteral("sort"), 0).toInt();
  if (m_tab != Tab::Recent && sort != 0)
  {
    static TimePlayed s_played;
    s_played.Reload();
    std::map<QString, qint64> key;
    for (const Entry& e : m_entries)
    {
      key[e.path] = sort == 1 ?
                        settings.value(LastPlayedKey(e.path), 0).toLongLong() :
                        std::chrono::duration_cast<std::chrono::seconds>(
                            s_played.GetTimePlayed(e.game->GetGameID()))
                            .count();
    }
    std::stable_sort(m_entries.begin(), m_entries.end(), [&key](const Entry& a, const Entry& b) {
      return key[a.path] > key[b.path];
    });
  }

  m_selected = 0;
  for (size_t i = 0; i < m_entries.size(); ++i)
  {
    if (m_entries[i].path == selected_path)
      m_selected = static_cast<int>(i);
  }
  m_focus.assign(m_entries.size(), 0.0);
  if (!m_focus.empty())
    m_focus[m_selected] = 1.0;
  m_scroll = std::max(0, m_selected - 2) * 344.0;
  m_list_scroll = std::max(0, m_selected - 3);
  m_hero_t = 0;
  update();
}

const BigPictureWindow::Entry* BigPictureWindow::Selected() const
{
  if (m_selected < 0 || m_selected >= static_cast<int>(m_entries.size()))
    return nullptr;
  return &m_entries[m_selected];
}

void BigPictureWindow::SetSelected(int index)
{
  const int count = static_cast<int>(m_entries.size());
  index = std::clamp(index, 0, std::max(0, count - 1));
  if (index == m_selected || count == 0)
    return;
  m_selected = index;
  m_hero_t = 0;
  m_audio->Play(Sound::Move);
}

void BigPictureWindow::Toast(const QString& text)
{
  m_toast = text;
  m_toast_t = 2.6;
}

void BigPictureWindow::SetMode(Mode mode)
{
  if (mode == Mode::Menu || mode == Mode::ConfirmQuit || mode == Mode::Pause)
    m_overlay_t = 0;
  m_mode = mode;
}

// ================================================================== input

void BigPictureWindow::PollController()
{
  if (!m_active || !isVisible() || !isActiveWindow())
    return;
  if (m_mode == Mode::InGame || m_mode == Mode::Launching || m_mode == Mode::FadingOut ||
      m_mode == Mode::Closing)
    return;
  using namespace ManettePad;
  const Status pad = Read();
  if (m_mode == Mode::PadTest || m_mode == Mode::DeviceDiag)
  {
    // Every button is being tested: hold START for one second to leave.
    const qint64 t = m_clock.elapsed();
    if (pad.buttons & BTN_START)
    {
      if (m_test_start_since < 0)
        m_test_start_since = t;
      else if (t - m_test_start_since > 1000)
      {
        m_test_start_since = -1;
        m_audio->Play(Sound::Back);
        BeginTransition(-1);
        m_mode = m_mode_before_players;
        m_prev_held = ~0u;
      }
    }
    else
    {
      m_test_start_since = -1;
    }
    return;
  }

  constexpr float STICK = 0.5f;
  u32 held = 0;
  if ((pad.buttons & BTN_UP) || pad.y > STICK)
    held |= Bit(int(Action::Up));
  if ((pad.buttons & BTN_DOWN) || pad.y < -STICK)
    held |= Bit(int(Action::Down));
  if ((pad.buttons & BTN_LEFT) || pad.x < -STICK)
    held |= Bit(int(Action::Left));
  if ((pad.buttons & BTN_RIGHT) || pad.x > STICK)
    held |= Bit(int(Action::Right));
  if (pad.buttons & BTN_A)
    held |= Bit(int(Action::Accept));
  if (pad.buttons & BTN_B)
    held |= Bit(int(Action::Back));
  if (pad.buttons & BTN_START)
    held |= Bit(int(Action::Menu));
  if (pad.buttons & BTN_L)
    held |= Bit(int(Action::PageUp));
  if (pad.buttons & BTN_R)
    held |= Bit(int(Action::PageDown));
  if (pad.buttons & BTN_Y)
    held |= Bit(int(Action::Favorite));
  if (pad.buttons & BTN_X)
    held |= Bit(int(Action::Options));
  if (pad.buttons & BTN_Z)
    held |= Bit(int(Action::Sort));

  // Option: A/B and X/Y swapped in the menus only (never in a game)
  if (m_swap_menu_buttons)
  {
    const auto swap = [&held](Action p, Action q) {
      const bool hp = held & Bit(int(p)), hq = held & Bit(int(q));
      held &= ~(Bit(int(p)) | Bit(int(q)));
      if (hp)
        held |= Bit(int(q));
      if (hq)
        held |= Bit(int(p));
    };
    swap(Action::Accept, Action::Back);
    swap(Action::Options, Action::Favorite);
  }

  const qint64 now = m_clock.elapsed();

  // C buttons / right stick in the library: jump by letter (or by 10 games when not sorted
  // by name). On an N64 controller C-Left / C-Up are also X / Y: only the right stick of
  // other controllers jumps.
  if (m_mode == Mode::Library && m_tab != Tab::Settings && !m_entries.empty() &&
      !LastUsedIsN64())
  {
    const bool list = m_view == Prefs::View::List;
    const float v = list ? -pad.cy : pad.cx;  // list: down = forward; covers: right = forward
    const int dir = v > STICK ? 1 : v < -STICK ? -1 : 0;
    if (dir != 0 && (m_jump_dir != dir || now - m_jump_last > 260))
    {
      m_jump_dir = dir;
      m_jump_last = now;
      JumpLetter(dir);
    }
    else if (dir == 0)
    {
      m_jump_dir = 0;
    }
  }

  for (int a = 0; a < int(Action::Count); ++a)
  {
    const bool is_held = held & Bit(a);
    const bool was_held = m_prev_held & Bit(a);
    if (is_held && !was_held)
    {
      m_held_since[a] = m_last_repeat[a] = now;
      Press(static_cast<Action>(a));
    }
    else if (is_held && was_held && a <= int(Action::Right))
    {
      // Held longer, it speeds up: 85 ms, then 40 ms after 1.2 s, 20 ms after 2.5 s
      const qint64 held_for = now - m_held_since[a];
      const qint64 rate = held_for > 2500 ? 20 : held_for > 1200 ? 40 : REPEAT_RATE_MS;
      if (held_for > REPEAT_DELAY_MS && now - m_last_repeat[a] > rate)
      {
        m_last_repeat[a] = now;
        Press(static_cast<Action>(a));
      }
    }
  }
  m_prev_held = held;
}

// While a game runs: the chosen button (Guide, Select, L3 + R3 or START + Z) opens the menu.
void BigPictureWindow::PollInGame()
{
  if (!m_active || m_mode != Mode::InGame)
    return;
  if (!CoreIsEmulationRunning() || CoreIsEmulationPaused())
    return;
  using namespace ManettePad;
  const Status pad = Read();
  const int choice = Prefs::MenuButton();
  bool open = false;
  if (choice == 0 || choice == 1)
    open |= (pad.buttons & BTN_GUIDE) != 0;
#ifndef _WIN32
  // SteamOS keeps Guide for its own menu
  if (choice == 0 && !LastUsedIsN64())
    open |= (pad.buttons & BTN_BACK) != 0;
#endif
  if (choice == 2)
    open |= (pad.buttons & BTN_BACK) != 0;
  if (choice == 3)
    open |= (pad.buttons & BTN_L3) && (pad.buttons & BTN_R3);
  if (choice == 0 || choice == 4)
    open |= (pad.buttons & BTN_START) && (pad.buttons & BTN_Z);
  if (open && !m_home_was_down)
    OpenPause();
  m_home_was_down = open;
}

bool BigPictureWindow::QuitFromHotkey()
{
  if (!m_active || m_mode != Mode::InGame || !CoreIsEmulationRunning())
    return false;
  // Same path as "Quitter le jeu": a picture and a resume point first, then the fade out.
  OpenPause();
  QTimer::singleShot(450, this, [this] {
    if (m_mode == Mode::Pause)
      QuitGame();
  });
  return true;
}

void BigPictureWindow::QuitGame()
{
  m_audio->Play(Sound::Back);
  SaveResume();
  // Fade to black over the paused game, then stop: the menu comes back from black.
  m_mode = Mode::Closing;
  m_close_t = 0;
  m_stop_sent = false;
}

void BigPictureWindow::keyPressEvent(QKeyEvent* event)
{
  if (m_mode == Mode::DeviceDiag && (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right))
  {
    m_diag_index += event->key() == Qt::Key_Right ? 1 : -1;
    update();
    return;
  }
  if (m_mode == Mode::PadTest || m_mode == Mode::DeviceDiag)
  {
    if (event->key() == Qt::Key_Escape && !event->isAutoRepeat())
    {
      BeginTransition(-1);
      m_mode = m_mode_before_players;
    }
    return;
  }
  const bool repeat = event->isAutoRepeat();
  const auto once = [&](Action a) {
    if (!repeat)
      Press(a);
  };
  switch (event->key())
  {
  case Qt::Key_Up:
    Press(Action::Up);
    break;
  case Qt::Key_Down:
    Press(Action::Down);
    break;
  case Qt::Key_Left:
    Press(Action::Left);
    break;
  case Qt::Key_Right:
    Press(Action::Right);
    break;
  case Qt::Key_Return:
  case Qt::Key_Enter:
  case Qt::Key_Space:
    once(Action::Accept);
    break;
  case Qt::Key_Escape:
  case Qt::Key_Backspace:
    once(Action::Back);
    break;
  case Qt::Key_Tab:
  case Qt::Key_F1:
    once(Action::Menu);
    break;
  case Qt::Key_PageUp:
  case Qt::Key_Q:
    once(Action::PageUp);
    break;
  case Qt::Key_PageDown:
  case Qt::Key_E:
    once(Action::PageDown);
    break;
  case Qt::Key_F:
    once(Action::Favorite);
    break;
  case Qt::Key_S:
    once(Action::Sort);
    break;
  default:
    QWidget::keyPressEvent(event);
  }
}

void BigPictureWindow::Press(Action action)
{
  if (m_mode == Mode::Intro)
  {
    // Any button skips the intro.
    if (action == Action::Accept || action == Action::Back || action == Action::Menu)
      m_intro_t = std::max<qreal>(m_intro_t, 0.92);
    return;
  }
  HandleAction(action);
  update();
}

void BigPictureWindow::ToggleFavorite()
{
  const Entry* sel = Selected();
  if (!sel)
    return;
  const bool added = !m_favorites.contains(sel->path);
  if (added)
    m_favorites.insert(sel->path);
  else
    m_favorites.remove(sel->path);
  ManetteQSettings().setValue(QStringLiteral("favorites"),
                                    QStringList(m_favorites.begin(), m_favorites.end()));
  m_audio->Play(Sound::Toggle);
  Toast(added ? tr("Ajouté aux favoris") : tr("Retiré des favoris"));
  if (m_tab == Tab::Favorites)
    ApplyTab();
}

void BigPictureWindow::HandleAction(Action action)
{
  switch (m_mode)
  {
  case Mode::Library:
    if (m_tab == Tab::Settings)
      HandleSettings(action);
    else
      HandleLibrary(action);
    break;
  case Mode::Menu:
    HandleMenu(action);
    break;
  case Mode::GameSettings:
    HandleSettings(action);
    break;
  case Mode::Gallery:
    HandleGallery(action);
    break;
  case Mode::Stats:
    HandleStats(action);
    break;
  case Mode::Pause:
    if (m_pause_settings)
      HandleSettings(action);
    else
      HandlePause(action);
    break;
  case Mode::ConfirmQuit:
    if (action == Action::Accept)
    {
      m_audio->Play(Sound::Confirm);
      m_mode = Mode::Library;
      emit QuitRequested();
    }
    else if (action == Action::Back)
    {
      m_audio->Play(Sound::Cancel);
      BeginTransition(0);
      m_mode = Mode::Library;
    }
    break;
  default:
    break;
  }
}

void BigPictureWindow::HandleLibrary(Action action)
{
  const bool list = m_view == Prefs::View::List;
  switch (action)
  {
  case Action::Right:
  case Action::Left:
  {
    if (list)
      break;
    const int dir = action == Action::Right ? 1 : -1;
    if (m_hero_focus)
    {
      if (const Entry* sel = Selected())
      {
        const int n = static_cast<int>(HeroButtons(*sel).size());
        const int next = std::clamp(m_hero_button + dir, 0, n - 1);
        if (next != m_hero_button)
        {
          m_hero_button = next;
          m_audio->Play(Sound::Move);
        }
      }
      break;
    }
    SetSelected(m_selected + dir);
    break;
  }
  case Action::Down:
    if (!list)
    {
      // Back from the buttons to the covers
      if (m_hero_focus)
      {
        m_hero_focus = false;
        m_audio->Play(Sound::Move);
      }
      break;
    }
    SetSelected(m_selected + 1);
    break;
  case Action::Up:
    if (!list)
    {
      // Up: the game's buttons (Reprendre, Nouvelle partie, Réglages, Favori)
      if (!m_hero_focus && Selected())
      {
        m_hero_focus = true;
        m_hero_button = 0;
        m_audio->Play(Sound::Move);
      }
      break;
    }
    SetSelected(m_selected - 1);
    break;
  case Action::PageDown:
  case Action::PageUp:
  {
    const int dir = action == Action::PageDown ? 1 : -1;
    BeginTransition(dir);
    // "Récents" is no longer a tab: the sort has a "recent" order
    {
      constexpr Tab order[] = {Tab::All, Tab::Favorites, Tab::Settings};
      int pos = 0;
      for (int t = 0; t < 3; ++t)
      {
        if (order[t] == m_tab)
          pos = t;
      }
      m_tab = order[(pos + 3 + dir) % 3];
    }
    m_audio->Play(Sound::Page);
    m_settings_in_items = false;
    m_hero_focus = false;
    ApplyTab();
    break;
  }
  case Action::Accept:
    if (const Entry* sel = Selected())
    {
      if (m_hero_focus)
      {
        const auto buttons = HeroButtons(*sel);
        ActivateHero(buttons[std::clamp(m_hero_button, 0, int(buttons.size()) - 1)]);
      }
      else if (HasResume(*sel))
      {
        Launch(*sel, 0, true);
      }
      else
      {
        StartNewGame(*sel);
      }
    }
    else
    {
      m_audio->Play(Sound::Error);
    }
    break;
  case Action::Sort:
    CycleSort();
    break;
  case Action::Favorite:
    ToggleFavorite();
    break;
  case Action::Options:
    if (Selected())
      ActivateHero(HERO_SETTINGS);
    break;
  case Action::Back:
    m_audio->Play(Sound::Back);
    SetMode(Mode::ConfirmQuit);
    break;
  case Action::Menu:
    m_audio->Play(Sound::MenuOpen);
    SetMode(Mode::Menu);
    m_menu_index = 0;
    break;
  default:
    break;
  }
}

void BigPictureWindow::HandleSettings(Action action)
{
  const int cats = static_cast<int>(Cats().size());
  m_settings_cat = std::clamp(m_settings_cat, 0, std::max(0, cats - 1));
  auto& items = Cats()[m_settings_cat].items;
  const int count = static_cast<int>(items.size());

  // L / R still switch tabs in the library.
  if ((action == Action::PageUp || action == Action::PageDown) && !m_pause_settings &&
      m_mode != Mode::GameSettings)
  {
    SaveBigPictureSettings();
    HandleLibrary(action);
    return;
  }

  if (!m_settings_in_items)
  {
    switch (action)
    {
    case Action::Down:
      if (m_settings_cat < cats - 1)
      {
        m_settings_cat++;
        m_settings_item = 0;
        m_settings_scroll = 0;
        m_audio->Play(Sound::Move);
      }
      break;
    case Action::Up:
      if (m_settings_cat > 0)
      {
        m_settings_cat--;
        m_settings_item = 0;
        m_settings_scroll = 0;
        m_audio->Play(Sound::Move);
      }
      break;
    case Action::Right:
    case Action::Accept:
      m_settings_in_items = true;
      m_settings_item = 0;
      m_audio->Play(Sound::Confirm);
      break;
    case Action::Back:
      SaveBigPictureSettings();
      m_audio->Play(Sound::Back);
      BeginTransition(-1);
      if (m_pause_settings)
      {
        m_pause_settings = false;
      }
      else
      {
        m_tab = Tab::All;
        ApplyTab();
      }
      break;
    case Action::Menu:
      if (!m_pause_settings)
      {
        m_audio->Play(Sound::MenuOpen);
        SetMode(Mode::Menu);
        m_menu_index = 0;
      }
      break;
    default:
      break;
    }
    return;
  }

  BigPictureSettingItem& item = items[std::clamp(m_settings_item, 0, std::max(0, count - 1))];
  switch (action)
  {
  case Action::Down:
    if (m_settings_item < count - 1)
    {
      m_settings_item++;
      m_audio->Play(Sound::Move);
    }
    break;
  case Action::Up:
    if (m_settings_item > 0)
    {
      m_settings_item--;
      m_audio->Play(Sound::Move);
    }
    break;
  case Action::Left:
  case Action::Right:
    if (item.kind == BigPictureSettingItem::Kind::Action)
    {
      if (action == Action::Left)
      {
        m_settings_in_items = false;
        m_audio->Play(Sound::Back);
      }
      break;
    }
    item.change(action == Action::Right ? 1 : -1);
    m_audio->Play(Sound::Toggle);
    break;
  case Action::Accept:
    if (item.activate)
      item.activate();
    m_audio->Play(item.kind == BigPictureSettingItem::Kind::Action ? Sound::Confirm :
                                                                      Sound::Toggle);
    break;
  case Action::Back:
    m_settings_in_items = false;
    m_audio->Play(Sound::Back);
    SaveBigPictureSettings();
    if (m_pause_settings && m_quick)
    {
      BeginTransition(-1);
      m_pause_settings = false;
      m_quick = false;
    }
    break;
  case Action::Options:
    if (item.clear)
    {
      item.clear();
      m_audio->Play(Sound::Erase);
    }
    break;
  default:
    break;
  }
}

void BigPictureWindow::HandleMenu(Action action)
{
  constexpr int ITEMS = 7;
  switch (action)
  {
  case Action::Down:
    m_menu_index = (m_menu_index + 1) % ITEMS;
    m_audio->Play(Sound::Move);
    break;
  case Action::Up:
    m_menu_index = (m_menu_index + ITEMS - 1) % ITEMS;
    m_audio->Play(Sound::Move);
    break;
  case Action::Back:
  case Action::Menu:
    m_audio->Play(Sound::Back);
    BeginTransition(0);
    m_mode = Mode::Library;
    break;
  case Action::Accept:
    m_audio->Play(Sound::Confirm);
    BeginTransition(0);
    m_mode = Mode::Library;
    switch (m_menu_index)
    {
    case 1:
      m_tab = Tab::Settings;
      m_settings_in_items = false;
      break;
    case 2:
      OpenGallery();
      break;
    case 3:
      OpenStats();
      break;
    case 4:
      emit ConfigureControllersRequested();
      break;
    case 5:
      emit ClassicRequested();
      break;
    case 6:
      SetMode(Mode::ConfirmQuit);
      break;
    default:
      break;
    }
    break;
  default:
    break;
  }
}

// ================================================================== pause menu

std::vector<int> BigPictureWindow::PauseItems() const
{
  return {PAUSE_RESUME, PAUSE_SAVE,  PAUSE_LOAD,       PAUSE_QUICK,
          PAUSE_SETTINGS, PAUSE_RESET, PAUSE_SCREENSHOT, PAUSE_QUIT};
}

QString BigPictureWindow::PauseShotPath() const
{
  return ManetteDataDir(QStringLiteral("Pause")) + QStringLiteral("/pause.png");
}

QString BigPictureWindow::SlotThumbPath(int slot) const
{
  std::filesystem::path path;
  if (!CoreGetSaveStatePath(slot, path))
    return {};
  return QString::fromStdString(path.string()) + QStringLiteral(".png");
}

void BigPictureWindow::LoadSlotThumbs()
{
  for (int s = 1; s <= SLOT_COUNT; ++s)
    m_slot_thumbs[s] = QPixmap(SlotThumbPath(s));
}

// The newest screenshot of RMG's folder, taken after "since" (ms since epoch)
static QString NewestScreenshot(qint64 since)
{
  const QDir dir(QString::fromStdString(CoreGetScreenshotDirectory().string()));
  QString best;
  qint64 best_time = since - 1000;
  for (const QFileInfo& info : dir.entryInfoList({QStringLiteral("*.png")}, QDir::Files))
  {
    const qint64 t = info.lastModified().toMSecsSinceEpoch();
    if (t > best_time)
    {
      best_time = t;
      best = info.filePath();
    }
  }
  return best;
}

void BigPictureWindow::OpenPause()
{
  // Capture the current image for the save-state thumbnails, then pause.
  QFile::remove(PauseShotPath());
  const qint64 asked = QDateTime::currentMSecsSinceEpoch();
  CoreTakeScreenshot();
  QTimer::singleShot(220, this, [this, asked] {
    const QString shot = NewestScreenshot(asked);
    if (!shot.isEmpty())
    {
      QFile::remove(PauseShotPath());
      QFile::rename(shot, PauseShotPath());  // not kept in the gallery
    }
    if (m_mode == Mode::Pause && !CoreIsEmulationPaused())
      CorePauseEmulation();
  });
  LoadSlotThumbs();
  m_mode = Mode::Pause;
  m_overlay_t = 0;
  m_pause_index = 0;
  m_pause_settings = false;
  m_prev_held = ~0u;
  m_last_tick = m_clock.elapsed();
  showFullScreen();
  raise();
  activateWindow();
  setFocus();
  m_poll_timer->start();
  m_anim_timer->start();
  m_audio->Play(Sound::MenuOpen);
}

void BigPictureWindow::Resume()
{
  SaveBigPictureSettings();
  m_mode = Mode::InGame;
  m_pause_settings = false;
  m_poll_timer->stop();
  m_anim_timer->stop();
  hide();
  m_home_was_down = true;
  if (CoreIsEmulationPaused())
    CoreResumeEmulation();
  m_game_timer->start();
  emit ResumeRequested();
}

// Save states are written by the emulation thread: let it run a moment, then pause again.
static void RunBriefly(QObject* owner, std::function<bool()> still_paused_menu)
{
  if (CoreIsEmulationPaused())
    CoreResumeEmulation();
  QTimer::singleShot(300, owner, [still_paused_menu] {
    if (still_paused_menu() && CoreIsEmulationRunning() && !CoreIsEmulationPaused())
      CorePauseEmulation();
  });
}

void BigPictureWindow::HandlePause(Action action)
{
  const std::vector<int> items = PauseItems();
  const int n = static_cast<int>(items.size());
  m_pause_index = std::clamp(m_pause_index, 0, n - 1);
  const int item = items[m_pause_index];

  switch (action)
  {
  case Action::Down:
    m_pause_index = (m_pause_index + 1) % n;
    m_audio->Play(Sound::Move);
    break;
  case Action::Up:
    m_pause_index = (m_pause_index + n - 1) % n;
    m_audio->Play(Sound::Move);
    break;
  case Action::Left:
  case Action::Right:
    if (item == PAUSE_SAVE || item == PAUSE_LOAD)
    {
      m_slot = (m_slot - 1 + (action == Action::Right ? 1 : -1) + SLOT_COUNT) % SLOT_COUNT + 1;
      m_audio->Play(Sound::Toggle);
    }
    break;
  case Action::Back:
  case Action::Menu:
    m_audio->Play(Sound::Back);
    Resume();
    break;
  case Action::Accept:
    switch (item)
    {
    case PAUSE_RESUME:
      m_audio->Play(Sound::Confirm);
      Resume();
      break;
    case PAUSE_SAVE:
    {
      CoreSetSaveStateSlot(m_slot);
      RunBriefly(this, [this] { return m_mode == Mode::Pause; });
      CoreSaveState();
      const QString thumb = SlotThumbPath(m_slot);
      if (!thumb.isEmpty())
      {
        QFile::remove(thumb);
        QFile::copy(PauseShotPath(), thumb);
        m_slot_thumbs[m_slot] = QPixmap(thumb);
      }
      m_audio->Play(Sound::SaveState);
      Toast(tr("État sauvegardé dans l'emplacement %1").arg(m_slot));
      break;
    }
    case PAUSE_LOAD:
    {
      std::filesystem::path path;
      if (!CoreGetSaveStatePath(m_slot, path) || !std::filesystem::exists(path))
      {
        m_audio->Play(Sound::Error);
        Toast(tr("L'emplacement %1 est vide").arg(m_slot));
        break;
      }
      m_audio->Play(Sound::LoadState);
      CoreSetSaveStateSlot(m_slot);
      if (CoreIsEmulationPaused())
        CoreResumeEmulation();
      CoreLoadSaveState();
      QTimer::singleShot(250, this, [this] {
        if (m_mode == Mode::Pause)
          Resume();
      });
      break;
    }
    case PAUSE_SETTINGS:
      m_audio->Play(Sound::Confirm);
      BeginTransition(1);
      m_pause_settings = true;
      m_quick = false;
      m_settings_cat = 0;
      for (size_t i = 0; i < m_settings.size(); ++i)
      {
        if (m_settings[i].name == tr("Graphismes"))
          m_settings_cat = static_cast<int>(i);
      }
      m_settings_in_items = false;
      break;
    case PAUSE_RESET:
      m_audio->Play(Sound::Confirm);
      if (CoreIsEmulationPaused())
        CoreResumeEmulation();
      CoreResetEmulation(false);
      Resume();
      break;
    case PAUSE_SCREENSHOT:
    {
      // The picture taken when the menu opened, in RMG's screenshot folder
      const QString name =
          (m_running_entry.title.isEmpty() ? QStringLiteral("Capture") : m_running_entry.title)
              .remove(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]"))) +
          QDateTime::currentDateTime().toString(QStringLiteral(" yyyy-MM-dd HH-mm-ss")) +
          QStringLiteral(".png");
      const QString out =
          QString::fromStdString(CoreGetScreenshotDirectory().string()) + QStringLiteral("/") + name;
      if (QFile::copy(PauseShotPath(), out))
      {
        m_audio->Play(Sound::Confirm);
        Toast(tr("Capture d'écran enregistrée"));
      }
      else
      {
        m_audio->Play(Sound::Error);
        Toast(tr("Capture impossible"));
      }
      break;
    }
    case PAUSE_QUIT:
      QuitGame();
      break;
    case PAUSE_QUICK:
      m_audio->Play(Sound::Confirm);
      BeginTransition(1);
      m_pause_settings = true;
      m_quick = true;
      m_settings_cat = 0;
      m_settings_item = 0;
      m_settings_in_items = true;
      break;
    default:
      break;
    }
    break;
  default:
    break;
  }
}

// ================================================================== animation

void BigPictureWindow::BeginTransition(int direction)
{
  if (m_transition == Prefs::Transition::None || m_frame.isNull() || !isVisible())
    return;
  m_trans_from = m_frame;
  m_trans_dir = direction;
  m_trans_t = 0;
}

void BigPictureWindow::Tick()
{
  const qint64 now = m_clock.elapsed();
  const qreal dt = std::clamp((now - m_last_tick) / 1000.0, 0.0, 0.1);
  m_last_tick = now;
  m_time += dt;

  // Exponential smoothing, frame-rate independent.
  const auto approach = [dt](qreal& value, qreal target, qreal speed) {
    value += (target - value) * (1 - std::exp(-speed * dt));
    if (std::abs(target - value) < 0.0005)
      value = target;
  };

  if (m_mode == Mode::Intro)
  {
    m_intro_t += dt / (Prefs::GetIntroStyle() == Prefs::IntroStyle::N64 ? 4.6 : 3.0);
    if (m_intro_t >= 1)
    {
      m_intro_done = true;
      m_mode = Mode::Library;
      m_trans_from = m_frame;
      m_trans_dir = 0;
      m_trans_t = m_frame.isNull() ? 1 : 0;
      m_audio->StartMusic();
    }
  }

  if (m_mode == Mode::FadingOut)
  {
    m_fade_out = std::min<qreal>(1, m_fade_out + dt / 0.45);
    setWindowOpacity(1.0 - EaseInOut(m_fade_out));
    if (m_fade_out >= 1)
    {
      m_mode = Mode::InGame;
      Suspend();
      setWindowOpacity(1.0);
      m_game_timer->start();
      emit ResumeRequested();
      return;
    }
  }
  if (m_fade_in < 1)
    m_fade_in = std::min<qreal>(1, m_fade_in + dt / 0.6);
  if (m_mode == Mode::Closing)
  {
    m_close_t = std::min<qreal>(1, m_close_t + dt / 0.9);
    if (m_close_t >= 1 && !m_stop_sent)
    {
      m_stop_sent = true;
      emit StopRequested();
    }
  }
  if (Prefs::GetTheme() == Prefs::Theme::GameColor && m_ambient.isValid())
  {
    ACCENT = m_ambient;
    ACCENT_SOFT = m_ambient.lighter(150);
  }

  for (size_t i = 0; i < m_focus.size(); ++i)
    approach(m_focus[i], static_cast<int>(i) == m_selected ? 1.0 : 0.0, 14);
  approach(m_scroll, std::max(0, m_selected - 2) * 344.0, 10);
  approach(m_list_scroll, std::max(0, m_selected - 3), 10);
  m_hero_t = std::min<qreal>(1, m_hero_t + dt / 0.35);
  m_overlay_t = std::min<qreal>(1, m_overlay_t + dt / 0.28);
  if (m_trans_t < 1)
    m_trans_t = std::min<qreal>(1, m_trans_t + dt / 0.38);
  if (m_toast_t > 0)
    m_toast_t = std::max<qreal>(0, m_toast_t - dt);

  if (m_settings_in_items || m_pause_settings || m_tab == Tab::Settings)
    approach(m_settings_scroll, std::max(0, m_settings_item - 5), 12);

  // Ambient glow follows the selected game's cover colour.
  QColor target = SURFACE;
  if (const Entry* sel = Selected(); sel && m_tab != Tab::Settings)
    target = AmbientFor(*sel);
  if (!m_ambient.isValid())
    m_ambient = target;
  const qreal k = 1 - std::exp(-4 * dt);
  m_ambient = QColor::fromRgbF(
      static_cast<float>(m_ambient.redF() + (target.redF() - m_ambient.redF()) * k),
      static_cast<float>(m_ambient.greenF() + (target.greenF() - m_ambient.greenF()) * k),
      static_cast<float>(m_ambient.blueF() + (target.blueF() - m_ambient.blueF()) * k));

  update();
}

// ================================================================== assets

// Cover file: RMG's Covers folder (as the classic interface) or the downloaded one
QString BigPictureWindow::CoverFile(const Entry& entry) const
{
  const QString rmg_dir = QString::fromStdString((CoreGetUserDataDirectory() / "Covers").string());
  const ManetteGame& g = *entry.game;
  QStringList names = {QString::fromStdString(g.good_name), QString::fromStdString(g.internal_name),
                       QString::fromStdString(g.md5), entry.title,
                       QString::fromStdString(g.GetFileName())};
  for (QString name : names)
  {
    if (name.isEmpty())
      continue;
    name.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    for (const char* ext : {".png", ".jpg", ".jpeg"})
    {
      const QString path = rmg_dir + QStringLiteral("/") + name + QString::fromLatin1(ext);
      if (QFile::exists(path))
        return path;
    }
  }
  const QString downloaded = ManetteDataDir(QStringLiteral("Jaquettes")) + QStringLiteral("/") +
                             QString::fromStdString(g.GetGameID()) + QStringLiteral(".png");
  if (QFile::exists(downloaded))
    return downloaded;
  return {};
}

QPixmap BigPictureWindow::CoverFor(const Entry& entry, const QSize& size)
{
  const QString key = entry.path + QStringLiteral("@%1x%2").arg(size.width()).arg(size.height());
  const auto it = m_cover_cache.constFind(key);
  if (it != m_cover_cache.constEnd())
    return *it;

  QPixmap pixmap;
  const QString file = CoverFile(entry);
  if (!file.isEmpty() && pixmap.load(file))
  {
    // Fill the frame, cropping what overflows (N64 boxes are wider than tall)
    const QSize target = size * devicePixelRatioF();
    pixmap = pixmap.scaled(target, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    pixmap = pixmap.copy((pixmap.width() - target.width()) / 2,
                         (pixmap.height() - target.height()) / 2, target.width(),
                         target.height());
    pixmap.setDevicePixelRatio(devicePixelRatioF());
  }
  else
  {
    pixmap = QPixmap();
    DownloadCover(entry);
  }
  if (m_cover_cache.size() > 400)
    m_cover_cache.clear();
  m_cover_cache.insert(key, pixmap);
  return pixmap;
}

namespace
{
// libretro-thumbnails file names: lower case letters and digits only, for matching
QString MatchKey(QString s)
{
  s = s.toLower();
  s.replace(QStringLiteral("&"), QStringLiteral("and"));
  s.remove(QRegularExpression(QStringLiteral("[^a-z0-9]")));
  return s;
}

// "Legend of Zelda, The - Ocarina of Time (U) (V1.2) [!]" -> key of the title, preferred region
std::pair<QString, QString> TitleAndRegion(const QString& good_name)
{
  QString region;
  const QRegularExpression code(QStringLiteral("\\((U|E|J|JU|UE|JE|F|G|S|I|A|Europe|USA|Japan)\\)"));
  const auto m = code.match(good_name);
  if (m.hasMatch())
  {
    const QString c = m.captured(1);
    region = c.startsWith(QLatin1Char('U')) || c == QStringLiteral("USA") || c == QStringLiteral("JU") ?
                 QStringLiteral("usa") :
             c == QStringLiteral("J") || c == QStringLiteral("Japan") ? QStringLiteral("japan") :
                                                                        QStringLiteral("europe");
  }
  QString title = good_name;
  title.remove(QRegularExpression(QStringLiteral("\\s*[\\(\\[][^\\)\\]]*[\\)\\]]")));
  return {MatchKey(title), region};
}

QStringList s_thumb_names;      // every box art of the repository
bool s_thumb_list_loading = false;
QNetworkAccessManager* s_net = nullptr;
}  // namespace

void BigPictureWindow::DownloadCover(const Entry& entry)
{
  if (!ManetteQSettings().value(QStringLiteral("download_covers"), true).toBool())
    return;
  const QString id = QString::fromStdString(entry.game->GetGameID());
  if (id.isEmpty() || m_cover_tried.contains(id))
    return;
  if (!s_net)
    s_net = new QNetworkAccessManager(qApp);

  const QString list_file = ManetteDataDir(QStringLiteral("Jaquettes")) + QStringLiteral("/liste.txt");
  if (s_thumb_names.isEmpty())
  {
    QFile f(list_file);
    if (f.open(QIODevice::ReadOnly) &&
        QFileInfo(f).lastModified().daysTo(QDateTime::currentDateTime()) < 30)
    {
      s_thumb_names = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    }
  }
  if (s_thumb_names.isEmpty())
  {
    if (s_thumb_list_loading)
      return;
    s_thumb_list_loading = true;
    QNetworkRequest req(QUrl(QStringLiteral(
        "https://api.github.com/repos/libretro-thumbnails/Nintendo_-_Nintendo_64/git/trees/master?recursive=1")));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("RMG-Manette"));
    QNetworkReply* reply = s_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, list_file] {
      reply->deleteLater();
      s_thumb_list_loading = false;
      const QByteArray data = reply->readAll();
      const QRegularExpression re(QStringLiteral("\"path\":\"Named_Boxarts/([^\"]+)\\.png\""));
      QStringList names;
      for (auto it = re.globalMatch(QString::fromUtf8(data)); it.hasNext();)
        names << it.next().captured(1);
      if (names.isEmpty())
        return;
      s_thumb_names = names;
      QFile f(list_file);
      if (f.open(QIODevice::WriteOnly))
        f.write(names.join(QLatin1Char('\n')).toUtf8());
      m_cover_tried.clear();
      m_cover_cache.clear();
      update();
    });
    return;
  }
  m_cover_tried.insert(id, true);

  // Best match: same title, then the region of the ROM
  const auto [key, region] = TitleAndRegion(QString::fromStdString(entry.game->good_name).isEmpty() ?
                                                entry.title :
                                                QString::fromStdString(entry.game->good_name));
  const QString alt = MatchKey(entry.title);
  QString best;
  int best_score = 0;
  for (const QString& name : s_thumb_names)
  {
    const auto [nkey, nregion_unused] = TitleAndRegion(name);
    (void)nregion_unused;
    int score = 0;
    if (nkey == key || nkey == alt)
      score = 10;
    else
      continue;
    const QString lower = name.toLower();
    if (!region.isEmpty() && lower.contains(region))
      score += 5;
    if (lower.contains(QStringLiteral("(beta")) || lower.contains(QStringLiteral("(proto")) ||
        lower.contains(QStringLiteral("(demo")))
      score -= 4;
    if (score > best_score)
    {
      best_score = score;
      best = name;
    }
  }
  if (best.isEmpty())
    return;
  QString file_name = best;
  QNetworkRequest req(QUrl(QStringLiteral("https://raw.githubusercontent.com/libretro-thumbnails/"
                                          "Nintendo_-_Nintendo_64/master/Named_Boxarts/") +
                           QString::fromUtf8(QUrl::toPercentEncoding(file_name)) +
                           QStringLiteral(".png")));
  req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("RMG-Manette"));
  QNetworkReply* reply = s_net->get(req);
  const QString out = ManetteDataDir(QStringLiteral("Jaquettes")) + QStringLiteral("/") + id +
                      QStringLiteral(".png");
  connect(reply, &QNetworkReply::finished, this, [this, reply, out] {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError)
      return;
    const QByteArray data = reply->readAll();
    QImage image;
    if (!image.loadFromData(data))
      return;
    image.save(out);
    m_cover_cache.clear();
    m_ambient_cache.clear();
    update();
  });
}

QColor BigPictureWindow::AmbientFor(const Entry& entry)
{
  const auto it = m_ambient_cache.constFind(entry.path);
  if (it != m_ambient_cache.constEnd())
    return *it;

  QColor color(47, 123, 216);  // default: blue of the A button
  QImage image;
  const QString file = CoverFile(entry);
  if (!file.isEmpty())
    image.load(file);
  if (!image.isNull())
  {
    const QImage px = image.scaled(1, 1, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    color = px.pixelColor(0, 0);
    int h, s, v;
    color.getHsv(&h, &s, &v);
    color = QColor::fromHsv(std::max(h, 0), std::clamp(s + 40, 90, 230), std::clamp(v, 120, 220));
  }
  m_ambient_cache.insert(entry.path, color);
  return color;
}

QString BigPictureWindow::PlayInfo(const Entry& entry) const
{
  QStringList parts;
  const qint64 last =
      ManetteQSettings().value(LastPlayedKey(entry.path), 0).toLongLong();
  if (last > 0)
  {
    const QDate day = QDateTime::fromSecsSinceEpoch(last).date();
    const qint64 days = day.daysTo(QDate::currentDate());
    if (days == 0)
      parts << tr("Dernière partie aujourd'hui");
    else if (days == 1)
      parts << tr("Dernière partie hier");
    else
      parts << tr("Dernière partie il y a %1 jours").arg(days);
  }
  else
  {
    parts << tr("Jamais lancé");
  }

  static TimePlayed s_time_played;
  static qint64 s_reloaded = -1;
  const qint64 now = QDateTime::currentSecsSinceEpoch();
  if (now - s_reloaded > 5)
  {
    s_time_played.Reload();
    s_reloaded = now;
  }
  const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(
                           s_time_played.GetTimePlayed(entry.game->GetGameID()))
                           .count();
  if (minutes >= 60)
    parts << tr("%1 h %2 de jeu").arg(minutes / 60).arg(minutes % 60, 2, 10, QLatin1Char('0'));
  else if (minutes > 0)
    parts << tr("%1 min de jeu").arg(minutes);
  if (entry.discs.size() > 1)
    parts << tr("%1 disques").arg(entry.discs.size());
  return parts.join(QStringLiteral("  ·  "));
}

QString BigPictureWindow::ControllerName() const
{
  const std::string name = ManettePad::LastUsedName();
  return QString::fromStdString(name);
}

// ================================================================== drawing helpers

QFont BigPictureWindow::Font(bool display, int px, int weight) const
{
  QFont font(display ? m_display_family : m_text_family);
  font.setPixelSize(std::max(1, px));
  font.setWeight(static_cast<QFont::Weight>(weight));
  font.setHintingPreference(QFont::PreferNoHinting);
  return font;
}

namespace
{
// Nintendo 64 icons for the names used by the menus (the GameCube names of the original)
QString N64IconName(const QString& name)
{
  static const QHash<QString, QString> map = {
      {QStringLiteral("X"), QStringLiteral("C-Left")},
      {QStringLiteral("Y"), QStringLiteral("C-Up")},
      {QStringLiteral("Start-Pause"), QStringLiteral("Start")},
      {QStringLiteral("C-Stick"), QStringLiteral("C")},
      {QStringLiteral("C-Stick-U"), QStringLiteral("C-Up")},
      {QStringLiteral("C-Stick-D"), QStringLiteral("C-Down")},
      {QStringLiteral("C-Stick-L"), QStringLiteral("C-Left")},
      {QStringLiteral("C-Stick-R"), QStringLiteral("C-Right")},
  };
  if (const auto it = map.constFind(name); it != map.constEnd())
    return it.value();
  if (name.startsWith(QStringLiteral("C-Stick")))
    return QStringLiteral("C");
  if (name.startsWith(QStringLiteral("L-")))
    return QStringLiteral("L");
  if (name.startsWith(QStringLiteral("R-")))
    return QStringLiteral("R");
  if (name.startsWith(QStringLiteral("Z-")))
    return QStringLiteral("Z");
  return name;
}
}  // namespace

void BigPictureWindow::DrawIcon(QPainter& painter, const QString& name, const QRectF& rect)
{
  const QString mapped = N64IconName(name);
  DrawIconRaw(painter, mapped, rect);
  // "C-Stick-LR": dots on the C buttons that act
  if (mapped == QStringLiteral("C") && name != QStringLiteral("C-Stick"))
  {
    const qsizetype dash = name.lastIndexOf(QLatin1Char('-'));
    const QString dir = dash > 0 ? name.mid(dash + 1) : QString();
    const qreal side = std::min(rect.width(), rect.height());
    for (const QChar ch : dir)
    {
      const QPointF v = ch == QLatin1Char('U') ? QPointF(0, -1) :
                        ch == QLatin1Char('D') ? QPointF(0, 1) :
                        ch == QLatin1Char('L') ? QPointF(-1, 0) :
                                                 QPointF(1, 0);
      DrawDpadDot(painter, rect.center() + v * (side * 0.36), side * 0.09);
    }
  }
}

void BigPictureWindow::DrawIconRaw(QPainter& painter, const QString& name, const QRectF& rect)
{
  const auto it = m_icons.constFind(name);
  if (it == m_icons.constEnd())
    return;
  // Keep the icon's proportions inside the rectangle.
  const QSizeF size = QSizeF(it->size()).scaled(rect.size(), Qt::KeepAspectRatio);
  const QRectF target(rect.center().x() - size.width() / 2, rect.center().y() - size.height() / 2,
                      size.width(), size.height());
  painter.drawPixmap(target, *it, QRectF(it->rect()));
}

void BigPictureWindow::DrawHints(QPainter& painter,
                                 const std::vector<std::pair<QString, QString>>& left,
                                 const std::vector<std::pair<QString, QString>>& right)
{
  const qreal k = height() / 1080.0;
  const QFont font = Font(false, int(20 * k), 400);
  painter.setFont(font);
  const QFontMetricsF fm(font);
  const qreal icon = 52 * k;
  const qreal y = height() - 38 * k - icon / 2;

  // A click on a hint does what its button does
  const auto hint_action = [](const QString& name) -> std::optional<Action> {
    if (name == QStringLiteral("A"))
      return Action::Accept;
    if (name == QStringLiteral("B"))
      return Action::Back;
    if (name == QStringLiteral("X"))
      return Action::Options;
    if (name == QStringLiteral("Y"))
      return Action::Favorite;
    if (name == QStringLiteral("Z"))
      return Action::Sort;
    if (name == QStringLiteral("Start-Pause"))
      return Action::Menu;
    if (name == QStringLiteral("L"))
      return Action::PageUp;
    if (name == QStringLiteral("R"))
      return Action::PageDown;
    if (name.endsWith(QStringLiteral("-U")))
      return Action::Up;
    if (name.endsWith(QStringLiteral("-D")))
      return Action::Down;
    if (name.endsWith(QStringLiteral("-L")))
      return Action::Left;
    if (name.endsWith(QStringLiteral("-R")))
      return Action::Right;
    return std::nullopt;
  };
  const auto hint_hit = [&](const QString& name, const QRectF& r) {
    if (const auto a = hint_action(name))
      AddHit(r, [this, a] { HandleAction(*a); });
  };

  qreal x = 80 * k;
  for (const auto& [btn, label] : left)
  {
    const qreal x0 = x;
    DrawIcon(painter, btn, QRectF(x, y - icon / 2, icon, icon));
    x += icon + 12 * k;
    painter.setPen(TEXT_SOFT);
    painter.drawText(QRectF(x, y - icon / 2, 600 * k, icon), Qt::AlignVCenter, label);
    x += fm.horizontalAdvance(label);
    hint_hit(btn, QRectF(x0, y - icon / 2, x - x0, icon));
    x += 36 * k;
  }

  x = width() - 80 * k;
  for (auto it = right.rbegin(); it != right.rend(); ++it)
  {
    const qreal x1 = x;
    const qreal tw = fm.horizontalAdvance(it->second);
    painter.setPen(TEXT_SOFT);
    painter.drawText(QRectF(x - tw, y - icon / 2, tw + 2, icon), Qt::AlignVCenter, it->second);
    x -= tw + 10 * k + icon;
    DrawIcon(painter, it->first, QRectF(x, y - icon / 2, icon, icon));
    hint_hit(it->first, QRectF(x, y - icon / 2, x1 - x, icon));
    x -= 36 * k;
  }
}

qreal BigPictureWindow::DrawPillButton(QPainter& painter, qreal x, qreal y, const QString& icon,
                                       const QString& label, bool primary, qreal alpha,
                                       qreal forced_width)
{
  const qreal k = height() / 1080.0;
  const QFont font = Font(false, int((primary ? 24 : 22) * k), primary ? 600 : 500);
  const QFontMetricsF fm(font);
  const qreal icon_size = (primary ? 40 : 36) * k;
  const qreal h = 68 * k;
  const qreal w = forced_width > 0 ?
                      forced_width :
                      16 * k + icon_size + 14 * k + fm.horizontalAdvance(label) + 32 * k;
  const QRectF r(x, y, w, h);
  painter.save();
  painter.setOpacity(painter.opacity() * alpha);
  painter.setPen(Qt::NoPen);
  if (primary)
  {
    // soft glow under the main button
    QColor glow = ACCENT;
    for (int i = 3; i >= 1; --i)
    {
      glow.setAlphaF(0.07f);
      painter.setBrush(glow);
      painter.drawRoundedRect(r.adjusted(-i * 5 * k, -i * 4 * k + 6 * k, i * 5 * k, i * 6 * k),
                              (16 + i * 5) * k, (16 + i * 5) * k);
    }
  }
  painter.setBrush(primary ? ACCENT : RAISED);
  painter.drawRoundedRect(r, 16 * k, 16 * k);
  if (!icon.isEmpty())
    DrawIcon(painter, icon, QRectF(x + 16 * k, y + (h - icon_size) / 2, icon_size, icon_size));
  painter.setFont(font);
  painter.setPen(primary ? BG : TEXT);
  painter.drawText(QRectF(x + 16 * k + icon_size + 14 * k, y, w, h), Qt::AlignVCenter, label);
  painter.restore();
  return w;
}

void BigPictureWindow::DrawToast(QPainter& painter)
{
  if (m_toast_t <= 0 || m_toast.isEmpty())
    return;
  const qreal k = height() / 1080.0;
  const qreal a = std::min<qreal>(1, std::min(m_toast_t / 0.3, (2.6 - m_toast_t) / 0.25));
  const QFont font = Font(false, int(22 * k), 500);
  const qreal w = QFontMetricsF(font).horizontalAdvance(m_toast) + 64 * k;
  const qreal y = height() - 150 * k + (1 - EaseOut(a)) * 24 * k;
  const QRectF r((width() - w) / 2, y, w, 60 * k);
  painter.save();
  painter.setOpacity(a);
  painter.setPen(Qt::NoPen);
  painter.setBrush(TEXT);
  painter.drawRoundedRect(r, 30 * k, 30 * k);
  painter.setFont(font);
  painter.setPen(BG);
  painter.drawText(r, Qt::AlignCenter, m_toast);
  painter.restore();
}

// ================================================================== frame composition

void BigPictureWindow::paintEvent(QPaintEvent*)
{
  m_hits.clear();
  m_swap_menu_buttons =
      ManetteQSettings().value(QStringLiteral("swap_menu_buttons"), false).toBool();
  const qreal dpr = devicePixelRatioF();
  const QSize px_size = size() * dpr;
  if (m_frame.size() != px_size)
  {
    m_frame = QPixmap(px_size);
    m_frame.setDevicePixelRatio(dpr);
  }
  m_frame.fill(Qt::transparent);
  {
    QPainter p(&m_frame);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.setRenderHint(QPainter::TextAntialiasing);
    DrawScene(p);
  }

  QPainter painter(this);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);
  painter.setCompositionMode(QPainter::CompositionMode_Source);
  painter.fillRect(rect(), Qt::transparent);
  painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

  if (m_trans_t >= 1 || m_trans_from.isNull())
  {
    painter.drawPixmap(0, 0, m_frame);
    return;
  }

  const qreal e = EaseInOut(m_trans_t);
  const qreal w = width();
  switch (m_transition)
  {
  case Prefs::Transition::Slide:
  {
    const qreal dir = m_trans_dir == 0 ? 1 : m_trans_dir;
    painter.setOpacity(1 - e * 0.6);
    painter.drawPixmap(QPointF(-dir * w * e * 0.35, 0), m_trans_from);
    painter.setOpacity(e);
    painter.drawPixmap(QPointF(dir * w * (1 - e) * 0.35, 0), m_frame);
    break;
  }
  case Prefs::Transition::Zoom:
  {
    const QPointF c = rect().center();
    painter.save();
    painter.translate(c);
    painter.scale(1 + 0.06 * e, 1 + 0.06 * e);
    painter.translate(-c);
    painter.setOpacity(1 - e);
    painter.drawPixmap(0, 0, m_trans_from);
    painter.restore();
    painter.save();
    painter.translate(c);
    painter.scale(0.95 + 0.05 * e, 0.95 + 0.05 * e);
    painter.translate(-c);
    painter.setOpacity(e);
    painter.drawPixmap(0, 0, m_frame);
    painter.restore();
    break;
  }
  default:  // Fade
    painter.drawPixmap(0, 0, m_frame);
    painter.setOpacity(1 - e);
    painter.drawPixmap(0, 0, m_trans_from);
    break;
  }
}

void BigPictureWindow::DrawScene(QPainter& painter)
{
  if (m_mode == Mode::Intro)
  {
    painter.fillRect(rect(), BG);
    DrawIntro(painter);
    return;
  }
  if (m_mode == Mode::Pause || m_mode == Mode::Closing)
  {
    DrawPause(painter);
    DrawToast(painter);
    if (m_mode == Mode::Closing)
    {
      QColor veil = Qt::black;
      veil.setAlphaF(static_cast<float>(EaseInOut(m_close_t)));
      painter.fillRect(rect(), veil);
    }
    return;
  }

  painter.fillRect(rect(), BG);
  if (m_mode == Mode::PadTest)
  {
    DrawBackground(painter);
    DrawPadTest(painter);
    return;
  }
  if (m_mode == Mode::DeviceDiag)
  {
    DrawBackground(painter);
    DrawDeviceDiag(painter);
    DrawToast(painter);
    update();  // live
    return;
  }
  if (m_mode == Mode::Gallery)
  {
    if (m_shot_view)
      painter.fillRect(rect(), Qt::black);
    else
    {
      DrawBackground(painter);
      DrawTopBar(painter);
    }
    DrawGallery(painter);
    DrawToast(painter);
    return;
  }
  if (m_mode == Mode::Stats)
  {
    DrawBackground(painter);
    DrawTopBar(painter);
    DrawStats(painter);
    return;
  }
  if (m_mode == Mode::Launching || m_mode == Mode::FadingOut)
  {
    // Loading screen: the cover in the middle, a spinner, then the game fades in behind.
    const qreal k = height() / 1080.0;
    DrawBackground(painter);
    const QSizeF cs(500 * k, 360 * k);
    const QRectF cr((width() - cs.width()) / 2, height() / 2.0 - cs.height() / 2 - 60 * k,
                    cs.width(), cs.height());
    const QPixmap cover =
        m_running_entry.game ? CoverFor(m_running_entry, cs.toSize()) : QPixmap();
    QPainterPath clip;
    clip.addRoundedRect(cr, 20 * k, 20 * k);
    if (!cover.isNull())
    {
      painter.save();
      painter.setClipPath(clip, Qt::IntersectClip);
      painter.drawPixmap(cr, cover, QRectF(cover.rect()));
      painter.restore();
    }
    else
    {
      painter.fillPath(clip, RAISED);
    }
    painter.setFont(Font(true, int(36 * k), 700));
    painter.setPen(TEXT);
    painter.drawText(QRectF(0, cr.bottom() + 30 * k, width(), 50 * k), Qt::AlignCenter,
                     m_running_entry.title);
    // spinner
    const QPointF sc(width() / 2.0, cr.bottom() + 130 * k);
    for (int i = 0; i < 12; ++i)
    {
      const qreal a = i * std::numbers::pi / 6 + m_time * 5;
      QColor c = TEXT;
      c.setAlphaF(static_cast<float>(0.15 + 0.85 * (i / 11.0)));
      painter.setPen(QPen(c, 4 * k, Qt::SolidLine, Qt::RoundCap));
      painter.drawLine(QPointF(sc.x() + std::cos(a) * 12 * k, sc.y() + std::sin(a) * 12 * k),
                       QPointF(sc.x() + std::cos(a) * 22 * k, sc.y() + std::sin(a) * 22 * k));
    }
    return;
  }

  DrawBackground(painter);
  DrawTopBar(painter);
  if (m_tab == Tab::Settings)
    DrawSettings(painter, 170 * height() / 1080.0);
  else if (m_entries.empty())
    DrawEmpty(painter);
  else if (m_view == Prefs::View::List)
    DrawList(painter);
  else
    DrawCoverShelf(painter);
  if (m_mode == Mode::Library)
    DrawJumpLetter(painter);

  if (m_mode == Mode::Menu)
    DrawMenu(painter);
  else if (m_mode == Mode::ConfirmQuit)
    DrawDialog(painter, tr("Quitter RMG ?"),
               {{QStringLiteral("A"), tr("Quitter")}, {QStringLiteral("B"), tr("Annuler")}});
  DrawToast(painter);
  if (m_fade_in < 1)
  {
    QColor veil = Qt::black;
    veil.setAlphaF(static_cast<float>(1 - EaseInOut(m_fade_in)));
    painter.fillRect(rect(), veil);
  }
}

// ================================================================== background effects

void BigPictureWindow::DrawBackground(QPainter& painter)
{
  const qreal k = height() / 1080.0;
  const qreal t = m_time;
  QColor glow = m_ambient.isValid() ? m_ambient : ACCENT;
  if (m_tab == Tab::Settings && m_mode != Mode::Setup)
    glow = ACCENT;

  const auto halo = [&](qreal strength) {
    const qreal breathe = 1 + 0.05 * std::sin(t * 0.8);
    const QPointF c(width() - 430 * k, 290 * k);
    QRadialGradient g(c, 760 * k * breathe);
    QColor inner = glow;
    inner.setAlphaF(static_cast<float>(0.26 * strength));
    QColor mid = glow;
    mid.setAlphaF(static_cast<float>(0.10 * strength));
    QColor outer = glow;
    outer.setAlphaF(0.0f);
    g.setColorAt(0, inner);
    g.setColorAt(0.55, mid);
    g.setColorAt(1, outer);
    painter.fillRect(rect(), g);
  };

  switch (m_effect)
  {
  case Prefs::Effect::Halo:
    halo(1.0);
    break;

  case Prefs::Effect::Particles:
  {
    halo(0.55);
    painter.save();
    painter.setPen(Qt::NoPen);
    for (int i = 0; i < 90; ++i)
    {
      const qreal speed = 0.012 + Hash01(i, 1) * 0.03;
      const qreal x = std::fmod(Hash01(i, 2) + std::sin(t * 0.2 + i) * 0.01 + 1.0, 1.0) * width();
      const qreal y = (1 - std::fmod(Hash01(i, 3) + t * speed, 1.0)) * height();
      const qreal r = (1.5 + Hash01(i, 4) * 4.5) * k;
      const qreal twinkle = 0.5 + 0.5 * std::sin(t * (1 + Hash01(i, 5) * 2) + i);
      QColor c = Hash01(i, 6) > 0.5 ? glow.lighter(140) : TEXT;
      c.setAlphaF(static_cast<float>(0.08 + 0.22 * twinkle));
      painter.setBrush(c);
      painter.drawEllipse(QPointF(x, y), r, r);
    }
    painter.restore();
    break;
  }

  case Prefs::Effect::Cubes:
  {
    halo(0.5);
    painter.save();
    painter.setBrush(Qt::NoBrush);
    for (int i = 0; i < 11; ++i)
    {
      const qreal size = (70 + Hash01(i, 7) * 170) * k;
      const qreal x = std::fmod(Hash01(i, 8) + t * (0.004 + Hash01(i, 9) * 0.008), 1.2) *
                          (width() + size * 2) -
                      size;
      const qreal y = (0.08 + Hash01(i, 10) * 0.84) * height() + std::sin(t * 0.5 + i) * 18 * k;
      const qreal angle = t * (6 + Hash01(i, 11) * 12) * (i % 2 ? 1 : -1);
      QColor c = i % 3 == 0 ? glow : TEXT;
      c.setAlphaF(static_cast<float>(0.05 + Hash01(i, 12) * 0.08));
      QPen pen(c, (1.5 + Hash01(i, 13)) * k);
      pen.setJoinStyle(Qt::RoundJoin);
      painter.setPen(pen);
      painter.save();
      painter.translate(x, y);
      painter.rotate(angle);
      painter.drawPath(CubePath(size));
      painter.restore();
    }
    painter.restore();
    break;
  }

  case Prefs::Effect::Waves:
  {
    halo(0.55);
    painter.save();
    painter.setBrush(Qt::NoBrush);
    for (int i = 0; i < 5; ++i)
    {
      QPainterPath path;
      const qreal base = height() * (0.58 + i * 0.075);
      const qreal amp = (18 + i * 9) * k;
      const qreal freq = 0.0022 / k * (1 + i * 0.25);
      const qreal phase = t * (0.35 + i * 0.12) * (i % 2 ? 1 : -1);
      for (int xi = 0; xi <= 96; ++xi)
      {
        const qreal x = width() * xi / 96.0;
        const qreal y = base + std::sin(x * freq + phase) * amp +
                        std::sin(x * freq * 2.3 - phase * 0.7) * amp * 0.35;
        if (xi == 0)
          path.moveTo(x, y);
        else
          path.lineTo(x, y);
      }
      QColor c = i % 2 ? glow : TEXT;
      c.setAlphaF(static_cast<float>(0.07 + 0.035 * i));
      painter.setPen(QPen(c, (2 + i * 0.4) * k));
      painter.drawPath(path);
    }
    painter.restore();
    break;
  }

  case Prefs::Effect::None:
    break;

  case Prefs::Effect::Cube3D:
  {
    halo(0.5);
    // Big wireframe cube slowly turning, like the GameCube BIOS menu.
    const qreal ax = t * 0.21, ay = t * 0.33;
    const QPointF centre(width() * 0.72, height() * 0.42);
    const qreal size = 260 * k;
    const auto project = [&](qreal x, qreal y, qreal z, qreal s) {
      // rotate around Y then X
      const qreal x1 = x * std::cos(ay) + z * std::sin(ay);
      const qreal z1 = -x * std::sin(ay) + z * std::cos(ay);
      const qreal y1 = y * std::cos(ax) - z1 * std::sin(ax);
      const qreal z2 = y * std::sin(ax) + z1 * std::cos(ax);
      const qreal persp = 3.2 / (3.2 + z2);
      return QPointF(centre.x() + x1 * s * persp, centre.y() + y1 * s * persp);
    };
    static constexpr int edges[12][2] = {{0, 1}, {1, 3}, {3, 2}, {2, 0}, {4, 5}, {5, 7},
                                         {7, 6}, {6, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (int pass = 0; pass < 2; ++pass)
    {
      const qreal s = pass == 0 ? size : size * 0.42;
      QPointF pts[8];
      for (int v = 0; v < 8; ++v)
        pts[v] = project(v & 1 ? 1 : -1, v & 2 ? 1 : -1, v & 4 ? 1 : -1, s);
      QColor c = pass == 0 ? TEXT : glow;
      c.setAlphaF(pass == 0 ? 0.10f : 0.22f);
      painter.setPen(QPen(c, (pass == 0 ? 2.0 : 3.0) * k));
      for (const auto& e : edges)
        painter.drawLine(pts[e[0]], pts[e[1]]);
    }
    break;
  }
  }
}

void BigPictureWindow::DrawIntro(QPainter& painter)
{
  if (Prefs::GetIntroStyle() == Prefs::IntroStyle::N64)
  {
    DrawIntroN64(painter);
    return;
  }
  const qreal k = height() / 1080.0;
  const qreal t = m_intro_t;
  const QPointF c(width() / 2.0, height() / 2.0 - 40 * k);

  // Soft glow growing behind the cube
  QRadialGradient g(c, 520 * k * (0.6 + 0.4 * EaseOut(t / 0.5)));
  QColor inner = ACCENT;
  inner.setAlphaF(static_cast<float>(0.30 * EaseOut(t / 0.4) * (1 - EaseInOut((t - 0.8) / 0.2))));
  QColor outer = ACCENT;
  outer.setAlphaF(0.0f);
  g.setColorAt(0, inner);
  g.setColorAt(1, outer);
  painter.fillRect(rect(), g);

  // The cube draws itself, then turns a quarter and settles.
  const qreal draw = EaseInOut(t / 0.45);
  const qreal spin = EaseOut((t - 0.25) / 0.35);
  const qreal size = 230 * k * (0.85 + 0.15 * spin);
  painter.save();
  painter.translate(c);
  painter.rotate(-90 * (1 - spin));
  QPen pen(TEXT, 7 * k);
  pen.setJoinStyle(Qt::RoundJoin);
  pen.setCapStyle(Qt::RoundCap);
  const QPainterPath cube = CubePath(size);
  const qreal len = cube.length() / pen.widthF();
  pen.setDashPattern({len * draw + 0.001, len + 1});
  painter.setPen(pen);
  painter.setOpacity(1 - EaseInOut((t - 0.85) / 0.15));
  painter.drawPath(cube);
  painter.restore();

  // Title
  const qreal title_a = EaseOut((t - 0.42) / 0.25) * (1 - EaseInOut((t - 0.85) / 0.15));
  QFont title = Font(true, int(84 * k), 700);
  title.setLetterSpacing(QFont::AbsoluteSpacing, -2 * k);
  painter.setFont(title);
  painter.setOpacity(title_a);
  painter.setPen(TEXT);
  painter.drawText(QRectF(0, c.y() + 170 * k + (1 - title_a) * 20 * k, width(), 100 * k),
                   Qt::AlignHCenter | Qt::AlignTop, QStringLiteral("RMG"));
  QFont sub = Font(false, int(22 * k), 600);
  sub.setLetterSpacing(QFont::AbsoluteSpacing, 4 * k);
  painter.setFont(sub);
  painter.setPen(ACCENT_SOFT);
  painter.drawText(QRectF(0, c.y() + 272 * k, width(), 40 * k), Qt::AlignHCenter,
                   tr("MODE MANETTE"));
  painter.setOpacity(1);
}

// Four coloured blocks (red, green, blue, yellow, like the colours of the console era) fly
// in turning, lock together into one block, then the title appears.
void BigPictureWindow::DrawIntroN64(QPainter& painter)
{
  const qreal k = height() / 1080.0;
  const qreal t = m_intro_t;
  const QPointF c(width() / 2.0, height() / 2.0 - 70 * k);
  const qreal out = 1 - EaseInOut((t - 0.86) / 0.14);

  QRadialGradient g(c, 560 * k);
  QColor inner(255, 255, 255);
  inner.setAlphaF(static_cast<float>(0.10 * EaseOut(t / 0.4) * out));
  QColor outer = inner;
  outer.setAlphaF(0.0f);
  g.setColorAt(0, inner);
  g.setColorAt(1, outer);
  painter.fillRect(rect(), g);

  const QColor colors[4] = {QColor(226, 42, 52), QColor(32, 168, 72), QColor(36, 98, 214),
                            QColor(246, 190, 22)};
  const QPointF spots[4] = {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
  const qreal s = 92 * k;  // half-size of a block
  painter.save();
  painter.setOpacity(out);
  for (int i = 0; i < 4; ++i)
  {
    const qreal arrive = EaseOut((t - 0.06 * i) / 0.42);
    const qreal turn = (1 - arrive) * (i % 2 ? -1 : 1) * 200;
    const QPointF from = c + QPointF(spots[i].x() * 900 * k, spots[i].y() * 600 * k);
    const QPointF to = c + QPointF(spots[i].x() * s * 0.98, spots[i].y() * s * 0.98);
    const QPointF pos = from + (to - from) * arrive;
    painter.save();
    painter.translate(pos);
    painter.rotate(turn);
    // a block with a lit top edge and a darker bottom edge
    const QRectF r(-s * 0.94, -s * 0.94, s * 1.88, s * 1.88);
    QLinearGradient shade(r.topLeft(), r.bottomRight());
    shade.setColorAt(0, colors[i].lighter(135));
    shade.setColorAt(0.55, colors[i]);
    shade.setColorAt(1, colors[i].darker(150));
    painter.setPen(Qt::NoPen);
    painter.setBrush(shade);
    painter.drawRoundedRect(r, 18 * k, 18 * k);
    painter.setBrush(QColor(255, 255, 255, 46));
    painter.drawRoundedRect(QRectF(r.left() + 12 * k, r.top() + 12 * k, r.width() - 24 * k,
                                   r.height() * 0.28),
                            12 * k, 12 * k);
    painter.restore();
  }
  // a short flash when they lock together
  const qreal lock = std::clamp((t - 0.5) / 0.12, 0.0, 1.0);
  if (lock > 0 && lock < 1)
  {
    QColor flash(255, 255, 255);
    flash.setAlphaF(static_cast<float>(0.35 * std::sin(lock * std::numbers::pi)));
    painter.setBrush(flash);
    painter.drawRoundedRect(QRectF(c.x() - 2 * s, c.y() - 2 * s, 4 * s, 4 * s), 24 * k, 24 * k);
  }
  painter.restore();

  const qreal title_a = EaseOut((t - 0.52) / 0.22) * out;
  QFont title = Font(true, int(84 * k), 700);
  title.setLetterSpacing(QFont::AbsoluteSpacing, -2 * k);
  painter.setFont(title);
  painter.setOpacity(title_a);
  painter.setPen(TEXT);
  painter.drawText(QRectF(0, c.y() + 230 * k + (1 - title_a) * 20 * k, width(), 100 * k),
                   Qt::AlignHCenter | Qt::AlignTop, QStringLiteral("RMG"));
  QFont sub = Font(false, int(22 * k), 600);
  sub.setLetterSpacing(QFont::AbsoluteSpacing, 4 * k);
  painter.setFont(sub);
  painter.setPen(ACCENT_SOFT);
  painter.drawText(QRectF(0, c.y() + 332 * k, width(), 40 * k), Qt::AlignHCenter,
                   tr("NINTENDO 64 · MODE MANETTE"));
  painter.setOpacity(1);
}

// ================================================================== library screens

void BigPictureWindow::DrawTopBar(QPainter& painter)
{
  const qreal k = height() / 1080.0;
  const qreal cy = 84 * k;

  // Logo: four coloured blocks + name
  {
    const QColor colors[4] = {QColor(226, 42, 52), QColor(32, 168, 72), QColor(36, 98, 214),
                              QColor(246, 190, 22)};
    const qreal b = 15 * k, g = 3 * k;
    painter.setPen(Qt::NoPen);
    for (int i = 0; i < 4; ++i)
    {
      painter.setBrush(colors[i]);
      painter.drawRoundedRect(QRectF(84 * k + (i % 2) * (b + g), cy - b - g / 2 + (i / 2) * (b + g),
                                     b, b),
                              3.5 * k, 3.5 * k);
    }
  }
  painter.setFont(Font(true, int(30 * k), 700));
  painter.setPen(TEXT);
  painter.drawText(QRectF(136 * k, cy - 30 * k, 400 * k, 60 * k), Qt::AlignVCenter,
                   QStringLiteral("RMG"));

  // Tabs, with a sliding highlight
  const QString tabs[] = {tr("Bibliothèque"), tr("Favoris"), tr("Réglages")};
  constexpr Tab tab_ids[] = {Tab::All, Tab::Favorites, Tab::Settings};
  constexpr int tab_count = 3;
  const QFontMetricsF fm(Font(false, int(20 * k), 600));
  qreal widths[tab_count];
  qreal pill_w = 12 * k;
  for (int i = 0; i < tab_count; ++i)
  {
    widths[i] = fm.horizontalAdvance(tabs[i]) + 56 * k;
    pill_w += widths[i] + 6 * k;
  }
  pill_w -= 6 * k;
  const qreal pill_h = 58 * k;
  const qreal pill_x = (width() - pill_w) / 2;
  const QRectF pill(pill_x, cy - pill_h / 2, pill_w, pill_h);
  painter.setPen(Qt::NoPen);
  painter.setBrush(SURFACE);
  painter.drawRoundedRect(pill, pill_h / 2, pill_h / 2);

  static qreal s_hl_x = -1, s_hl_w = 0;
  qreal tx = pill_x + 6 * k;
  qreal target_x = tx, target_w = widths[0];
  for (int i = 0; i < tab_count; ++i)
  {
    if (m_tab == tab_ids[i])
    {
      target_x = tx;
      target_w = widths[i];
    }
    tx += widths[i] + 6 * k;
  }
  if (s_hl_x < 0)
  {
    s_hl_x = target_x;
    s_hl_w = target_w;
  }
  s_hl_x += (target_x - s_hl_x) * 0.25;
  s_hl_w += (target_w - s_hl_w) * 0.25;
  const QRectF hl(s_hl_x, cy - pill_h / 2 + 6 * k, s_hl_w, pill_h - 12 * k);
  painter.setBrush(ACCENT);
  painter.drawRoundedRect(hl, hl.height() / 2, hl.height() / 2);

  tx = pill_x + 6 * k;
  for (int i = 0; i < tab_count; ++i)
  {
    const QRectF r(tx, cy - pill_h / 2 + 6 * k, widths[i], pill_h - 12 * k);
    const bool active = m_tab == tab_ids[i];
    AddHit(r, [this, tab = tab_ids[i]] {
      if (m_tab == tab || (m_mode != Mode::Library))
        return;
      BeginTransition(static_cast<int>(tab) > static_cast<int>(m_tab) ? 1 : -1);
      m_tab = tab;
      m_audio->Play(Sound::Page);
      m_settings_in_items = false;
      m_hero_focus = false;
      ApplyTab();
    });
    painter.setFont(Font(false, int(20 * k), active ? 600 : 500));
    painter.setPen(active ? BG : TEXT_SOFT);
    painter.drawText(r, Qt::AlignCenter, tabs[i]);
    tx += widths[i] + 6 * k;
  }
  const qreal icon = 46 * k;
  const QRectF lr(pill.left() - 14 * k - icon, cy - icon / 2, icon, icon);
  const QRectF rr(pill.right() + 14 * k, cy - icon / 2, icon, icon);
  DrawIcon(painter, QStringLiteral("L"), lr);
  DrawIcon(painter, QStringLiteral("R"), rr);
  AddHit(lr, [this] { HandleAction(Action::PageUp); });
  AddHit(rr, [this] { HandleAction(Action::PageDown); });

  // Clock
  const QString clock = QTime::currentTime().toString(QStringLiteral("HH:mm"));
  const QFont clock_font = Font(true, int(26 * k), 500);
  painter.setFont(clock_font);
  painter.setPen(TEXT);
  const qreal clock_w = QFontMetricsF(clock_font).horizontalAdvance(clock);
  const qreal clock_x = width() - 80 * k - clock_w;
  painter.drawText(QRectF(clock_x, cy - 30 * k, clock_w + 4, 60 * k), Qt::AlignVCenter, clock);

  // The controller in use
  const QString pad = ControllerName();
  if (!pad.isEmpty())
  {
    const QFont chip_font = Font(false, int(18 * k), 600);
    painter.setFont(chip_font);
    const QString text = QStringLiteral("J1  ") + pad;
    const qreal tw = std::min<qreal>(QFontMetricsF(chip_font).horizontalAdvance(text), 360 * k);
    const QRectF chip(clock_x - 28 * k - tw - 40 * k, cy - 22 * k, tw + 40 * k, 44 * k);
    painter.setPen(QPen(LINE, 1.5 * k));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(chip, 22 * k, 22 * k);
    painter.setPen(TEXT_SOFT);
    painter.drawText(chip.adjusted(20 * k, 0, -20 * k, 0), Qt::AlignVCenter,
                     QFontMetricsF(chip_font).elidedText(text, Qt::ElideRight, tw));
  }
}

void BigPictureWindow::DrawHero(QPainter& painter, const Entry& sel, qreal left, qreal top,
                                qreal max_width)
{
  const qreal k = height() / 1080.0;
  const qreal e = EaseOut(m_hero_t);
  painter.save();
  painter.setOpacity(e);
  painter.translate((1 - e) * 26 * k, 0);

  qreal y = top;
  QStringList meta;
  meta << QStringLiteral("NINTENDO 64");
  const QString region = RegionName(sel.game->region).toUpper();
  if (!region.isEmpty())
    meta << region;
  if (!sel.game->header_id.empty())
    meta << QString::fromStdString(sel.game->header_id);
  QFont meta_font = Font(false, int(18 * k), 600);
  meta_font.setLetterSpacing(QFont::AbsoluteSpacing, 1.5 * k);
  painter.setFont(meta_font);
  painter.setPen(ACCENT_SOFT);
  painter.drawText(QRectF(left, y, max_width, 28 * k), Qt::AlignVCenter,
                   meta.join(QStringLiteral("   ·   ")));
  y += 40 * k;

  const QString maker;
  const QString desc = GameDescription(*sel.game);
  const bool has_desc = !maker.isEmpty() || !desc.isEmpty();

  // Title: up to two lines, shrinks if needed (a bit more with the description line, so the
  // buttons stay above the shelf).
  const QRectF title_box(left, y, max_width, (has_desc ? 140 : 170) * k);
  int title_px = int(76 * k);
  QFont title_font = Font(true, title_px, 700);
  title_font.setLetterSpacing(QFont::AbsoluteSpacing, -2 * k);
  QRectF needed;
  for (;;)
  {
    needed = QFontMetricsF(title_font)
                 .boundingRect(title_box, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop,
                               sel.title);
    if (needed.height() <= title_box.height() || title_px <= int(44 * k))
      break;
    title_px -= std::max(1, int(4 * k));
    title_font.setPixelSize(title_px);
  }
  painter.setFont(title_font);
  painter.setPen(TEXT);
  painter.drawText(title_box, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, sel.title);
  y += std::min(needed.height(), title_box.height()) + 18 * k;

  painter.setFont(Font(false, int(22 * k), 400));
  painter.setPen(TEXT_SOFT);
  painter.drawText(QRectF(left, y, max_width, 32 * k), Qt::AlignVCenter, PlayInfo(sel));
  y += 44 * k;

  // The game's own short description, as on the console's menu: one line
  if (has_desc)
  {
    const QFont maker_font = Font(false, int(18 * k), 700);
    const QFont desc_font = Font(false, int(20 * k), 400);
    qreal x = left;
    if (!maker.isEmpty())
    {
      const QString m = maker.toUpper();
      painter.setFont(maker_font);
      painter.setPen(TEXT);
      painter.drawText(QRectF(x, y, max_width, 28 * k), Qt::AlignLeft | Qt::AlignVCenter, m);
      x += QFontMetricsF(maker_font).horizontalAdvance(m) + 18 * k;
    }
    if (!desc.isEmpty() && x < left + max_width - 80 * k)
    {
      painter.setFont(desc_font);
      painter.setPen(TEXT_SOFT);
      const qreal w = left + max_width - x;
      painter.drawText(QRectF(x, y, w, 28 * k), Qt::AlignLeft | Qt::AlignVCenter,
                       QFontMetricsF(desc_font).elidedText(desc, Qt::ElideRight, w));
    }
    y += 42 * k;
  }
  else
  {
    y += 8 * k;
  }

  qreal bx = left;
  const auto buttons = HeroButtons(sel);
  for (size_t i = 0; i < buttons.size(); ++i)
  {
    const int b = buttons[i];
    qreal w = 0;
    const qreal h = 68 * k;
    if (b == HERO_RESUME)
    {
      // Main button with the picture of the saved moment
      const QFont f1 = Font(false, int(22 * k), 600);
      const QFont f2 = Font(false, int(14 * k), 500);
      const QDateTime when = QFileInfo(ResumeStatePath(sel.game->GetGameID())).lastModified();
      const QString sub = tr("Sauvegarde auto · %1").arg(QLocale().toString(when, QStringLiteral("d MMM, HH:mm")));
      const qreal tw = std::max(QFontMetricsF(f1).horizontalAdvance(tr("Reprendre")),
                                QFontMetricsF(f2).horizontalAdvance(sub));
      w = 10 * k + 84 * k + 12 * k + 36 * k + 12 * k + tw + 26 * k;
      const QRectF r(bx, y, w, h);
      w = DrawPillButton(painter, bx, y, QString(), QString(), true, 1.0, w);
      const QRectF thumb(bx + 10 * k, y + 10 * k, 84 * k, h - 20 * k);
      painter.save();
      QPainterPath clip;
      clip.addRoundedRect(thumb, 10 * k, 10 * k);
      painter.setClipPath(clip);
      const QPixmap& shot = ResumeShot(sel);
      if (!shot.isNull())
        painter.drawPixmap(thumb, shot, QRectF(shot.rect()));
      else
        painter.fillRect(thumb, SURFACE);
      painter.restore();
      DrawIcon(painter, QStringLiteral("A"),
               QRectF(thumb.right() + 12 * k, y + (h - 36 * k) / 2, 36 * k, 36 * k));
      const qreal tx = thumb.right() + 12 * k + 36 * k + 12 * k;
      painter.setPen(BG);
      painter.setFont(f1);
      painter.drawText(QRectF(tx, y + 8 * k, tw + 4, 30 * k), Qt::AlignVCenter, tr("Reprendre"));
      painter.setFont(f2);
      QColor sub_col = BG;
      sub_col.setAlphaF(0.75f);
      painter.setPen(sub_col);
      painter.drawText(QRectF(tx, y + 36 * k, tw + 4, 22 * k), Qt::AlignVCenter, sub);
      (void)r;
    }
    else
    {
      const bool primary = b == HERO_PLAY;
      const QString icon = b == HERO_PLAY ? QStringLiteral("A") :
                           b == HERO_NEW  ? QStringLiteral("Start-Pause") :
                           b == HERO_SETTINGS ? QStringLiteral("X") :
                                                QStringLiteral("Y");
      const QString label = b == HERO_PLAY     ? tr("Jouer") :
                            b == HERO_NEW      ? tr("Nouvelle partie") :
                            b == HERO_SETTINGS ? tr("Réglages") :
                            m_favorites.contains(sel.path) ? tr("Retirer des favoris") :
                                                             tr("Favori");
      w = DrawPillButton(painter, bx, y, icon, label, primary);
    }
    AddHit(QRectF(bx, y, w, h), [this, i] {
      m_hero_focus = true;
      m_hero_button = static_cast<int>(i);
      HandleAction(Action::Accept);
    });
    if (m_hero_focus && static_cast<int>(i) == m_hero_button)
    {
      painter.setPen(QPen(ACCENT_SOFT, 3 * k));
      painter.setBrush(Qt::NoBrush);
      painter.drawRoundedRect(QRectF(bx, y, w, h).adjusted(-6 * k, -6 * k, 6 * k, 6 * k), 21 * k,
                              21 * k);
    }
    bx += w + 14 * k;
  }
  painter.restore();
}

static void DrawStar(QPainter& painter, const QPointF& c, qreal k)
{
  QPainterPath star;
  for (int p = 0; p < 10; ++p)
  {
    const qreal rad = (p % 2 ? 4.5 : 10) * k;
    const qreal ang = -std::numbers::pi / 2 + p * std::numbers::pi / 5;
    const QPointF pt(c.x() + rad * std::cos(ang), c.y() + rad * std::sin(ang));
    if (p == 0)
      star.moveTo(pt);
    else
      star.lineTo(pt);
  }
  star.closeSubpath();
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(13, 11, 26, 200));
  painter.drawEllipse(c, 16 * k, 16 * k);
  painter.setBrush(WARN);
  painter.drawPath(star);
}

void BigPictureWindow::DrawCoverShelf(QPainter& painter)
{
  const qreal k = height() / 1080.0;
  const qreal left = 80 * k;
  const Entry* sel = Selected();
  if (!sel)
    return;

  const bool resume = HasResume(*sel);
  DrawHero(painter, *sel, left, 216 * k, resume ? 1000 * k : 1150 * k);
  if (resume)
  {
    const qreal chh = 388 * k, cw = chh * 4.0 / 3.0;
    DrawResumeCard(painter, *sel, QRectF(width() - 80 * k - cw, 168 * k, cw, chh));
  }

  // ---- Shelf
  const qreal label_y = 576 * k;
  const QString tab_title = m_tab == Tab::Recent    ? tr("Récemment joués") :
                            m_tab == Tab::Favorites ? tr("Favoris") :
                                                      tr("Tous les jeux");
  const QFont label_font = Font(true, int(28 * k), 600);
  painter.setFont(label_font);
  painter.setPen(TEXT);
  painter.drawText(QRectF(left, label_y, 800 * k, 40 * k), Qt::AlignVCenter, tab_title);
  const qreal lw = QFontMetricsF(label_font).horizontalAdvance(tab_title);
  painter.setFont(Font(false, int(20 * k), 400));
  painter.setPen(TEXT_DIM);
  painter.drawText(QRectF(left + lw + 14 * k, label_y + 3 * k, 800 * k, 40 * k), Qt::AlignVCenter,
                   tr("%1 / %2").arg(m_selected + 1).arg(m_entries.size()));

  // Sort order, on the right
  if (m_tab != Tab::Recent)
  {
    const int sort = ManetteQSettings().value(QStringLiteral("sort"), 0).toInt();
    const QString names[] = {tr("Nom"), tr("Dernière partie"), tr("Temps de jeu")};
    const QString current = names[std::clamp(sort, 0, 2)];
    const QFont bold = Font(false, int(18 * k), 600);
    const qreal bw = QFontMetricsF(bold).horizontalAdvance(current) + 32 * k;
    const qreal right = width() - 80 * k;
    const QRectF chip(right - bw, label_y + 4 * k, bw, 34 * k);
    painter.setPen(QPen(LINE, 1 * k));
    painter.setBrush(SURFACE);
    painter.drawRoundedRect(chip, 14 * k, 14 * k);
    painter.setFont(bold);
    painter.setPen(TEXT);
    painter.drawText(chip, Qt::AlignCenter, current);
    const QFont normal = Font(false, int(18 * k), 500);
    painter.setFont(normal);
    painter.setPen(TEXT_SOFT);
    const QString label = tr("Trier :");
    const qreal lw2 = QFontMetricsF(normal).horizontalAdvance(label);
    painter.drawText(QRectF(chip.left() - 12 * k - lw2, chip.top(), lw2 + 2, chip.height()),
                     Qt::AlignVCenter, label);
    DrawIcon(painter, QStringLiteral("Z"),
             QRectF(chip.left() - 12 * k - lw2 - 46 * k, chip.center().y() - 17 * k, 36 * k,
                    34 * k));
  }

  // N64 boxes are wider than tall
  const qreal cw = 316 * k, ch = 232 * k;
  const qreal extra_w = 40 * k, extra_h = 30 * k;
  const qreal gap = 28 * k;
  const qreal step = cw + gap;
  const qreal bottom = 985 * k;
  const int count = static_cast<int>(m_entries.size());

  painter.save();
  painter.setClipRect(QRectF(0, 600 * k, width(), 400 * k));
  const int first = std::max(0, int(m_scroll * k / step) - 1);
  const int visible = int(width() / step) + 3;
  qreal offset = 0;  // extra width of focused covers before "first"
  for (int i = 0; i < first && i < count; ++i)
    offset += EaseOut(m_focus[i]) * extra_w;

  for (int i = first; i < std::min(count, first + visible); ++i)
  {
    const Entry& e = m_entries[i];
    const qreal f = EaseOut(m_focus[i]);
    const qreal w = cw + extra_w * f;
    const qreal h = ch + extra_h * f;
    const qreal x = left + i * step - m_scroll * k + offset;
    offset += f * extra_w;
    const QRectF r(x, bottom - h, w, h);
    if (r.right() < 0 || r.left() > width())
      continue;
    AddHit(r, [this, i] { ClickEntry(i); });

    // Shadow under the focused cover
    if (f > 0.01)
    {
      QRadialGradient sh(QPointF(r.center().x(), r.bottom()), r.width() * 0.8);
      QColor sc = m_ambient.isValid() ? m_ambient : ACCENT;
      sc.setAlphaF(static_cast<float>(0.35 * f));
      QColor so = sc;
      so.setAlphaF(0.0f);
      sh.setColorAt(0, sc);
      sh.setColorAt(1, so);
      painter.fillRect(r.adjusted(-60 * k, r.height() * 0.5, 60 * k, 40 * k), sh);
    }

    QPainterPath clip;
    clip.addRoundedRect(r, 16 * k, 16 * k);
    painter.setOpacity(0.72 + 0.28 * f);
    const QPixmap cover = CoverFor(e, QSize(int(cw + extra_w), int(ch + extra_h)));
    if (!cover.isNull())
    {
      painter.save();
      painter.setClipPath(clip, Qt::IntersectClip);
      painter.drawPixmap(r, cover, QRectF(cover.rect()));
      painter.restore();
    }
    else
    {
      painter.fillPath(clip, RAISED);
      painter.setFont(Font(true, int(24 * k), 700));
      painter.setPen(TEXT);
      painter.drawText(r.adjusted(20 * k, 20 * k, -20 * k, -20 * k),
                       Qt::AlignLeft | Qt::AlignBottom | Qt::TextWordWrap, e.title);
    }
    painter.setOpacity(1.0);
    if (m_favorites.contains(e.path))
      DrawStar(painter, QPointF(r.right() - 26 * k, r.top() + 26 * k), k);
    {
      // "HD" when a texture pack is installed for this game
      const QString id = QString::fromStdString(e.game->GetGameID());
      auto hd = m_hd_known.find(id);
      if (hd == m_hd_known.end())
        hd = m_hd_known.insert(id, HasTexturePack(*e.game));
      if (hd.value())
      {
        const QRectF chip(r.left() + 10 * k, r.top() + 10 * k, 44 * k, 26 * k);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(10, 8, 20, 200));
        painter.drawRoundedRect(chip, 8 * k, 8 * k);
        painter.setFont(Font(false, int(14 * k), 800));
        painter.setPen(QColor(255, 255, 255));
        painter.drawText(chip, Qt::AlignCenter, QStringLiteral("HD"));
      }
    }
    if (f > 0.01)
    {
      const qreal pulse = 0.75 + 0.25 * std::sin(m_time * 3.2);
      QColor ring = TEXT;
      ring.setAlphaF(static_cast<float>(f * pulse));
      painter.setBrush(Qt::NoBrush);
      painter.setPen(QPen(ring, 4 * k));
      const qreal grow = 9 * k * f;
      painter.drawRoundedRect(r.adjusted(-grow, -grow, grow, grow), 22 * k, 22 * k);
    }
  }
  painter.restore();

  if (m_mode == Mode::Library)
  {
    const bool can_resume = HasResume(*sel);
    DrawHints(painter, {{QStringLiteral("Control_Stick"), tr("Naviguer")}, {QStringLiteral("C-Stick"), tr("Rapide")}},
              {{QStringLiteral("A"), m_hero_focus ? tr("Valider") :
                                     can_resume   ? tr("Reprendre") :
                                                    tr("Jouer")},
               {QStringLiteral("Y"), tr("Favori")},
               {QStringLiteral("Z"), tr("Trier")},
               {QStringLiteral("B"), tr("Quitter")},
               {QStringLiteral("Start-Pause"), tr("Menu")}});
  }
}

void BigPictureWindow::DrawList(QPainter& painter)
{
  const qreal k = height() / 1080.0;
  const Entry* sel = Selected();
  if (!sel)
    return;

  // ---- Left: list of banners
  const qreal left = 80 * k;
  const qreal top = 170 * k;
  const qreal list_w = 1000 * k;
  const qreal row_h = 112 * k;
  const qreal view_h = height() - top - 110 * k;
  const int count = static_cast<int>(m_entries.size());

  painter.save();
  painter.setClipRect(QRectF(0, top - 10 * k, left + list_w + 40 * k, view_h + 20 * k));
  const int first = std::max(0, int(m_list_scroll) - 1);
  const int last = std::min(count, first + int(view_h / row_h) + 3);
  for (int i = first; i < last; ++i)
  {
    const Entry& e = m_entries[i];
    const qreal f = EaseOut(m_focus[i]);
    const qreal y = top + (i - m_list_scroll) * row_h;
    const QRectF r(left + f * 14 * k, y + 6 * k, list_w - f * 14 * k, row_h - 12 * k);
    AddHit(r.intersected(QRectF(0, top - 10 * k, left + list_w + 40 * k, view_h + 20 * k)),
           [this, i] { ClickEntry(i); });

    painter.setPen(Qt::NoPen);
    QColor bg = RAISED;
    bg.setAlphaF(static_cast<float>(0.45 + 0.55 * f));
    painter.setBrush(bg);
    painter.drawRoundedRect(r, 16 * k, 16 * k);

    // Small cover
    const QSizeF bsize(124 * k, 88 * k);
    const QRectF br(r.left() + 10 * k, r.center().y() - bsize.height() / 2, bsize.width(),
                    bsize.height());
    const QPixmap banner = CoverFor(e, bsize.toSize());
    QPainterPath bclip;
    bclip.addRoundedRect(br, 10 * k, 10 * k);
    painter.save();
    painter.setClipPath(bclip, Qt::IntersectClip);
    if (!banner.isNull())
      painter.drawPixmap(br, banner, QRectF(banner.rect()));
    else
      painter.fillRect(br, SURFACE_DEEP);
    painter.restore();

    painter.setOpacity(0.75 + 0.25 * f);
    painter.setFont(Font(true, int(26 * k), 600));
    painter.setPen(TEXT);
    const QFontMetricsF tfm(painter.font());
    const qreal text_x = br.right() + 26 * k;
    painter.drawText(QRectF(text_x, r.top() + 16 * k, r.right() - text_x - 70 * k, 40 * k),
                     Qt::AlignVCenter,
                     tfm.elidedText(e.title, Qt::ElideRight, r.right() - text_x - 70 * k));
    QStringList meta;
    meta << QStringLiteral("Nintendo 64");
    const QString region = RegionName(e.game->region);
    if (!region.isEmpty())
      meta << region;
    if (e.discs.size() > 1)
      meta << tr("%1 disques").arg(e.discs.size());
    painter.setFont(Font(false, int(19 * k), 400));
    painter.setPen(TEXT_SOFT);
    painter.drawText(QRectF(text_x, r.top() + 56 * k, 600 * k, 30 * k), Qt::AlignVCenter,
                     meta.join(QStringLiteral("  ·  ")));
    painter.setOpacity(1);
    if (m_favorites.contains(e.path))
      DrawStar(painter, QPointF(r.right() - 34 * k, r.center().y()), k);

    if (f > 0.01)
    {
      const qreal pulse = 0.75 + 0.25 * std::sin(m_time * 3.2);
      QColor ring = TEXT;
      ring.setAlphaF(static_cast<float>(f * pulse));
      painter.setBrush(Qt::NoBrush);
      painter.setPen(QPen(ring, 3.5 * k));
      painter.drawRoundedRect(r.adjusted(-3 * k, -3 * k, 3 * k, 3 * k), 18 * k, 18 * k);
    }
  }
  painter.restore();

  // Scrollbar
  if (count > 1)
  {
    const qreal track_x = left + list_w + 22 * k;
    painter.setPen(Qt::NoPen);
    painter.setBrush(SURFACE);
    painter.drawRoundedRect(QRectF(track_x, top, 6 * k, view_h), 3 * k, 3 * k);
    const qreal thumb_h = std::max<qreal>(40 * k, view_h * std::min(1.0, (view_h / row_h) / count));
    const qreal thumb_y = top + (view_h - thumb_h) * m_selected / std::max(1, count - 1);
    painter.setBrush(TEXT_DIM);
    painter.drawRoundedRect(QRectF(track_x, thumb_y, 6 * k, thumb_h), 3 * k, 3 * k);
  }

  // ---- Right: details of the selected game (no cover: the list already shows the banners)
  const qreal px = left + list_w + 80 * k;
  const qreal pw = width() - px - 80 * k;
  qreal y = top + 10 * k;
  if (HasResume(*sel))
  {
    // The resume point, like the covers view
    const QPixmap& shot = ResumeShot(*sel);
    const qreal aspect = shot.isNull() ? 16.0 / 9.0 : qreal(shot.width()) / shot.height();
    const qreal ch = std::min<qreal>(pw / aspect, 400 * k);
    const QRectF card(px, y, ch * aspect, ch);
    DrawResumeCard(painter, *sel, card);
    y = card.bottom() + 30 * k;
  }

  const qreal e = EaseOut(m_hero_t);
  painter.save();
  painter.setOpacity(e);
  painter.translate((1 - e) * 22 * k, 0);
  QFont tf = Font(true, int(36 * k), 700);
  painter.setFont(tf);
  painter.setPen(TEXT);
  const QRectF tbox(px, y, pw, 96 * k);
  painter.drawText(tbox, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, sel->title);
  y += std::min<qreal>(QFontMetricsF(tf)
                           .boundingRect(tbox, Qt::TextWordWrap | Qt::AlignLeft, sel->title)
                           .height(),
                       96 * k) +
       10 * k;
  painter.setFont(Font(false, int(19 * k), 400));
  painter.setPen(TEXT_SOFT);
  painter.drawText(QRectF(px, y, pw, 30 * k), Qt::AlignLeft | Qt::AlignVCenter, PlayInfo(*sel));
  y += 46 * k;

  // The game's own short description, as on the console's menu
  const QString maker;
  const QString desc = GameDescription(*sel->game);
  if (!maker.isEmpty())
  {
    painter.setFont(Font(false, int(19 * k), 600));
    painter.setPen(TEXT);
    painter.drawText(QRectF(px, y, pw, 28 * k), Qt::AlignLeft | Qt::AlignVCenter, maker);
    y += 34 * k;
  }
  if (!desc.isEmpty())
  {
    painter.setFont(Font(false, int(20 * k), 400));
    painter.setPen(TEXT_SOFT);
    const QRectF dbox(px, y, pw, std::max<qreal>(0, height() - 130 * k - y));
    painter.drawText(dbox, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, desc);
  }
  painter.restore();

  if (m_mode == Mode::Library)
  {
    DrawHints(painter,
              {{QStringLiteral("Control_Stick-U"), QString()},
               {QStringLiteral("Control_Stick-D"), tr("Naviguer")},
               {QStringLiteral("C-Stick"), tr("Rapide")}},
              {{QStringLiteral("A"), HasResume(*sel) ? tr("Reprendre") : tr("Jouer")},
               {QStringLiteral("Y"), tr("Favori")},
               {QStringLiteral("Z"), tr("Trier")},
               {QStringLiteral("B"), tr("Quitter")},
               {QStringLiteral("Start-Pause"), tr("Menu")}});
  }
}

void BigPictureWindow::DrawEmpty(QPainter& painter)
{
  const qreal k = height() / 1080.0;
  painter.setFont(Font(true, int(44 * k), 700));
  painter.setPen(TEXT);
  QString title, help;
  if (m_tab == Tab::Favorites)
  {
    title = tr("Aucun favori pour l'instant");
    help = tr("Dans la bibliothèque, appuie sur C-haut (Y) sur un jeu pour l'ajouter ici.");
  }
  else if (m_tab == Tab::Recent)
  {
    title = tr("Aucun jeu lancé récemment");
    help = tr("Les jeux que tu lances depuis le mode manette apparaîtront ici.");
  }
  else
  {
    title = tr("Aucun jeu");
    help = tr("Choisis le dossier de tes jeux N64 : Réglages › Bibliothèque › Dossier des jeux.");
  }
  painter.drawText(QRectF(0, height() / 2.0 - 80 * k, width(), 70 * k), Qt::AlignCenter, title);
  painter.setFont(Font(false, int(24 * k), 400));
  painter.setPen(TEXT_SOFT);
  painter.drawText(QRectF(0, height() / 2.0, width(), 50 * k), Qt::AlignCenter, help);
  if (m_mode == Mode::Library)
  {
    DrawHints(painter, {{QStringLiteral("L"), QString()}, {QStringLiteral("R"), tr("Onglets")}},
              {{QStringLiteral("Start-Pause"), tr("Menu")}});
  }
}

// ================================================================== settings screen

void BigPictureWindow::DrawSettings(QPainter& painter, qreal top)
{
  const qreal k = height() / 1080.0;
  const qreal left = 80 * k;
  const qreal cats_w = 420 * k;
  const auto& all_cats = Cats();
  const int ncat = static_cast<int>(all_cats.size());
  m_settings_cat = std::clamp(m_settings_cat, 0, ncat - 1);

  if (m_pause_settings || m_mode == Mode::GameSettings)
  {
    QFont title = Font(true, int(48 * k), 700);
    painter.setFont(title);
    painter.setPen(TEXT);
    const QString text = m_mode == Mode::GameSettings ? m_game_settings_title : tr("Réglages");
    painter.drawText(QRectF(left, top - 110 * k, width() - 2 * left, 70 * k), Qt::AlignVCenter,
                     QFontMetricsF(title).elidedText(text, Qt::ElideRight, width() - 2 * left));
    if (m_mode == Mode::GameSettings)
    {
      painter.setFont(Font(false, int(20 * k), 400));
      painter.setPen(TEXT_SOFT);
      painter.drawText(QRectF(left, top - 46 * k, width(), 30 * k), Qt::AlignVCenter,
                       tr("« Global » suit les réglages généraux de Dolphin."));
    }
  }

  // Categories
  const qreal cat_h = 62 * k;
  for (int i = 0; i < ncat; ++i)
  {
    const QRectF r(left, top + 20 * k + i * (cat_h + 6 * k), cats_w, cat_h);
    const bool active = i == m_settings_cat;
    AddHit(r, [this, i] {
      if (m_settings_cat != i || m_settings_in_items)
        m_audio->Play(Sound::Move);
      m_settings_cat = i;
      m_settings_in_items = false;
      m_settings_item = 0;
      m_settings_scroll = 0;
    });
    if (active)
    {
      painter.setPen(Qt::NoPen);
      painter.setBrush(m_settings_in_items ? RAISED : TEXT);
      painter.drawRoundedRect(r, 14 * k, 14 * k);
    }
    painter.setFont(Font(false, int(23 * k), active ? 600 : 500));
    painter.setPen(active && !m_settings_in_items ? BG : (active ? TEXT : TEXT_SOFT));
    painter.drawText(r.adjusted(24 * k, 0, 0, 0), Qt::AlignVCenter, all_cats[i].name);
  }

  // Items
  const auto& items = all_cats[m_settings_cat].items;
  const int count = static_cast<int>(items.size());
  const qreal ix = left + cats_w + 60 * k;
  const qreal iw = width() - ix - 96 * k;
  const qreal row_h = 74 * k;
  const qreal list_top = top + 20 * k;
  const qreal view_h = height() - list_top - 230 * k;

  painter.save();
  painter.setClipRect(QRectF(ix - 20 * k, list_top - 6 * k, iw + 40 * k, view_h + 12 * k));
  const qreal scroll = m_settings_in_items ? m_settings_scroll : 0;
  for (int i = 0; i < count; ++i)
  {
    const BigPictureSettingItem& item = items[i];
    const qreal y = list_top + (i - scroll) * (row_h + 6 * k);
    if (y + row_h < list_top - 10 * k || y > list_top + view_h)
      continue;
    const QRectF r(ix, y, iw, row_h);
    const bool focused = m_settings_in_items && i == m_settings_item;
    // Value area of a choice: as wide as its text needs (the label gives way), then a
    // smaller font, so values are never cut ("Ubershaders (asynchrone)")
    const QFont label_font = Font(false, int(23 * k), focused ? 600 : 500);
    const qreal label_w = QFontMetricsF(label_font).horizontalAdvance(item.label);
    QString value_text;
    QFont value_font = Font(false, int(22 * k), 600);
    qreal vbox = 300 * k;
    if (item.kind == BigPictureSettingItem::Kind::Choice)
    {
      value_text = item.value ? item.value() : QString();
      const qreal room = std::max<qreal>(300 * k, r.width() - 28 * k - std::min(label_w, r.width() * 0.45) - 56 * k);
      int px = int(22 * k);
      qreal tw = QFontMetricsF(value_font).horizontalAdvance(value_text);
      while (tw + 70 * k > room && px > int(15 * k))
      {
        --px;
        value_font = Font(false, px, 600);
        tw = QFontMetricsF(value_font).horizontalAdvance(value_text);
      }
      vbox = std::clamp<qreal>(tw + 70 * k, 300 * k, room);
      value_text = QFontMetricsF(value_font).elidedText(value_text, Qt::ElideRight, vbox - 60 * k);
    }
    const qreal value_edge = 28 * k + vbox;
    AddHitAt(r.intersected(QRectF(ix - 20 * k, list_top - 6 * k, iw + 40 * k, view_h + 12 * k)),
             [this, i, r, value_edge](QPointF pos) {
               const bool was = m_settings_in_items && m_settings_item == i;
               m_settings_in_items = true;
               m_settings_item = i;
               // The value area on the right: its left half goes back, its right half forward
               if (pos.x() > r.right() - value_edge)
                 HandleAction(pos.x() < r.right() - value_edge / 2 ? Action::Left : Action::Right);
               else if (was)
                 HandleAction(Action::Accept);
               else
                 m_audio->Play(Sound::Move);
             });
    painter.setPen(Qt::NoPen);
    painter.setBrush(focused ? TEXT : SURFACE);
    painter.drawRoundedRect(r, 16 * k, 16 * k);
    const QColor fg = focused ? BG : TEXT;
    painter.setFont(label_font);
    painter.setPen(fg);
    {
      const qreal right_room = item.kind == BigPictureSettingItem::Kind::Choice ?
                                   value_edge + 16 * k :
                                   item.kind == BigPictureSettingItem::Kind::Toggle ? 120 * k :
                                                                                      200 * k;
      const QRectF lr = r.adjusted(28 * k, 0, -right_room, 0);
      painter.drawText(lr, Qt::AlignVCenter,
                       QFontMetricsF(label_font).elidedText(item.label, Qt::ElideRight, lr.width()));
    }

    switch (item.kind)
    {
    case BigPictureSettingItem::Kind::Toggle:
    {
      const bool on = item.is_on && item.is_on();
      const QRectF sw(r.right() - 28 * k - 64 * k, r.center().y() - 18 * k, 64 * k, 36 * k);
      painter.setPen(Qt::NoPen);
      painter.setBrush(on ? ACCENT : (focused ? QColor(200, 196, 220) : LINE));
      painter.drawRoundedRect(sw, 18 * k, 18 * k);
      painter.setBrush(Qt::white);
      const qreal kx = on ? sw.right() - 32 * k : sw.left() + 4 * k;
      painter.drawEllipse(QRectF(kx, sw.top() + 4 * k, 28 * k, 28 * k));
      break;
    }
    case BigPictureSettingItem::Kind::Choice:
    {
      const QRectF vr(r.right() - 28 * k - vbox, r.top(), vbox, r.height());
      painter.setFont(value_font);
      painter.setPen(fg);
      painter.drawText(vr.adjusted(30 * k, 0, -30 * k, 0), Qt::AlignCenter, value_text);
      QPen arrow(fg, 2.5 * k);
      arrow.setCapStyle(Qt::RoundCap);
      arrow.setJoinStyle(Qt::RoundJoin);
      painter.setPen(arrow);
      painter.setBrush(Qt::NoBrush);
      const qreal cy = r.center().y();
      QPolygonF l, rr;
      l << QPointF(vr.left() + 12 * k, cy - 8 * k) << QPointF(vr.left() + 4 * k, cy)
        << QPointF(vr.left() + 12 * k, cy + 8 * k);
      rr << QPointF(vr.right() - 12 * k, cy - 8 * k) << QPointF(vr.right() - 4 * k, cy)
         << QPointF(vr.right() - 12 * k, cy + 8 * k);
      painter.drawPolyline(l);
      painter.drawPolyline(rr);
      break;
    }
    case BigPictureSettingItem::Kind::Action:
    {
      if (item.value)
      {
        painter.setFont(Font(false, int(21 * k), 500));
        painter.setPen(focused ? BG : TEXT_SOFT);
        painter.drawText(QRectF(r.right() - 400 * k, r.top(), 340 * k, r.height()),
                         Qt::AlignVCenter | Qt::AlignRight, item.value());
      }
      QPen arrow(fg, 2.5 * k);
      arrow.setCapStyle(Qt::RoundCap);
      painter.setPen(arrow);
      const qreal cx = r.right() - 36 * k, cy = r.center().y();
      QPolygonF chev;
      chev << QPointF(cx - 6 * k, cy - 10 * k) << QPointF(cx + 4 * k, cy)
           << QPointF(cx - 6 * k, cy + 10 * k);
      painter.drawPolyline(chev);
      break;
    }
    }
  }
  painter.restore();

  // Help for the focused option
  QString help;
  if (m_settings_in_items && m_settings_item < count)
    help = items[m_settings_item].help;
  if (!help.isEmpty())
  {
    painter.setFont(Font(false, int(21 * k), 400));
    painter.setPen(TEXT_SOFT);
    painter.drawText(QRectF(ix, height() - 190 * k, iw, 70 * k),
                     Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignVCenter, help);
  }

  if (m_mode == Mode::Library || m_mode == Mode::Pause || m_mode == Mode::GameSettings)
  {
    if (m_settings_in_items)
    {
      std::vector<std::pair<QString, QString>> right = {
          {QStringLiteral("D-Pad-L"), QString()},
          {QStringLiteral("D-Pad-R"), tr("Modifier")},
          {QStringLiteral("A"), tr("Valider")}};
      if (m_settings_item < count && items[m_settings_item].clear)
        right.push_back({QStringLiteral("X"), tr("Effacer")});
      right.push_back({QStringLiteral("B"), tr("Catégories")});
      DrawHints(painter,
                {{QStringLiteral("D-Pad-U"), QString()}, {QStringLiteral("D-Pad-D"), tr("Choisir")}},
                right);
    }
    else
    {
      DrawHints(painter,
                {{QStringLiteral("D-Pad-U"), QString()},
                 {QStringLiteral("D-Pad-D"), tr("Catégorie")}},
                {{QStringLiteral("A"), tr("Ouvrir")}, {QStringLiteral("B"), tr("Retour")}});
    }
  }
}

// ================================================================== overlays

void BigPictureWindow::DrawMenu(QPainter& painter)
{
  m_hits.clear();  // the menu covers the screen under it
  const qreal k = height() / 1080.0;
  const qreal e = EaseOut(m_overlay_t);
  QColor shade(9, 7, 20);
  shade.setAlphaF(static_cast<float>(0.72 * e));
  painter.fillRect(rect(), shade);

  painter.save();
  painter.translate(-(1 - e) * 120 * k, 0);
  painter.setOpacity(e);
  const QRectF panel(0, 0, 700 * k, height());
  QColor panel_color = SURFACE;
  panel_color.setAlpha(245);
  painter.fillRect(panel, panel_color);

  QFont label = Font(false, int(18 * k), 600);
  label.setLetterSpacing(QFont::AbsoluteSpacing, 1.5 * k);
  painter.setFont(label);
  painter.setPen(ACCENT_SOFT);
  painter.drawText(QRectF(64 * k, 80 * k, 600 * k, 30 * k), Qt::AlignVCenter, tr("MENU"));
  painter.setFont(Font(true, int(48 * k), 700));
  painter.setPen(TEXT);
  painter.drawText(QRectF(64 * k, 116 * k, 600 * k, 64 * k), Qt::AlignVCenter,
                   QStringLiteral("RMG"));

  const QString items[] = {tr("Retour à la bibliothèque"), tr("Réglages"),
                           tr("Galerie de captures"),      tr("Statistiques"),
                           tr("Configurer les manettes"),  tr("Interface classique"),
                           tr("Quitter RMG")};
  constexpr int COUNT = 7;
  qreal y = 205 * k;
  for (int i = 0; i < COUNT; ++i)
  {
    if (i == COUNT - 1)
    {
      painter.fillRect(QRectF(64 * k, y + 2 * k, 572 * k, 1 * k), LINE);
      y += 12 * k;
    }
    // items slide in one after the other
    const qreal ie = EaseOut((m_overlay_t - i * 0.05) / 0.7);
    const QRectF r(64 * k - (1 - ie) * 40 * k, y, 572 * k, 62 * k);
    const bool active = i == m_menu_index;
    AddHit(r, [this, i] {
      m_menu_index = i;
      HandleAction(Action::Accept);
    });
    painter.setOpacity(e * ie);
    if (active)
    {
      painter.setPen(Qt::NoPen);
      painter.setBrush(ACCENT);
      painter.drawRoundedRect(r, 16 * k, 16 * k);
      DrawIcon(painter, QStringLiteral("A"),
               QRectF(r.right() - 24 * k - 36 * k, r.center().y() - 18 * k, 36 * k, 36 * k));
    }
    painter.setFont(Font(false, int(24 * k), active ? 600 : 500));
    painter.setPen(active ? BG : (i == COUNT - 1 ? DANGER : TEXT));
    painter.drawText(r.adjusted(28 * k, 0, -80 * k, 0), Qt::AlignVCenter, items[i]);
    y += 68 * k;
  }
  painter.setOpacity(e);

  const qreal icon = 36 * k;
  const qreal hy = height() - 80 * k - icon;
  painter.setFont(Font(false, int(20 * k), 400));
  painter.setPen(TEXT_SOFT);
  DrawIcon(painter, QStringLiteral("D-Pad-U"), QRectF(64 * k, hy, icon, icon));
  DrawIcon(painter, QStringLiteral("D-Pad-D"), QRectF(64 * k + icon + 4 * k, hy, icon, icon));
  painter.drawText(QRectF(64 * k + 2 * icon + 16 * k, hy, 200 * k, icon), Qt::AlignVCenter,
                   tr("Choisir"));
  DrawIcon(painter, QStringLiteral("A"), QRectF(300 * k, hy, icon, icon));
  painter.drawText(QRectF(300 * k + icon + 10 * k, hy, 200 * k, icon), Qt::AlignVCenter,
                   tr("Valider"));
  DrawIcon(painter, QStringLiteral("B"), QRectF(470 * k, hy, icon, icon));
  painter.drawText(QRectF(470 * k + icon + 10 * k, hy, 200 * k, icon), Qt::AlignVCenter,
                   tr("Retour"));
  painter.restore();
}

void BigPictureWindow::DrawDialog(QPainter& painter, const QString& title,
                                  const std::vector<std::pair<QString, QString>>& hints)
{
  m_hits.clear();
  const qreal k = height() / 1080.0;
  const qreal e = EaseOut(m_overlay_t);
  QColor shade(9, 7, 20);
  shade.setAlphaF(static_cast<float>(0.75 * e));
  painter.fillRect(rect(), shade);

  painter.save();
  const QPointF c = rect().center();
  painter.translate(c);
  const qreal s = 0.92 + 0.08 * e;
  painter.scale(s, s);
  painter.translate(-c);
  painter.setOpacity(e);
  const QRectF box((width() - 760 * k) / 2, (height() - 280 * k) / 2, 760 * k, 280 * k);
  painter.setPen(Qt::NoPen);
  painter.setBrush(SURFACE);
  painter.drawRoundedRect(box, 28 * k, 28 * k);

  painter.setFont(Font(true, int(40 * k), 700));
  painter.setPen(TEXT);
  painter.drawText(QRectF(box.left(), box.top() + 50 * k, box.width(), 70 * k), Qt::AlignCenter,
                   title);

  qreal total = 0;
  for (const auto& [btn, label] : hints)
    total += QFontMetricsF(Font(false, int(22 * k), 500)).horizontalAdvance(label) + 120 * k;
  qreal x = box.center().x() - (total - 16 * k) / 2;
  for (size_t i = 0; i < hints.size(); ++i)
  {
    x += DrawPillButton(painter, x, box.bottom() - 110 * k, hints[i].first, hints[i].second,
                        i == 0) +
         16 * k;
  }
  painter.restore();
}

void BigPictureWindow::DrawPause(QPainter& painter)
{
  m_hits.clear();  // the menu covers the screen under it
  const qreal k = height() / 1080.0;
  const qreal e = EaseOut(m_overlay_t);

  // The game stays visible behind a dark veil.
  QColor shade(9, 7, 20);
  shade.setAlphaF(static_cast<float>(0.72 * e));
  painter.fillRect(rect(), shade);

  if (m_pause_settings)
  {
    QColor deep(13, 11, 26);
    deep.setAlphaF(0.94f);
    painter.fillRect(rect(), deep);
    DrawSettings(painter, 190 * k);
    return;
  }

  painter.save();
  painter.translate(-(1 - e) * 120 * k, 0);
  painter.setOpacity(e);
  painter.fillRect(QRectF(0, 0, 700 * k, height()), QColor(23, 20, 43, 240));

  QFont label = Font(false, int(18 * k), 600);
  label.setLetterSpacing(QFont::AbsoluteSpacing, 1.5 * k);
  painter.setFont(label);
  painter.setPen(ACCENT_SOFT);
  painter.drawText(QRectF(64 * k, 80 * k, 600 * k, 30 * k), Qt::AlignVCenter,
                   tr("JEU EN PAUSE"));
  QFont title = Font(true, int(44 * k), 700);
  title.setLetterSpacing(QFont::AbsoluteSpacing, -1 * k);
  painter.setFont(title);
  painter.setPen(TEXT);
  painter.drawText(QRectF(64 * k, 116 * k, 572 * k, 120 * k),
                   Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop,
                   m_running_entry.title.isEmpty() ? tr("Jeu en cours") : m_running_entry.title);

  const std::vector<int> items = PauseItems();
  qreal y = 280 * k;
  for (int i = 0; i < static_cast<int>(items.size()); ++i)
  {
    QString text;
    switch (items[i])
    {
    case PAUSE_RESUME:
      text = tr("Reprendre");
      break;
    case PAUSE_SAVE:
      text = tr("Sauvegarder l'état");
      break;
    case PAUSE_LOAD:
      text = tr("Charger un état");
      break;
    case PAUSE_SETTINGS:
      text = tr("Tous les réglages");
      break;
    case PAUSE_QUICK:
      text = tr("Réglages rapides");
      break;
    case PAUSE_RESET:
      text = tr("Redémarrer le jeu");
      break;
    case PAUSE_SCREENSHOT:
      text = tr("Capture d'écran");
      break;
    case PAUSE_QUIT:
      text = tr("Quitter le jeu");
      break;
    default:
      break;
    }
    if (items[i] == PAUSE_QUIT)
    {
      painter.fillRect(QRectF(64 * k, y + 2 * k, 572 * k, 1 * k), LINE);
      y += 16 * k;
    }
    const qreal ie = EaseOut((m_overlay_t - i * 0.05) / 0.7);
    const QRectF r(64 * k - (1 - ie) * 40 * k, y, 572 * k, 74 * k);
    const bool active = i == m_pause_index;
    AddHit(r, [this, i] {
      m_pause_index = i;
      HandleAction(Action::Accept);
    });
    painter.setOpacity(e * ie);
    if (active)
    {
      painter.setPen(Qt::NoPen);
      painter.setBrush(TEXT);
      painter.drawRoundedRect(r, 16 * k, 16 * k);
    }
    painter.setFont(Font(false, int(25 * k), active ? 600 : 500));
    painter.setPen(active ? BG : (items[i] == PAUSE_QUIT ? DANGER : TEXT));
    painter.drawText(r.adjusted(28 * k, 0, 0, 0), Qt::AlignVCenter, text);
    if (active && (items[i] == PAUSE_SAVE || items[i] == PAUSE_LOAD))
    {
      DrawIcon(painter, QStringLiteral("D-Pad-L"),
               QRectF(r.right() - 150 * k, r.center().y() - 17 * k, 34 * k, 34 * k));
      painter.drawText(QRectF(r.right() - 114 * k, r.top(), 50 * k, r.height()), Qt::AlignCenter,
                       QString::number(m_slot));
      DrawIcon(painter, QStringLiteral("D-Pad-R"),
               QRectF(r.right() - 62 * k, r.center().y() - 17 * k, 34 * k, 34 * k));
    }
    else if (active)
    {
      DrawIcon(painter, QStringLiteral("A"),
               QRectF(r.right() - 24 * k - 36 * k, r.center().y() - 18 * k, 36 * k, 36 * k));
    }
    y += 80 * k;
  }
  painter.setOpacity(e);
  painter.restore();

  // Save slots
  painter.save();
  painter.setOpacity(e);
  const qreal sx = 780 * k;
  const qreal sw = width() - sx - 80 * k;
  painter.setFont(Font(true, int(30 * k), 600));
  painter.setPen(TEXT);
  painter.drawText(QRectF(sx, 80 * k, sw, 44 * k), Qt::AlignVCenter, tr("États sauvegardés"));
  const qreal gap = 22 * k;
  const qreal cw = (sw - gap) / 2;
  const qreal ch = 150 * k;
  for (int s = 1; s <= SLOT_COUNT; ++s)
  {
    const int col = (s - 1) % 2, row = (s - 1) / 2;
    const qreal ce = EaseOut((m_overlay_t - 0.1 - s * 0.03) / 0.6);
    const QRectF r(sx + col * (cw + gap), 150 * k + row * (ch + gap) + (1 - ce) * 20 * k, cw, ch);
    const bool current = s == m_slot;
    AddHit(r, [this, s] {
      if (m_slot != s)
        m_audio->Play(Sound::Move);
      m_slot = s;
    });
    painter.setOpacity(e * ce);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(23, 20, 43, 235));
    painter.drawRoundedRect(r, 18 * k, 18 * k);
    if (current)
    {
      painter.setBrush(Qt::NoBrush);
      painter.setPen(QPen(TEXT, 3.5 * k));
      painter.drawRoundedRect(r.adjusted(-4 * k, -4 * k, 4 * k, 4 * k), 20 * k, 20 * k);
    }
    u64 when = 0;
    {
      std::filesystem::path state_path;
      if (CoreGetSaveStatePath(s, state_path))
      {
        const QFileInfo info(QString::fromStdString(state_path.string()));
        if (info.exists())
          when = static_cast<u64>(info.lastModified().toSecsSinceEpoch());
      }
    }
    // Thumbnail on the left of the card, with the game's proportions (4:3 or 16:9)
    const QRectF tr_(r.left() + 12 * k, r.top() + 12 * k, (ch - 24 * k) * m_game_aspect,
                     ch - 24 * k);
    QPainterPath tclip;
    tclip.addRoundedRect(tr_, 10 * k, 10 * k);
    painter.save();
    painter.setClipPath(tclip, Qt::IntersectClip);
    if (when != 0 && !m_slot_thumbs[s].isNull())
    {
      // The capture is the game's picture: shown at the game's proportions
      painter.drawPixmap(tr_, m_slot_thumbs[s], QRectF(m_slot_thumbs[s].rect()));
    }
    else
    {
      painter.fillRect(tr_, SURFACE_DEEP);
      painter.setFont(Font(true, int(40 * k), 700));
      painter.setPen(current ? TEXT : TEXT_DIM);
      painter.drawText(tr_, Qt::AlignCenter, QString::number(s));
    }
    painter.restore();
    const qreal text_x = tr_.right() + 18 * k;
    painter.setFont(Font(false, int(22 * k), 600));
    painter.setPen(TEXT);
    painter.drawText(QRectF(text_x, r.top() + 26 * k, r.right() - text_x - 10 * k, 34 * k),
                     Qt::AlignVCenter, tr("Emplacement %1").arg(s));
    painter.setFont(Font(false, int(19 * k), 400));
    painter.setPen(TEXT_SOFT);
    painter.drawText(QRectF(text_x, r.top() + 64 * k, r.right() - text_x - 10 * k, 34 * k),
                     Qt::AlignVCenter,
                     when == 0 ? tr("Libre") :
                                 QDateTime::fromSecsSinceEpoch(static_cast<qint64>(when))
                                     .toString(QStringLiteral("dd/MM/yyyy  HH:mm")));
  }
  painter.restore();

  DrawHints(painter, {}, {{QStringLiteral("A"), tr("Valider")},
                          {QStringLiteral("B"), tr("Reprendre")}});
}

// ================================================================== controller setup









// ================================================================== settings sources

std::vector<BigPictureSettingCategory>& BigPictureWindow::Cats()
{
  if (m_mode == Mode::GameSettings)
    return m_game_settings;
  if (m_pause_settings && m_quick)
    return m_quick_settings;
  return m_settings;
}



// ================================================================== remap one control






// ================================================================== stick calibration





// ================================================================== memory card manager





// ================================================================== controllers






void BigPictureWindow::Notify(const QString& text)
{
  const bool over_game = m_mode == Mode::InGame || !isVisible();
  if (!over_game)
  {
    Toast(text);
    return;
  }
  // Over the game: a small click-through window, top right.
  QScreen* screen = windowHandle() ? windowHandle()->screen() : QGuiApplication::primaryScreen();
  if (!screen)
    return;
  const QRect g = screen->geometry();
  const qreal k = g.height() / 1080.0;
  m_notify_text = text;
  m_notify_t = 0;
  QFont font(m_text_family);
  font.setPixelSize(int(64 * k * 0.34));
  font.setWeight(QFont::DemiBold);
  const int w = int(QFontMetricsF(font).horizontalAdvance(text) + 140 * k);
  const int h = int(80 * k);
  m_notifier->setGeometry(g.right() - w - int(40 * k), g.top() + int(40 * k), w, h);
  m_notifier->show();
  m_notifier->raise();
  if (!m_notify_timer)
  {
    m_notify_timer = new QTimer(this);
    m_notify_timer->setInterval(16);
    connect(m_notify_timer, &QTimer::timeout, this, [this] {
      m_notify_t += 0.016;
      if (m_notify_t >= 3.0)
      {
        m_notify_timer->stop();
        m_notifier->hide();
        return;
      }
      m_notifier->update();
    });
  }
  m_notify_timer->start();
}



// ================================================================== updates


// ================================================================== GameCube intro


// ================================================================== USB driver (no Zadig)



// ================================================================== launch, resume, sorting

std::vector<int> BigPictureWindow::HeroButtons(const Entry& entry) const
{
  if (HasResume(entry))
    return {HERO_RESUME, HERO_NEW, HERO_SETTINGS, HERO_FAVORITE};
  return {HERO_PLAY, HERO_SETTINGS, HERO_FAVORITE};
}

void BigPictureWindow::ActivateHero(int button)
{
  const Entry* sel = Selected();
  if (!sel)
    return;
  switch (button)
  {
  case HERO_RESUME:
    Launch(*sel, 0, true);
    break;
  case HERO_PLAY:
  case HERO_NEW:
    StartNewGame(*sel);
    break;
  case HERO_SETTINGS:
    m_audio->Play(Sound::Confirm);
    BeginTransition(1);
    m_tab = Tab::Settings;
    m_settings_cat = 0;
    for (size_t i = 0; i < m_settings.size(); ++i)
    {
      if (m_settings[i].name == tr("Graphismes"))
        m_settings_cat = static_cast<int>(i);
    }
    m_settings_in_items = false;
    break;
  case HERO_FAVORITE:
    ToggleFavorite();
    break;
  default:
    break;
  }
}

void BigPictureWindow::StartNewGame(const Entry& entry)
{
  // Multi-disc games start on disc 1: the game asks for the next one and Dolphin swaps it.
  Launch(entry, 0, false);
}

void BigPictureWindow::Launch(const Entry& entry, int disc, bool resume)
{
  const std::string id = entry.game->GetGameID();
  if (resume)
  {
    disc = ManetteQSettings()
               .value(QStringLiteral("resume_disc/") + QString::fromStdString(id), 0)
               .toInt();
  }
  disc = std::clamp(disc, 0, std::max(0, static_cast<int>(entry.discs.size()) - 1));
  const QString path = entry.discs.isEmpty() ? entry.path : entry.discs[disc];
  const QString state = resume ? ResumeStatePath(id) : QString();

  m_audio->Play(Sound::Launch);
  m_pending_state = state;
  ManetteQSettings().setValue(LastPlayedKey(entry.path), QDateTime::currentSecsSinceEpoch());
  m_running_entry = entry;
  m_disc = disc;
  m_hero_focus = false;
  setCursor(Qt::BlankCursor);
  m_audio->StopMusic();
  BeginTransition(0);
  m_mode = Mode::Launching;
  m_fade_in = 1;
  // Let the select sound play before the game takes over.
  QTimer::singleShot(350, this, [this, path, state] { emit LaunchRequested(path, state); });
}

QString BigPictureWindow::ResumeStatePath(const std::string& game_id) const
{
  return ManetteDataDir(QStringLiteral("Reprise")) + QStringLiteral("/") +
         QString::fromStdString(game_id) + QStringLiteral(".st");
}

QString BigPictureWindow::ResumeShotPath(const std::string& game_id) const
{
  return ManetteDataDir(QStringLiteral("Reprise")) + QStringLiteral("/") +
         QString::fromStdString(game_id) + QStringLiteral(".png");
}

bool BigPictureWindow::HasResume(const Entry& entry) const
{
  if (!entry.game ||
      !ManetteQSettings().value(QStringLiteral("resume"), true).toBool())
    return false;
  const QString id = QString::fromStdString(entry.game->GetGameID());
  if (id.isEmpty())
    return false;
  auto it = m_resume_known.find(id);
  if (it == m_resume_known.end())
    it = m_resume_known.insert(id, QFileInfo::exists(ResumeStatePath(id.toStdString())));
  return it.value();
}

const QPixmap& BigPictureWindow::ResumeShot(const Entry& entry)
{
  const QString path = ResumeShotPath(entry.game->GetGameID());
  if (path != m_resume_shot_path)
  {
    m_resume_shot_path = path;
    m_resume_shot = QPixmap(path);
  }
  return m_resume_shot;
}

void BigPictureWindow::SaveResume()
{
  if (!m_running_entry.game || !CoreIsEmulationRunning() ||
      !ManetteQSettings().value(QStringLiteral("resume"), true).toBool())
    return;
  const std::string id = m_running_entry.game->GetGameID();
  if (id.empty())
    return;
  // Written by the emulation thread at its next frame: let it run behind the black veil.
  if (CoreIsEmulationPaused())
    CoreResumeEmulation();
  CoreSaveState(std::filesystem::path(ResumeStatePath(id).toStdString()));
  const QString shot = ResumeShotPath(id);
  QFile::remove(shot);
  QFile::copy(PauseShotPath(), shot);
  m_resume_known.clear();
  m_resume_shot_path.clear();
}

void BigPictureWindow::CycleSort()
{
  if (m_tab == Tab::Recent || m_tab == Tab::Settings)
  {
    m_audio->Play(Sound::Error);
    return;
  }
  auto& settings = ManetteQSettings();
  const int sort = (settings.value(QStringLiteral("sort"), 0).toInt() + 1) % 3;
  settings.setValue(QStringLiteral("sort"), sort);
  m_audio->Play(Sound::Toggle);
  BeginTransition(0);
  ApplyTab();
}

void BigPictureWindow::DrawResumeCard(QPainter& painter, const Entry& sel, const QRectF& r)
{
  const qreal k = height() / 1080.0;
  const qreal e = EaseOut(m_hero_t);
  painter.save();
  painter.setOpacity(e);
  QPainterPath clip;
  clip.addRoundedRect(r, 26 * k, 26 * k);

  // shadow
  painter.setPen(Qt::NoPen);
  for (int i = 1; i <= 4; ++i)
  {
    painter.setBrush(QColor(0, 0, 0, 22));
    painter.drawRoundedRect(r.adjusted(-i * 4 * k, -i * 2 * k + 10 * k, i * 4 * k, i * 6 * k),
                            (26 + i * 4) * k, (26 + i * 4) * k);
  }

  painter.setClipPath(clip);
  const QPixmap& shot = ResumeShot(sel);
  if (!shot.isNull())
  {
    // The card has the game's proportions (4:3 or 16:9)
    painter.drawPixmap(r, shot, QRectF(shot.rect()));
  }
  else
  {
    painter.fillRect(r, SURFACE);
  }
  QLinearGradient shade(r.left(), r.bottom() - 180 * k, r.left(), r.bottom());
  shade.setColorAt(0, QColor(0, 0, 0, 0));
  shade.setColorAt(1, QColor(8, 4, 2, 235));
  painter.fillRect(r, shade);
  painter.setClipping(false);
  painter.setPen(QPen(LINE, 1.5 * k));
  painter.setBrush(Qt::NoBrush);
  painter.drawPath(clip);

  const auto tag = [&](const QString& text, bool left, QColor color) {
    const QFont f = Font(false, int(14 * k), 700);
    QFont spaced = f;
    spaced.setLetterSpacing(QFont::AbsoluteSpacing, 1 * k);
    const qreal tw = QFontMetricsF(spaced).horizontalAdvance(text) + 28 * k;
    const QRectF t(left ? r.left() + 20 * k : r.right() - 20 * k - tw, r.top() + 20 * k, tw,
                   34 * k);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(10, 5, 2, 190));
    painter.drawRoundedRect(t, 11 * k, 11 * k);
    painter.setFont(spaced);
    painter.setPen(color);
    painter.drawText(t, Qt::AlignCenter, text);
  };
  tag(tr("●  REPRISE AUTOMATIQUE"), true, QColor(255, 246, 236));

  painter.setFont(Font(true, int(28 * k), 700));
  painter.setPen(QColor(255, 246, 236));
  painter.drawText(QRectF(r.left() + 28 * k, r.bottom() - 92 * k, r.width() - 56 * k, 40 * k),
                   Qt::AlignVCenter, tr("Reprends là où tu t'étais arrêté"));
  const QDateTime when = QFileInfo(ResumeStatePath(sel.game->GetGameID())).lastModified();
  const qint64 days = when.date().daysTo(QDate::currentDate());
  const QString day = days == 0 ? tr("Aujourd'hui") :
                      days == 1 ? tr("Hier") :
                                  QLocale().toString(when.date(), QStringLiteral("d MMMM"));
  painter.setFont(Font(false, int(17 * k), 500));
  painter.setPen(QColor(232, 200, 168));
  painter.drawText(QRectF(r.left() + 28 * k, r.bottom() - 52 * k, r.width() - 56 * k, 28 * k),
                   Qt::AlignVCenter,
                   tr("%1 à %2 · Sauvegarde automatique").arg(day, when.toString(QStringLiteral("HH:mm"))));
  painter.restore();
}










void BigPictureWindow::DrawDpadDot(QPainter& painter, QPointF p, qreal r)
{
  // Bright yellow with a soft glow: readable on the white cross and on the accent colour
  QRadialGradient glow(p, r * 2.4);
  glow.setColorAt(0, QColor(255, 214, 40, 200));
  glow.setColorAt(1, QColor(255, 214, 40, 0));
  painter.setPen(Qt::NoPen);
  painter.setBrush(glow);
  painter.drawEllipse(p, r * 2.4, r * 2.4);
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(255, 214, 40));
  painter.drawEllipse(p, r, r);
  painter.setBrush(QColor(255, 250, 220));
  painter.drawEllipse(p, r * 0.45, r * 0.45);
}


// ================================================================== sleep





// ================================================================== mouse

void BigPictureWindow::AddHit(const QRectF& rect, std::function<void()> click)
{
  m_hits.push_back({rect, [click = std::move(click)](QPointF) { click(); }});
}

void BigPictureWindow::AddHitAt(const QRectF& rect, std::function<void(QPointF)> click)
{
  m_hits.push_back({rect, std::move(click)});
}

void BigPictureWindow::ClickEntry(int index)
{
  if (m_mode != Mode::Library || index < 0 || index >= static_cast<int>(m_entries.size()))
    return;
  m_hero_focus = false;
  if (index == m_selected)
    HandleAction(Action::Accept);
  else
    SetSelected(index);
}

void BigPictureWindow::mouseMoveEvent(QMouseEvent* event)
{
  QWidget::mouseMoveEvent(event);
  if (m_mode == Mode::InGame || m_mode == Mode::Launching)
    return;  // never over a game
  if (cursor().shape() != Qt::ArrowCursor)
    setCursor(Qt::ArrowCursor);
  m_cursor_timer->start();
}

void BigPictureWindow::mousePressEvent(QMouseEvent* event)
{
  if (m_mode == Mode::InGame || m_mode == Mode::Launching || m_mode == Mode::Intro)
    return;
  setCursor(Qt::ArrowCursor);
  m_cursor_timer->start();
  if (event->button() == Qt::RightButton || event->button() == Qt::BackButton)
  {
    // Like Esc (and B): back, cancel, leave a tester
    QKeyEvent esc(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    keyPressEvent(&esc);
    return;
  }
  if (event->button() == Qt::MiddleButton)
  {
    HandleAction(Action::Menu);
    return;
  }
  if (event->button() != Qt::LeftButton)
    return;
  const QPointF pos = event->position();
  for (auto it = m_hits.rbegin(); it != m_hits.rend(); ++it)
  {
    if (it->rect.contains(pos))
    {
      const auto click = it->click;  // the list may be rebuilt by the click
      click(pos);
      update();
      return;
    }
  }
}

void BigPictureWindow::wheelEvent(QWheelEvent* event)
{
  if (m_mode == Mode::InGame || m_mode == Mode::Launching)
    return;
  const QPoint d = event->angleDelta();
  const int delta = std::abs(d.y()) >= std::abs(d.x()) ? d.y() : -d.x();
  m_wheel_accum += delta;
  // The covers shelf scrolls sideways; everything else up and down
  const bool sideways = m_mode == Mode::Library && m_tab != Tab::Settings &&
                        m_view != Prefs::View::List && !m_settings_in_items;
  while (std::abs(m_wheel_accum) >= 120)
  {
    const bool back = m_wheel_accum > 0;
    m_wheel_accum += back ? -120 : 120;
    HandleAction(sideways ? (back ? Action::Left : Action::Right) :
                            (back ? Action::Up : Action::Down));
  }
  event->accept();
}

// ================================================================== fast scrolling

QChar BigPictureWindow::InitialOf(const Entry& e) const
{
  for (const QChar ch : e.title)
  {
    if (ch.isLetter())
      return ch.toUpper().decompositionTag() == QChar::NoDecomposition ?
                 ch.toUpper() :
                 ch.toUpper().decomposition().at(0);
    if (ch.isDigit())
      return QLatin1Char('#');
  }
  return QLatin1Char('#');
}

void BigPictureWindow::JumpLetter(int dir)
{
  const int count = static_cast<int>(m_entries.size());
  const int sort = ManetteQSettings().value(QStringLiteral("sort"), 0).toInt();
  int target = m_selected;
  if (sort == 0)
  {
    // Sorted by name: the first game of the next (or previous) letter
    const QChar cur = InitialOf(m_entries[m_selected]);
    if (dir > 0)
    {
      target = m_selected;
      while (target < count - 1 && InitialOf(m_entries[target]) == cur)
        ++target;
    }
    else
    {
      // Start of this letter; if already there, start of the previous one
      int start = m_selected;
      while (start > 0 && InitialOf(m_entries[start - 1]) == cur)
        --start;
      if (start == m_selected && start > 0)
      {
        const QChar prev = InitialOf(m_entries[start - 1]);
        start -= 1;
        while (start > 0 && InitialOf(m_entries[start - 1]) == prev)
          --start;
      }
      target = start;
    }
  }
  else
  {
    target = std::clamp(m_selected + dir * 10, 0, count - 1);
  }
  if (target == m_selected)
    return;
  m_hero_focus = false;
  SetSelected(target);
  m_jump_letter = sort == 0 ? QString(InitialOf(m_entries[target])) :
                              QStringLiteral("%1").arg(target + 1);
  m_jump_shown = m_clock.elapsed();
}

void BigPictureWindow::DrawJumpLetter(QPainter& painter)
{
  if (m_jump_letter.isEmpty())
    return;
  const qint64 age = m_clock.elapsed() - m_jump_shown;
  if (age > 900)
    return;
  const qreal k = height() / 1080.0;
  const qreal a = age < 600 ? 1.0 : 1.0 - (age - 600) / 300.0;
  painter.save();
  painter.setOpacity(a);
  const QRectF r(width() / 2.0 - 90 * k, height() / 2.0 - 90 * k, 180 * k, 180 * k);
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(10, 8, 20, 210));
  painter.drawRoundedRect(r, 36 * k, 36 * k);
  painter.setFont(Font(true, int(96 * k), 700));
  painter.setPen(TEXT);
  painter.drawText(r, Qt::AlignCenter, m_jump_letter);
  painter.restore();
  update();
}

// ================================================================== controller diagnostic


// ================================================================== screenshot gallery

void BigPictureWindow::OpenGallery()
{
  m_shots.clear();
  m_shot_thumbs.clear();
  m_shot_full_path.clear();
  m_shot_full = {};
  std::vector<std::pair<qint64, QString>> found;
  QDirIterator it(QString::fromStdString(CoreGetScreenshotDirectory().string()),
                  {QStringLiteral("*.png"), QStringLiteral("*.jpg")}, QDir::Files,
                  QDirIterator::Subdirectories);
  while (it.hasNext())
  {
    it.next();
    const QFileInfo info = it.fileInfo();
    if (info.fileName().startsWith(QStringLiteral("bigpicture_")))
      continue;  // the pause menu's background
    found.emplace_back(info.lastModified().toSecsSinceEpoch(), info.filePath());
  }
  std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
  for (auto& [when, path] : found)
    m_shots.push_back(std::move(path));
  m_shot_index = 0;
  m_shot_view = false;
  m_shot_delete_armed = false;
  BeginTransition(0);
  m_mode = Mode::Gallery;
}

QString BigPictureWindow::ShotGameTitle(const QString& path) const
{
  // "<game> 2026-11-23 10-00-00.png" (pause menu) or "<game>-012.png" (mupen64plus)
  QString name = QFileInfo(path).completeBaseName();
  name.remove(QRegularExpression(QStringLiteral("\\s\\d{4}-\\d\\d-\\d\\d.*$")));
  name.remove(QRegularExpression(QStringLiteral("-\\d+$")));
  return CleanTitle(name);
}

void BigPictureWindow::HandleGallery(Action action)
{
  const int count = static_cast<int>(m_shots.size());
  constexpr int COLS = 4;
  const auto move = [&](int delta) {
    if (count == 0)
      return;
    const int next = std::clamp(m_shot_index + delta, 0, count - 1);
    if (next != m_shot_index)
    {
      m_shot_index = next;
      m_shot_delete_armed = false;
      m_audio->Play(Sound::Move);
    }
  };
  switch (action)
  {
  case Action::Left:
    move(-1);
    break;
  case Action::Right:
    move(1);
    break;
  case Action::Up:
    move(m_shot_view ? -1 : -COLS);
    break;
  case Action::Down:
    move(m_shot_view ? 1 : COLS);
    break;
  case Action::Accept:
    if (count > 0 && !m_shot_view)
    {
      m_shot_view = true;
      m_audio->Play(Sound::Confirm);
    }
    break;
  case Action::Options:
  {
    if (count == 0)
      break;
    if (!m_shot_delete_armed)
    {
      m_shot_delete_armed = true;
      m_audio->Play(Sound::MenuOpen);
      Toast(tr("Appuie encore sur X pour supprimer cette capture"));
      break;
    }
    const QString path = m_shots[m_shot_index];
    if (QFile::remove(path))
    {
      m_audio->Play(Sound::Erase);
      Toast(tr("Capture supprimée"));
      m_shots.erase(m_shots.begin() + m_shot_index);
      m_shot_thumbs.erase(path);
      m_shot_index = std::clamp(m_shot_index, 0, std::max(0, int(m_shots.size()) - 1));
      if (m_shots.empty())
        m_shot_view = false;
    }
    else
    {
      m_audio->Play(Sound::Error);
      Toast(tr("Suppression impossible"));
    }
    m_shot_delete_armed = false;
    break;
  }
  case Action::Favorite:
    if (count > 0)
    {
      QDesktopServices::openUrl(
          QUrl::fromLocalFile(QFileInfo(m_shots[m_shot_index]).absolutePath()));
    }
    break;
  case Action::Back:
  case Action::Menu:
    m_audio->Play(Sound::Back);
    m_shot_delete_armed = false;
    if (m_shot_view)
    {
      m_shot_view = false;
      break;
    }
    BeginTransition(0);
    m_mode = Mode::Library;
    break;
  default:
    break;
  }
}

void BigPictureWindow::DrawGallery(QPainter& painter)
{
  const qreal k = height() / 1080.0;
  const int count = static_cast<int>(m_shots.size());
  if (m_shot_view && count > 0)
  {
    const QString& path = m_shots[m_shot_index];
    if (path != m_shot_full_path)
    {
      m_shot_full_path = path;
      m_shot_full = QPixmap(path);
    }
    if (!m_shot_full.isNull())
    {
      const QSize sz = m_shot_full.size().scaled(size(), Qt::KeepAspectRatio);
      const QRectF r((width() - sz.width()) / 2.0, (height() - sz.height()) / 2.0, sz.width(),
                     sz.height());
      painter.setRenderHint(QPainter::SmoothPixmapTransform);
      painter.drawPixmap(r, m_shot_full, m_shot_full.rect());
    }
    // Caption on a shade at the top
    QLinearGradient shade(0, 0, 0, 150 * k);
    shade.setColorAt(0, QColor(0, 0, 0, 200));
    shade.setColorAt(1, QColor(0, 0, 0, 0));
    painter.fillRect(QRectF(0, 0, width(), 150 * k), shade);
    painter.setFont(Font(true, int(32 * k), 700));
    painter.setPen(TEXT);
    painter.drawText(QRectF(80 * k, 30 * k, width() - 160 * k, 48 * k), Qt::AlignVCenter,
                     ShotGameTitle(path));
    painter.setFont(Font(false, int(20 * k), 400));
    painter.setPen(TEXT_SOFT);
    painter.drawText(QRectF(80 * k, 78 * k, width() - 160 * k, 30 * k), Qt::AlignVCenter,
                     tr("%1   ·   %2 / %3")
                         .arg(QLocale().toString(QFileInfo(path).lastModified(),
                                                 QStringLiteral("d MMMM yyyy, HH:mm")))
                         .arg(m_shot_index + 1)
                         .arg(count));
    QLinearGradient bottom(0, height() - 130 * k, 0, height());
    bottom.setColorAt(0, QColor(0, 0, 0, 0));
    bottom.setColorAt(1, QColor(0, 0, 0, 200));
    painter.fillRect(QRectF(0, height() - 130 * k, width(), 130 * k), bottom);
    DrawHints(painter, {{QStringLiteral("D-Pad-L"), tr("Précédente")},
                        {QStringLiteral("D-Pad-R"), tr("Suivante")}},
              {{QStringLiteral("X"), m_shot_delete_armed ? tr("Confirmer") : tr("Supprimer")},
               {QStringLiteral("B"), tr("Retour")}});
    return;
  }

  QFont label = Font(false, int(18 * k), 600);
  label.setLetterSpacing(QFont::AbsoluteSpacing, 1.5 * k);
  painter.setFont(label);
  painter.setPen(ACCENT_SOFT);
  painter.drawText(QRectF(80 * k, 170 * k, 1200 * k, 30 * k), Qt::AlignVCenter,
                   tr("GALERIE DE CAPTURES · %1").arg(count));
  if (count == 0)
  {
    painter.setFont(Font(true, int(36 * k), 700));
    painter.setPen(TEXT);
    painter.drawText(QRectF(0, 400 * k, width(), 60 * k), Qt::AlignCenter,
                     tr("Aucune capture pour l'instant"));
    painter.setFont(Font(false, int(22 * k), 400));
    painter.setPen(TEXT_SOFT);
    painter.drawText(QRectF(0, 470 * k, width(), 40 * k), Qt::AlignCenter,
                     tr("En jeu : menu › Capture d'écran"));
    DrawHints(painter, {}, {{QStringLiteral("B"), tr("Retour")}});
    return;
  }

  constexpr int COLS = 4;
  const qreal gx = 80 * k, gy = 220 * k, gap = 24 * k;
  const qreal cw = (width() - 2 * gx - (COLS - 1) * gap) / COLS;
  const qreal ch = cw * 9 / 16;
  const qreal row_h = ch + 56 * k;
  const int rows = std::max(1, int((height() - gy - 120 * k) / row_h));
  const int sel_row = m_shot_index / COLS;
  const int total_rows = (count + COLS - 1) / COLS;
  const int first_row = std::clamp(sel_row - rows / 2, 0, std::max(0, total_rows - rows));
  int loaded = 0;
  for (int i = first_row * COLS; i < std::min(count, (first_row + rows) * COLS); ++i)
  {
    const int col = i % COLS, row = i / COLS - first_row;
    const QRectF r(gx + col * (cw + gap), gy + row * row_h, cw, ch);
    const QString& path = m_shots[i];
    auto it = m_shot_thumbs.find(path);
    if (it == m_shot_thumbs.end() && loaded < 2)
    {
      // A few thumbnails per frame, the next ones on the following frames
      QImageReader reader(path);
      const QSize full = reader.size();
      if (full.isValid())
        reader.setScaledSize(full.scaled(int(cw), int(ch), Qt::KeepAspectRatioByExpanding));
      it = m_shot_thumbs.emplace(path, QPixmap::fromImage(reader.read())).first;
      ++loaded;
    }
    QPainterPath clip;
    clip.addRoundedRect(r, 14 * k, 14 * k);
    painter.setPen(Qt::NoPen);
    painter.setBrush(SURFACE);
    painter.drawPath(clip);
    if (it != m_shot_thumbs.end() && !it->second.isNull())
    {
      painter.save();
      painter.setClipPath(clip);
      const QPixmap& pm = it->second;
      const QSizeF s = QSizeF(pm.size()).scaled(r.size(), Qt::KeepAspectRatioByExpanding);
      painter.drawPixmap(QRectF(r.center().x() - s.width() / 2, r.center().y() - s.height() / 2,
                                s.width(), s.height()),
                         pm, pm.rect());
      painter.restore();
    }
    const bool active = i == m_shot_index;
    if (active)
    {
      painter.setBrush(Qt::NoBrush);
      painter.setPen(QPen(ACCENT, 5 * k));
      painter.drawRoundedRect(r.adjusted(-4 * k, -4 * k, 4 * k, 4 * k), 17 * k, 17 * k);
    }
    painter.setFont(Font(false, int(18 * k), active ? 600 : 400));
    painter.setPen(active ? TEXT : TEXT_SOFT);
    painter.drawText(QRectF(r.left(), r.bottom() + 8 * k, r.width(), 30 * k), Qt::AlignVCenter,
                     QFontMetricsF(painter.font())
                         .elidedText(ShotGameTitle(path), Qt::ElideRight, r.width()));
    AddHit(r, [this, i] {
      if (m_shot_index == i)
        HandleGallery(Action::Accept);
      else
      {
        m_shot_index = i;
        m_shot_delete_armed = false;
        m_audio->Play(Sound::Move);
      }
    });
  }
  if (loaded > 0)
    update();
  DrawHints(painter, {{QStringLiteral("A"), tr("Voir")}, {QStringLiteral("Y"), tr("Ouvrir le dossier")}},
            {{QStringLiteral("X"), m_shot_delete_armed ? tr("Confirmer") : tr("Supprimer")},
             {QStringLiteral("B"), tr("Retour")}});
}

// ================================================================== statistics

void BigPictureWindow::OpenStats()
{
  TimePlayed played;
  played.Reload();
  const QSettings& settings = ManetteQSettings();
  m_stats_top.clear();
  m_stats_recent.clear();
  m_stats_total_min = 0;
  m_stats_played = 0;
  std::set<std::string> seen;  // multi-disc games share one ID
  for (const Entry& e : m_all)
  {
    if (!e.game)
      continue;
    const std::string id = e.game->GetGameID();
    const qint64 minutes =
        std::chrono::duration_cast<std::chrono::minutes>(played.GetTimePlayed(id)).count();
    const qint64 last = settings.value(LastPlayedKey(e.path), 0).toLongLong();
    if (!seen.insert(id).second)
      continue;
    if (minutes > 0)
      m_stats_top.emplace_back(e, minutes);
    if (last > 0)
      m_stats_recent.emplace_back(e, last);
    if (minutes > 0 || last > 0)
      ++m_stats_played;
    m_stats_total_min += minutes;
  }
  const auto by_value = [](const auto& a, const auto& b) { return a.second > b.second; };
  std::sort(m_stats_top.begin(), m_stats_top.end(), by_value);
  std::sort(m_stats_recent.begin(), m_stats_recent.end(), by_value);
  if (m_stats_top.size() > 7)
    m_stats_top.resize(7);
  if (m_stats_recent.size() > 7)
    m_stats_recent.resize(7);
  BeginTransition(0);
  m_mode = Mode::Stats;
}

void BigPictureWindow::HandleStats(Action action)
{
  if (action == Action::Back || action == Action::Menu || action == Action::Accept)
  {
    m_audio->Play(Sound::Back);
    BeginTransition(0);
    m_mode = Mode::Library;
  }
}

void BigPictureWindow::DrawStats(QPainter& painter)
{
  const qreal k = height() / 1080.0;
  const auto duration = [this](qint64 minutes) {
    if (minutes >= 60)
      return tr("%1 h %2").arg(minutes / 60).arg(minutes % 60, 2, 10, QLatin1Char('0'));
    return tr("%1 min").arg(minutes);
  };
  QFont label = Font(false, int(18 * k), 600);
  label.setLetterSpacing(QFont::AbsoluteSpacing, 1.5 * k);
  painter.setFont(label);
  painter.setPen(ACCENT_SOFT);
  painter.drawText(QRectF(80 * k, 170 * k, 1200 * k, 30 * k), Qt::AlignVCenter,
                   tr("STATISTIQUES"));

  // Three big numbers
  const QString big[][2] = {
      {duration(m_stats_total_min), tr("de jeu au total")},
      {QString::number(m_stats_played), tr("jeux lancés")},
      {QString::number(m_all.size()), tr("jeux dans la bibliothèque")},
  };
  const qreal bw = (width() - 160 * k - 2 * 24 * k) / 3;
  for (int i = 0; i < 3; ++i)
  {
    const QRectF r(80 * k + i * (bw + 24 * k), 215 * k, bw, 130 * k);
    painter.setPen(Qt::NoPen);
    painter.setBrush(SURFACE);
    painter.drawRoundedRect(r, 18 * k, 18 * k);
    painter.setFont(Font(true, int(46 * k), 700));
    painter.setPen(i == 0 ? ACCENT : TEXT);
    painter.drawText(r.adjusted(28 * k, 14 * k, -28 * k, -56 * k), Qt::AlignLeft | Qt::AlignVCenter,
                     big[i][0]);
    painter.setFont(Font(false, int(20 * k), 400));
    painter.setPen(TEXT_SOFT);
    painter.drawText(r.adjusted(28 * k, 74 * k, -28 * k, -16 * k), Qt::AlignLeft | Qt::AlignVCenter,
                     big[i][1]);
  }

  const qreal col_w = (width() - 160 * k - 48 * k) / 2;
  const qreal top = 385 * k, row_h = 74 * k;
  for (int side = 0; side < 2; ++side)
  {
    const qreal x = 80 * k + side * (col_w + 48 * k);
    painter.setFont(label);
    painter.setPen(ACCENT_SOFT);
    painter.drawText(QRectF(x, top, col_w, 30 * k), Qt::AlignVCenter,
                     side == 0 ? tr("LES PLUS JOUÉS") : tr("DERNIÈRES PARTIES"));
    const auto& list = side == 0 ? m_stats_top : m_stats_recent;
    if (list.empty())
    {
      painter.setFont(Font(false, int(22 * k), 400));
      painter.setPen(TEXT_DIM);
      painter.drawText(QRectF(x, top + 44 * k, col_w, 40 * k), Qt::AlignVCenter,
                       tr("Rien pour l'instant"));
      continue;
    }
    const qint64 most = m_stats_top.empty() ? 1 : std::max<qint64>(1, m_stats_top.front().second);
    for (size_t i = 0; i < list.size(); ++i)
    {
      const auto& [entry, value] = list[i];
      const QRectF r(x, top + 44 * k + i * row_h, col_w, row_h - 10 * k);
      if (r.bottom() > height() - 110 * k)
        break;
      painter.setPen(Qt::NoPen);
      painter.setBrush(SURFACE);
      painter.drawRoundedRect(r, 14 * k, 14 * k);
      if (side == 0)
      {
        // Bar: share of the most played game
        QColor bar = ACCENT;
        bar.setAlpha(60);
        painter.setBrush(bar);
        painter.drawRoundedRect(QRectF(r.left(), r.top(), r.width() * value / most, r.height()),
                                14 * k, 14 * k);
      }
      const qreal cover_h = r.height() - 12 * k;
      const QPixmap cover = CoverFor(entry, QSize(int(cover_h * 0.72), int(cover_h)));
      qreal tx = r.left() + 18 * k;
      if (!cover.isNull())
      {
        painter.drawPixmap(QRectF(r.left() + 8 * k, r.top() + 6 * k, cover_h * 0.72, cover_h), cover,
                           cover.rect());
        tx = r.left() + 8 * k + cover_h * 0.72 + 14 * k;
      }
      const QString right = side == 0 ?
                                duration(value) :
                                QLocale().toString(QDateTime::fromSecsSinceEpoch(value),
                                                   QStringLiteral("d MMM, HH:mm"));
      painter.setFont(Font(false, int(20 * k), 500));
      const qreal rw = QFontMetricsF(painter.font()).horizontalAdvance(right) + 24 * k;
      painter.setPen(TEXT_SOFT);
      painter.drawText(QRectF(r.right() - rw, r.top(), rw - 18 * k, r.height()),
                       Qt::AlignVCenter | Qt::AlignRight, right);
      painter.setFont(Font(false, int(21 * k), 600));
      painter.setPen(TEXT);
      painter.drawText(QRectF(tx, r.top(), r.right() - rw - tx, r.height()), Qt::AlignVCenter,
                       QFontMetricsF(painter.font())
                           .elidedText(entry.title, Qt::ElideRight, r.right() - rw - tx));
    }
  }
  DrawHints(painter, {}, {{QStringLiteral("B"), tr("Retour")}});
}

// ================================================================== real Wii Remote pairing




// ================================================================== controller tester

void BigPictureWindow::DrawPadTest(QPainter& painter)
{
  using namespace ManettePad;
  const qreal k = height() / 1080.0;
  const Status st = Read();
  QFont label = Font(false, int(18 * k), 600);
  label.setLetterSpacing(QFont::AbsoluteSpacing, 1.5 * k);
  painter.setFont(label);
  painter.setPen(ACCENT_SOFT);
  painter.drawText(QRectF(0, 70 * k, width(), 30 * k), Qt::AlignCenter, tr("TEST DE LA MANETTE"));
  painter.setFont(Font(true, int(36 * k), 700));
  painter.setPen(TEXT);
  const QString name = ControllerName();
  painter.drawText(QRectF(0, 105 * k, width(), 56 * k), Qt::AlignCenter,
                   name.isEmpty() ? tr("Aucune manette") : name);

  // Buttons: the N64 icon, lit when pressed (as the menus read them)
  struct B
  {
    const char* icon;
    u32 bit;
    QPointF pos;  // at 1080p, around the centre
  };
  const B buttons[] = {
      {"L", BTN_L, {-420, -200}}, {"R", BTN_R, {420, -200}},  {"Z", BTN_Z, {-120, -200}},
      {"Start", BTN_START, {0, -20}}, {"A", BTN_A, {250, 110}},  {"B", BTN_B, {150, 30}},
  };
  const QPointF c(width() / 2.0, height() / 2.0 + 40 * k);
  const qreal size = 110 * k;
  for (const B& b : buttons)
  {
    const bool on = st.buttons & b.bit;
    const QRectF r(c.x() + b.pos.x() * k - size / 2, c.y() + b.pos.y() * k - size / 2, size, size);
    if (on)
    {
      QRadialGradient glow(r.center(), size * 0.8);
      QColor g = ACCENT;
      g.setAlphaF(0.6f);
      glow.setColorAt(0, g);
      g.setAlphaF(0.0f);
      glow.setColorAt(1, g);
      painter.fillRect(r.adjusted(-size * 0.4, -size * 0.4, size * 0.4, size * 0.4), glow);
    }
    painter.setOpacity(on ? 1.0 : 0.45);
    DrawIcon(painter, QString::fromLatin1(b.icon), r);
    painter.setOpacity(1.0);
  }
  // D-pad with the pressed direction
  {
    QString dir;
    if (st.buttons & BTN_UP)
      dir += QLatin1Char('U');
    if (st.buttons & BTN_DOWN)
      dir += QLatin1Char('D');
    if (st.buttons & BTN_LEFT)
      dir += QLatin1Char('L');
    if (st.buttons & BTN_RIGHT)
      dir += QLatin1Char('R');
    const QString icon = dir.isEmpty() ? QStringLiteral("D-Pad") : QStringLiteral("D-Pad-") + dir;
    DrawIcon(painter, m_icons.contains(icon) ? icon : QStringLiteral("D-Pad"),
             QRectF(c.x() - 420 * k - 70 * k, c.y() + 40 * k, 140 * k, 140 * k));
  }
  // Stick: its circle and the live position
  const auto stick = [&](qreal cx, qreal x, qreal y, const QString& text) {
    const qreal rad = 110 * k;
    const QPointF sc(c.x() + cx * k, c.y() + 120 * k);
    painter.setPen(QPen(LINE, 3 * k));
    painter.setBrush(SURFACE);
    painter.drawEllipse(sc, rad, rad);
    painter.setPen(QPen(RAISED, 1.5 * k));
    painter.drawLine(QPointF(sc.x() - rad, sc.y()), QPointF(sc.x() + rad, sc.y()));
    painter.drawLine(QPointF(sc.x(), sc.y() - rad), QPointF(sc.x(), sc.y() + rad));
    painter.setPen(Qt::NoPen);
    painter.setBrush(ACCENT);
    painter.drawEllipse(QPointF(sc.x() + x * rad, sc.y() - y * rad), 16 * k, 16 * k);
    painter.setFont(Font(false, int(18 * k), 500));
    painter.setPen(TEXT_SOFT);
    painter.drawText(QRectF(sc.x() - 150 * k, sc.y() + rad + 12 * k, 300 * k, 30 * k),
                     Qt::AlignCenter,
                     tr("%1  ·  X %2 %  Y %3 %").arg(text).arg(int(x * 100)).arg(int(y * 100)));
  };
  stick(-170, st.x, st.y, tr("Stick"));

  // The four C buttons (N64 controller: its C buttons; other controllers: the right stick)
  {
    const bool n64 = LastUsedIsN64();
    const bool c_up = n64 ? (st.buttons & BTN_Y) != 0 : st.cy > 0.5f;
    const bool c_left = n64 ? (st.buttons & BTN_X) != 0 : st.cx < -0.5f;
    const bool c_right = st.cx > 0.5f;
    const bool c_down = st.cy < -0.5f;
    const struct
    {
      const char* icon;
      bool on;
      QPointF pos;
    } cs[] = {{"C-Up", c_up, {420, -60}},
              {"C-Left", c_left, {340, 20}},
              {"C-Right", c_right, {500, 20}},
              {"C-Down", c_down, {420, 100}}};
    const qreal csize = 92 * k;
    for (const auto& cb : cs)
    {
      const QRectF r(c.x() + cb.pos.x() * k - csize / 2, c.y() + cb.pos.y() * k - csize / 2,
                     csize, csize);
      if (cb.on)
      {
        QRadialGradient glow(r.center(), csize * 0.8);
        QColor g = ACCENT;
        g.setAlphaF(0.6f);
        glow.setColorAt(0, g);
        g.setAlphaF(0.0f);
        glow.setColorAt(1, g);
        painter.fillRect(r.adjusted(-csize * 0.4, -csize * 0.4, csize * 0.4, csize * 0.4), glow);
      }
      painter.setOpacity(cb.on ? 1.0 : 0.45);
      DrawIconRaw(painter, QString::fromLatin1(cb.icon), r);
      painter.setOpacity(1.0);
    }
  }

  painter.setFont(Font(false, int(20 * k), 400));
  painter.setPen(TEXT_SOFT);
  painter.drawText(QRectF(0, height() - 70 * k, width(), 30 * k), Qt::AlignCenter,
                   tr("Les boutons tels que les menus les lisent   ·   Maintiens START ou Échap "
                      "pour quitter"));
  update();  // live
}

void BigPictureWindow::DrawDeviceDiag(QPainter& painter)
{
  const qreal k = height() / 1080.0;
  const auto devices = ManettePad::Devices();
  QFont label = Font(false, int(18 * k), 600);
  label.setLetterSpacing(QFont::AbsoluteSpacing, 1.5 * k);
  painter.setFont(label);
  painter.setPen(ACCENT_SOFT);
  painter.drawText(QRectF(0, 70 * k, width(), 30 * k), Qt::AlignCenter,
                   tr("DIAGNOSTIC DES MANETTES"));
  if (devices.empty())
  {
    painter.setFont(Font(true, int(36 * k), 700));
    painter.setPen(TEXT);
    painter.drawText(QRectF(0, 400 * k, width(), 60 * k), Qt::AlignCenter, tr("Aucune manette"));
    return;
  }
  const int n = static_cast<int>(devices.size());
  m_diag_index = ((m_diag_index % n) + n) % n;
  painter.setFont(Font(true, int(36 * k), 700));
  painter.setPen(TEXT);
  painter.drawText(QRectF(200 * k, 105 * k, width() - 400 * k, 56 * k), Qt::AlignCenter,
                   QString::fromStdString(devices[m_diag_index].name));
  painter.setFont(Font(false, int(18 * k), 400));
  painter.setPen(TEXT_SOFT);
  painter.drawText(QRectF(0, 162 * k, width(), 28 * k), Qt::AlignCenter,
                   tr("%1 / %2%3")
                       .arg(m_diag_index + 1)
                       .arg(n)
                       .arg(devices[m_diag_index].n64 ? tr("   ·   manette N64 reconnue") :
                                                        QString()));
  for (int side = 0; side < 2; ++side)
  {
    const QRectF r(side == 0 ? 120 * k : width() - 200 * k, 100 * k, 80 * k, 80 * k);
    painter.setPen(Qt::NoPen);
    painter.setBrush(RAISED);
    painter.drawEllipse(r);
    painter.setPen(QPen(TEXT, 5 * k, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    const qreal cx = r.center().x(), cy = r.center().y(), d = side == 0 ? -1 : 1;
    painter.drawLine(QPointF(cx - d * 8 * k, cy - 14 * k), QPointF(cx + d * 8 * k, cy));
    painter.drawLine(QPointF(cx + d * 8 * k, cy), QPointF(cx - d * 8 * k, cy + 14 * k));
    AddHit(r, [this, side] {
      m_diag_index += side == 0 ? -1 : 1;
      m_audio->Play(Sound::Move);
    });
  }
  const auto inputs = ManettePad::RawInputs(m_diag_index);
  const int cols = 6;
  const qreal gx = 80 * k, gy = 210 * k, gw = width() - 160 * k;
  const qreal cw = (gw - (cols - 1) * 12 * k) / cols, ch = 54 * k;
  int i = 0;
  for (const auto& [name, value] : inputs)
  {
    const qreal v = std::clamp<qreal>(value, 0, 1);
    const QRectF r(gx + (i % cols) * (cw + 12 * k), gy + (i / cols) * (ch + 10 * k), cw, ch);
    const bool on = v > 0.5;
    painter.setPen(Qt::NoPen);
    painter.setBrush(on ? ACCENT : SURFACE);
    painter.drawRoundedRect(r, 12 * k, 12 * k);
    if (!on && v > 0.02)
    {
      painter.setBrush(RAISED);
      painter.drawRoundedRect(QRectF(r.left(), r.top(), r.width() * v, r.height()), 12 * k, 12 * k);
    }
    painter.setFont(Font(false, int(17 * k), on ? 700 : 500));
    painter.setPen(on ? BG : TEXT);
    painter.drawText(r.adjusted(14 * k, 0, -60 * k, 0), Qt::AlignVCenter,
                     QFontMetricsF(painter.font())
                         .elidedText(QString::fromStdString(name), Qt::ElideRight, r.width() - 74 * k));
    painter.setPen(on ? BG : TEXT_DIM);
    painter.drawText(r.adjusted(0, 0, -14 * k, 0), Qt::AlignVCenter | Qt::AlignRight,
                     QString::number(int(v * 100)));
    ++i;
  }
  painter.setFont(Font(false, int(20 * k), 400));
  painter.setPen(TEXT_SOFT);
  painter.drawText(QRectF(0, height() - 70 * k, width(), 30 * k), Qt::AlignCenter,
                   tr("Sans toucher la manette, rien ne devrait s'allumer   ·   ← → : autre manette   ·   "
                      "Maintiens START ou Échap pour quitter"));
}
