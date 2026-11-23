// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "BigPictureAudio.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <filesystem>

#include <SDL3/SDL.h>

#include "Manette.hpp"

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#define DR_FLAC_IMPLEMENTATION
#include "ThirdParty/dr_flac.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace
{
constexpr unsigned int OUTPUT_RATE = 48000;

// Part of the file name (lower case) that identifies each sound.
constexpr const char* SOUND_KEYS[] = {
    "startup",          // Startup
    "option move 1",    // Move
    "option move 3",    // Page (tabs)
    "option confirm",   // Confirm
    "menu back",        // Back
    "option cancel",    // Cancel
    "menu select",      // Launch
    "press start",      // MenuOpen
    "option select",    // Toggle
    "error",            // Error
    "memory card copy", // SaveState
    "memory card move", // LoadState
    "memory card erase",// Erase
};
constexpr const char* MUSIC_KEY = "main menu";

std::string Lower(std::string s)
{
  std::ranges::transform(s, s.begin(), [](unsigned char c) { return std::tolower(c); });
  return s;
}

// Decodes a FLAC file to interleaved stereo 16-bit at its own rate (resampled while mixing).
BigPictureAudio::Clip DecodeFlac(const std::filesystem::path& path)
{
#ifdef _WIN32
  drflac* flac = drflac_open_file_w(path.wstring().c_str(), nullptr);
#else
  drflac* flac = drflac_open_file(path.string().c_str(), nullptr);
#endif
  BigPictureAudio::Clip clip;
  if (!flac)
    return clip;
  const unsigned int channels = flac->channels;
  const unsigned int rate = flac->sampleRate;
  const drflac_uint64 total = flac->totalPCMFrameCount;
  if (channels == 0 || rate == 0 || total == 0)
  {
    drflac_close(flac);
    return clip;
  }
  std::vector<drflac_int16> data(static_cast<size_t>(total) * channels);
  const drflac_uint64 frames = drflac_read_pcm_frames_s16(flac, total, data.data());
  drflac_close(flac);

  clip.step = static_cast<double>(rate) / OUTPUT_RATE;
  clip.samples.resize(static_cast<size_t>(frames) * 2);
  for (size_t i = 0; i < frames; ++i)
  {
    clip.samples[i * 2] = data[i * channels];
    clip.samples[i * 2 + 1] = data[i * channels + (channels > 1 ? 1 : 0)];
  }
  return clip;
}

// Linear interpolation of a stereo frame at a fractional position.
inline void Sample(const BigPictureAudio::Clip& clip, double pos, float* l, float* r)
{
  const size_t frames = clip.Frames();
  const size_t a = static_cast<size_t>(pos);
  const size_t b = a + 1 < frames ? a + 1 : a;
  const float t = static_cast<float>(pos - static_cast<double>(a));
  constexpr float scale = 1.0f / 32768.0f;
  const float la = clip.samples[a * 2] * scale, lb = clip.samples[b * 2] * scale;
  const float ra = clip.samples[a * 2 + 1] * scale, rb = clip.samples[b * 2 + 1] * scale;
  *l = la + (lb - la) * t;
  *r = ra + (rb - ra) * t;
}
}  // namespace

std::string BigPictureAudio::SoundDirectory()
{
  return ManetteDataDir(QStringLiteral("Sons")).toStdString() + "/";
}

// Without sound files: short soft tones, so the menus are not silent.
static BigPictureAudio::Clip Tone(std::initializer_list<std::pair<float, float>> notes, float volume)
{
  BigPictureAudio::Clip clip;
  clip.step = 1.0;
  for (const auto& [freq, seconds] : notes)
  {
    const int frames = static_cast<int>(seconds * OUTPUT_RATE);
    for (int i = 0; i < frames; ++i)
    {
      const float t = static_cast<float>(i) / OUTPUT_RATE;
      const float env = std::min(1.0f, i / 240.0f) * std::exp(-t * 18.0f);
      const float v = (std::sin(6.2831853f * freq * t) + 0.25f * std::sin(6.2831853f * freq * 2 * t)) *
                      env * volume;
      const s16 sample = static_cast<s16>(std::clamp(v, -1.0f, 1.0f) * 32767);
      clip.samples.push_back(sample);
      clip.samples.push_back(sample);
    }
  }
  return clip;
}

