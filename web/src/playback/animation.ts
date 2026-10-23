// Turns the frames of one C++ response into something that can be drawn at any
// moment. Nothing here creates or merges frames: each frame is shown exactly as
// C++ recorded it. Timing lives in the player (player.ts).
import type { Frame, Snapshot } from '../engine/protocol';
import { focusFor, interpolate, layoutFrames, viewBoxFor, type Layout, type LayoutStrategy, type ViewBox } from '../layout/layout';

export interface Animation<S extends Snapshot> {
  readonly frames: readonly Frame<S>[];
  /** Node positions for each frame, each continuing from the one before. */
  readonly layouts: readonly Layout[];
  /** What was on screen before the operation (null on first load). */
  readonly from: Layout | null;
  /** One viewBox for the whole operation, so the drawing never jumps while it plays. */
  readonly viewBox: ViewBox;
  /** Around the nodes the operation highlights: what to show when everything would be too small. */
  readonly focus: ViewBox | null;
}

export function createAnimation<S extends Snapshot>(
  frames: readonly Frame<S>[],
  from: Layout | null,
  strategy: LayoutStrategy<S>,
): Animation<S> {
  const layouts = layoutFrames(frames, from, strategy);
  const viewBox = viewBoxFor(from ? [from, ...layouts] : layouts, strategy.box);
  return { frames, layouts, from, viewBox, focus: focusFor(frames, layouts, strategy.box) };
}

/**
 * Frame `index` and its node positions, `t` of the way (0 to 1) through the
 * move from the previous frame's positions. Highlights, pointers, output and
 * every node and edge position all come from this one frame and this one
 * layout, so they can never disagree.
 */
export function sample<S extends Snapshot>(animation: Animation<S>, index: number, t: number): { frame: Frame<S>; layout: Layout } {
  const before = index === 0 ? animation.from : animation.layouts[index - 1];
  return { frame: animation.frames[index], layout: interpolate(before, animation.layouts[index], ease(t)) };
}

/** The final layout of an animation: where the next operation starts from. */
export function finalLayout(animation: Animation<Snapshot>): Layout {
  return animation.layouts[animation.layouts.length - 1];
}

function ease(t: number): number {
  return t < 0.5 ? 2 * t * t : 1 - (-2 * t + 2) ** 2 / 2;
}
