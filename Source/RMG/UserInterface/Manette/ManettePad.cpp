/*
 * Rosalie's Mupen GUI - mode manette (fork DestyNova42/RMG)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 */
#include "ManettePad.hpp"

#include <algorithm>
#include <cmath>
#include <map>

#include <SDL3/SDL.h>

#include <RMG-Core/Directories.hpp>

#include <QDir>
#include <QStandardPaths>

namespace
{
struct Open
{
  SDL_Gamepad* pad = nullptr;
  std::string name;
  bool n64 = false;
  u32 last_buttons = 0;
};

std::map<SDL_JoystickID, Open> s_pads;
bool s_init = false;
std::string s_last_name;
bool s_last_n64 = false;
Uint64 s_last_scan = 0;

bool IsN64(const std::string& name)
{
  std::string lower = name;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return lower.find("n64") != std::string::npos;
}

void Scan()
{
  const Uint64 now = SDL_GetTicks();
  if (s_last_scan != 0 && now - s_last_scan < 500)
    return;
  s_last_scan = now;
  int count = 0;
  SDL_JoystickID* ids = SDL_GetGamepads(&count);
  std::map<SDL_JoystickID, bool> present;
  for (int i = 0; i < count; ++i)
  {
    present[ids[i]] = true;
    if (s_pads.count(ids[i]))
      continue;
    SDL_Gamepad* pad = SDL_OpenGamepad(ids[i]);
    if (!pad)
      continue;
    Open o;
    o.pad = pad;
    const char* name = SDL_GetGamepadName(pad);
    o.name = name ? name : "Manette";
    o.n64 = IsN64(o.name);
    s_pads[ids[i]] = o;
  }
  SDL_free(ids);
  for (auto it = s_pads.begin(); it != s_pads.end();)
  {
    if (!present.count(it->first))
    {
      SDL_CloseGamepad(it->second.pad);
      it = s_pads.erase(it);
    }
    else
    {
      ++it;
    }
  }
}

// Which physical button carries this letter (Xbox: A south; Nintendo: A east…)
SDL_GamepadButton ButtonWithLabel(SDL_Gamepad* pad, SDL_GamepadButtonLabel label,
                                  SDL_GamepadButton fallback)
{
  for (SDL_GamepadButton b : {SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST,
                              SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH})
  {
    if (SDL_GetGamepadButtonLabel(pad, b) == label)
      return b;
  }
  return fallback;
}

float Axis(SDL_Gamepad* pad, SDL_GamepadAxis axis)
{
  return std::clamp(SDL_GetGamepadAxis(pad, axis) / 32767.0f, -1.0f, 1.0f);
}

ManettePad::Status ReadOne(Open& o)
{
  using namespace ManettePad;
  Status st;
  st.connected = true;
  SDL_Gamepad* pad = o.pad;
  const auto down = [pad](SDL_GamepadButton b) { return SDL_GetGamepadButton(pad, b); };
  u32 b = 0;
  if (o.n64)
  {
    // Nintendo Switch Online N64 controller (as RMG's input profile): A south, B east,
    // C-Left west, C-Up north, C-Right "back", C-Down right trigger, Z left trigger.
    if (down(SDL_GAMEPAD_BUTTON_SOUTH))
      b |= BTN_A;
    if (down(SDL_GAMEPAD_BUTTON_EAST))
      b |= BTN_B;
    if (down(SDL_GAMEPAD_BUTTON_WEST))
      b |= BTN_X;
    if (down(SDL_GAMEPAD_BUTTON_NORTH))
      b |= BTN_Y;
    const bool c_right = down(SDL_GAMEPAD_BUTTON_BACK);
    const bool c_down = Axis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 0.5f;
    st.cx = (c_right ? 1.0f : 0.0f) - (down(SDL_GAMEPAD_BUTTON_WEST) ? 1.0f : 0.0f);
    st.cy = (down(SDL_GAMEPAD_BUTTON_NORTH) ? 1.0f : 0.0f) - (c_down ? 1.0f : 0.0f);
    if (Axis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 0.5f)
      b |= BTN_Z;
  }
  else
  {
    const SDL_GamepadButton a =
        ButtonWithLabel(pad, SDL_GAMEPAD_BUTTON_LABEL_A, SDL_GAMEPAD_BUTTON_SOUTH);
    const SDL_GamepadButton bb =
        ButtonWithLabel(pad, SDL_GAMEPAD_BUTTON_LABEL_B, SDL_GAMEPAD_BUTTON_EAST);
    const SDL_GamepadButton x =
        ButtonWithLabel(pad, SDL_GAMEPAD_BUTTON_LABEL_X, SDL_GAMEPAD_BUTTON_WEST);
    const SDL_GamepadButton y =
        ButtonWithLabel(pad, SDL_GAMEPAD_BUTTON_LABEL_Y, SDL_GAMEPAD_BUTTON_NORTH);
    if (down(a))
      b |= BTN_A;
    if (down(bb))
      b |= BTN_B;
    if (down(x))
      b |= BTN_X;
    if (down(y))
      b |= BTN_Y;
    if (down(SDL_GAMEPAD_BUTTON_BACK))
      b |= BTN_BACK;
    if (Axis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 0.5f)
      b |= BTN_Z;
    if (Axis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 0.5f)
      b |= BTN_ZR;
    st.cx = Axis(pad, SDL_GAMEPAD_AXIS_RIGHTX);
    st.cy = -Axis(pad, SDL_GAMEPAD_AXIS_RIGHTY);
  }
  if (down(SDL_GAMEPAD_BUTTON_START))
    b |= BTN_START;
  if (down(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER))
    b |= BTN_L;
  if (down(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER))
    b |= BTN_R;
  if (down(SDL_GAMEPAD_BUTTON_DPAD_UP))
    b |= BTN_UP;
  if (down(SDL_GAMEPAD_BUTTON_DPAD_DOWN))
    b |= BTN_DOWN;
  if (down(SDL_GAMEPAD_BUTTON_DPAD_LEFT))
    b |= BTN_LEFT;
  if (down(SDL_GAMEPAD_BUTTON_DPAD_RIGHT))
    b |= BTN_RIGHT;
  if (down(SDL_GAMEPAD_BUTTON_GUIDE))
    b |= BTN_GUIDE;
  if (down(SDL_GAMEPAD_BUTTON_LEFT_STICK))
    b |= BTN_L3;
  if (down(SDL_GAMEPAD_BUTTON_RIGHT_STICK))
    b |= BTN_R3;
  st.x = Axis(pad, SDL_GAMEPAD_AXIS_LEFTX);
  st.y = -Axis(pad, SDL_GAMEPAD_AXIS_LEFTY);
  st.buttons = b;

  // The controller touched last names the controller in use
  const bool moved = std::abs(st.x) > 0.5f || std::abs(st.y) > 0.5f;
  if ((b & ~o.last_buttons) != 0 || moved)
  {
    s_last_name = o.name;
    s_last_n64 = o.n64;
  }
  o.last_buttons = b;
  return st;
}
}  // namespace

