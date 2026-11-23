/*
 * Rosalie's Mupen GUI - mode manette (fork DestyNova42/RMG)
 *  GPL-3.0, see LICENSE
 *
 * Menu sounds made from scratch, in the spirit of the instrument banks of Nintendo 64 games:
 * marimba, celesta, harp, soft square, a 32 kHz-like softness and a small room reverb.
 */
#include "ManetteSynth.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace
{
constexpr float RATE = 48000.0f;
constexpr float TAU = 6.28318530718f;

using Buffer = std::vector<float>;  // mono

void Ensure(Buffer& b, size_t n)
{
  if (b.size() < n)
    b.resize(n, 0.0f);
}

// Fades the last 40 ms of a note so it never stops with a click
float Release(size_t i, size_t n)
{
  const float left = static_cast<float>(n - i) / (0.04f * RATE);
  return std::min(1.0f, left);
}

float Midi(float note)
{
  return 440.0f * std::pow(2.0f, (note - 69.0f) / 12.0f);
}

// Struck bar: fundamental + the 4th and 10th partials of a tuned marimba bar
void Marimba(Buffer& b, float start, float note, float gain, float length = 0.35f)
{
  const float f = Midi(note);
  const size_t s0 = static_cast<size_t>(start * RATE);
  const size_t n = static_cast<size_t>(length * RATE);
  Ensure(b, s0 + n);
  for (size_t i = 0; i < n; ++i)
  {
    const float t = i / RATE;
    const float attack = std::min(1.0f, t / 0.002f);
    const float v = std::sin(TAU * f * t) * std::exp(-t * 9.0f) +
                    0.35f * std::sin(TAU * f * 3.93f * t) * std::exp(-t * 38.0f) +
                    0.10f * std::sin(TAU * f * 9.9f * t) * std::exp(-t * 90.0f);
    b[s0 + i] += v * attack * gain * Release(i, n);
  }
}

// Celesta / music box: bright, bell-like, long ring
void Celesta(Buffer& b, float start, float note, float gain, float length = 0.9f)
{
  const float f = Midi(note);
  const size_t s0 = static_cast<size_t>(start * RATE);
  const size_t n = static_cast<size_t>(length * RATE);
  Ensure(b, s0 + n);
  for (size_t i = 0; i < n; ++i)
  {
    const float t = i / RATE;
    const float attack = std::min(1.0f, t / 0.003f);
    const float v = std::sin(TAU * f * t) * std::exp(-t * 4.5f) +
                    0.42f * std::sin(TAU * f * 2.0f * t) * std::exp(-t * 7.0f) +
                    0.18f * std::sin(TAU * f * 3.01f * t) * std::exp(-t * 12.0f) +
                    0.08f * std::sin(TAU * f * 4.17f * t) * std::exp(-t * 20.0f);
    b[s0 + i] += v * attack * gain * Release(i, n);
  }
}

// Plucked string (Karplus-Strong): harp
void Harp(Buffer& b, float start, float note, float gain, float length = 1.2f)
{
  const float f = Midi(note);
  const size_t s0 = static_cast<size_t>(start * RATE);
  const size_t n = static_cast<size_t>(length * RATE);
  Ensure(b, s0 + n);
  const size_t period = std::max<size_t>(2, static_cast<size_t>(RATE / f));
  std::vector<float> line(period);
  uint32_t seed = static_cast<uint32_t>(note * 7919);
  for (float& x : line)
  {
    seed = seed * 1664525u + 1013904223u;
    // soft excitation: noise smoothed toward a sine pluck
    x = 0.5f * ((seed >> 9) / static_cast<float>(1u << 23) - 1.0f);
  }
  for (size_t i = 0; i < period; ++i)
    line[i] = 0.6f * line[i] + 0.4f * std::sin(TAU * i / period);
  size_t idx = 0;
  float prev = 0;
  for (size_t i = 0; i < n; ++i)
  {
    const float cur = line[idx];
    const float next = 0.5f * (cur + prev) * 0.996f;
    prev = cur;
    line[idx] = next;
    idx = (idx + 1) % period;
    const float t = i / RATE;
    b[s0 + i] += cur * gain * std::exp(-t * 2.0f) * Release(i, n);
  }
}

// Soft square (pulse) with a gentle low-pass: chip-like, but warm
void Square(Buffer& b, float start, float note, float gain, float length, float bend = 0.0f)
{
  const size_t s0 = static_cast<size_t>(start * RATE);
  const size_t n = static_cast<size_t>(length * RATE);
  Ensure(b, s0 + n);
  float phase = 0, lp = 0;
  for (size_t i = 0; i < n; ++i)
  {
    const float t = i / RATE;
    const float f = Midi(note + bend * (t / length));
    phase += f / RATE;
    phase -= std::floor(phase);
    const float raw = phase < 0.5f ? 1.0f : -1.0f;
    lp += (raw - lp) * 0.18f;
    const float env = std::min(1.0f, t / 0.004f) * std::min(1.0f, (length - t) / 0.03f);
    b[s0 + i] += lp * env * gain;
  }
}

// Warm pad (detuned saws, low-passed, slow attack): the startup chord
void Pad(Buffer& b, float start, float note, float gain, float length)
{
  const size_t s0 = static_cast<size_t>(start * RATE);
  const size_t n = static_cast<size_t>(length * RATE);
  Ensure(b, s0 + n);
  float p1 = 0, p2 = 0.3f, lp = 0;
  const float f1 = Midi(note - 0.06f), f2 = Midi(note + 0.06f);
  for (size_t i = 0; i < n; ++i)
  {
    const float t = i / RATE;
    p1 += f1 / RATE;
    p2 += f2 / RATE;
    p1 -= std::floor(p1);
    p2 -= std::floor(p2);
    const float raw = (2 * p1 - 1) + (2 * p2 - 1);
    lp += (raw - lp) * 0.06f;
    const float env = std::min(1.0f, t / 0.35f) * std::min(1.0f, (length - t) / 0.8f);
    b[s0 + i] += lp * 0.5f * env * gain;
  }
}

// Filtered noise sweep (erase)
void Sweep(Buffer& b, float start, float gain, float length, bool down)
{
  const size_t s0 = static_cast<size_t>(start * RATE);
  const size_t n = static_cast<size_t>(length * RATE);
  Ensure(b, s0 + n);
  uint32_t seed = 12345;
  float lp = 0;
  for (size_t i = 0; i < n; ++i)
  {
    const float t = i / RATE;
    seed = seed * 1664525u + 1013904223u;
    const float noise = (seed >> 9) / static_cast<float>(1u << 23) - 1.0f;
    const float x = down ? 1 - t / length : t / length;
    lp += (noise - lp) * (0.02f + 0.3f * x * x);
    const float env = std::min(1.0f, t / 0.01f) * (1 - t / length);
    b[s0 + i] += lp * env * gain;
  }
}

// The softness of 32 kHz samples, then a small room (Schroeder reverb), then stereo s16
BigPictureAudio::Clip Finish(Buffer b, float wet = 0.22f, float tail = 0.35f)
{
  b.resize(b.size() + static_cast<size_t>(tail * RATE), 0.0f);
  // low-pass around 12 kHz
  float lp = 0;
  for (float& x : b)
  {
    lp += (x - lp) * 0.75f;
    x = lp;
  }
  // reverb: 4 combs + 2 all-pass
  const int comb_len[4] = {1557, 1617, 1491, 1422};
  const int ap_len[2] = {225, 556};
  Buffer rev(b.size(), 0.0f);
  for (int c = 0; c < 4; ++c)
  {
    std::vector<float> d(comb_len[c], 0.0f);
    size_t idx = 0;
    float filt = 0;
    for (size_t i = 0; i < b.size(); ++i)
    {
      const float out = d[idx];
      filt = out * 0.7f + filt * 0.3f;
      d[idx] = b[i] + filt * 0.78f;
      idx = (idx + 1) % d.size();
      rev[i] += out * 0.25f;
    }
  }
  for (int a = 0; a < 2; ++a)
  {
    std::vector<float> d(ap_len[a], 0.0f);
    size_t idx = 0;
    for (float& x : rev)
    {
      const float buf = d[idx];
      const float out = -x + buf;
      d[idx] = x + buf * 0.5f;
      idx = (idx + 1) % d.size();
      x = out;
    }
  }
  // normalise to a comfortable peak
  float peak = 0.0001f;
  for (size_t i = 0; i < b.size(); ++i)
    peak = std::max(peak, std::abs(b[i] + rev[i] * wet));
  const float norm = 0.4f / peak;
  BigPictureAudio::Clip clip;
  clip.step = 1.0;
  clip.samples.reserve(b.size() * 2);
  for (size_t i = 0; i < b.size(); ++i)
  {
    // a little width: the reverb is slightly offset between the two sides
    const float r2 = i >= 7 ? rev[i - 7] : 0.0f;
    const float l = (b[i] + rev[i] * wet) * norm;
    const float r = (b[i] + r2 * wet) * norm;
    clip.samples.push_back(static_cast<s16>(std::clamp(l, -1.0f, 1.0f) * 32767));
    clip.samples.push_back(static_cast<s16>(std::clamp(r, -1.0f, 1.0f) * 32767));
  }
  // trim the silent end
  size_t frames = clip.samples.size() / 2;
  while (frames > 1 && std::abs(clip.samples[(frames - 1) * 2]) < 40)
    --frames;
  clip.samples.resize(frames * 2);
  return clip;
}
}  // namespace