BigPictureAudio::BigPictureAudio()
{
  // Opening the stream and decoding happen off the UI thread (COM is initialised there as MTA).
  m_loader = std::thread([this] {
    OpenStream();
    LoadAll();
    m_loaded = true;
  });
}

BigPictureAudio::~BigPictureAudio()
{
  if (m_loader.joinable())
    m_loader.join();
  CloseStream();
}

void BigPictureAudio::LoadAll()
{
  {
    std::lock_guard lk(m_mutex);
    const auto set = [this](Sound s, BigPictureAudio::Clip c) {
      m_sounds[static_cast<size_t>(s)] = std::move(c);
    };
    set(Sound::Move, Tone({{1320, 0.05f}}, 0.18f));
    set(Sound::Page, Tone({{990, 0.06f}}, 0.2f));
    set(Sound::Confirm, Tone({{1046, 0.06f}, {1568, 0.09f}}, 0.22f));
    set(Sound::Back, Tone({{784, 0.06f}, {523, 0.08f}}, 0.2f));
    set(Sound::Cancel, Tone({{523, 0.08f}}, 0.2f));
    set(Sound::Launch, Tone({{784, 0.07f}, {1046, 0.07f}, {1568, 0.16f}}, 0.24f));
    set(Sound::MenuOpen, Tone({{880, 0.06f}, {1318, 0.08f}}, 0.2f));
    set(Sound::Toggle, Tone({{1175, 0.05f}}, 0.2f));
    set(Sound::Error, Tone({{330, 0.09f}, {262, 0.12f}}, 0.24f));
    set(Sound::SaveState, Tone({{1046, 0.06f}, {1318, 0.06f}, {1568, 0.1f}}, 0.22f));
    set(Sound::LoadState, Tone({{1568, 0.06f}, {1318, 0.06f}, {1046, 0.1f}}, 0.22f));
    set(Sound::Erase, Tone({{440, 0.07f}, {330, 0.1f}}, 0.22f));
  }
  const std::filesystem::path dir = SoundDirectory();
  std::error_code ec;
  if (!std::filesystem::is_directory(dir, ec))
    return;

  for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
  {
    if (!entry.is_regular_file() || Lower(entry.path().extension().string()) != ".flac")
      continue;
    const std::string name = Lower(entry.path().filename().string());

    if (name.find(MUSIC_KEY) != std::string::npos)
    {
      Clip clip = DecodeFlac(entry.path());
      std::lock_guard lk(m_mutex);
      m_music = std::move(clip);
      continue;
    }
    for (size_t i = 0; i < std::size(SOUND_KEYS); ++i)
    {
      if (name.find(SOUND_KEYS[i]) != std::string::npos)
      {
        Clip clip = DecodeFlac(entry.path());
        std::lock_guard lk(m_mutex);
        m_sounds[i] = std::move(clip);
        break;
      }
    }
  }
}

void BigPictureAudio::Play(Sound sound)
{
  if (!m_loaded)
    return;
  const Clip& clip = m_sounds[static_cast<size_t>(sound)];
  if (clip.empty())
    return;
  std::lock_guard lk(m_mutex);
  // Fast scrolling: the previous instance fades out in a few ms (no click), and quick
  // repeats are played softer so they don't pile up into a buzz.
  const auto now = std::chrono::steady_clock::now();
  auto& last = m_last_play[static_cast<size_t>(sound) % m_last_play.size()];
  const auto since = std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count();
  last = now;
  float gain = 1.0f;
  if (since < 70)
    return;  // faster than the ear can separate: skip
  if (since < 160)
    gain = 0.55f;
  for (Voice& v : m_voices)
  {
    if (v.clip == &clip && v.fade < 0)
      v.fade = 240;  // 5 ms at 48 kHz
  }
  // Never more than a handful of effects at once.
  while (m_voices.size() >= 6)
    m_voices.erase(m_voices.begin());
  m_voices.push_back({&clip, 0.0, gain, -1});
}

void BigPictureAudio::StartMusic()
{
  std::lock_guard lk(m_mutex);
  m_music_on = true;
}

