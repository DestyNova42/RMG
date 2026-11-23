/*
 * Rosalie's Mupen GUI - mode manette (fork DestyNova42/RMG)
 *  GPL-3.0, see LICENSE
 */

// Settings of RMG (and of the controller mode itself) as a controller-navigable list.

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <QString>

#include "Manette.hpp"

struct BigPictureSettingItem
{
  enum class Kind
  {
    Toggle,
    Choice,
    Action,
  };

  QString label;
  QString help;
  Kind kind = Kind::Choice;
  std::function<QString()> value;   // Choice: current value, shown between arrows
  std::function<bool()> is_on;      // Toggle: current state
  std::function<void(int)> change;  // Left/Right (-1/+1). Toggles flip.
  std::function<void()> activate;   // A button (actions; toggles flip; choices step forward)
  std::function<void()> clear;      // X button (optional), e.g. unmap a control
};

struct BigPictureSettingCategory
{
  QString name;
  std::vector<BigPictureSettingItem> items;
};

// What the settings need from the controller-mode window.
struct BigPictureSettingsHost
{
  std::function<void(const QString&)> notify;  // short message on screen
  std::function<void()> open_classic;
  std::function<void()> interface_changed;  // a controller-mode preference changed
  std::function<void()> device_diagnostic;
  std::function<void()> test_controller;
  std::function<void()> refresh_library;
  std::function<void()> choose_rom_directory;
  std::function<void()> configure_controllers;  // RMG's own input dialog
  std::function<bool()> in_game;
};

// Controller-mode preferences (Manette.ini).
namespace BigPicturePrefs
{
enum class View
{
  Covers,
  List,
};
enum class Effect
{
  Halo,
  Particles,
  Cubes,
  Waves,
  None,
  Cube3D,
};
// Night, then the colours of the Nintendo 64 models
enum class Theme
{
  Night,
  Charcoal,
  JungleGreen,
  FireOrange,
  GrapePurple,
  IceBlue,
  SmokeBlack,
  WatermelonRed,
  Gold,
  GameColor,
};
enum class IntroStyle
{
  N64,
  Minimal,
};
enum class Transition
{
  Fade,
  Slide,
  Zoom,
  None,
};

View GetView();
Effect GetEffect();
Theme GetTheme();
IntroStyle GetIntroStyle();
bool CheckUpdatesOnStart();
Transition GetTransition();
bool IntroEnabled();
bool MusicEnabled();
int MusicVolume();  // 0-100
int SfxVolume();    // 0-100
bool SwapMenuButtons();
int MenuButton();   // 0 auto, 1 Guide, 2 Select, 3 L3+R3, 4 START+Z
bool ResumeEnabled();
bool StartInControllerMode();
}  // namespace BigPicturePrefs

std::vector<BigPictureSettingCategory> BuildBigPictureSettings(const BigPictureSettingsHost& host);

// Short list for the pause menu.
std::vector<BigPictureSettingCategory> BuildBigPictureQuickSettings();

// Writes changed settings to disk.
void SaveBigPictureSettings();

// Raw access to the mupen64plus configuration (plugin sections such as Video-GLideN64)
int ManetteConfigInt(const char* section, const char* key, int def);
bool ManetteConfigBool(const char* section, const char* key, bool def);
void ManetteConfigSet(const char* section, const char* key, int value);
void ManetteConfigSet(const char* section, const char* key, bool value);
// True when the graphics plugin in use is ParaLLEl-RDP
bool ManetteUsesParallel();
