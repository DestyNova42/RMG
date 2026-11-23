/*
 * Rosalie's Mupen GUI - mode manette (fork DestyNova42/RMG)
 *  GPL-3.0, see LICENSE
 */
#include "ManetteInput.hpp"

#include <algorithm>
#include <cmath>
#include <map>

#include <SDL3/SDL.h>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <RMG-Core/Directories.hpp>
#include <RMG-Core/Settings.hpp>

#include "ManettePad.hpp"

namespace ManetteInput
{
namespace
{
std::string Section(int port)
{
  return "Rosalie's Mupen GUI - Input Plugin Profile " + std::to_string(port);
}

// RMG-Input's profile database (Data/InputProfileDB.json)
QJsonObject DbEntry(bool n64)
{
  QFile file(QString::fromStdString(
      (CoreGetSharedDataDirectory() / "InputProfileDB.json").string()));
  if (!file.open(QIODevice::ReadOnly))
    return {};
  const QJsonArray all = QJsonDocument::fromJson(file.readAll()).array();
  QJsonObject fallback;
  for (const QJsonValue& v : all)
  {
    const QJsonObject o = v.toObject();
    const QJsonArray names = o.value(QStringLiteral("DeviceName")).toArray();
    for (const QJsonValue& n : names)
    {
      const QString name = n.toString();
      if (name == QStringLiteral("fallback_profile"))
        fallback = o;
      if (n64 && name.contains(QStringLiteral("N64")))
        return o;
    }
  }
  return fallback;
}

// Per device: buttons and axes held when the wait started
struct Snapshot
{
  std::vector<bool> buttons;
  std::vector<float> axes;
};
std::map<int, Snapshot> s_snap;

SDL_Gamepad* PadOf(int device_index)
{
  int count = 0;
  SDL_JoystickID* ids = SDL_GetGamepads(&count);
  SDL_Gamepad* pad = nullptr;
  if (ids && device_index >= 0 && device_index < count)
    pad = SDL_GetGamepadFromID(ids[device_index]);
  if (!pad && ids && device_index >= 0 && device_index < count)
    pad = SDL_OpenGamepad(ids[device_index]);
  SDL_free(ids);
  return pad;
}

float AxisValue(SDL_Gamepad* pad, int axis)
{
  return SDL_GetGamepadAxis(pad, static_cast<SDL_GamepadAxis>(axis)) / 32767.0f;
}
}  // namespace

const std::vector<N64Input>& Inputs()
{
  static const std::vector<N64Input> inputs = {
      {"A", "A", "A"},
      {"B", "B", "B"},
      {"ZTrigger", "Z", "Z"},
      {"Start", "START", "Start"},
      {"LeftTrigger", "L", "L"},
      {"RightTrigger", "R", "R"},
      {"AnalogStickUp", "Stick : haut", "Control_Stick-U"},
      {"AnalogStickDown", "Stick : bas", "Control_Stick-D"},
      {"AnalogStickLeft", "Stick : gauche", "Control_Stick-L"},
      {"AnalogStickRight", "Stick : droite", "Control_Stick-R"},
      {"CButtonUp", "C haut", "C-Up"},
      {"CButtonDown", "C bas", "C-Down"},
      {"CButtonLeft", "C gauche", "C-Left"},
      {"CButtonRight", "C droite", "C-Right"},
      {"DpadUp", "Croix : haut", "D-Pad-U"},
      {"DpadDown", "Croix : bas", "D-Pad-D"},
      {"DpadLeft", "Croix : gauche", "D-Pad-L"},
      {"DpadRight", "Croix : droite", "D-Pad-R"},
  };
  return inputs;
}

bool PluggedIn(int port)
{
  if (!CoreSettingsSectionExists(Section(port)))
    return false;
  return CoreSettingsGetBoolValue(SettingsID::Input_PluggedIn, Section(port));
}

void SetPluggedIn(int port, bool on)
{
  const std::string s = Section(port);
  if (on && !CoreSettingsSectionExists(s))
  {
    ApplyDefault(port, false);
    return;
  }
  CoreSettingsSetValue(SettingsID::Input_PluggedIn, s, on);
}

std::vector<Binding> Get(int port, const char* key)
{
  const std::string s = Section(port);
  const std::string k = key;
  const auto names = CoreSettingsGetStringListValue(s, k + "_Name");
  const auto types = CoreSettingsGetIntListValue(s, k + "_InputType");
  const auto data = CoreSettingsGetIntListValue(s, k + "_Data");
  const auto extra = CoreSettingsGetIntListValue(s, k + "_ExtraData");
  std::vector<Binding> out;
  const size_t n = std::min({types.size(), data.size(), extra.size()});
  for (size_t i = 0; i < n; ++i)
  {
    Binding b;
    b.type = types[i];
    b.data = data[i];
    b.extra = extra[i];
    b.name = i < names.size() ? names[i] : std::string();
    out.push_back(b);
  }
  return out;
}

void Set(int port, const char* key, const std::vector<Binding>& bindings)
{
  const std::string s = Section(port);
  const std::string k = key;
  std::vector<std::string> names;
  std::vector<int> types, data, extra;
  for (const Binding& b : bindings)
  {
    names.push_back(b.name);
    types.push_back(b.type);
    data.push_back(b.data);
    extra.push_back(b.extra);
  }
  CoreSettingsSetValue(s, k + "_Name", names);
  CoreSettingsSetValue(s, k + "_InputType", types);
  CoreSettingsSetValue(s, k + "_Data", data);
  CoreSettingsSetValue(s, k + "_ExtraData", extra);
}

int Pak(int port)
{
  return CoreSettingsSectionExists(Section(port)) ?
             CoreSettingsGetIntValue(SettingsID::Input_Pak, Section(port)) :
             0;
}
void SetPak(int port, int pak)
{
  CoreSettingsSetValue(SettingsID::Input_Pak, Section(port), pak);
}
int Deadzone(int port)
{
  return CoreSettingsSectionExists(Section(port)) ?
             CoreSettingsGetIntValue(SettingsID::Input_Deadzone, Section(port)) :
             9;
}
void SetDeadzone(int port, int value)
{
  CoreSettingsSetValue(SettingsID::Input_Deadzone, Section(port), value);
}
int Sensitivity(int port)
{
  return CoreSettingsSectionExists(Section(port)) ?
             CoreSettingsGetIntValue(SettingsID::Input_Sensitivity, Section(port)) :
             100;
}
void SetSensitivity(int port, int value)
{
  CoreSettingsSetValue(SettingsID::Input_Sensitivity, Section(port), value);
}

void ApplyDefault(int port, bool n64_controller)
{
  const std::string s = Section(port);
  const bool existed = CoreSettingsSectionExists(s);
  const QJsonObject db = DbEntry(n64_controller);
  CoreSettingsSetValue(SettingsID::Input_PluggedIn, s, true);
  CoreSettingsSetValue(SettingsID::Input_DeviceType, s, 2);  // automatic: the port's controller
  CoreSettingsSetValue(SettingsID::Input_DeviceName, s, std::string("Automatic"));
  CoreSettingsSetValue(SettingsID::Input_DevicePath, s, std::string(""));
  CoreSettingsSetValue(SettingsID::Input_DeviceSerial, s, std::string(""));
  CoreSettingsSetValue(SettingsID::Input_DeviceGuid, s, std::string(""));
  if (!existed)
  {
    CoreSettingsSetValue(SettingsID::Input_Deadzone, s, 9);
    CoreSettingsSetValue(SettingsID::Input_Sensitivity, s, 100);
    CoreSettingsSetValue(SettingsID::Input_Pak, s, 0);
  }
  for (const N64Input& in : Inputs())
  {
    const QString k = QString::fromLatin1(in.key);
    Binding b;
    b.type = db.value(k + QStringLiteral("_InputType")).toString().toInt();
    b.data = db.value(k + QStringLiteral("_Data")).toString().toInt();
    b.extra = db.value(k + QStringLiteral("_ExtraData")).toString().toInt();
    b.name = db.value(k + QStringLiteral("_Name")).toString().toStdString();
    if (b.name.empty())
      Set(port, in.key, {});
    else
      Set(port, in.key, {b});
  }
}

void EnsureDefaults(bool n64_controller)
{
  if (!CoreSettingsSectionExists(Section(0)))
  {
    ApplyDefault(0, n64_controller);
    Save();
  }
}

void DetectBegin(int device_index)
{
  ManettePad::Init();
  SDL_UpdateJoysticks();
  Snapshot snap;
  if (SDL_Gamepad* pad = PadOf(device_index))
  {
    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; ++b)
      snap.buttons.push_back(SDL_GetGamepadButton(pad, static_cast<SDL_GamepadButton>(b)));
    for (int a = 0; a < SDL_GAMEPAD_AXIS_COUNT; ++a)
      snap.axes.push_back(AxisValue(pad, a));
  }
  s_snap[device_index] = snap;
}

