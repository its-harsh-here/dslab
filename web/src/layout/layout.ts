// Where each node is drawn, frame by frame (docs/architecture.md, 3.2).
// Layout only reads the links in a snapshot to place nodes. It never changes a
// link and never decides the outcome of an operation: C++ did that already.
import type { Frame, LinearNode, NodeId, Snapshot } from '../engine/protocol';

export interface Point {
  readonly x: number;
  readonly y: number;
}

export interface Layout {
  readonly nodes: ReadonlyMap<NodeId, Point>;
  /** Named markers such as TOP or FRONT: where each one points. */
  readonly markers: ReadonlyMap<string, Point>;
}

/** Node size and the extra room the drawing needs around the nodes. */
export interface Box {
  readonly width: number;
  readonly height: number;
  readonly padLeft: number;
  readonly padRight: number;
  readonly padTop: number;
  readonly padBottom: number;
}

/** How one kind of structure is laid out. */
export interface LayoutStrategy<S extends Snapshot> {
  /** IDs reachable from the entry pointer(s). */
  reachable(snapshot: S): Set<NodeId>;
  /** Normal positions, used when every node is reachable. */
  pack(snapshot: S): Map<NodeId, Point>;
  /** Where a node with no earlier position waits until it is linked in. */
  staging(frame: Frame<S>, placed: ReadonlyMap<NodeId, Point>): Point;
  /** Marker positions, given the node positions. */
  markers(snapshot: S, nodes: ReadonlyMap<NodeId, Point>): Map<string, Point>;
  readonly box: Box;
}

/** IDs along the `next` links from `entry` (stops safely on a loop). */
export function chainFrom(nodes: readonly LinearNode[], entry: NodeId | null): NodeId[] {
  const nextOf = new Map(nodes.map((node) => [node.id, node.next]));
  const chain: NodeId[] = [];
  const seen = new Set<NodeId>();
  for (let id = entry; id !== null && !seen.has(id); id = nextOf.get(id) ?? null) {
    chain.push(id);
    seen.add(id);
  }
  return chain;
}

/**
 * Positions for one frame:
 *  - when every node is reachable (the structure is consistent), all nodes get
 *    their normal places, so nodes slide when the structure changes shape;
 *  - otherwise (in the middle of an operation) every node keeps its previous
 *    position, so nothing jumps while links are being changed;
 *  - a node with no previous position waits in the structure's staging spot.
 */
export function layoutFrame<S extends Snapshot>(
  frame: Frame<S>,
  previous: Layout | null,
  strategy: LayoutStrategy<S>,
): Layout {
  const { snapshot } = frame;
  const reachable = strategy.reachable(snapshot);
  const consistent = snapshot.nodes.every((node) => reachable.has(node.id));

  let nodes: Map<NodeId, Point>;
  if (consistent || previous === null) {
    nodes = strategy.pack(snapshot);
  } else {
    nodes = new Map();
    for (const node of snapshot.nodes) {
      const kept = previous.nodes.get(node.id);
      if (kept) nodes.set(node.id, kept);
    }
  }
  for (const node of snapshot.nodes) {
    if (!nodes.has(node.id)) {
      nodes.set(node.id, strategy.staging(frame, nodes));
    }
  }
  return { nodes, markers: strategy.markers(snapshot, nodes) };
}

/** Layouts for every frame of one response, each continuing from the one before. */
export function layoutFrames<S extends Snapshot>(
  frames: readonly Frame<S>[],
  start: Layout | null,
  strategy: LayoutStrategy<S>,
): Layout[] {
  const layouts: Layout[] = [];
  let previous = start;
  for (const frame of frames) {
    previous = layoutFrame(frame, previous, strategy);
    layouts.push(previous);
  }
  return layouts;
}

/** A layout part of the way (t from 0 to 1) from `from` to `to`. */
export function interpolate(from: Layout | null, to: Layout, t: number): Layout {
  if (from === null || t >= 1) {
    return to;
  }
  const nodes = new Map<NodeId, Point>();
  for (const [id, end] of to.nodes) {
    nodes.set(id, mix(from.nodes.get(id) ?? end, end, t));
  }
  const markers = new Map<string, Point>();
  for (const [name, end] of to.markers) {
    markers.set(name, mix(from.markers.get(name) ?? end, end, t));
  }
  return { nodes, markers };
}

function mix(a: Point, b: Point, t: number): Point {
  return { x: a.x + (b.x - a.x) * t, y: a.y + (b.y - a.y) * t };
}

export interface ViewBox {
  readonly x: number;
  readonly y: number;
  readonly width: number;
  readonly height: number;
}

/** One viewBox that fits every node and marker of every given layout. */
export function viewBoxFor(layouts: readonly Layout[], box: Box): ViewBox {
  let minX = 0;
  let minY = 0;
  let maxX = 0;
  let maxY = 0;
  for (const layout of layouts) {
    for (const point of [...layout.nodes.values(), ...layout.markers.values()]) {
      minX = Math.min(minX, point.x);
      minY = Math.min(minY, point.y);
      maxX = Math.max(maxX, point.x);
      maxY = Math.max(maxY, point.y);
    }
  }
  const x = minX - box.padLeft;
  const y = minY - box.padTop;
  return {
    x,
    y,
    width: maxX + box.width + box.padRight - x,
    height: maxY + box.height + box.padBottom - y,
  };
}

/**
 * Where an operation acts: one box around every node any of its frames
 * highlights (at that frame's position), or null when nothing is highlighted.
 */
export function focusFor(frames: readonly Frame[], layouts: readonly Layout[], box: Box): ViewBox | null {
  let minX = Infinity;
  let minY = Infinity;
  let maxX = -Infinity;
  let maxY = -Infinity;
  frames.forEach((frame, index) => {
    for (const id of frame.highlight.nodes) {
      const at = layouts[index].nodes.get(id);
      if (!at) continue;
      minX = Math.min(minX, at.x);
      minY = Math.min(minY, at.y);
      maxX = Math.max(maxX, at.x);
      maxY = Math.max(maxY, at.y);
    }
  });
  if (minX === Infinity) return null;
  return { x: minX - box.padLeft, y: minY - box.padTop, width: maxX - minX + box.width + box.padLeft + box.padRight,
    height: maxY - minY + box.height + box.padTop + box.padBottom };
}
