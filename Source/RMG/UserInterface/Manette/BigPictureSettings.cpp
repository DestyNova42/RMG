/*
 * Rosalie's Mupen GUI - mode manette (fork DestyNova42/RMG)
 *  GPL-3.0, see LICENSE
 */
#include "BigPictureSettings.h"
#include "ManetteInput.hpp"
#include "ManettePad.hpp"
#include "ManetteZelda.hpp"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <utility>

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>

#include <RMG-Core/Directories.hpp>
#include <RMG-Core/Emulation.hpp>
#include <RMG-Core/Plugins.hpp>
#include <RMG-Core/Settings.hpp>
#include <RMG-Core/Volume.hpp>

using Item = BigPictureSettingItem;

namespace
{
QString Tr(const char* text)
{
  return QCoreApplication::translate("BigPictureSettings", text);
}

QSettings& Q()
{
  return ManetteQSettings();
}

Item Toggle(const QString& label, const QString& help, std::function<bool()> get,
            std::function<void(bool)> set)
{
  Item item;
  item.label = label;
  item.help = help;
  item.kind = Item::Kind::Toggle;
  item.is_on = get;
  item.change = [get, set](int) { set(!get()); };
  item.activate = [get, set] { set(!get()); };
  return item;
}

Item Choice(const QString& label, const QString& help, std::vector<QString> options,
            std::function<int()> get, std::function<void(int)> set)
{
  Item item;
  item.label = label;
  item.help = help;
  item.kind = Item::Kind::Choice;
  auto opts = std::make_shared<std::vector<QString>>(std::move(options));
  item.value = [opts, get] {
    const int i = std::clamp(get(), 0, static_cast<int>(opts->size()) - 1);
    return (*opts)[i];
  };
  item.change = [opts, get, set](int dir) {
    const int n = static_cast<int>(opts->size());
    set((std::clamp(get(), 0, n - 1) + dir + n) % n);
  };
  item.activate = [change = item.change] { change(1); };
  return item;
}

// A choice among values (the index is found from the current value)
template <typename T>
Item ValueChoice(const QString& label, const QString& help,
                 std::vector<std::pair<T, QString>> options, std::function<T()> get,
                 std::function<void(T)> set)
{
  std::vector<QString> names;
  for (const auto& o : options)
    names.push_back(o.second);
  auto opts = std::make_shared<std::vector<std::pair<T, QString>>>(std::move(options));
  return Choice(
      label, help, names,
      [opts, get] {
        const T v = get();
        for (size_t i = 0; i < opts->size(); ++i)
        {
          if ((*opts)[i].first == v)
            return static_cast<int>(i);
        }
        return 0;
      },
      [opts, set](int i) { set((*opts)[i].first); });
}

Item Action(const QString& label, const QString& help, std::function<void()> fn)
{
  Item item;
  item.label = label;
  item.help = help;
  item.kind = Item::Kind::Action;
  item.activate = std::move(fn);
  item.change = [](int) {};
  return item;
}

Item PrefToggle(const QString& label, const QString& help, const QString& key, bool def,
                std::function<void()> changed)
{
  return Toggle(
      label, help, [key, def] { return Q().value(key, def).toBool(); },
      [key, changed](bool v) {
        Q().setValue(key, v);
        if (changed)
          changed();
      });
}

Item PrefChoice(const QString& label, const QString& help, const QString& key,
                std::vector<QString> options, int def, std::function<void()> changed)
{
  return Choice(
      label, help, std::move(options), [key, def] { return Q().value(key, def).toInt(); },
      [key, changed](int v) {
        Q().setValue(key, v);
        if (changed)
          changed();
      });
}

// mupen64plus settings known to RMG
Item CoreToggle(const QString& label, const QString& help, SettingsID id)
{
  return Toggle(
      label, help, [id] { return CoreSettingsGetBoolValue(id); },
      [id](bool v) { CoreSettingsSetValue(id, v); });
}

constexpr const char* PARALLEL = "Video-Parallel";

const QString NEXT_GAME = QStringLiteral("Prend effet au prochain lancement d'un jeu.");

std::vector<CorePlugin> PluginsOf(CorePluginType type)
{
  std::vector<CorePlugin> out;
  for (const CorePlugin& p : CoreGetAllPlugins())
  {
    if (p.Type == type)
      out.push_back(p);
  }
  return out;
}

bool Contains(const std::string& s, const char* what)
{
  std::string lower = s;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return lower.find(what) != std::string::npos;
}
}  // namespace

// ---------------------------------------------------------------- raw configuration

