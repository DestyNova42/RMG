/*
 * Rosalie's Mupen GUI - mode manette (fork DestyNova42/RMG)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 */
#ifndef MANETTE_HPP
#define MANETTE_HPP

#include <cstdint>

#include <QSettings>
#include <QString>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using s16 = std::int16_t;
using s32 = std::int32_t;

// Preferences of the controller mode: <user config>/Manette.ini
QSettings& ManetteQSettings();
// <user data>/Manette/<sub>, created if needed
QString ManetteDataDir(const QString& sub = {});

#endif  // MANETTE_HPP
