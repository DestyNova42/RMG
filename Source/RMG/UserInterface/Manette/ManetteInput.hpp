/*
 * Rosalie's Mupen GUI - mode manette (fork DestyNova42/RMG)
 *  GPL-3.0, see LICENSE
 *
 * Controller mapping of RMG's input plugin, edited from the controller mode.
 */
#ifndef MANETTEINPUT_HPP
#define MANETTEINPUT_HPP

#include <optional>
#include <string>
#include <vector>

namespace ManetteInput
{
// One input of the controller that does an N64 button (a button can have several)
struct Binding
{
  int type = 0;   // 0 gamepad button, 1 gamepad axis (RMG-Input's InputType)
  int data = 0;   // SDL button or axis
  int extra = 0;  // axis: 1 positive, 0 negative
  std::string name;
};

// The N64 inputs, in the order of the assistant
struct N64Input
{
  const char* key;    // settings key prefix, e.g. "A", "CButtonUp", "AnalogStickLeft"
  const char* label;  // French name
  const char* icon;   // icon of the controller mode
};
const std::vector<N64Input>& Inputs();

bool PluggedIn(int port);
void SetPluggedIn(int port, bool on);
std::vector<Binding> Get(int port, const char* key);
void Set(int port, const char* key, const std::vector<Binding>& bindings);
int Pak(int port);  // 0 Controller Pak, 1 Rumble Pak, 2 Transfer Pak, 3 none
void SetPak(int port, int pak);
int Deadzone(int port);  // percent
void SetDeadzone(int port, int value);
int Sensitivity(int port);  // percent
void SetSensitivity(int port, int value);

// The usual mapping for this controller (N64 controller or standard layout)
void ApplyDefault(int port, bool n64_controller);
// First start: player 1 plugged in with the usual mapping
void EnsureDefaults(bool n64_controller);

// Waiting for a press on the controller of a player
void DetectBegin(int device_index);
std::optional<Binding> DetectPoll(int device_index);

void Save();
}  // namespace ManetteInput

#endif  // MANETTEINPUT_HPP