std::optional<Binding> DetectPoll(int device_index)
{
  SDL_UpdateJoysticks();
  SDL_Gamepad* pad = PadOf(device_index);
  if (!pad)
    return std::nullopt;
  Snapshot& snap = s_snap[device_index];
  if (snap.buttons.size() < SDL_GAMEPAD_BUTTON_COUNT)
    snap.buttons.resize(SDL_GAMEPAD_BUTTON_COUNT, false);
  if (snap.axes.size() < SDL_GAMEPAD_AXIS_COUNT)
    snap.axes.resize(SDL_GAMEPAD_AXIS_COUNT, 0.0f);
  for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; ++b)
  {
    const bool down = SDL_GetGamepadButton(pad, static_cast<SDL_GamepadButton>(b));
    if (down && !snap.buttons[b])
    {
      Binding out;
      out.type = 0;
      out.data = b;
      const char* name = SDL_GetGamepadStringForButton(static_cast<SDL_GamepadButton>(b));
      out.name = name ? name : "?";
      return out;
    }
    if (!down)
      snap.buttons[b] = false;
  }
  for (int a = 0; a < SDL_GAMEPAD_AXIS_COUNT; ++a)
  {
    const float v = AxisValue(pad, a);
    const float before = snap.axes[a];
    if (std::abs(v) > 0.6f && std::abs(before) < 0.3f)
    {
      Binding out;
      out.type = 1;
      out.data = a;
      out.extra = v > 0 ? 1 : 0;
      const char* name = SDL_GetGamepadStringForAxis(static_cast<SDL_GamepadAxis>(a));
      out.name = std::string(name ? name : "?") + (v > 0 ? "+" : "-");
      return out;
    }
    if (std::abs(v) < 0.3f)
      snap.axes[a] = v;
  }
  return std::nullopt;
}

void Save()
{
  CoreSettingsSave();
}
}  // namespace ManetteInput
