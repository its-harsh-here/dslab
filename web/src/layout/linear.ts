// Layouts of the linear structures: the stack (vertical), and the queue and
// list (horizontal).
import type {
  CircularListSnapshot,
  DequeSnapshot,
  DoublyListSnapshot,
  HashTableSnapshot,
  ListSnapshot,
  NodeId,
  QueueSnapshot,
  StackSnapshot,
} from '../engine/protocol';
import { chainFrom, type LayoutStrategy, type Point } from './layout';

// ---------------------------------------------------------------------------
// Stack: drawn bottom-up. The bottom node is at y = 0 and each node above it
// is one row higher (more negative y), so a push adds a row on top.

export const STACK_NODE = { width: 120, height: 40 };
const STACK_ROW = 64;
const STACK_STAGING_DX = 180; // how far right a pushed node waits before it is linked in

export const stackLayout: LayoutStrategy<StackSnapshot> = {
  reachable: (snapshot) => new Set(chainFrom(snapshot.nodes, snapshot.top)),

  pack(snapshot) {
    const chain = chainFrom(snapshot.nodes, snapshot.top);
    const positions = new Map<number, Point>();
    chain.forEach((id, index) => {
      const slotFromBottom = chain.length - 1 - index;
      positions.set(id, { x: 0, y: -slotFromBottom * STACK_ROW });
    });
    return positions;
  },

  // To the right of the slot above the current top, at the height it will end up.
  staging(frame, placed) {
    const top = frame.snapshot.top === null ? undefined : placed.get(frame.snapshot.top);
    return top ? { x: STACK_STAGING_DX, y: top.y - STACK_ROW } : { x: STACK_STAGING_DX, y: 0 };
  },

  markers(snapshot, nodes) {
    const top = snapshot.top === null ? undefined : nodes.get(snapshot.top);
    return new Map([['TOP', top ?? { x: 0, y: 0 }]]);
  },

  box: { ...STACK_NODE, padLeft: 110, padRight: 90, padTop: 24, padBottom: 56 },
};

// ---------------------------------------------------------------------------
// Queue: drawn left to right from front to rear.

export const CHAIN_NODE = { width: 96, height: 40 };
export const CHAIN_SLOT = 150; // distance between neighbouring nodes
const QUEUE_STAGING_Y = 130; // the row where a new node waits

export const queueLayout: LayoutStrategy<QueueSnapshot> = {
  reachable: (snapshot) => new Set(chainFrom(snapshot.nodes, snapshot.front)),

  pack(snapshot) {
    const positions = new Map<number, Point>();
    chainFrom(snapshot.nodes, snapshot.front).forEach((id, index) => {
      positions.set(id, { x: index * CHAIN_SLOT, y: 0 });
    });
    return positions;
  },

  // Below the slot after the current rear, where the new node will end up.
  staging(frame, placed) {
    const rear = frame.snapshot.rear === null ? undefined : placed.get(frame.snapshot.rear);
    return rear ? { x: rear.x + CHAIN_SLOT, y: QUEUE_STAGING_Y } : { x: 0, y: QUEUE_STAGING_Y };
  },

  markers(snapshot, nodes) {
    const front = snapshot.front === null ? undefined : nodes.get(snapshot.front);
    const rear = snapshot.rear === null ? undefined : nodes.get(snapshot.rear);
    return new Map([
      ['FRONT', front ?? { x: 0, y: 0 }],
      ['REAR', rear ?? { x: 0, y: 0 }],
    ]);
  },

  box: { ...CHAIN_NODE, padLeft: 24, padRight: 110, padTop: 56, padBottom: 72 },
};

// ---------------------------------------------------------------------------
// Singly linked list: drawn left to right from head. TAIL marks the last
// reachable node; the list itself stores only head.

const LIST_STAGING_Y = 150;

