// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Menu music and sound effects of the controller mode.
// The sounds are not shipped: they are read from <User>/BigPicture/Sounds/ (FLAC files).

#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <SDL3/SDL.h>

#include "Manette.hpp"


class BigPictureAudio
{
public:
  enum class Sound
  {
    Startup,
    Move,
    Page,
    Confirm,
    Back,
    Cancel,
    Launch,
    MenuOpen,
    Toggle,
    Error,
    SaveState,
    LoadState,
    Erase,
    Count,
  };

  BigPictureAudio();
  ~BigPictureAudio();

  BigPictureAudio(const BigPictureAudio&) = delete;
  BigPictureAudio& operator=(const BigPictureAudio&) = delete;

  void Play(Sound sound);
  void StartMusic();
  void StopMusic();
  // 0..1 each; music 0 disables it.
  void SetVolumes(float music, float sfx);
  // Reopens the output if it was lost (e.g. while a game ran)
  void EnsureStream();
  bool HasMusic() const { return m_loaded && !m_music.empty(); }

  static std::string SoundDirectory();

  struct Clip
  {
    std::vector<s16> samples;  // interleaved stereo, at the file's own rate
    double step = 1.0;         // source frames per output frame
    size_t Frames() const { return samples.size() / 2; }
    bool empty() const { return samples.empty(); }
  };

private:
  struct Voice
  {
    const Clip* clip;
    double pos;
    float gain = 1.0f;
    int fade = -1;  // frames left of a quick fade-out (-1: playing normally)
  };

  void LoadAll();
  void OpenStream();
  void CloseStream();
  long Mix(float* out, long frames);

  static void SDLCALL DataCallback(void* user_data, SDL_AudioStream* stream, int additional_amount,
                                   int total_amount);

  std::array<Clip, static_cast<size_t>(Sound::Count)> m_sounds;
  Clip m_music;

  std::mutex m_mutex;
  std::vector<Voice> m_voices;
  std::array<std::chrono::steady_clock::time_point, 32> m_last_play{};
  float m_limiter = 1.0f;
  double m_music_pos = 0;
  bool m_music_on = false;
  bool m_audio_ref = false;
  float m_music_gain = 0.0f;
  float m_music_volume = 0.45f;
  float m_sfx_volume = 0.9f;

  std::atomic<bool> m_loaded{false};
  std::thread m_loader;

  SDL_AudioStream* m_stream = nullptr;
};
