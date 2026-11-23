#include "effects.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

/// @brief Everything that belongs to the Match-3 game.
namespace match3 {

/// @brief Easing curves and constants internal to `effects.cpp`.
namespace {

/// @brief Seconds a pop-in takes to grow to full size.
constexpr float kPopSeconds = 0.3f;
/// @brief Seconds a landed gem squashes for.
constexpr float kLandSeconds = 0.14f;
/// @brief How far a bump leans towards its neighbour, in cells.
constexpr float kBumpDistance = 0.32f;
/// @brief Downward pull on sparks, in pixels per second squared.
constexpr float kGravity = 900.0f;
/// @brief Most sparks alive at once, so a giant chain can't slow the game.
constexpr std::size_t kMaxParticles = 1500;

/// @brief Smooth start and smooth stop ("ease in-out cubic").
/// @param t Linear progress in `[0, 1]`.
/// @return Eased progress in `[0, 1]`.
float EaseInOut(float t) {
  return t < 0.5f ? 4.0f * t * t * t
                  : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

/// @brief "Ease out back": rushes past 1.0 a little, then settles, which
/// gives pop-ins a small bounce.
/// @param t Linear progress in `[0, 1]`.
/// @return Eased progress (peaks around 1.1).
float EaseOutBack(float t) {
  constexpr float kOvershoot = 1.70158f;
  const float u = t - 1.0f;
  return 1.0f + (kOvershoot + 1.0f) * u * u * u + kOvershoot * u * u;
}

/// @brief Returns how far `now` is through a motion, clamped to `[0, 1]`.
/// @param now Animation clock in seconds.
/// @param start When the motion began.
/// @param duration How long it lasts (`<= 0` counts as already finished).
/// @return Linear progress.
float Progress(float now, float start, float duration) {
  if (duration <= 0.0f) return 1.0f;
  return std::clamp((now - start) / duration, 0.0f, 1.0f);
}

}  // namespace

void MotionTracker::Reset(int columns, int rows) {
  columns_ = columns;
  rows_ = rows;
  // `assign` replaces every element with a default-constructed Motion, so no
  // animation from the previous board survives.
  motions_.assign(static_cast<std::size_t>(columns * rows), Motion{});
}

void MotionTracker::Slide(GridPoint cell, CellPosition from, MotionKind kind,
                          float now, float duration) {
  Motion* motion = Find(cell.x, cell.y);
  if (motion == nullptr) return;
  motion->kind = kind;
  motion->offset = {from.x - static_cast<float>(cell.x),
                    from.y - static_cast<float>(cell.y)};
  motion->start = now;
  motion->duration = duration;
}

void MotionTracker::Bump(GridPoint cell, GridPoint toward, float now,
                         float duration) {
  Motion* motion = Find(cell.x, cell.y);
  if (motion == nullptr) return;
  motion->kind = MotionKind::Bump;
  motion->offset = {static_cast<float>(toward.x - cell.x),
                    static_cast<float>(toward.y - cell.y)};
  motion->start = now;
  motion->duration = duration;
}

void MotionTracker::PopIn(GridPoint cell, float start) {
  if (Motion* motion = Find(cell.x, cell.y)) motion->pop_start = start;
}

CellPosition MotionTracker::Position(int x, int y, float now) const {
  const CellPosition home{static_cast<float>(x), static_cast<float>(y)};
  const Motion* motion = Find(x, y);
  if (motion == nullptr) return home;
  const float t = Progress(now, motion->start, motion->duration);
  // `factor` is how much of `offset` is still left to travel.
  float factor = 0.0f;
  switch (motion->kind) {
    case MotionKind::None:
      return home;
    case MotionKind::Swap:
      factor = 1.0f - EaseInOut(t);
      break;
    case MotionKind::Fall:
      // `t * t` starts slow and speeds up, like something dropped.
      factor = 1.0f - t * t;
      break;
    case MotionKind::Bump:
      // Half a sine wave: out to the neighbour and back home again.
      factor = kBumpDistance * std::sin(std::numbers::pi_v<float> * t);
      break;
  }
  return {home.x + motion->offset.x * factor,
          home.y + motion->offset.y * factor};
}

float MotionTracker::Scale(int x, int y, float now) const {
  const Motion* motion = Find(x, y);
  if (motion == nullptr) return 1.0f;
  float scale = 1.0f;
  if (now < motion->pop_start) return 0.0f;  // Not born yet.
  const float pop = (now - motion->pop_start) / kPopSeconds;
  if (pop < 1.0f) scale *= std::max(0.0f, EaseOutBack(pop));
  if (motion->kind == MotionKind::Fall) {
    const float landed = now - (motion->start + motion->duration);
    if (landed > 0.0f && landed < kLandSeconds) {
      scale *= 1.0f - 0.08f * std::sin(std::numbers::pi_v<float> * landed /
                                       kLandSeconds);
    }
  }
  return scale;
}

const MotionTracker::Motion* MotionTracker::Find(int x, int y) const {
  if (x < 0 || x >= columns_ || y < 0 || y >= rows_) return nullptr;
  return &motions_[static_cast<std::size_t>(y * columns_ + x)];
}

MotionTracker::Motion* MotionTracker::Find(int x, int y) {
  // The usual trick to avoid writing the same code twice: call the const
  // overload through a const view of `*this`, then cast the const away.
  // That is safe because `*this` really is non-const in this overload.
  return const_cast<Motion*>(std::as_const(*this).Find(x, y));
}

void Effects::Clear() {
  ghosts_.clear();
  particles_.clear();
  texts_.clear();
  beams_.clear();
  shockwaves_.clear();
}

void Effects::Update(float dt, float now) {
  for (Particle& p : particles_) {
    p.vy += kGravity * dt;
    // Air drag: lose a little speed every frame, scaled by frame time.
    const float drag = std::pow(0.08f, dt);
    p.vx *= drag;
    p.vy *= drag;
    p.x += p.vx * dt;
    p.y += p.vy * dt;
    p.life -= dt;
  }
  // `std::erase_if` (C++20) removes every element the lambda accepts, in one
  // pass. It replaces the older "erase-remove idiom":
  // `v.erase(std::remove_if(v.begin(), v.end(), pred), v.end())`.
  // The lambdas capture `now` by value (`[now]`): a copy of a float, which
  // can never dangle.
  std::erase_if(particles_, [](const Particle& p) { return p.life <= 0.0f; });
  std::erase_if(
      ghosts_, [now](const Ghost& g) { return now - g.start > kGhostSeconds; });
  std::erase_if(texts_, [now](const FloatingText& t) {
    return now - t.start > kTextSeconds;
  });
  std::erase_if(beams_,
                [now](const Beam& b) { return now - b.start > kBeamSeconds; });
  std::erase_if(shockwaves_, [now](const Shockwave& s) {
    return now - s.start > kShockSeconds;
  });
}

void Effects::AddGhost(PlacedGem gem, float now) {
  ghosts_.push_back({gem, now});
}

void Effects::AddBurst(float x, float y, GemColor color, int count,
                       float speed) {
  std::uniform_real_distribution<float> angle(0.0f,
                                              2.0f * std::numbers::pi_v<float>);
  std::uniform_real_distribution<float> spread(0.35f, 1.0f);
  std::uniform_real_distribution<float> life(0.35f, 0.75f);
  std::uniform_real_distribution<float> size(2.0f, 4.5f);
  std::uniform_real_distribution<float> coin(0.0f, 1.0f);
  for (int i = 0; i < count && particles_.size() < kMaxParticles; ++i) {
    const float a = angle(rng_);
    const float s = speed * spread(rng_);
    Particle p;
    p.x = x;
    p.y = y;
    p.vx = std::cos(a) * s;
    // A small upward kick so sparks fountain up before gravity wins.
    p.vy = std::sin(a) * s - speed * 0.35f;
    p.max_life = life(rng_);
    p.life = p.max_life;
    p.size = size(rng_);
    p.color = color;
    p.white = coin(rng_) < 0.25f;
    particles_.push_back(p);
  }
}

void Effects::AddText(FloatingText text) { texts_.push_back(text); }

void Effects::AddBeam(Beam beam) { beams_.push_back(beam); }

void Effects::AddShockwave(Shockwave wave) { shockwaves_.push_back(wave); }

}  // namespace match3