export const listLayout: LayoutStrategy<ListSnapshot> = {
  reachable: (snapshot) => new Set(chainFrom(snapshot.nodes, snapshot.head)),

  pack(snapshot) {
    const positions = new Map<number, Point>();
    chainFrom(snapshot.nodes, snapshot.head).forEach((id, index) => {
      positions.set(id, { x: index * CHAIN_SLOT, y: 0 });
    });
    return positions;
  },

  // Below the gap after "prev" (insert in the middle), else below the head.
  staging(frame, placed) {
    for (const id of Object.values(frame.pointers)) {
      const at = id === null ? undefined : placed.get(id);
      if (at) return { x: at.x + CHAIN_SLOT / 2, y: LIST_STAGING_Y };
    }
    const head = frame.snapshot.head === null ? undefined : placed.get(frame.snapshot.head);
    return head ? { x: head.x, y: LIST_STAGING_Y } : { x: 0, y: LIST_STAGING_Y };
  },

  markers(snapshot, nodes) {
    const chain = chainFrom(snapshot.nodes, snapshot.head);
    const head = chain.length > 0 ? nodes.get(chain[0]) : undefined;
    const tail = chain.length > 0 ? nodes.get(chain[chain.length - 1]) : undefined;
    return new Map([
      ['HEAD', head ?? { x: 0, y: 0 }],
      ['TAIL', tail ?? { x: 0, y: 0 }],
    ]);
  },

  box: { ...CHAIN_NODE, padLeft: 24, padRight: 110, padTop: 56, padBottom: 86 },
};

// ---------------------------------------------------------------------------
// Deque: drawn like the queue, front to rear. A node pushed at the front
// waits below the slot before the front; one pushed at the rear, below the
// slot after the rear.

export const dequeLayout: LayoutStrategy<DequeSnapshot> = {
  reachable: (snapshot) => new Set(chainFrom(snapshot.nodes, snapshot.front)),

  pack(snapshot) {
    const positions = new Map<number, Point>();
    chainFrom(snapshot.nodes, snapshot.front).forEach((id, index) => {
      positions.set(id, { x: index * CHAIN_SLOT, y: 0 });
    });
    return positions;
  },

  staging(frame, placed) {
    const atFront = frame.stepId.startsWith('deque.pushFront');
    const end = atFront ? frame.snapshot.front : frame.snapshot.rear;
    const at = end === null ? undefined : placed.get(end);
    if (!at) return { x: 0, y: QUEUE_STAGING_Y };
    return { x: at.x + (atFront ? -CHAIN_SLOT : CHAIN_SLOT), y: QUEUE_STAGING_Y };
  },

  markers(snapshot, nodes) {
    const front = snapshot.front === null ? undefined : nodes.get(snapshot.front);
    const rear = snapshot.rear === null ? undefined : nodes.get(snapshot.rear);
    return new Map([
      ['FRONT', front ?? { x: 0, y: 0 }],
      ['REAR', rear ?? { x: 0, y: 0 }],
    ]);
  },

  box: { ...CHAIN_NODE, padLeft: 70, padRight: 110, padTop: 56, padBottom: 72 },
};

// ---------------------------------------------------------------------------
// Doubly linked list: drawn like the singly linked list; TAIL is a real
// pointer here, so its marker follows snapshot.tail.

export const doublyListLayout: LayoutStrategy<DoublyListSnapshot> = {
  reachable: (snapshot) => new Set(chainFrom(snapshot.nodes, snapshot.head)),

  pack(snapshot) {
    const positions = new Map<number, Point>();
    chainFrom(snapshot.nodes, snapshot.head).forEach((id, index) => {
      positions.set(id, { x: index * CHAIN_SLOT, y: 0 });
    });
    return positions;
  },

  // Below the gap in front of "curr" (a middle insert), else below the head.
  staging(frame, placed) {
    for (const id of Object.values(frame.pointers)) {
      const at = id === null ? undefined : placed.get(id);
      if (at) return { x: at.x - CHAIN_SLOT / 2, y: LIST_STAGING_Y };
    }
    const atEnd = frame.snapshot.tail === null ? undefined : placed.get(frame.snapshot.tail);
    return atEnd ? { x: atEnd.x + CHAIN_SLOT / 2, y: LIST_STAGING_Y } : { x: 0, y: LIST_STAGING_Y };
  },

  markers(snapshot, nodes) {
    const head = snapshot.head === null ? undefined : nodes.get(snapshot.head);
    const tail = snapshot.tail === null ? undefined : nodes.get(snapshot.tail);
    return new Map([
      ['HEAD', head ?? { x: 0, y: 0 }],
      ['TAIL', tail ?? { x: 0, y: 0 }],
    ]);
  },

  box: { ...CHAIN_NODE, padLeft: 70, padRight: 110, padTop: 56, padBottom: 86 },
};