int ManetteConfigInt(const char* section, const char* key, int def)
{
  return CoreSettingsGetIntValue(std::string(section), std::string(key), def);
}

bool ManetteConfigBool(const char* section, const char* key, bool def)
{
  return CoreSettingsGetBoolValue(std::string(section), std::string(key), def);
}

void ManetteConfigSet(const char* section, const char* key, int value)
{
  CoreSettingsSetValue(std::string(section), std::string(key), value);
}

void ManetteConfigSet(const char* section, const char* key, bool value)
{
  CoreSettingsSetValue(std::string(section), std::string(key), value);
}

// GLideN64 keeps its settings in GLideN64.ini (its own Qt settings), group of the profile
static QString GlideIniPath()
{
  return QString::fromStdString((CoreGetUserConfigDirectory() / "GLideN64.ini").string());
}

static void GlideEnsureFile()
{
  const QString path = GlideIniPath();
  if (QFile::exists(path))
    return;
  const QString shared =
      QString::fromStdString((CoreGetSharedDataDirectory() / "GLideN64.ini").string());
  if (QFile::exists(shared) && QFile::copy(shared, path))
  {
    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);
    return;
  }
  QSettings ini(path, QSettings::IniFormat);
  ini.setValue(QStringLiteral("version"), 29);
  ini.setValue(QStringLiteral("profile"), QStringLiteral("User"));
}

int ManetteGlideInt(const char* key, int def)
{
  QSettings ini(GlideIniPath(), QSettings::IniFormat);
  const QString profile = ini.value(QStringLiteral("profile"), QStringLiteral("User")).toString();
  return ini.value(profile + QStringLiteral("/") + QString::fromLatin1(key), def).toInt();
}

QString ManetteGlideString(const char* key)
{
  QSettings ini(GlideIniPath(), QSettings::IniFormat);
  const QString profile = ini.value(QStringLiteral("profile"), QStringLiteral("User")).toString();
  return ini.value(profile + QStringLiteral("/") + QString::fromLatin1(key)).toString();
}

void ManetteGlideSet(const char* key, int value)
{
  GlideEnsureFile();
  QSettings ini(GlideIniPath(), QSettings::IniFormat);
  const QString profile = ini.value(QStringLiteral("profile"), QStringLiteral("User")).toString();
  ini.setValue(profile + QStringLiteral("/") + QString::fromLatin1(key), value);
}

bool ManetteUsesParallel()
{
  return Contains(CoreSettingsGetStringValue(SettingsID::Core_GFX_Plugin), "parallel");
}

// ---------------------------------------------------------------- preferences

namespace BigPicturePrefs
{
static int Pref(const char* key, int def)
{
  return Q().value(QString::fromLatin1(key), def).toInt();
}
View GetView()
{
  return static_cast<View>(std::clamp(Pref("view", 0), 0, 1));
}
Effect GetEffect()
{
  return static_cast<Effect>(std::clamp(Pref("effect", 0), 0, 5));
}
Theme GetTheme()
{
  return static_cast<Theme>(std::clamp(Pref("theme", 0), 0, 9));
}
IntroStyle GetIntroStyle()
{
  return static_cast<IntroStyle>(std::clamp(Pref("intro_style", 0), 0, 1));
}
bool CheckUpdatesOnStart()
{
  return Q().value(QStringLiteral("check_updates"), true).toBool();
}
Transition GetTransition()
{
  return static_cast<Transition>(std::clamp(Pref("transition", 0), 0, 3));
}
bool IntroEnabled()
{
  return Q().value(QStringLiteral("intro"), true).toBool();
}
bool MusicEnabled()
{
  return Q().value(QStringLiteral("music"), true).toBool();
}
int MusicVolume()
{
  return std::clamp(Pref("music_volume", 60), 0, 100);
}
int SfxVolume()
{
  return std::clamp(Pref("sfx_volume", 90), 0, 100);
}
bool SwapMenuButtons()
{
  return Q().value(QStringLiteral("swap_menu_buttons"), false).toBool();
}
int MenuButton()
{
  return std::clamp(Pref("menu_button", 0), 0, 4);
}
bool ResumeEnabled()
{
  return Q().value(QStringLiteral("resume"), true).toBool();
}
bool StartInControllerMode()
{
  return Q().value(QStringLiteral("onstart"), true).toBool();
}
}  // namespace BigPicturePrefs

void SaveBigPictureSettings()
{
  CoreSettingsSave();
  Q().sync();
}

// ---------------------------------------------------------------- graphics items

