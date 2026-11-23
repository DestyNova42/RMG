// Mode manette — the Zelda games of the GameCube discs
#include "ManetteZelda.hpp"

#include "BigPictureWindow.h"
#include "Manette.hpp"

#include <RMG-Core/Directories.hpp>
#include <RMG-Core/Plugins.hpp>
#include <RMG-Core/Settings.hpp>

#include <QFile>
#include <QFileInfo>
#include <QSettings>

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace ManetteZelda
{
namespace
{
struct KnownDump
{
  const char* md5;
  Game game;
  bool pal;
};

// GameCube versions (Collector's Edition, Master Quest bonus disc, Zelda Collection)
const KnownDump KNOWN[] = {
    {"2C27B4E000E85FD78DBCA551F1B1C965", Game::Ocarina, true},
    {"CD09029EDCFB7C097AC01986A0F83D3F", Game::Ocarina, false},
    {"33FB7852C180B18EA0B9620B630F413F", Game::Ocarina, false},
    {"0C13E0449A28EA5B925CDB8AF8D29768", Game::Ocarina, false},
    {"1618403427E4344A57833043DB5CE3C3", Game::MasterQuest, true},
    {"DA35577FE54579F6A266931CC75F512D", Game::MasterQuest, false},
    {"69895C5C78442260F6EAFB2506DC482A", Game::MasterQuest, false},
    {"DBE9AF0DB46256E42B5C67902B696549", Game::Majora, true},
    {"AC0751DBC23AB2EC0C3144203ACA0003", Game::Majora, false},
};

std::string Upper(std::string s)
{
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::toupper(c); });
  return s;
}

bool Has(const std::string& text, const char* part)
{
  return Upper(text).find(Upper(part)) != std::string::npos;
}

std::string TrimRight(std::string s)
{
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
    s.pop_back();
  return s;
}

bool PalId(const std::string& id)
{
  // last letter of the cartridge ID: the country
  return id.size() == 4 && std::string("PDFISXYLU").find(id[3]) != std::string::npos;
}

// Same name as the one mupen64plus gives to the save files (main.c, get_save_filename)
std::filesystem::path SaveFile(const ManetteGame& game, const char* ext)
{
  const std::filesystem::path dir = CoreGetSaveDirectory();
  // an older file named after the full database name is used first
  {
    const std::filesystem::path old = dir / (game.good_name + ext);
    std::error_code ec;
    if (std::filesystem::exists(old, ec) && std::filesystem::file_size(old, ec) > 0)
      return old;
  }
  const std::string header = TrimRight(game.internal_name);
  const std::string md5 = game.md5.substr(0, 8);
  std::string name;
  if (CoreSettingsGetIntValue(SettingsID::CoreOverLay_SaveFileNameFormat) == 0)
    name = header;
  else if (game.good_name.find("(unknown rom)") == std::string::npos)
    name = game.good_name.substr(0, 32) + "-" + md5;
  else if (!header.empty())
    name = header + "-" + md5;
  else
    name = "unknown-" + md5;
  for (char& c : name)
  {
    if (std::string(":<>\"/\\|?*").find(c) != std::string::npos)
      c = '_';
  }
  return dir / (name + ext);
}

// The save memories are kept with each 32-bit word byte-swapped in mupen64plus files
qint64 FileOffset(qint64 logical)
{
  return logical ^ 3;
}

bool PatchSave(const std::filesystem::path& path, qint64 size,
               const std::vector<std::pair<qint64, unsigned char>>& bytes,
               const std::vector<qint64>& erased_to_zero)
{
  const QString file = QString::fromStdString(path.string());
  QByteArray data;
  {
    QFile in(file);
    if (in.open(QIODevice::ReadOnly))
      data = in.readAll();
  }
  if (data.size() != size)
  {
    // no save yet: an erased memory, like a new cartridge
    data = QByteArray(static_cast<int>(size), static_cast<char>(0xFF));
  }
  for (const qint64 at : erased_to_zero)
  {
    char& c = data[static_cast<int>(FileOffset(at))];
    if (static_cast<unsigned char>(c) == 0xFF)
      c = 0;
  }
  for (const auto& [at, value] : bytes)
    data[static_cast<int>(FileOffset(at))] = static_cast<char>(value);

  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  QFile out(file);
  if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return false;
  return out.write(data) == data.size();
}