void BigPictureAudio::SetVolumes(float music, float sfx)
{
  std::lock_guard lk(m_mutex);
  // Perceptual curve: 50 % on the slider sounds like half as loud.
  m_music_volume = 0.7f * music * music;
  m_sfx_volume = sfx * sfx;
}

void BigPictureAudio::StopMusic()
{
  std::lock_guard lk(m_mutex);
  m_music_on = false;
  m_voices.clear();
}

long BigPictureAudio::Mix(float* out, long frames)
{
  std::fill(out, out + frames * 2, 0.0f);
  std::lock_guard lk(m_mutex);

  if (!m_music.empty() && (m_music_on || m_music_gain > 0.0f))
  {
    const double music_frames = static_cast<double>(m_music.Frames());
    for (long i = 0; i < frames; ++i)
    {
      // short fades in and out
      const float target = m_music_on ? 1.0f : 0.0f;
      m_music_gain += (target - m_music_gain) * 0.0001f;
      if (!m_music_on && m_music_gain < 0.001f)
      {
        m_music_gain = 0.0f;
        break;
      }
      float l, r;
      Sample(m_music, m_music_pos, &l, &r);
      const float g = m_music_gain * m_music_volume;
      out[i * 2] += l * g;
      out[i * 2 + 1] += r * g;
      m_music_pos += m_music.step;
      if (m_music_pos >= music_frames - 1)
        m_music_pos = 0;
    }
  }

  for (Voice& v : m_voices)
  {
    const double clip_frames = static_cast<double>(v.clip->Frames()) - 1;
    for (long i = 0; i < frames && v.pos < clip_frames && v.fade != 0;
         ++i, v.pos += v.clip->step)
    {
      float l, r;
      Sample(*v.clip, v.pos, &l, &r);
      float g = m_sfx_volume * v.gain * 0.8f;
      if (v.fade > 0)
      {
        g *= static_cast<float>(v.fade) / 240.0f;
        --v.fade;
      }
      out[i * 2] += l * g;
      out[i * 2 + 1] += r * g;
    }
  }
  std::erase_if(m_voices, [](const Voice& v) {
    return v.fade == 0 || v.pos >= static_cast<double>(v.clip->Frames()) - 1;
  });

  // Smooth limiter: lowers the gain quickly when the mix gets too loud, then recovers
  // slowly, instead of clipping (which sounds saturated).
  for (long i = 0; i < frames; ++i)
  {
    const float peak = std::max(std::abs(out[i * 2]), std::abs(out[i * 2 + 1]));
    const float wanted = peak * m_limiter > 0.9f ? 0.9f / peak : 1.0f;
    if (wanted < m_limiter)
      m_limiter += (wanted - m_limiter) * 0.5f;
    else
      m_limiter += (wanted - m_limiter) * 0.0005f;
    out[i * 2] = std::clamp(out[i * 2] * m_limiter, -1.0f, 1.0f);
    out[i * 2 + 1] = std::clamp(out[i * 2 + 1] * m_limiter, -1.0f, 1.0f);
  }
  return frames;
}

void SDLCALL BigPictureAudio::DataCallback(void* user_data, SDL_AudioStream* stream,
                                           int additional_amount, int)
{
  auto* self = static_cast<BigPictureAudio*>(user_data);
  const int frames = additional_amount / static_cast<int>(sizeof(float) * 2);
  if (frames <= 0)
    return;
  std::vector<float> buffer(static_cast<size_t>(frames) * 2);
  self->Mix(buffer.data(), frames);
  SDL_PutAudioStreamData(stream, buffer.data(), frames * static_cast<int>(sizeof(float) * 2));
}

void BigPictureAudio::OpenStream()
{
  if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO))
    return;
  SDL_AudioSpec spec{};
  spec.format = SDL_AUDIO_F32;
  spec.channels = 2;
  spec.freq = OUTPUT_RATE;
  m_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, DataCallback, this);
  if (m_stream)
    SDL_ResumeAudioStreamDevice(m_stream);
}

void BigPictureAudio::CloseStream()
{
  if (m_stream)
  {
    SDL_DestroyAudioStream(m_stream);
    m_stream = nullptr;
  }
}