static void AddGraphics(std::vector<Item>& items)
{
  // Graphics plugin: GLideN64 (fast, HD textures) or ParaLLEl-RDP (exact, Vulkan)
  {
    const auto gfx = PluginsOf(CorePluginType::Gfx);
    std::vector<std::pair<std::string, QString>> opts;
    for (const CorePlugin& p : gfx)
    {
      QString name = QString::fromStdString(p.Name);
      if (Contains(p.File, "gliden64"))
        name = Tr("GLideN64 (rapide, textures HD)");
      else if (Contains(p.File, "parallel"))
        name = Tr("ParaLLEl-RDP (précis, Vulkan)");
      else if (Contains(p.File, "angrylion"))
        name = Tr("Angrylion (logiciel, très lent)");
      else if (Contains(p.File, "softrdp"))
        name = Tr("SoftRDP (logiciel)");
      opts.emplace_back(p.File, name);
    }
    if (!opts.empty())
    {
      items.push_back(ValueChoice<std::string>(
          Tr("Moteur graphique"),
          Tr("GLideN64 : rapide, haute résolution, textures HD. ParaLLEl-RDP : rendu exact "
             "comme la console (Vulkan). ") +
              NEXT_GAME,
          opts, [] { return CoreSettingsGetStringValue(SettingsID::Core_GFX_Plugin); },
          [](std::string file) {
            if ((CoreIsEmulationRunning() || CoreIsEmulationPaused()))
              return;
            CoreSettingsSetValue(SettingsID::Core_GFX_Plugin, file);
            // ParaLLEl-RDP needs the ParaLLEl RSP, GLideN64 the HLE one
            const bool parallel = Contains(file, "parallel");
            for (const CorePlugin& rsp : PluginsOf(CorePluginType::Rsp))
            {
              if ((parallel && Contains(rsp.File, "parallel")) ||
                  (!parallel && Contains(rsp.File, "hle")))
              {
                CoreSettingsSetValue(SettingsID::Core_RSP_Plugin, rsp.File);
              }
            }
            CoreApplyPluginSettings();
          }));
    }
  }

  // Internal resolution
  items.push_back(Toggle(
      Tr("Zelda GameCube : rendu exact"),
      Tr("Ocarina of Time, Master Quest et Majora's Mask des disques GameCube utilisent "
         "ParaLLEl-RDP, quel que soit le moteur choisi : tous les effets s'affichent comme sur "
         "la console. Plus gourmand : le son peut grésiller si le PC ou la console ne suit pas. ") +
          NEXT_GAME,
      [] { return ManetteZelda::AccurateRenderer(); },
      [](bool on) { ManetteZelda::SetAccurateRenderer(on); }));
  items.push_back(ValueChoice<int>(
      Tr("Résolution interne"),
      Tr("Native : l'image de la N64. ×2, ×3… : plus net. « Écran » (GLideN64) suit la "
         "résolution de l'écran. ") +
          NEXT_GAME,
      {{-1, Tr("Écran")}, {1, Tr("Native (240p)")}, {2, Tr("×2 (480p)")}, {3, Tr("×3 (720p)")},
       {4, Tr("×4 (960p)")}, {5, Tr("×5")}, {6, Tr("×6 (1440p)")}, {8, Tr("×8 (4K)")}},
      [] {
        if (ManetteUsesParallel())
          return ManetteConfigInt(PARALLEL, "Upscaling", 1);
        const int f = ManetteGlideInt("frameBufferEmulation/nativeResFactor", 0);
        return f == 0 ? -1 : f;
      },
      [](int v) {
        if (ManetteUsesParallel())
        {
          // ParaLLEl: 1, 2, 4 or 8
          const int p = v <= 1 ? 1 : v <= 2 ? 2 : v <= 4 ? 4 : 8;
          ManetteConfigSet(PARALLEL, "Upscaling", p);
          return;
        }
        ManetteGlideSet("frameBufferEmulation/nativeResFactor", v < 0 ? 0 : v);
      }));

  items.push_back(ValueChoice<int>(
      Tr("Format d'image"),
      Tr("4:3 : comme sur un téléviseur de l'époque. 16:9 : étiré, ou élargi pour les "
         "rares jeux qui le gèrent. ") +
          NEXT_GAME,
      {{1, Tr("4:3")}, {3, Tr("4:3 ajusté")}, {2, Tr("16:9")}, {4, Tr("16:9 ajusté")},
       {0, Tr("Étirer à l'écran")}},
      [] {
        if (ManetteUsesParallel())
          return ManetteConfigBool(PARALLEL, "WidescreenStretch", false) ? 2 : 1;
        return ManetteGlideInt("frameBufferEmulation/aspect", 1);
      },
      [](int v) {
        if (ManetteUsesParallel())
        {
          ManetteConfigSet(PARALLEL, "WidescreenStretch", v == 2 || v == 4 || v == 0);
          return;
        }
        ManetteGlideSet("frameBufferEmulation/aspect", v);
      }));

  items.push_back(Toggle(
      Tr("Mise à l'échelle entière (pixel perfect)"),
      Tr("La hauteur de l'image est un multiple exact des 240 lignes de la N64 : pixels "
         "réguliers, petites bandes noires autour. ") +
          NEXT_GAME,
      [] {
        return ManetteUsesParallel() ? ManetteConfigBool(PARALLEL, "IntegerScaling", false) :
                                       ManetteGlideInt("frameBufferEmulation/integerScaling", 0) != 0;
      },
      [](bool v) {
        ManetteConfigSet(PARALLEL, "IntegerScaling", v);
        ManetteGlideSet("frameBufferEmulation/integerScaling", v ? 1 : 0);
      }));
  items.push_back(Toggle(
      Tr("Synchronisation verticale"), Tr("Supprime les déchirures de l'image. ") + NEXT_GAME,
      [] {
        return ManetteUsesParallel() ? ManetteConfigBool(PARALLEL, "VSync", false) :
                                       (ManetteGlideInt("video/verticalSync", 0) != 0);
      },
      [](bool v) {
        ManetteConfigSet(PARALLEL, "VSync", v);
        ManetteGlideSet("video/verticalSync", v ? 1 : 0);
      }));

  // GLideN64 only
  items.push_back(ValueChoice<int>(
      Tr("Anticrénelage (GLideN64)"), Tr("Lisse les bords des polygones. ") + NEXT_GAME,
      {{0, Tr("Désactivé")}, {2, Tr("MSAA ×2")}, {4, Tr("MSAA ×4")}, {8, Tr("MSAA ×8")},
       {16, Tr("MSAA ×16")}},
      [] { return ManetteGlideInt("video/multisampling", 0); },
      [](int v) { ManetteGlideSet("video/multisampling", v); }));
  items.push_back(Toggle(
      Tr("FXAA (GLideN64)"), Tr("Anticrénelage léger, rapide. ") + NEXT_GAME,
      [] { return (ManetteGlideInt("video/fxaa", 0) != 0); },
      [](bool v) { ManetteGlideSet("video/fxaa", v ? 1 : 0); }));
  items.push_back(ValueChoice<int>(
      Tr("Filtrage anisotrope (GLideN64)"), Tr("Textures plus nettes au loin. ") + NEXT_GAME,
      {{0, Tr("Désactivé")}, {2, Tr("×2")}, {4, Tr("×4")}, {8, Tr("×8")}, {16, Tr("×16")}},
      [] { return ManetteGlideInt("texture/anisotropy", 0); },
      [](int v) { ManetteGlideSet("texture/anisotropy", v); }));
  items.push_back(ValueChoice<int>(
      Tr("Filtrage des textures (GLideN64)"),
      Tr("3 points : le filtrage de la N64. Standard : plus doux. ") + NEXT_GAME,
      {{0, Tr("3 points (N64)")}, {1, Tr("Standard")}},
      [] { return (ManetteGlideInt("texture/bilinearMode", 0) == 1 ? 1 : 0); },
      [](int v) { ManetteGlideSet("texture/bilinearMode", v); }));
  items.push_back(Toggle(
      Tr("Textures HD (GLideN64)"),
      Tr("Utilise les packs de textures HD du dossier hires_texture. ") + NEXT_GAME,
      [] { return (ManetteGlideInt("textureFilter/txHiresEnable", 0) != 0); },
      [](bool v) { ManetteGlideSet("textureFilter/txHiresEnable", v ? 1 : 0); }));
  items.push_back(ValueChoice<int>(
      Tr("Amélioration des textures (GLideN64)"),
      Tr("Agrandit et lisse les textures d'origine. ") + NEXT_GAME,
      {{0, Tr("Aucune")}, {4, Tr("HQ2X")}, {8, Tr("HQ4X")}, {9, Tr("2xBRZ")}, {10, Tr("3xBRZ")},
       {11, Tr("4xBRZ")}, {12, Tr("5xBRZ")}, {13, Tr("6xBRZ")}},
      [] { return ManetteGlideInt("textureFilter/txEnhancementMode", 0); },
      [](int v) { ManetteGlideSet("textureFilter/txEnhancementMode", v); }));
  items.push_back(Toggle(
      Tr("Filtre hybride (GLideN64)"),
      Tr("Agrandissement plus net en haute résolution. ") + NEXT_GAME,
      [] { return (ManetteGlideInt("generalEmulation/enableHybridFilter", 1) != 0); },
      [](bool v) { ManetteGlideSet("generalEmulation/enableHybridFilter", v ? 1 : 0); }));

  // ParaLLEl only
  items.push_back(Toggle(
      Tr("Anticrénelage VI (ParaLLEl)"),
      Tr("Le lissage des bords de la console. ") + NEXT_GAME,
      [] { return ManetteConfigBool(PARALLEL, "VIAA", true); },
      [](bool v) { ManetteConfigSet(PARALLEL, "VIAA", v); }));
  items.push_back(Toggle(
      Tr("Filtre bilinéaire VI (ParaLLEl)"),
      Tr("Désactivé : pixels nets. ") + NEXT_GAME,
      [] { return ManetteConfigBool(PARALLEL, "VIBilerp", true); },
      [](bool v) { ManetteConfigSet(PARALLEL, "VIBilerp", v); }));
  items.push_back(Toggle(
      Tr("Rognage du débordement (ParaLLEl)"),
      Tr("Coupe les bandes noires des bords. ") + NEXT_GAME,
      [] { return ManetteConfigBool(PARALLEL, "CropOverscanEnable", false); },
      [](bool v) { ManetteConfigSet(PARALLEL, "CropOverscanEnable", v); }));
}