QString LanguageKey(const ManetteGame& game)
{
  return QStringLiteral("zelda_langue/") + QString::fromStdString(game.GetGameID());
}
}  // namespace

Info Identify(const ManetteGame& game)
{
  Info info;
  const std::string md5 = Upper(game.md5);
  for (const KnownDump& k : KNOWN)
  {
    if (md5 == k.md5)
    {
      info.game = k.game;
      info.pal = k.pal;
      info.gamecube = true;
      info.known = true;
      return info;
    }
  }

  const std::string id = game.header_id;
  const bool zelda_id = id.size() == 4 && (id.substr(1, 2) == "ZL" || id.substr(1, 2) == "ZS");
  const bool zelda_name = Has(game.internal_name, "ZELDA");
  if (!zelda_id && !zelda_name)
    return info;

  const bool unknown = Has(game.good_name, "(unknown rom)");
  if (Has(game.good_name, "(GC)"))
  {
    info.gamecube = true;
    info.known = true;
  }
  else if (unknown)
  {
    // not in the database: very likely taken from a GameCube disc
    info.gamecube = true;
  }
  else
  {
    return info;  // a cartridge dump: it has its own language menu
  }

  const std::string all = game.good_name + " " + game.internal_name + " " + game.file;
  if (Has(all, "MAJORA") || id.substr(1, 2) == "ZS")
    info.game = Game::Majora;
  else if (Has(all, "MASTER QUEST") || Has(game.good_name, " URA ") || Has(game.GetFileName(), "MQ"))
    info.game = Game::MasterQuest;
  else
    info.game = Game::Ocarina;
  info.pal = Has(game.good_name, "(E)") || PalId(id);
  return info;
}

bool OffersLanguage(const Info& info)
{
  return info.game != Game::None && info.gamecube && info.pal;
}

QStringList Languages(Game game)
{
  if (game == Game::Majora)
  {
    return {QStringLiteral("English"), QStringLiteral("Deutsch"), QStringLiteral("Français"),
            QStringLiteral("Español")};
  }
  return {QStringLiteral("English"), QStringLiteral("Deutsch"), QStringLiteral("Français")};
}

int LastLanguage(const ManetteGame& game, Game kind)
{
  const int count = static_cast<int>(Languages(kind).size());
  const int index = ManetteQSettings().value(LanguageKey(game), 2).toInt();  // Français
  return std::clamp(index, 0, count - 1);
}

bool ApplyLanguage(const ManetteGame& game, Game kind, int index)
{
  ManetteQSettings().setValue(LanguageKey(game), index);
  if (kind == Game::Majora)
  {
    // Options page of the flash memory (and its backup): id 0xA51D, then the language
    // (1 English, 2 German, 3 French, 4 Spanish)
    const unsigned char lang = static_cast<unsigned char>(index + 1);
    std::vector<std::pair<qint64, unsigned char>> bytes;
    std::vector<qint64> zero;
    for (const qint64 page : {0x18000, 0x1C000})
    {
      bytes.push_back({page + 0, 0xA5});
      bytes.push_back({page + 1, 0x1D});
      bytes.push_back({page + 2, lang});
      zero.push_back(page + 3);  // sound, language menu, Z targeting: defaults
      zero.push_back(page + 4);
      zero.push_back(page + 5);
    }
    return PatchSave(SaveFile(game, ".fla"), 0x20000, bytes, zero);
  }
  // Ocarina of Time / Master Quest: third byte of the SRAM header
  // (0 English, 1 German, 2 French), read by the file select screen
  return PatchSave(SaveFile(game, ".sra"), 0x8000, {{2, static_cast<unsigned char>(index)}}, {});
}

