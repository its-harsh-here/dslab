// Layout of every tree: x = in-order rank, y = depth. Because
// x follows in-order, a left child is always left of its parent and a right
// child always right of it, even when the parent has only one child.
import type { NodeId, TreeNode, TreeSnapshot } from '../engine/protocol';
import type { LayoutStrategy, Point } from './layout';

export const TREE_RADIUS = 22;
const COLUMN = 60; // horizontal distance between in-order neighbours
const ROW = 84; // vertical distance between depths

/** IDs reachable from the root through left/right links. */
export function treeReachable(snapshot: TreeSnapshot): Set<NodeId> {
  const byId = new Map(snapshot.nodes.map((node) => [node.id, node]));
  const seen = new Set<NodeId>();
  const waiting: NodeId[] = snapshot.root === null ? [] : [snapshot.root];
  while (waiting.length > 0) {
    const id = waiting.pop()!;
    const node = byId.get(id);
    if (!node || seen.has(id)) continue;
    seen.add(id);
    if (node.left !== null) waiting.push(node.left);
    if (node.right !== null) waiting.push(node.right);
  }
  return seen;
}

export const treeLayout: LayoutStrategy<TreeSnapshot> = {
  reachable: treeReachable,

  pack(snapshot) {
    const byId = new Map<NodeId, TreeNode>(snapshot.nodes.map((node) => [node.id, node]));
    const positions = new Map<NodeId, Point>();
    let rank = 0;
    const place = (id: NodeId | null, depth: number) => {
      const node = id === null ? undefined : byId.get(id);
      if (!node || positions.has(node.id)) return;
      place(node.left, depth + 1);
      positions.set(node.id, { x: rank * COLUMN, y: depth * ROW });
      rank += 1;
      place(node.right, depth + 1);
    };
    place(snapshot.root, 0);
    return positions;
  },

  // One row below the first node the step names (the new node's parent),
  // else below the root, else at the top.
  staging(frame, placed) {
    for (const id of Object.values(frame.pointers)) {
      const at = id === null ? undefined : placed.get(id);
      if (at) return { x: at.x, y: at.y + ROW };
    }
    const root = frame.snapshot.root === null ? undefined : placed.get(frame.snapshot.root);
    return root ? { x: root.x, y: root.y + ROW } : { x: 0, y: 0 };
  },

  markers(snapshot, nodes) {
    const root = snapshot.root === null ? undefined : nodes.get(snapshot.root);
    return new Map([['ROOT', root ?? { x: 0, y: 0 }]]);
  },

  // Points are node centres, so the box reaches one radius each way.
  box: { width: TREE_RADIUS, height: TREE_RADIUS + 30, padLeft: TREE_RADIUS + 34, padRight: 70, padTop: 76, padBottom: 12 },
};

// The red-black tree: as above, with room below the lowest nodes for the NIL
// leaves that can be shown (they hang 58 px below a node's centre).
export const redBlackLayout: LayoutStrategy<TreeSnapshot> = {
  ...treeLayout,
  box: { ...treeLayout.box, height: TREE_RADIUS + 40 },
};
