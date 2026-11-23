/*
 * Rosalie's Mupen GUI - mode manette (fork DestyNova42/RMG)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 */
#ifndef MANETTEPAD_HPP
#define MANETTEPAD_HPP

#include <string>
#include <vector>

#include "Manette.hpp"

// The controllers, read directly with SDL for the menus (the game reads them through the
// input plugin). Buttons follow the letters printed on the controller.
namespace ManettePad
{
enum Button : u32
{
  BTN_A = 1u << 0,       // accept (N64: A)
  BTN_B = 1u << 1,       // back (N64: B)
  BTN_X = 1u << 2,       // options (N64: C-Left)
  BTN_Y = 1u << 3,       // favorite (N64: C-Up)
  BTN_START = 1u << 4,
  BTN_L = 1u << 5,       // shoulders (N64: L / R)
  BTN_R = 1u << 6,
  BTN_Z = 1u << 7,       // left trigger (N64: Z)
  BTN_ZR = 1u << 8,      // right trigger
  BTN_UP = 1u << 9,
  BTN_DOWN = 1u << 10,
  BTN_LEFT = 1u << 11,
  BTN_RIGHT = 1u << 12,
  BTN_GUIDE = 1u << 13,
  BTN_BACK = 1u << 14,   // Select / View (N64: C-Right)
  BTN_L3 = 1u << 15,
  BTN_R3 = 1u << 16,
};

struct Status
{
  u32 buttons = 0;
  float x = 0, y = 0;    // left stick, -1..1, up = +1
  float cx = 0, cy = 0;  // right stick (N64: C buttons on the "C" stick), up = +1
  bool connected = false;
};

// Starts SDL's gamepad subsystem (once).
void Init();
// Reads every controller and merges them (any controller drives the menus).
Status Read();
// Same, one controller (index in Devices()).
Status ReadDevice(int index);
struct Device
{
  std::string name;
  int type = 0;  // SDL_GamepadType
  bool n64 = false;
};
std::vector<Device> Devices();
// Name of the controller used last (empty: none)
std::string LastUsedName();
bool LastUsedIsN64();
// Every raw input of a controller, for the diagnostic screen: name and value 0..1
std::vector<std::pair<std::string, float>> RawInputs(int index);
// Rumble on every controller, 0..1 for ms milliseconds
void Rumble(float strength, int ms);
}  // namespace ManettePad

#endif  // MANETTEPAD_HPP