// ---------------------------------------------------------------- the list

std::vector<BigPictureSettingCategory> BuildBigPictureSettings(const BigPictureSettingsHost& host)
{
  std::vector<BigPictureSettingCategory> cats;
  const auto changed = host.interface_changed;
  const auto notify = host.notify;

  // ---- Interface
  {
    BigPictureSettingCategory c{Tr("Interface"), {}};
    c.items.push_back(PrefChoice(Tr("Affichage des jeux"),
                                 Tr("Jaquettes en rangée, ou liste avec les jaquettes."),
                                 QStringLiteral("view"), {Tr("Jaquettes"), Tr("Liste")}, 0,
                                 changed));
    c.items.push_back(PrefChoice(Tr("Trier les jeux"), Tr("Aussi avec Z dans la bibliothèque."),
                                 QStringLiteral("sort"),
                                 {Tr("Nom"), Tr("Dernière partie"), Tr("Temps de jeu")}, 0,
                                 changed));
    c.items.push_back(PrefChoice(
        Tr("Thème de couleur"), Tr("Les couleurs des modèles de Nintendo 64."),
        QStringLiteral("theme"),
        {Tr("Nuit"), Tr("Anthracite (classique)"), Tr("Vert jungle"), Tr("Orange feu"),
         Tr("Violet raisin"), Tr("Bleu glacier"), Tr("Noir fumé"), Tr("Rouge pastèque"),
         Tr("Or"), Tr("Couleur du jeu")},
        0, changed));
    c.items.push_back(PrefChoice(Tr("Effet d'arrière-plan"), {}, QStringLiteral("effect"),
                                 {Tr("Halo"), Tr("Particules"), Tr("Cubes"), Tr("Vagues"),
                                  Tr("Aucun"), Tr("Cube 3D")},
                                 0, changed));
    c.items.push_back(PrefToggle(Tr("Inverser A/B et X/Y dans les menus"),
                                 Tr("Uniquement dans les menus, jamais en jeu."),
                                 QStringLiteral("swap_menu_buttons"), false, changed));
    c.items.push_back(PrefChoice(Tr("Transitions"), {}, QStringLiteral("transition"),
                                 {Tr("Fondu"), Tr("Glissement"), Tr("Zoom"), Tr("Aucune")}, 0,
                                 changed));
    c.items.push_back(PrefToggle(Tr("Animation de démarrage"), {}, QStringLiteral("intro"), true,
                                 changed));
    c.items.push_back(PrefChoice(Tr("Style de l'animation"), {}, QStringLiteral("intro_style"),
                                 {Tr("Nintendo 64"), Tr("Minimal")}, 0, changed));
    c.items.push_back(PrefToggle(
        Tr("Musique du menu"),
        Tr("Mets tes fichiers .flac dans le dossier Manette/Sons (nom contenant « main menu »)."),
        QStringLiteral("music"), true, changed));
    c.items.push_back(Choice(
        Tr("Volume de la musique"), {},
        {Tr("0 %"), Tr("10 %"), Tr("20 %"), Tr("30 %"), Tr("40 %"), Tr("50 %"), Tr("60 %"),
         Tr("70 %"), Tr("80 %"), Tr("90 %"), Tr("100 %")},
        [] { return BigPicturePrefs::MusicVolume() / 10; },
        [changed](int v) {
          Q().setValue(QStringLiteral("music_volume"), v * 10);
          if (changed)
            changed();
        }));
    c.items.push_back(Choice(
        Tr("Volume des effets sonores"), {},
        {Tr("0 %"), Tr("10 %"), Tr("20 %"), Tr("30 %"), Tr("40 %"), Tr("50 %"), Tr("60 %"),
         Tr("70 %"), Tr("80 %"), Tr("90 %"), Tr("100 %")},
        [] { return (BigPicturePrefs::SfxVolume() + 5) / 10; },
        [changed](int v) {
          Q().setValue(QStringLiteral("sfx_volume"), v * 10);
          if (changed)
            changed();
        }));
    c.items.push_back(PrefToggle(Tr("Démarrer RMG en mode manette"), {},
                                 QStringLiteral("onstart"), true, nullptr));
    cats.push_back(std::move(c));
  }

  // ---- Library
  {
    BigPictureSettingCategory c{Tr("Bibliothèque"), {}};
    const auto choose = host.choose_rom_directory;
    c.items.push_back(Action(
        Tr("Dossier des jeux"),
        QString::fromStdString(CoreSettingsGetStringValue(SettingsID::RomBrowser_Directory))
                .isEmpty() ?
            Tr("Aucun dossier choisi.") :
            Tr("Actuel : %1").arg(QString::fromStdString(
                CoreSettingsGetStringValue(SettingsID::RomBrowser_Directory))),
        [choose] {
          if (choose)
            choose();
        }));
    const auto refresh = host.refresh_library;
    c.items.push_back(Toggle(
        Tr("Chercher aussi dans les sous-dossiers"), {},
        [] { return CoreSettingsGetBoolValue(SettingsID::RomBrowser_Recursive); },
        [refresh](bool v) {
          CoreSettingsSetValue(SettingsID::RomBrowser_Recursive, v);
          if (refresh)
            refresh();
        }));
    c.items.push_back(Action(Tr("Rafraîchir la bibliothèque"), {}, [refresh] {
      if (refresh)
        refresh();
    }));
    c.items.push_back(PrefToggle(Tr("Reprendre"),
                                 Tr("En quittant un jeu, une sauvegarde automatique permet de "
                                    "reprendre exactement où tu en étais."),
                                 QStringLiteral("resume"), true, nullptr));
    c.items.push_back(PrefToggle(
        Tr("Télécharger les jaquettes"),
        Tr("Depuis libretro-thumbnails, pour les jeux qui n'en ont pas."),
        QStringLiteral("download_covers"), true, refresh));
    cats.push_back(std::move(c));
  }

  // ---- Controllers
  {
    BigPictureSettingCategory c{Tr("Manettes"), {}};
    auto port = std::make_shared<int>(0);
    c.items.push_back(Choice(Tr("Joueur"), Tr("Les réglages ci-dessous s'appliquent à ce joueur."),
                             {Tr("Joueur 1"), Tr("Joueur 2"), Tr("Joueur 3"), Tr("Joueur 4")},
                             [port] { return *port; }, [port](int v) { *port = v; }));
    c.items.push_back(Toggle(
        Tr("Manette branchée"), Tr("Le joueur 1 utilise la 1re manette connectée, le joueur 2 la 2e…"),
        [port] { return ManetteInput::PluggedIn(*port); },
        [port](bool v) {
          ManetteInput::SetPluggedIn(*port, v);
          ManetteInput::Save();
        }));
    const auto remap = host.remap;
    c.items.push_back(Action(Tr("Configurer les touches"),
                             Tr("Chaque touche N64, une par une. Plusieurs boutons peuvent faire "
                                "la même touche (par exemple R et ZR pour R)."),
                             [remap, port] {
                               if (remap)
                                 remap(*port);
                             }));
    c.items.push_back(Action(
        Tr("Réglage automatique"),
        Tr("Remet les touches habituelles pour la manette de ce joueur (manette N64 Switch Online "
           "reconnue)."),
        [port, notify] {
          const auto devices = ManettePad::Devices();
          const bool n64 = *port < static_cast<int>(devices.size()) && devices[*port].n64;
          ManetteInput::ApplyDefault(*port, n64);
          ManetteInput::Save();
          if (notify)
            notify(n64 ? Tr("Touches de la manette N64 appliquées") :
                         Tr("Touches habituelles appliquées"));
        }));
    c.items.push_back(Choice(
        Tr("Accessoire"),
        Tr("Controller Pak : sauvegardes de certains jeux. Rumble Pak : vibrations."),
        {Tr("Controller Pak"), Tr("Rumble Pak"), Tr("Transfer Pak"), Tr("Aucun")},
        [port] { return std::clamp(ManetteInput::Pak(*port), 0, 3); },
        [port](int v) {
          ManetteInput::SetPak(*port, v);
          ManetteInput::Save();
        }));
    c.items.push_back(Choice(
        Tr("Zone morte du stick"), {},
        {Tr("0 %"), Tr("3 %"), Tr("6 %"), Tr("9 %"), Tr("12 %"), Tr("15 %"), Tr("20 %"),
         Tr("25 %"), Tr("30 %")},
        [port] {
          static const int v[] = {0, 3, 6, 9, 12, 15, 20, 25, 30};
          const int d = ManetteInput::Deadzone(*port);
          int best = 0;
          for (int i = 0; i < 9; ++i)
            if (std::abs(v[i] - d) < std::abs(v[best] - d))
              best = i;
          return best;
        },
        [port](int i) {
          static const int v[] = {0, 3, 6, 9, 12, 15, 20, 25, 30};
          ManetteInput::SetDeadzone(*port, v[i]);
          ManetteInput::Save();
        }));
    c.items.push_back(Choice(
        Tr("Sensibilité du stick"), Tr("100 % : course complète du stick N64."),
        {Tr("70 %"), Tr("80 %"), Tr("90 %"), Tr("100 %"), Tr("110 %"), Tr("120 %"), Tr("130 %")},
        [port] { return std::clamp((ManetteInput::Sensitivity(*port) - 70 + 5) / 10, 0, 6); },
        [port](int i) {
          ManetteInput::SetSensitivity(*port, 70 + i * 10);
          ManetteInput::Save();
        }));
    c.items.push_back(PrefChoice(
        Tr("Bouton du menu en jeu"),
        Tr("Sur SteamOS / Bazzite, le bouton Guide ouvre le menu Steam : choisis Select (View), "
           "les deux sticks enfoncés ou START + Z."),
        QStringLiteral("menu_button"),
        {Tr("Automatique"), Tr("Guide / Home"), Tr("Select (View)"),
         Tr("Les deux sticks (L3 + R3)"), Tr("START + Z")},
        0, nullptr));
    const auto test = host.test_controller;
    c.items.push_back(Action(Tr("Tester la manette"),
                             Tr("Vue en direct des boutons et des sticks. Maintiens START pour "
                                "quitter."),
                             [test] {
                               if (test)
                                 test();
                             }));
    const auto diag = host.device_diagnostic;
    c.items.push_back(Action(Tr("Diagnostic des manettes"),
                             Tr("Toutes les entrées de chaque manette, en direct."), [diag] {
                               if (diag)
                                 diag();
                             }));
    cats.push_back(std::move(c));
  }

  // ---- Graphics
  {
    BigPictureSettingCategory c{Tr("Graphismes"), {}};
    AddGraphics(c.items);
    c.items.push_back(CoreToggle(Tr("Messages à l'écran"),
                                 Tr("Sauvegardes, vitesse… affichés par-dessus le jeu."),
                                 SettingsID::GUI_OnScreenDisplayEnabled));
    cats.push_back(std::move(c));
  }

  // ---- Sound
  {
    BigPictureSettingCategory c{Tr("Son"), {}};
    c.items.push_back(Choice(
        Tr("Volume du jeu"), {},
        {Tr("0 %"), Tr("10 %"), Tr("20 %"), Tr("30 %"), Tr("40 %"), Tr("50 %"), Tr("60 %"),
         Tr("70 %"), Tr("80 %"), Tr("90 %"), Tr("100 %")},
        [] { return (CoreSettingsGetIntValue(SettingsID::Audio_Volume) + 5) / 10; },
        [](int v) {
          CoreSettingsSetValue(SettingsID::Audio_Volume, v * 10);
          if ((CoreIsEmulationRunning() || CoreIsEmulationPaused()))
            CoreSetVolume(v * 10);
        }));
    c.items.push_back(CoreToggle(Tr("Couper le son du jeu"), {}, SettingsID::Audio_Muted));
    c.items.push_back(CoreToggle(Tr("Synchroniser le son"),
                                 Tr("Évite les craquements, au prix d'un peu de latence."),
                                 SettingsID::Audio_Synchronize));
    c.items.push_back(CoreToggle(Tr("Inverser gauche et droite"), {},
                                 SettingsID::Audio_SwapChannels));
    cats.push_back(std::move(c));
  }

  // ---- System
  {
    BigPictureSettingCategory c{Tr("Système"), {}};
    c.items.push_back(CoreToggle(Tr("Pause quand la fenêtre perd le focus"), {},
                                 SettingsID::GUI_PauseEmulationOnFocusLoss));
    c.items.push_back(CoreToggle(Tr("Reprendre au retour du focus"), {},
                                 SettingsID::GUI_ResumeEmulationOnFocus));
    const auto open_classic = host.open_classic;
    c.items.push_back(Action(Tr("Interface classique"),
                             Tr("L'interface d'origine de RMG, à la souris."), [open_classic] {
                               if (open_classic)
                                 open_classic();
                             }));
    {
      auto armed = std::make_shared<qint64>(0);
      c.items.push_back(Action(
          Tr("Remettre les réglages du mode manette par défaut"),
          Tr("Appuie deux fois sur A. Les favoris, le temps de jeu et les reprises sont "
             "conservés."),
          [armed, notify, changed] {
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            if (now - *armed > 4000)
            {
              *armed = now;
              if (notify)
                notify(Tr("Appuie encore sur A pour tout réinitialiser"));
              return;
            }
            *armed = 0;
            for (const char* key :
                 {"view", "sort", "theme", "effect", "swap_menu_buttons", "transition", "intro",
                  "intro_style", "music", "music_volume", "sfx_volume", "onstart", "resume",
                  "download_covers", "menu_button"})
            {
              Q().remove(QString::fromLatin1(key));
            }
            if (changed)
              changed();
            if (notify)
              notify(Tr("Réglages par défaut rétablis"));
          }));
    }
    cats.push_back(std::move(c));
  }

  // ---- Advanced
  {
    BigPictureSettingCategory c{Tr("Avancé"), {}};
    c.items.push_back(ValueChoice<int>(
        Tr("Processeur émulé"),
        Tr("Recompilateur dynamique : rapide (recommandé). Interpréteurs : plus lents, pour "
           "déboguer. ") +
            NEXT_GAME,
        {{2, Tr("Recompilateur dynamique")}, {1, Tr("Interpréteur avec cache")},
         {0, Tr("Interpréteur pur")}},
        [] { return CoreSettingsGetIntValue(SettingsID::Core_CPU_Emulator); },
        [](int v) { CoreSettingsSetValue(SettingsID::Core_CPU_Emulator, v); }));
    {
      const auto rsp = PluginsOf(CorePluginType::Rsp);
      std::vector<std::pair<std::string, QString>> opts;
      for (const CorePlugin& p : rsp)
        opts.emplace_back(p.File, QString::fromStdString(p.Name));
      if (!opts.empty())
      {
        c.items.push_back(ValueChoice<std::string>(
            Tr("Processeur graphique (RSP)"),
            Tr("Choisi automatiquement avec le moteur graphique. ") + NEXT_GAME, opts,
            [] { return CoreSettingsGetStringValue(SettingsID::Core_RSP_Plugin); },
            [](std::string file) {
              if ((CoreIsEmulationRunning() || CoreIsEmulationPaused()))
                return;
              CoreSettingsSetValue(SettingsID::Core_RSP_Plugin, file);
              CoreApplyPluginSettings();
            }));
      }
    }
    c.items.push_back(CoreToggle(Tr("Interruptions aléatoires"),
                                 Tr("Comme la console. Désactivé pour certains speedruns."),
                                 SettingsID::Core_RandomizeInterrupt));
    cats.push_back(std::move(c));
  }
  return cats;
}

std::vector<BigPictureSettingCategory> BuildBigPictureQuickSettings()
{
  BigPictureSettingCategory c{Tr("Réglages rapides"), {}};
  c.items.push_back(Choice(
      Tr("Volume du jeu"), {},
      {Tr("0 %"), Tr("10 %"), Tr("20 %"), Tr("30 %"), Tr("40 %"), Tr("50 %"), Tr("60 %"),
       Tr("70 %"), Tr("80 %"), Tr("90 %"), Tr("100 %")},
      [] { return (CoreGetVolume() + 5) / 10; },
      [](int v) {
        CoreSetVolume(v * 10);
        CoreSettingsSetValue(SettingsID::Audio_Volume, v * 10);
      }));
  c.items.push_back(CoreToggle(Tr("Messages à l'écran"), {},
                               SettingsID::GUI_OnScreenDisplayEnabled));
  std::vector<BigPictureSettingCategory> cats;
  cats.push_back(std::move(c));
  // Graphics: shown for the next game
  BigPictureSettingCategory g{Tr("Graphismes"), {}};
  AddGraphics(g.items);
  g.items.erase(g.items.begin());  // not the plugin while a game runs
  cats.push_back(std::move(g));
  return cats;
}