// ---------------------------------------------------------------------------
// Circular list: drawn as a ring, clockwise from the first node (tail.next) at
// the top. A new node waits in the middle of the ring.

/** IDs once round the circle, starting at the first node (tail.next). */
export function circleFrom(snapshot: CircularListSnapshot): NodeId[] {
  if (snapshot.tail === null) return [];
  const nextOf = new Map(snapshot.nodes.map((node) => [node.id, node.next]));
  const order: NodeId[] = [];
  const seen = new Set<NodeId>();
  for (let id = nextOf.get(snapshot.tail) ?? null; id !== null && !seen.has(id); id = nextOf.get(id) ?? null) {
    order.push(id);
    seen.add(id);
  }
  // A node whose next is still NIL in this step (just created) ends the walk early.
  if (!seen.has(snapshot.tail)) order.push(snapshot.tail);
  return order;
}

/** The ring's radius for `count` nodes: neighbours stay a readable distance apart. */
export function ringRadius(count: number): number {
  return Math.max(105, (count * 132) / (2 * Math.PI));
}

export const circularListLayout: LayoutStrategy<CircularListSnapshot> = {
  reachable: (snapshot) => new Set(circleFrom(snapshot)),

  pack(snapshot) {
    const order = circleFrom(snapshot);
    const radius = ringRadius(order.length);
    const positions = new Map<number, Point>();
    order.forEach((id, index) => {
      const angle = -Math.PI / 2 + (index * 2 * Math.PI) / order.length;
      // A single node sits in the middle, with its self-loop drawn above it.
      const r = order.length === 1 ? 0 : radius;
      positions.set(id, { x: r * Math.cos(angle) - CHAIN_NODE.width / 2, y: r * Math.sin(angle) - CHAIN_NODE.height / 2 });
    });
    return positions;
  },

  staging: () => ({ x: -CHAIN_NODE.width / 2, y: -CHAIN_NODE.height / 2 }),

  markers(snapshot, nodes) {
    const tail = snapshot.tail === null ? undefined : nodes.get(snapshot.tail);
    return new Map([['TAIL', tail ?? { x: -CHAIN_NODE.width / 2, y: -CHAIN_NODE.height / 2 }]]);
  },

  box: { ...CHAIN_NODE, padLeft: 80, padRight: 80, padTop: 80, padBottom: 70 },
};

// ---------------------------------------------------------------------------
// Hash table: the bucket array runs down the left; each bucket's chain runs
// to the right of its cell. A new node waits to the right of its bucket's row.

export const HASH_NODE = { width: 84, height: 36 };
export const HASH_ROW = 62; // distance between buckets
export const HASH_FIRST_X = 150; // where a chain's first node sits
export const HASH_SLOT = 128; // distance between neighbours in a chain
export const HASH_CELL = { width: 92, height: 40 };

/** Each bucket's chain, as node IDs from its head. */
export function bucketChains(snapshot: HashTableSnapshot): NodeId[][] {
  return snapshot.buckets.map((first) => chainFrom(snapshot.nodes, first));
}

export const hashTableLayout: LayoutStrategy<HashTableSnapshot> = {
  reachable: (snapshot) => new Set(bucketChains(snapshot).flat()),

  pack(snapshot) {
    const positions = new Map<number, Point>();
    bucketChains(snapshot).forEach((chain, bucket) => {
      chain.forEach((id, index) => {
        positions.set(id, { x: HASH_FIRST_X + index * HASH_SLOT, y: bucket * HASH_ROW + (HASH_CELL.height - HASH_NODE.height) / 2 });
      });
    });
    return positions;
  },

  staging(frame) {
    const bucket = frame.bucket ?? 0;
    const length = bucketChains(frame.snapshot)[bucket]?.length ?? 0;
    return { x: HASH_FIRST_X + length * HASH_SLOT + 24, y: bucket * HASH_ROW - 22 };
  },

  // One marker per bucket cell, so the view always includes the whole array.
  markers(snapshot) {
    return new Map(snapshot.buckets.map((_, bucket) => [`B${bucket}`, { x: 0, y: bucket * HASH_ROW }]));
  },

  box: { width: HASH_CELL.width, height: HASH_CELL.height, padLeft: 56, padRight: 110, padTop: 44, padBottom: 28 },
};
