#ifndef MATCH3_AUDIO_H_
#define MATCH3_AUDIO_H_

#include <raylib.h>

#include <array>
#include <cstdint>

/// @brief Everything that belongs to the Match-3 game.
namespace match3 {

/// @brief Every sound the game makes. All of them are synthesised in code
/// at startup, so the game ships with no audio files.
enum class SoundEffect : std::uint8_t {
  /// @brief Soft tick when a gem is selected.
  Select = 0,
  /// @brief Rising whoosh when two gems swap.
  Swap,
  /// @brief Low "nope" buzz when a swap makes no match.
  Invalid,
  /// @brief Bell chime when gems clear. Played higher on each cascade.
  Clear,
  /// @brief Sparkly arpeggio when a special gem is made.
  Create,
  /// @brief Laser zap when a Line gem goes off.
  Line,
  /// @brief Deep boom when a Bomb goes off.
  Bomb,
  /// @brief Shimmering run of notes when a Rainbow goes off.
  Rainbow,
  /// @brief Rustle when a board with no moves is shuffled.
  Shuffle,
  /// @brief Falling three-note phrase when a game ends.
  GameOver,
};

/// @brief Number of SoundEffect values, for sizing the sound bank.
inline constexpr int kSoundEffectCount = 10;

/// @brief Owns every synthesised sound and plays them on request.
///
/// RAII: the constructor builds and uploads the sounds, and the destructor
/// unloads them. Construct it after `InitAudioDevice()` and destroy it
/// before `CloseAudioDevice()`. If no audio device could be opened, every
/// method quietly does nothing, so the game still runs.
class Audio {
 public:
  /// @brief Synthesises every sound and uploads it to the audio device.
  Audio();

  /// @brief Unloads every sound.
  ~Audio();

  /// @brief Non-copyable: a copy would unload the same sounds twice.
  Audio(const Audio&) = delete;
  /// @brief Non-copy-assignable for the same reason.
  Audio& operator=(const Audio&) = delete;

  /// @brief Plays a sound.
  /// @param effect Sound to play.
  /// @param pitch Playback speed: `1.0` is normal, `2.0` an octave up.
  void Play(SoundEffect effect, float pitch = 1.0f);

  /// @brief Plays the clear chime one step higher on a pentatonic scale for
  /// each cascade, so long chains climb up a tune.
  /// @param cascade Cascade step (1 = the swap's own clear).
  void PlayCascade(int cascade);

  /// @brief Turns all sound off or on.
  /// @param muted True to silence the game.
  void set_muted(bool muted) { muted_ = muted; }
  /// @brief True if sound is off.
  bool muted() const { return muted_; }

 private:
  /// @brief Copies of each sound that can play at the same time, so a fast
  /// cascade doesn't cut its own chimes off.
  static constexpr int kVoices = 4;

  /// @brief One sound and its extra voices.
  struct Bank {
    /// @brief The sound that owns the sample data.
    Sound base{};
    /// @brief Aliases: extra voices that share `base`'s samples instead of
    /// copying them (`LoadSoundAlias`). They must be unloaded before `base`.
    std::array<Sound, kVoices> voices{};
    /// @brief Voice to use next (round-robin).
    int next = 0;
  };

  /// @brief One bank per SoundEffect, indexed by its value.
  std::array<Bank, kSoundEffectCount> banks_{};
  /// @brief True if the sounds were loaded (an audio device was available).
  bool loaded_ = false;
  /// @brief True if sound is off.
  bool muted_ = false;
};

}  // namespace match3

#endif  // MATCH3_AUDIO_H_