BigPictureAudio::Clip ManetteSynthSound(BigPictureAudio::Sound sound)
{
  using S = BigPictureAudio::Sound;
  Buffer b;
  switch (sound)
  {
  case S::Move:  // a light marimba tick
    Marimba(b, 0, 88, 0.8f, 0.14f);
    return Finish(b, 0.12f, 0.15f);
  case S::Page:  // two quick celesta notes
    Celesta(b, 0, 84, 0.6f, 0.4f);
    Celesta(b, 0.045f, 91, 0.5f, 0.4f);
    return Finish(b, 0.18f, 0.25f);
  case S::Confirm:  // rising celesta fifth with a marimba under it
    Marimba(b, 0, 72, 0.5f, 0.25f);
    Celesta(b, 0, 84, 0.6f, 0.6f);
    Celesta(b, 0.06f, 91, 0.6f, 0.7f);
    return Finish(b, 0.2f, 0.3f);
  case S::Back:  // falling marimba
    Marimba(b, 0, 86, 0.7f, 0.2f);
    Marimba(b, 0.06f, 79, 0.7f, 0.25f);
    return Finish(b, 0.15f, 0.2f);
  case S::Cancel:
    Marimba(b, 0, 67, 0.8f, 0.3f);
    return Finish(b, 0.15f, 0.2f);
  case S::Toggle:  // woody click
    Marimba(b, 0, 93, 0.6f, 0.08f);
    Marimba(b, 0.03f, 100, 0.35f, 0.08f);
    return Finish(b, 0.1f, 0.12f);
  case S::MenuOpen:  // three-note celesta chime
    Celesta(b, 0, 79, 0.55f, 0.6f);
    Celesta(b, 0.05f, 84, 0.55f, 0.6f);
    Celesta(b, 0.10f, 88, 0.55f, 0.8f);
    return Finish(b, 0.22f, 0.35f);
  case S::Launch:  // harp glissando up, then a bright celesta chord
  {
    const float scale[] = {60, 64, 67, 72, 76, 79, 84, 88};
    for (int i = 0; i < 8; ++i)
      Harp(b, i * 0.035f, scale[i], 0.55f, 1.2f);
    for (float n : {84.0f, 88.0f, 91.0f, 96.0f})
      Celesta(b, 0.3f, n, 0.35f, 1.4f);
    return Finish(b, 0.3f, 0.8f);
  }
  case S::Error:  // two low soft squares, a little off
    Square(b, 0, 52, 0.35f, 0.11f);
    Square(b, 0.13f, 51, 0.35f, 0.16f, -0.3f);
    return Finish(b, 0.12f, 0.15f);
  case S::SaveState:  // ascending celesta + harp
    Harp(b, 0, 72, 0.4f, 0.8f);
    Celesta(b, 0, 84, 0.5f, 0.5f);
    Celesta(b, 0.07f, 88, 0.5f, 0.5f);
    Celesta(b, 0.14f, 91, 0.55f, 0.9f);
    return Finish(b, 0.25f, 0.45f);
  case S::LoadState:  // the same, descending
    Harp(b, 0, 79, 0.4f, 0.8f);
    Celesta(b, 0, 91, 0.5f, 0.5f);
    Celesta(b, 0.07f, 88, 0.5f, 0.5f);
    Celesta(b, 0.14f, 84, 0.55f, 0.9f);
    return Finish(b, 0.25f, 0.45f);
  case S::Erase:
    Sweep(b, 0, 0.6f, 0.3f, true);
    Marimba(b, 0.05f, 60, 0.5f, 0.3f);
    return Finish(b, 0.15f, 0.2f);
  case S::Startup:  // warm pad chord blooming, celesta sparkles on top
  {
    for (float n : {48.0f, 55.0f, 60.0f, 64.0f, 67.0f})
      Pad(b, 0, n, 0.28f, 2.6f);
    const float sparkle[] = {84, 88, 91, 96, 91, 100};
    for (int i = 0; i < 6; ++i)
      Celesta(b, 0.45f + i * 0.11f, sparkle[i], 0.3f, 1.4f);
    Harp(b, 0.2f, 60, 0.5f, 2.0f);
    Harp(b, 0.26f, 67, 0.45f, 2.0f);
    Harp(b, 0.32f, 72, 0.45f, 2.0f);
    return Finish(b, 0.35f, 1.2f);
  }
  default:
    return {};
  }
}
