// Layouts of the linear structures: the stack (vertical), and the queue and
// list (horizontal).
import type { ListSnapshot, QueueSnapshot, StackSnapshot } from '../engine/protocol';
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
