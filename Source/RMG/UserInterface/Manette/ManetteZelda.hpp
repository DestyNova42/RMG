// Mode manette — the Zelda games of the GameCube discs (Collector's Edition, Master Quest)
//
// Their ROMs have no language menu: on the GameCube, the console language was written into
// the game's save memory before it started. The same is done here, in the save file, just
// before the game starts. Nothing in the emulation itself is changed.
#pragma once

#include <QString>
#include <QStringList>

struct ManetteGame;

namespace ManetteZelda
{
enum class Game
{
  None,
  Ocarina,
  MasterQuest,
  Majora,
};

struct Info
{
  Game game = Game::None;
  bool gamecube = false;  // a ROM taken from a GameCube disc
  bool pal = false;       // European version: several languages inside
  bool known = false;     // found in the ROM database
};

Info Identify(const ManetteGame& game);

// The language choice is offered for the European GameCube versions only
bool OffersLanguage(const Info& info);

// Names shown in the menu, in the game's own order
QStringList Languages(Game game);

// Index of the language chosen last time (French the first time)
int LastLanguage(const ManetteGame& game, Game kind);

// Writes the language into the game's save file (created if needed) and remembers it
bool ApplyLanguage(const ManetteGame& game, Game kind, int index);

// Settings for a dump missing from the ROM database (save type, expansion pak) and the
// renderer chosen for these games. Called just before the game starts.
void PrepareLaunch(const ManetteGame& game);

// Option: run these games with ParaLLEl (exact rendering), whatever the global choice
bool AccurateRenderer();
void SetAccurateRenderer(bool on);
}  // namespace ManetteZelda
