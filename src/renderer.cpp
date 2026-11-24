#include "renderer.h"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "screen.h"
#include "theme.h"

/// @brief Everything that belongs to the Match-3 game.
namespace match3 {

/// @brief Drawing helpers internal to `renderer.cpp`.
///
/// They are free functions rather than private members because they need no
/// Renderer state; keeping them out of the header also means changing one
/// never forces other files to recompile.
namespace {

/// @brief Corner roundness of panels and buttons (fraction of shorter side).
constexpr float kRoundness = 0.18f;
/// @brief Number of segments used to draw each rounded corner.
constexpr int kCornerSegments = 8;
/// @brief Gem radius as a fraction of the cell size.
constexpr float kGemRadius = 0.36f;
/// @brief Seconds a cascade banner stays up.
constexpr float kBannerSeconds = 1.1f;
/// @brief Seconds the "shuffling" banner stays up.
constexpr float kShuffleBannerSeconds = 1.2f;

/// @brief Returns `color` with its alpha scaled by `factor`.
/// @param color Source colour.
/// @param factor `0.0` (transparent) to `1.0` (unchanged).
/// @return Faded copy (the parameter is a copy, so editing it is safe).
Color WithAlpha(Color color, float factor) {
  color.a = static_cast<unsigned char>(static_cast<float>(color.a) *
                                       std::clamp(factor, 0.0f, 1.0f));
  return color;
}

/// @brief Grows `rect` by `amount` on every side (negative shrinks it).
/// @param rect Rectangle to grow.
/// @param amount Pixels to add to each edge.
/// @return The larger rectangle, sharing the same centre.
Rectangle Outset(Rectangle rect, float amount) {
  return {rect.x - amount, rect.y - amount, rect.width + 2.0f * amount,
          rect.height + 2.0f * amount};
}

/// @brief Returns the centre point of `rect`.
/// @param rect Any rectangle.
/// @return Its centre.
Vector2 Centre(Rectangle rect) {
  return {rect.x + rect.width / 2.0f, rect.y + rect.height / 2.0f};
}

/// @brief "Ease out back" curve: overshoots 1.0 a little, then settles.
/// @param t Linear progress in `[0, 1]`.
/// @return Eased progress (peaks around 1.1).
float EaseOutBack(float t) {
  constexpr float kOvershoot = 1.70158f;
  const float u = t - 1.0f;
  return 1.0f + (kOvershoot + 1.0f) * u * u * u + kOvershoot * u * u;
}

/// @brief Draws `text` centred on `centre` with raylib's default font.
/// @param text Null-terminated string (raylib is a C API, so it takes
///        `const char*` rather than `std::string`).
/// @param centre Point to centre the text on.
/// @param size Font size in pixels.
/// @param color Text colour.
void DrawTextCentred(const char* text, Vector2 centre, int size, Color color) {
  const int width = MeasureText(text, size);
  DrawText(text, static_cast<int>(centre.x) - width / 2,
           static_cast<int>(centre.y) - size / 2, size, color);
}

/// @brief Draws centred text with a dark drop shadow, for text over gems.
/// @param text Null-terminated string.
/// @param centre Point to centre the text on.
/// @param size Font size in pixels.
/// @param color Text colour.
void DrawTextShadowed(const char* text, Vector2 centre, int size, Color color) {
  const float drop = std::max(2.0f, static_cast<float>(size) * 0.06f);
  DrawTextCentred(text, {centre.x + drop, centre.y + drop}, size,
                  Color{10, 8, 20, color.a});
  DrawTextCentred(text, centre, size, color);
}

/// @brief Fills the outline of a gem's shape. Each colour has its own shape
/// so colour-blind players can tell gems apart.
/// @param centre Shape centre.
/// @param radius Size (roughly the radius of a circle around the shape).
/// @param color Which gem's shape to draw.
/// @param tint Fill colour.
void DrawGemShape(Vector2 centre, float radius, GemColor color, Color tint) {
  // `DrawPoly(centre, sides, radius, rotation, colour)` draws a regular
  // polygon. Its first corner sits at `rotation` degrees, measured
  // clockwise from "pointing right" (screen y grows downwards), so -90
  // puts a corner at the top.
  switch (color) {
    case GemColor::Red:
      DrawCircleV(centre, radius * 0.92f, tint);
      break;
    case GemColor::Orange: {
      const float side = radius * 1.6f;
      DrawRectangleRounded(
          {centre.x - side / 2.0f, centre.y - side / 2.0f, side, side}, 0.35f,
          kCornerSegments, tint);
      break;
    }
    case GemColor::Yellow:
      DrawPoly(centre, 4, radius * 1.08f, 0.0f, tint);
      break;
    case GemColor::Green:
      // A triangle's centre of mass is below the middle of its bounding
      // box, so nudge it down to look centred in the cell.
      DrawPoly({centre.x, centre.y + radius * 0.14f}, 3, radius * 1.15f, -90.0f,
               tint);
      break;
    case GemColor::Blue:
      DrawPoly(centre, 6, radius * 1.0f, 0.0f, tint);
      break;
    case GemColor::Purple:
      DrawPoly({centre.x, centre.y + radius * 0.05f}, 5, radius * 1.02f, -90.0f,
               tint);
      break;
  }
}

/// @brief Draws a Rainbow gem: a spinning wheel of every gem colour.
/// @param centre Gem centre.
/// @param radius Gem radius.
/// @param now Animation clock, for the spin.
/// @param alpha Overall opacity.
void DrawRainbow(Vector2 centre, float radius, float now, float alpha) {
  DrawCircleV(centre, radius * 1.15f,
              WithAlpha(WHITE, (0.12f + 0.06f * std::sin(now * 5.0f)) * alpha));
  const float spin = now * 90.0f;
  for (int i = 0; i < kGemColorCount; ++i) {
    const float start = spin + static_cast<float>(i) * 60.0f;
    DrawCircleSector(centre, radius * 0.95f, start, start + 60.0f, 8,
                     WithAlpha(kGemColors[static_cast<std::size_t>(i)], alpha));
  }
  DrawCircleV(centre, radius * 0.42f,
              WithAlpha(Color{255, 250, 240, 255}, alpha));
  DrawRing(centre, radius * 0.92f, radius * 1.0f, 0.0f, 360.0f, 40,
           WithAlpha(WHITE, 0.8f * alpha));
}

/// @brief Draws the white stripes, glow, or core that mark a special gem.
/// @param centre Gem centre.
/// @param radius Gem radius.
/// @param special Which special gem.
/// @param now Animation clock, for pulsing.
/// @param alpha Overall opacity.
void DrawSpecialMark(Vector2 centre, float radius, Special special, float now,
                     float alpha) {
  const float pulse = 0.5f + 0.5f * std::sin(now * 6.0f);
  switch (special) {
    case Special::None:
    case Special::Rainbow:
      break;
    case Special::LineH:
    case Special::LineV: {
      const bool horizontal = special == Special::LineH;
      for (int k = -1; k <= 1; ++k) {
        const float offset = static_cast<float>(k) * radius * 0.34f;
        const float length = radius * (k == 0 ? 1.5f : 1.2f);
        const float thick = radius * 0.13f;
        const Rectangle bar =
            horizontal
                ? Rectangle{centre.x - length / 2.0f,
                            centre.y + offset - thick / 2.0f, length, thick}
                : Rectangle{centre.x + offset - thick / 2.0f,
                            centre.y - length / 2.0f, thick, length};
        DrawRectangleRounded(bar, 1.0f, 4,
                             WithAlpha(WHITE, (0.7f + 0.3f * pulse) * alpha));
      }
      break;
    }
    case Special::Bomb:
      DrawRing(centre, radius * (1.02f + 0.06f * pulse), radius * 1.16f, 0.0f,
               360.0f, 40, WithAlpha(Color{255, 236, 179, 255}, 0.7f * alpha));
      DrawCircleV(centre, radius * 0.44f,
                  WithAlpha(Color{22, 16, 34, 235}, alpha));
      DrawCircleV(centre, radius * (0.16f + 0.05f * pulse),
                  WithAlpha(Color{255, 214, 120, 255}, alpha));
      break;
  }
}

/// @brief Draws one gem: shadow, rim, body, facet, glint, and special mark.
/// @param centre Gem centre in pixels.
/// @param radius Gem radius in pixels (`0` draws nothing).
/// @param gem Which gem.
/// @param now Animation clock, for specials.
/// @param alpha Overall opacity.
/// @param flash `0..1` white flash drawn over the gem (used as it clears).
void DrawGem(Vector2 centre, float radius, Gem gem, float now, float alpha,
             float flash) {
  if (radius <= 0.5f) return;
  if (gem.special == Special::Rainbow) {
    DrawRainbow(centre, radius, now, alpha);
  } else {
    const Color base = GemTint(gem.color);
    DrawGemShape({centre.x, centre.y + radius * 0.12f}, radius, gem.color,
                 WithAlpha(Color{0, 0, 0, 255}, 0.35f * alpha));
    // `ColorBrightness` darkens with a negative factor and lightens with a
    // positive one.
    DrawGemShape(centre, radius, gem.color,
                 WithAlpha(ColorBrightness(base, -0.35f), alpha));
    DrawGemShape(centre, radius * 0.84f, gem.color, WithAlpha(base, alpha));
    DrawGemShape({centre.x - radius * 0.1f, centre.y - radius * 0.12f},
                 radius * 0.48f, gem.color,
                 WithAlpha(ColorBrightness(base, 0.45f), 0.55f * alpha));
    DrawCircleV({centre.x - radius * 0.34f, centre.y - radius * 0.36f},
                radius * 0.12f, WithAlpha(WHITE, 0.8f * alpha));
    DrawSpecialMark(centre, radius, gem.special, now, alpha);
  }
  if (flash > 0.0f) {
    DrawGemShape(centre, radius, gem.color, WithAlpha(WHITE, flash * alpha));
  }
}

/// @brief Draws a rounded outline around a cell.
/// @param layout Board placement.
/// @param cell Cell to outline.
/// @param color Outline colour.
/// @param thick Line thickness in pixels.
void OutlineCell(const BoardLayout& layout, GridPoint cell, Color color,
                 float thick) {
  const Rectangle rect = Outset(layout.CellRect(cell.x, cell.y), -3.0f);
  DrawRectangleRoundedLinesEx(rect, 0.25f, kCornerSegments, thick, color);
}

/// @brief Draws the board frame and its checkered cells, plus the hover,
/// hint, and selection highlights that sit under the gems.
/// @param board Board to draw.
/// @param layout Board placement (already shaken).
/// @param view UI state.
void DrawBoardBackground(const Board& board, const BoardLayout& layout,
                         const FrameView& view) {
  DrawRectangleRounded(Outset(layout.Bounds(), 14.0f), 0.04f, kCornerSegments,
                       kBoardFrame);
  for (int y = 0; y < board.height(); ++y) {
    for (int x = 0; x < board.width(); ++x) {
      const bool light = (x + y) % 2 == 0;
      Color fill = light ? kCellLight : kCellDark;
      if (view.hover && *view.hover == GridPoint{x, y}) fill = kCellHover;
      DrawRectangleRounded(Outset(layout.CellRect(x, y), -2.0f), 0.2f,
                           kCornerSegments, fill);
    }
  }
  if (view.hint) {
    const float pulse = 0.5f + 0.5f * std::sin(view.now * 7.0f);
    for (const GridPoint cell : {view.hint->a, view.hint->b}) {
      DrawRectangleRounded(Outset(layout.CellRect(cell.x, cell.y), -3.0f),
                           0.25f, kCornerSegments,
                           WithAlpha(kHint, 0.12f + 0.18f * pulse));
    }
  }
  if (view.selected) {
    const GridPoint s = *view.selected;
    DrawRectangleRounded(Outset(layout.CellRect(s.x, s.y), -3.0f), 0.25f,
                         kCornerSegments, WithAlpha(kAccent, 0.3f));
  }
}

/// @brief Draws every gem at its animated position.
/// @param board Board to draw.
/// @param layout Board placement (already shaken).
/// @param motion Gem slides and falls.
/// @param view UI state.
void DrawGems(const Board& board, const BoardLayout& layout,
              const MotionTracker& motion, const FrameView& view) {
  const float radius = layout.cell_size * kGemRadius;
  for (int y = 0; y < board.height(); ++y) {
    for (int x = 0; x < board.width(); ++x) {
      const std::optional<Gem>& gem = board.At({x, y});
      if (!gem) continue;
      const CellPosition pos = motion.Position(x, y, view.now);
      float scale = motion.Scale(x, y, view.now);
      // The selected gem breathes so it's obvious what is picked up.
      if (view.selected && *view.selected == GridPoint{x, y}) {
        scale *= 1.08f + 0.05f * std::sin(view.now * 8.0f);
      }
      DrawGem(layout.CellCentre(pos.x, pos.y), radius * scale, *gem, view.now,
              1.0f, 0.0f);
    }
  }
}

/// @brief Draws cleared gems shrinking away with a white flash.
/// @param layout Board placement (already shaken).
/// @param effects Effect lists.
/// @param now Animation clock.
void DrawGhosts(const BoardLayout& layout, const Effects& effects, float now) {
  const float radius = layout.cell_size * kGemRadius;
  for (const Ghost& ghost : effects.ghosts()) {
    const float t =
        std::clamp((now - ghost.start) / Effects::kGhostSeconds, 0.0f, 1.0f);
    // Swell a little for the first quarter, then shrink to nothing.
    const float scale =
        t < 0.25f ? 1.0f + 0.8f * t : 1.2f * (1.0f - (t - 0.25f) / 0.75f);
    const Vector2 centre = layout.CellCentre(
        static_cast<float>(ghost.gem.at.x), static_cast<float>(ghost.gem.at.y));
    DrawGem(centre, radius * scale, ghost.gem.gem, now, 1.0f - t * t,
            std::max(0.0f, 1.0f - t * 2.5f));
  }
}

/// @brief Draws Line beams and Bomb / Rainbow shockwaves.
/// @param layout Board placement (already shaken).
/// @param effects Effect lists.
/// @param now Animation clock.
void DrawBlasts(const BoardLayout& layout, const Effects& effects, float now) {
  const Rectangle bounds = layout.Bounds();
  for (const Beam& beam : effects.beams()) {
    const float t =
        std::clamp((now - beam.start) / Effects::kBeamSeconds, 0.0f, 1.0f);
    const float thick = layout.cell_size * 0.8f * (1.0f - t);
    const Vector2 c = layout.CellCentre(static_cast<float>(beam.origin.x),
                                        static_cast<float>(beam.origin.y));
    if (beam.horizontal) {
      DrawRectangleRec({bounds.x, c.y - thick / 2.0f, bounds.width, thick},
                       WithAlpha(kAccent, 0.45f * (1.0f - t)));
      DrawRectangleRec(
          {bounds.x, c.y - thick / 6.0f, bounds.width, thick / 3.0f},
          WithAlpha(WHITE, 0.9f * (1.0f - t)));
    } else {
      DrawRectangleRec({c.x - thick / 2.0f, bounds.y, thick, bounds.height},
                       WithAlpha(kAccent, 0.45f * (1.0f - t)));
      DrawRectangleRec(
          {c.x - thick / 6.0f, bounds.y, thick / 3.0f, bounds.height},
          WithAlpha(WHITE, 0.9f * (1.0f - t)));
    }
  }
  for (const Shockwave& wave : effects.shockwaves()) {
    const float t =
        std::clamp((now - wave.start) / Effects::kShockSeconds, 0.0f, 1.0f);
    const float eased = 1.0f - (1.0f - t) * (1.0f - t);
    const float outer = layout.cell_size * (0.3f + wave.radius * eased);
    const float thick = layout.cell_size * 0.28f * (1.0f - t) + 2.0f;
    const Vector2 c = layout.CellCentre(static_cast<float>(wave.at.x),
                                        static_cast<float>(wave.at.y));
    if (wave.rainbow) {
      // Rings in every gem colour, one inside the next.
      for (int i = 0; i < kGemColorCount; ++i) {
        const float r = outer * (1.0f - static_cast<float>(i) * 0.08f);
        DrawRing(c, std::max(0.0f, r - thick * 0.5f), r, 0.0f, 360.0f, 48,
                 WithAlpha(kGemColors[static_cast<std::size_t>(i)], 1.0f - t));
      }
    } else {
      DrawCircleV(c, outer,
                  WithAlpha(Color{255, 236, 179, 255}, 0.18f * (1.0f - t)));
      DrawRing(c, std::max(0.0f, outer - thick), outer, 0.0f, 360.0f, 48,
               WithAlpha(Color{255, 236, 179, 255}, 1.0f - t));
    }
  }
}

/// @brief Draws sparks and floating score numbers.
/// @param layout Board placement (not shaken, so they don't jitter).
/// @param effects Effect lists.
/// @param now Animation clock.
void DrawParticlesAndText(const BoardLayout& layout, const Effects& effects,
                          float now) {
  for (const Particle& p : effects.particles()) {
    const float life = std::clamp(p.life / p.max_life, 0.0f, 1.0f);
    const Color color =
        p.white ? WHITE : ColorBrightness(GemTint(p.color), 0.2f);
    DrawCircleV({p.x, p.y}, p.size * (0.4f + 0.6f * life),
                WithAlpha(color, life));
  }
  for (const FloatingText& text : effects.texts()) {
    const float t =
        std::clamp((now - text.start) / Effects::kTextSeconds, 0.0f, 1.0f);
    const Vector2 c = layout.CellCentre(text.at.x, text.at.y);
    const int size = 26 + 6 * std::min(text.cascade - 1, 4);
    // `TextFormat` is raylib's printf: it writes into a small internal
    // buffer and returns a pointer to it. The pointer is only good until
    // the next few TextFormat calls, so use it straight away.
    DrawTextShadowed(TextFormat("+%d", text.points), {c.x, c.y - t * 60.0f},
                     size, WithAlpha(kText, 1.0f - t * t));
  }
}

/// @brief Returns the cheer word for a cascade step.
/// @param cascade Cascade step (2 or more).
/// @return Pointer to a static string literal.
const char* CheerFor(int cascade) {
  if (cascade >= 6) return "UNSTOPPABLE!";
  if (cascade == 5) return "INCREDIBLE!";
  if (cascade == 4) return "AMAZING!";
  if (cascade == 3) return "GREAT!";
  return "NICE!";
}

/// @brief Returns the banner name for a combo swap.
/// @param combo Combo that went off.
/// @return Text to show, or `nullptr` for `Combo::None`. The strings are
///         literals with static storage, so the pointer never dangles.
const char* ComboName(Combo combo) {
  switch (combo) {
    case Combo::None:
      return nullptr;
    case Combo::Cross:
      return "CROSSFIRE!";
    case Combo::BigCross:
      return "MEGA CROSS!";
    case Combo::MegaBomb:
      return "MEGA BOMB!";
    case Combo::RainbowLine:
      return "LINE STORM!";
    case Combo::RainbowBomb:
      return "BOMB STORM!";
    case Combo::DoubleRainbow:
      return "BOARD WIPE!";
  }
  return nullptr;
}

/// @brief Draws the cascade cheer and the "shuffling" banner over the board.
/// @param layout Board placement.
/// @param view UI state.
void DrawBanners(const BoardLayout& layout, const FrameView& view) {
  const Vector2 centre = Centre(layout.Bounds());
  const float since = view.now - view.banner_start;
  const float since_combo = view.now - view.combo_start;
  const bool cascade_up = view.banner_cascade >= 2 && since < kBannerSeconds;
  const bool combo_up = view.combo != Combo::None && since_combo >= 0.0f &&
                        since_combo < kBannerSeconds;
  // Only one banner fits over the board, so the newer one wins. A combo
  // happens on the swap's own clear, so any cascade banner after it is
  // newer and takes over.
  const bool show_combo =
      combo_up && (!cascade_up || view.combo_start >= view.banner_start);
  // `ColorFromHSV` picks a colour by hue angle; sweeping the hue over
  // time makes the text cycle through the rainbow.
  const Color hue =
      ColorFromHSV(std::fmod(view.now * 240.0f, 360.0f), 0.45f, 1.0f);
  if (show_combo) {
    const float grow = EaseOutBack(std::min(since_combo / 0.25f, 1.0f));
    const float fade =
        std::clamp((kBannerSeconds - since_combo) / 0.3f, 0.0f, 1.0f);
    DrawTextShadowed(ComboName(view.combo), {centre.x, centre.y - 24.0f},
                     static_cast<int>(60.0f * grow), WithAlpha(hue, fade));
    DrawTextShadowed("COMBO", {centre.x, centre.y + 30.0f},
                     static_cast<int>(26.0f * grow), WithAlpha(kText, fade));
  } else if (cascade_up) {
    const float grow = EaseOutBack(std::min(since / 0.25f, 1.0f));
    const float fade = std::clamp((kBannerSeconds - since) / 0.3f, 0.0f, 1.0f);
    DrawTextShadowed(CheerFor(view.banner_cascade),
                     {centre.x, centre.y - 24.0f},
                     static_cast<int>(60.0f * grow), WithAlpha(hue, fade));
    DrawTextShadowed(TextFormat("CASCADE x%d", view.banner_cascade),
                     {centre.x, centre.y + 30.0f},
                     static_cast<int>(26.0f * grow), WithAlpha(kText, fade));
  }
  const float shuffle = view.now - view.shuffle_start;
  if (shuffle >= 0.0f && shuffle < kShuffleBannerSeconds) {
    const float fade =
        std::clamp((kShuffleBannerSeconds - shuffle) / 0.3f, 0.0f, 1.0f);
    DrawRectangleRec(
        {layout.origin_x, centre.y - 30.0f, layout.Bounds().width, 60.0f},
        WithAlpha(Color{10, 8, 20, 255}, 0.75f * fade));
    DrawTextCentred("NO MOVES LEFT - SHUFFLING", centre, 28,
                    WithAlpha(kText, fade));
  }
}

/// @brief Draws the pause cover or the game-over card over the board.
/// @param layout Board placement.
/// @param view UI state.
void DrawOverlays(const BoardLayout& layout, const FrameView& view) {
  const Rectangle cover = Outset(layout.Bounds(), 14.0f);
  const Vector2 centre = Centre(cover);
  if (view.paused) {
    // Opaque, so pausing can't be used to study the board for free.
    DrawRectangleRounded(cover, 0.04f, kCornerSegments, kBoardFrame);
    DrawTextCentred("PAUSED", {centre.x, centre.y - 20.0f}, 56, kText);
    DrawTextCentred("Press P to resume", {centre.x, centre.y + 30.0f}, 22,
                    kTextMuted);
    return;
  }
  if (!view.game_over) return;
  const float fade =
      std::clamp((view.now - view.over_start) / 0.4f, 0.0f, 1.0f);
  DrawRectangleRounded(cover, 0.04f, kCornerSegments,
                       WithAlpha(Color{8, 6, 16, 255}, 0.78f * fade));
  const char* title = view.mode == Mode::Blitz ? "TIME'S UP" : "OUT OF MOVES";
  DrawTextShadowed(title, {centre.x, centre.y - 120.0f}, 56,
                   WithAlpha(kText, fade));
  DrawTextCentred("FINAL SCORE", {centre.x, centre.y - 50.0f}, 20,
                  WithAlpha(kTextMuted, fade));
  DrawTextShadowed(TextFormat("%d", view.score), {centre.x, centre.y + 4.0f},
                   72, WithAlpha(kScoreDigits, fade));
  if (view.new_best) {
    const float pulse = 0.75f + 0.25f * std::sin(view.now * 6.0f);
    DrawTextCentred("NEW BEST!", {centre.x, centre.y + 66.0f}, 30,
                    WithAlpha(kSuccess, fade * pulse));
  } else {
    DrawTextCentred(TextFormat("BEST  %d", view.record.best_score),
                    {centre.x, centre.y + 66.0f}, 24,
                    WithAlpha(kTextMuted, fade));
  }
  DrawTextCentred(TextFormat("Longest cascade  x%d", view.best_cascade),
                  {centre.x, centre.y + 106.0f}, 20,
                  WithAlpha(kTextMuted, fade));
  DrawTextCentred("Click the board or press R to play again",
                  {centre.x, centre.y + 150.0f}, 20, WithAlpha(kText, fade));
}

/// @brief Draws a button with a hover highlight.
/// @param rect Button bounds.
/// @param label Button text.
/// @param mouse Mouse position.
/// @param fill Fill colour when not hovered.
/// @param text Text colour.
void DrawButton(Rectangle rect, const char* label, Vector2 mouse, Color fill,
                Color text) {
  const bool hover = CheckCollisionPointRec(mouse, rect);
  DrawRectangleRounded(rect, 0.3f, kCornerSegments,
                       hover ? ColorBrightness(fill, 0.15f) : fill);
  DrawTextCentred(label, Centre(rect), 20, text);
}

/// @brief Draws a sidebar panel with a small caption at the top.
/// @param rect Panel bounds.
/// @param caption Caption text.
void DrawPanel(Rectangle rect, const char* caption) {
  DrawRectangleRounded(rect, kRoundness, kCornerSegments, kBackground);
  DrawText(caption, static_cast<int>(rect.x + 16.0f),
           static_cast<int>(rect.y + 14.0f), 16, kTextMuted);
}

/// @brief Draws the sidebar: title, mode tabs, score, limit, best, buttons,
/// and the controls cheat sheet.
/// @param view UI state.
void DrawSidebar(const FrameView& view) {
  const Rectangle side = SidebarRect();
  DrawRectangleRounded(side, 0.06f, kCornerSegments, kPanel);

  // Title, one letter per gem colour.
  const char* title = "MATCH-3";
  constexpr int kTitleSize = 44;
  const int title_width = MeasureText(title, kTitleSize);
  float x = side.x + (side.width - static_cast<float>(title_width)) / 2.0f;
  // `std::strlen` counts characters up to the terminating '\0'.
  for (std::size_t i = 0; i < std::strlen(title); ++i) {
    // A two-character buffer ("M" + '\0') because DrawText wants a C string.
    const char letter[2] = {title[i], '\0'};
    const float bob =
        3.0f * std::sin(view.now * 3.0f + static_cast<float>(i) * 0.6f);
    DrawText(letter, static_cast<int>(x),
             static_cast<int>(side.y + 22.0f + bob), kTitleSize,
             kGemColors[i % kGemColors.size()]);
    // Spacing matches what MeasureText assumes: font size / 10 between
    // letters for raylib's default font.
    x += static_cast<float>(MeasureText(letter, kTitleSize) + kTitleSize / 10);
  }

  for (int i = 0; i < kModeCount; ++i) {
    const auto mode = static_cast<Mode>(i);
    const Rectangle tab = ModeTabRect(mode);
    const bool active = mode == view.mode;
    const bool hover = CheckCollisionPointRec(view.mouse, tab);
    const Color fill = active ? kAccent : (hover ? kPanelHover : kBackground);
    DrawRectangleRounded(tab, 0.3f, kCornerSegments, fill);
    DrawTextCentred(ModeName(mode), Centre(tab), 18,
                    active ? Color{24, 18, 40, 255} : kText);
  }

  const Rectangle score = ScorePanelRect();
  DrawPanel(score, "SCORE");
  DrawTextCentred(TextFormat("%d", view.score),
                  {score.x + score.width / 2.0f, score.y + 66.0f}, 56,
                  kScoreDigits);

  const Rectangle limit = LimitPanelRect();
  const Vector2 limit_value{limit.x + limit.width / 2.0f, limit.y + 54.0f};
  switch (view.mode) {
    case Mode::Moves: {
      DrawPanel(limit, "MOVES LEFT");
      DrawTextCentred(TextFormat("%d", view.moves_left), limit_value, 48,
                      view.moves_left <= 5 ? kDanger : kText);
      break;
    }
    case Mode::Blitz: {
      DrawPanel(limit, "TIME");
      const int seconds = static_cast<int>(std::ceil(view.time_left));
      const bool low = view.time_left <= 10.0f;
      DrawTextCentred(TextFormat("%d:%02d", seconds / 60, seconds % 60),
                      {limit_value.x, limit_value.y - 6.0f}, 40,
                      low ? kDanger : kText);
      const float total = RulesFor(Mode::Blitz).seconds;
      const Rectangle bar{limit.x + 16.0f, limit.y + limit.height - 20.0f,
                          limit.width - 32.0f, 8.0f};
      DrawRectangleRounded(bar, 1.0f, 4, kPanel);
      DrawRectangleRounded(
          {bar.x, bar.y,
           bar.width * std::clamp(view.time_left / total, 0.0f, 1.0f),
           bar.height},
          1.0f, 4, low ? kDanger : kAccent);
      if (!view.clock_running) {
        DrawText("starts on first swap",
                 static_cast<int>(limit.x + limit.width - 168.0f),
                 static_cast<int>(limit.y + 15.0f), 14, kTextMuted);
      }
      break;
    }
    case Mode::Zen: {
      DrawPanel(limit, "LONGEST CASCADE");
      DrawTextCentred(TextFormat("x%d", view.best_cascade), limit_value, 48,
                      kText);
      break;
    }
  }

  const Rectangle best = BestPanelRect();
  DrawPanel(best, TextFormat("BEST %s", ModeName(view.mode)));
  DrawText(TextFormat("%d", view.record.best_score),
           static_cast<int>(best.x + 16.0f), static_cast<int>(best.y + 38.0f),
           36, kText);
  DrawText(TextFormat("%d games", view.record.games),
           static_cast<int>(best.x + best.width - 110.0f),
           static_cast<int>(best.y + 40.0f), 16, kTextMuted);
  DrawText(TextFormat("chain x%d", view.record.best_cascade),
           static_cast<int>(best.x + best.width - 110.0f),
           static_cast<int>(best.y + 60.0f), 16, kTextMuted);

  DrawButton(NewGameButtonRect(), "NEW GAME", view.mouse, kAccent,
             Color{24, 18, 40, 255});
  DrawButton(HintButtonRect(), "HINT", view.mouse, kBackground, kText);
  DrawButton(SoundButtonRect(), view.muted ? "SOUND OFF" : "SOUND ON",
             view.mouse, kBackground, view.muted ? kTextMuted : kText);

  // `constexpr std::array` of C strings: a fixed list known at compile time.
  constexpr std::array<const char*, 4> kHelp{
      "Drag a gem, or click two neighbours",
      "Arrows move, Space picks up",
      "H hint   P pause   M sound",
      "R new game   1 / 2 / 3 mode",
  };
  float help_y = HintButtonRect().y + HintButtonRect().height + 22.0f;
  for (const char* line : kHelp) {
    DrawText(line, static_cast<int>(side.x + 22.0f), static_cast<int>(help_y),
             15, kTextMuted);
    help_y += 21.0f;
  }
}

}  // namespace

void Renderer::Draw(const Board& board, const BoardLayout& layout,
                    const MotionTracker& motion, const Effects& effects,
                    const FrameView& view) const {
  // A vertical gradient behind everything.
  DrawRectangleGradientV(0, 0, kScreenWidth, kScreenHeight, kBackgroundTop,
                         kBackground);

  // A copy of the layout moved by the shake offset. Board drawing uses the
  // copy; input keeps using the real layout, so a shake never misplaces a
  // click.
  BoardLayout shaken = layout;
  shaken.origin_x += view.shake.x;
  shaken.origin_y += view.shake.y;

  DrawBoardBackground(board, shaken, view);

  // Scissor mode clips drawing to a rectangle: new gems start above the
  // board and slide in, and they must stay hidden until they cross the top
  // edge.
  const Rectangle clip = shaken.Bounds();
  BeginScissorMode(static_cast<int>(clip.x), static_cast<int>(clip.y),
                   static_cast<int>(clip.width), static_cast<int>(clip.height));
  DrawGems(board, shaken, motion, view);
  DrawGhosts(shaken, effects, view.now);
  DrawBlasts(shaken, effects, view.now);
  EndScissorMode();

  if (view.hint) {
    const float pulse = 0.5f + 0.5f * std::sin(view.now * 7.0f);
    OutlineCell(shaken, view.hint->a, WithAlpha(kHint, 0.5f + 0.5f * pulse),
                3.0f);
    OutlineCell(shaken, view.hint->b, WithAlpha(kHint, 0.5f + 0.5f * pulse),
                3.0f);
  }
  if (view.selected) OutlineCell(shaken, *view.selected, kAccent, 4.0f);
  if (view.cursor) OutlineCell(shaken, *view.cursor, kCursor, 3.0f);

  DrawParticlesAndText(layout, effects, view.now);
  DrawBanners(layout, view);
  DrawOverlays(layout, view);
  DrawSidebar(view);
}

}  // namespace match3