QSettings& ManetteQSettings()
{
  static QSettings s_settings(
      QString::fromStdString((CoreGetUserConfigDirectory() / "Manette.ini").string()),
      QSettings::IniFormat);
  return s_settings;
}

QString ManetteDataDir(const QString& sub)
{
  QString dir = QString::fromStdString((CoreGetUserDataDirectory() / "Manette").string());
  if (!sub.isEmpty())
    dir += QStringLiteral("/") + sub;
  QDir().mkpath(dir);
  return dir;
}

namespace ManettePad
{
void Init()
{
  if (s_init)
    return;
  s_init = true;
  SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
  SDL_InitSubSystem(SDL_INIT_GAMEPAD);
  const std::string db = (CoreGetSharedDataDirectory() / "gamecontrollerdb.txt").string();
  SDL_AddGamepadMappingsFromFile(db.c_str());
}

Status Read()
{
  Init();
  SDL_UpdateJoysticks();
  Scan();
  Status all;
  for (auto& [id, o] : s_pads)
  {
    const Status st = ReadOne(o);
    all.connected = true;
    all.buttons |= st.buttons;
    if (std::abs(st.x) > std::abs(all.x))
      all.x = st.x;
    if (std::abs(st.y) > std::abs(all.y))
      all.y = st.y;
    if (std::abs(st.cx) > std::abs(all.cx))
      all.cx = st.cx;
    if (std::abs(st.cy) > std::abs(all.cy))
      all.cy = st.cy;
  }
  return all;
}

Status ReadDevice(int index)
{
  Init();
  SDL_UpdateJoysticks();
  Scan();
  int i = 0;
  for (auto& [id, o] : s_pads)
  {
    if (i++ == index)
      return ReadOne(o);
  }
  return {};
}

std::vector<Device> Devices()
{
  Init();
  Scan();
  std::vector<Device> out;
  for (auto& [id, o] : s_pads)
    out.push_back({o.name, static_cast<int>(SDL_GetGamepadType(o.pad)), o.n64});
  return out;
}

std::string LastUsedName()
{
  if (s_last_name.empty() && !s_pads.empty())
    return s_pads.begin()->second.name;
  return s_last_name;
}

bool LastUsedIsN64()
{
  if (s_last_name.empty() && !s_pads.empty())
    return s_pads.begin()->second.n64;
  return s_last_n64;
}

std::vector<std::pair<std::string, float>> RawInputs(int index)
{
  std::vector<std::pair<std::string, float>> out;
  int i = 0;
  for (auto& [id, o] : s_pads)
  {
    if (i++ != index)
      continue;
    SDL_UpdateJoysticks();
    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; ++b)
    {
      const auto button = static_cast<SDL_GamepadButton>(b);
      if (!SDL_GamepadHasButton(o.pad, button))
        continue;
      const char* name = SDL_GetGamepadStringForButton(button);
      out.emplace_back(name ? name : "?", SDL_GetGamepadButton(o.pad, button) ? 1.0f : 0.0f);
    }
    for (int a = 0; a < SDL_GAMEPAD_AXIS_COUNT; ++a)
    {
      const auto axis = static_cast<SDL_GamepadAxis>(a);
      if (!SDL_GamepadHasAxis(o.pad, axis))
        continue;
      const char* name = SDL_GetGamepadStringForAxis(axis);
      const float v = Axis(o.pad, axis);
      if (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
      {
        out.emplace_back(name ? name : "?", std::max(0.0f, v));
      }
      else
      {
        out.emplace_back(std::string(name ? name : "?") + "-", std::max(0.0f, -v));
        out.emplace_back(std::string(name ? name : "?") + "+", std::max(0.0f, v));
      }
    }
  }
  return out;
}

void Rumble(float strength, int ms)
{
  const Uint16 v = static_cast<Uint16>(std::clamp(strength, 0.0f, 1.0f) * 65535);
  for (auto& [id, o] : s_pads)
    SDL_RumbleGamepad(o.pad, v, v, static_cast<Uint32>(ms));
}
}  // namespace ManettePad
