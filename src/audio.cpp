#include "audio.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <random>
#include <vector>

/// @brief Everything that belongs to the Match-3 game.
namespace match3 {

/// @brief Sound synthesis helpers internal to `audio.cpp`.
///
/// Every sound is built the same way: work out each sample's value with a
/// little maths (sine waves, noise, envelopes), store the samples in a
/// vector, and hand them to raylib.
namespace {

/// @brief Samples per second.
constexpr int kSampleRate = 44100;
/// @brief A full turn in radians, for oscillator phase.
constexpr double kTwoPi = 2.0 * std::numbers::pi;

/// @brief Semitone steps of a major pentatonic scale over two octaves. Notes
/// from this scale always sound good together, whatever order they come in.
constexpr std::array<int, 10> kPentatonic{0, 2, 4, 7, 9, 12, 14, 16, 19, 21};

/// @brief Returns a frequency `semitones` above (or below) `base`.
/// @param base Frequency in Hz.
/// @param semitones Steps on a piano keyboard; 12 is one octave (double).
/// @return Frequency in Hz.
double Note(double base, double semitones) {
  return base * std::pow(2.0, semitones / 12.0);
}

/// @brief Triangle wave: softer than a square wave, brighter than a sine.
/// @param phase Phase in radians.
/// @return Sample in `[-1, 1]`.
double Triangle(double phase) {
  const double t = std::fmod(phase / kTwoPi, 1.0);
  return 4.0 * std::abs(t - 0.5) - 1.0;
}

/// @brief Builds an empty sample buffer for a sound `seconds` long.
/// @param seconds Length of the sound.
/// @return Zero-filled buffer.
std::vector<float> Buffer(double seconds) {
  return std::vector<float>(static_cast<std::size_t>(seconds * kSampleRate),
                            0.0f);
}

/// @brief Adds a decaying tone to `out`, starting `delay` seconds in.
/// @param out Buffer to mix into (passed by reference and changed).
/// @param delay Start time in seconds.
/// @param freq Frequency in Hz.
/// @param decay How fast it fades; higher = shorter.
/// @param volume Peak volume.
/// @param triangle True for a triangle wave, false for a sine wave.
void AddTone(std::vector<float>& out, double delay, double freq, double decay,
             double volume, bool triangle) {
  const auto first = static_cast<std::size_t>(delay * kSampleRate);
  for (std::size_t i = first; i < out.size(); ++i) {
    const double t = static_cast<double>(i - first) / kSampleRate;
    const double phase = kTwoPi * freq * t;
    // A 4 ms fade-in stops the click a sudden start would make.
    const double attack = std::min(1.0, t / 0.004);
    const double wave = triangle
                            ? Triangle(phase)
                            : std::sin(phase) + 0.25 * std::sin(2.0 * phase);
    out[i] += static_cast<float>(wave * volume * attack * std::exp(-decay * t));
  }
}

/// @brief Synthesises the samples for one sound effect.
/// @param effect Which sound.
/// @return Mono samples in roughly `[-1, 1]`.
std::vector<float> Synthesize(SoundEffect effect) {
  // A fixed seed: the noise in each sound is the same every run.
  std::mt19937 rng(7U);
  std::uniform_real_distribution<float> noise(-1.0f, 1.0f);

  switch (effect) {
    case SoundEffect::Select: {
      auto out = Buffer(0.06);
      AddTone(out, 0.0, 1320.0, 70.0, 0.25, false);
      return out;
    }
    case SoundEffect::Swap: {
      // A sine whose pitch glides up. The phase is accumulated sample by
      // sample because the frequency changes as it plays.
      auto out = Buffer(0.14);
      double phase = 0.0;
      for (std::size_t i = 0; i < out.size(); ++i) {
        const double t = static_cast<double>(i) / kSampleRate;
        const double u = t / 0.14;
        phase += kTwoPi * (420.0 + 600.0 * u) / kSampleRate;
        out[i] = static_cast<float>(std::sin(phase) * 0.22 *
                                    std::sin(u * std::numbers::pi));
      }
      return out;
    }
    case SoundEffect::Invalid: {
      auto out = Buffer(0.22);
      AddTone(out, 0.0, 196.0, 18.0, 0.3, true);
      AddTone(out, 0.09, 165.0, 18.0, 0.3, true);
      return out;
    }
    case SoundEffect::Clear: {
      auto out = Buffer(0.45);
      AddTone(out, 0.0, 784.0, 9.0, 0.32, false);
      AddTone(out, 0.0, 1568.0, 14.0, 0.10, false);
      return out;
    }
    case SoundEffect::Create: {
      auto out = Buffer(0.5);
      const std::array<double, 4> steps{0, 4, 7, 12};
      for (std::size_t n = 0; n < steps.size(); ++n) {
        AddTone(out, 0.05 * static_cast<double>(n), Note(1046.5, steps[n]),
                10.0, 0.16, true);
      }
      return out;
    }
    case SoundEffect::Line: {
      // A falling sawtooth sweep plus a little noise sounds like a laser.
      auto out = Buffer(0.32);
      double phase = 0.0;
      for (std::size_t i = 0; i < out.size(); ++i) {
        const double t = static_cast<double>(i) / kSampleRate;
        phase += kTwoPi * (1600.0 * std::exp(-9.0 * t) + 120.0) / kSampleRate;
        const double saw = std::fmod(phase / kTwoPi, 1.0) * 2.0 - 1.0;
        out[i] = static_cast<float>((saw * 0.18 + noise(rng) * 0.05) *
                                    std::exp(-7.0 * t));
      }
      return out;
    }
    case SoundEffect::Bomb: {
      // Low-passed noise (each sample leans on the one before, which smooths
      // out the hiss) plus a sine "thud" whose pitch drops.
      auto out = Buffer(0.6);
      double phase = 0.0;
      float smooth = 0.0f;
      for (std::size_t i = 0; i < out.size(); ++i) {
        const double t = static_cast<double>(i) / kSampleRate;
        smooth += (noise(rng) - smooth) * 0.12f;
        phase += kTwoPi * (40.0 + 90.0 * std::exp(-14.0 * t)) / kSampleRate;
        out[i] = static_cast<float>(smooth * 0.9 * std::exp(-6.0 * t) +
                                    std::sin(phase) * 0.6 * std::exp(-5.0 * t));
      }
      return out;
    }
    case SoundEffect::Rainbow: {
      auto out = Buffer(1.1);
      for (std::size_t n = 0; n < kPentatonic.size(); ++n) {
        AddTone(out, 0.045 * static_cast<double>(n),
                Note(784.0, kPentatonic[n]), 5.0, 0.10, false);
      }
      return out;
    }
    case SoundEffect::Shuffle: {
      auto out = Buffer(0.45);
      float smooth = 0.0f;
      for (std::size_t i = 0; i < out.size(); ++i) {
        const double t = static_cast<double>(i) / kSampleRate;
        smooth += (noise(rng) - smooth) * 0.3f;
        // A fast wobble in volume (tremolo) makes the noise "rustle".
        const double wobble = 0.5 + 0.5 * std::sin(kTwoPi * 22.0 * t);
        out[i] = static_cast<float>(smooth * 0.35 * wobble *
                                    std::sin(t / 0.45 * std::numbers::pi));
      }
      return out;
    }
    case SoundEffect::GameOver: {
      auto out = Buffer(1.3);
      const std::array<double, 3> steps{7, 4, 0};
      for (std::size_t n = 0; n < steps.size(); ++n) {
        AddTone(out, 0.18 * static_cast<double>(n), Note(523.25, steps[n]),
                n == 2 ? 2.5 : 6.0, 0.22, true);
      }
      return out;
    }
  }
  return Buffer(0.01);
}

/// @brief Converts float samples to a raylib Sound.
/// @param samples Mono samples in roughly `[-1, 1]`.
/// @return A loaded Sound (owns its own copy of the samples).
Sound MakeSound(const std::vector<float>& samples) {
  // raylib plays 16-bit integers. Clamp first so a loud mix clips cleanly
  // instead of wrapping around (which would be a nasty crackle).
  std::vector<std::int16_t> pcm(samples.size());
  std::ranges::transform(samples, pcm.begin(), [](float s) {
    return static_cast<std::int16_t>(std::clamp(s, -1.0f, 1.0f) * 32767.0f);
  });
  Wave wave{};
  wave.frameCount = static_cast<unsigned int>(pcm.size());
  wave.sampleRate = kSampleRate;
  wave.sampleSize = 16;
  wave.channels = 1;
  // raylib's C struct `Wave` has a non-const `void* data`, but
  // `LoadSoundFromWave` only reads it and copies the samples into the audio
  // device's own buffer. So `pcm` keeps ownership and frees itself (RAII)
  // when this function returns, after raylib has made its copy.
  wave.data = pcm.data();
  return LoadSoundFromWave(wave);
}

}  // namespace

Audio::Audio() {
  if (!IsAudioDeviceReady()) return;
  for (int i = 0; i < kSoundEffectCount; ++i) {
    Bank& bank = banks_[static_cast<std::size_t>(i)];
    bank.base = MakeSound(Synthesize(static_cast<SoundEffect>(i)));
    for (Sound& voice : bank.voices) voice = LoadSoundAlias(bank.base);
  }
  loaded_ = true;
}

Audio::~Audio() {
  if (!loaded_) return;
  for (Bank& bank : banks_) {
    // Aliases share `base`'s samples, so unload them first; unloading the
    // base first would leave the aliases pointing at freed memory.
    for (Sound& voice : bank.voices) UnloadSoundAlias(voice);
    UnloadSound(bank.base);
  }
}

void Audio::Play(SoundEffect effect, float pitch) {
  if (!loaded_ || muted_) return;
  Bank& bank = banks_[static_cast<std::size_t>(effect)];
  Sound& voice = bank.voices[static_cast<std::size_t>(bank.next)];
  bank.next = (bank.next + 1) % kVoices;
  SetSoundPitch(voice, std::clamp(pitch, 0.25f, 4.0f));
  PlaySound(voice);
}

void Audio::PlayCascade(int cascade) {
  const int step =
      std::clamp(cascade - 1, 0, static_cast<int>(kPentatonic.size()) - 1);
  // Pitch is a speed ratio: 2^(semitones / 12) moves up that many semitones.
  const float pitch = std::pow(
      2.0f,
      static_cast<float>(kPentatonic[static_cast<std::size_t>(step)]) / 12.0f);
  Play(SoundEffect::Clear, pitch);
}

}  // namespace match3