bool AccurateRenderer()
{
  return ManetteQSettings().value(QStringLiteral("zelda_parallel"), false).toBool();
}

void SetAccurateRenderer(bool on)
{
  ManetteQSettings().setValue(QStringLiteral("zelda_parallel"), on);
}

void PrepareLaunch(const ManetteGame& game)
{
  const Info info = Identify(game);
  if (info.game == Game::None || !info.gamecube || game.md5.empty())
    return;
  const std::string section = game.md5;

  // Game settings written by the controller mode (never over the user's own ones)
  const bool ours_settings = CoreSettingsGetBoolValue(section, "ManetteZeldaSettings", false) ||
                             CoreSettingsKeyExists(section, "ManetteZeldaRenderer");
  const bool user_settings =
      CoreSettingsGetBoolValue(SettingsID::Game_OverrideSettings, section) && !ours_settings;
  // - a dump missing from the database would be given a 4 Kbit EEPROM and no save would
  //   work: it gets the memory of the real game (SRAM for Ocarina, Flash for Majora)
  // - Majora's Mask of the Collector's Edition slows down in Clock Town, on the GameCube
  //   too: a faster CPU (counter factor 1) keeps it at full speed, and the sound with it
  const bool majora = info.game == Game::Majora;
  if (!user_settings && (!info.known || majora))
  {
    CoreSettingsSetValue(SettingsID::Game_OverrideSettings, section, true);
    CoreSettingsSetValue(SettingsID::Game_SaveType, section, majora ? 3 : 2);
    CoreSettingsSetValue(SettingsID::Game_DisableExtraMem, section, false);
    CoreSettingsSetValue(SettingsID::Game_TransferPak, section, false);
    CoreSettingsSetValue(SettingsID::Game_CountPerOp, section, majora ? 1 : 2);
    CoreSettingsSetValue(SettingsID::Game_SiDmaDuration, section, 2304);
    CoreSettingsSetValue(section, "ManetteZeldaSettings", true);
  }

  // Renderer: ParaLLEl draws every effect of these games like the console (Lens of Truth,
  // pause screen background, Majora's Mask transitions and pictograph box).
  const bool ours = CoreSettingsGetBoolValue(section, "ManetteZeldaRenderer", false);
  // (an earlier version turned it on by default: back to the user's choice)
  if (!ManetteQSettings().contains(QStringLiteral("zelda_parallel")))
    SetAccurateRenderer(false);
  if (!AccurateRenderer())
  {
    if (ours)
    {
      CoreSettingsSetValue(SettingsID::Game_GFX_Plugin, section, std::string());
      CoreSettingsSetValue(SettingsID::Game_RSP_Plugin, section, std::string());
      CoreSettingsSetValue(section, "ManetteZeldaRenderer", false);
    }
  }
  else
  {
    const std::string own_gfx = CoreSettingsGetStringValue(SettingsID::Game_GFX_Plugin, section);
    if (ours || own_gfx.empty())
    {
      std::string gfx, rsp;
      for (const CorePlugin& p : CoreGetAllPlugins())
      {
        const bool parallel = Has(p.File, "parallel");
        if (parallel && p.Type == CorePluginType::Gfx)
          gfx = p.File;
        else if (parallel && p.Type == CorePluginType::Rsp)
          rsp = p.File;
      }
      if (!gfx.empty() && !rsp.empty())
      {
        CoreSettingsSetValue(SettingsID::Game_GFX_Plugin, section, gfx);
        CoreSettingsSetValue(SettingsID::Game_RSP_Plugin, section, rsp);
        CoreSettingsSetValue(section, "ManetteZeldaRenderer", true);
      }
    }
  }
  CoreSettingsSave();
}
}  // namespace ManetteZelda
