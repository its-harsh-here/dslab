// What the page knows about each structure for its menus: names, the group it
// is listed under, one line about it, the headline costs, and a small icon.
// Presentation only: every fact about behaviour comes from C++ at run time.
import type { ReactNode } from 'react';

import type { StructureName } from '../engine/protocol';

export interface StructureInfo {
  readonly label: string;
  readonly group: 'Linear' | 'Trees' | 'Heaps and hashing';
  readonly blurb: string;
  /** Headline costs shown on the overview card. */
  readonly costs: readonly string[];
  /** A hue (0 to 360) that tints this structure's card and icon. */
  readonly hue: number;
  readonly icon: ReactNode;
}

const box = (x: number, y: number, w = 6, h = 5) => <rect x={x} y={y} width={w} height={h} rx={1.2} />;
const dot = (cx: number, cy: number, r = 2.4) => <circle cx={cx} cy={cy} r={r} />;
const line = (x1: number, y1: number, x2: number, y2: number) => <line x1={x1} y1={y1} x2={x2} y2={y2} />;

function Icon({ children }: { children: ReactNode }) {
  return (
    <svg className="ds-icon" viewBox="0 0 24 24" aria-hidden="true" focusable="false">
      {children}
    </svg>
  );
}

/** A small binary tree; `marks` fills some of its seven nodes (0 = root). */
function treeIcon(marks: readonly number[] = []) {
  const at: [number, number][] = [[12, 4.5], [6.5, 11.5], [17.5, 11.5], [3.5, 19], [9.5, 19], [14.5, 19], [20.5, 19]];
  const links: [number, number][] = [[0, 1], [0, 2], [1, 3], [1, 4], [2, 5], [2, 6]];
  return (
    <Icon>
      {links.map(([a, b]) => <line key={`${a}-${b}`} x1={at[a][0]} y1={at[a][1]} x2={at[b][0]} y2={at[b][1]} />)}
      {at.map(([x, y], index) => <circle key={index} cx={x} cy={y} r={2.3} className={marks.includes(index) ? 'fill' : undefined} />)}
    </Icon>
  );
}

export const STRUCTURE_INFO: Record<StructureName, StructureInfo> = {
  stack: {
    label: 'Stack', group: 'Linear', hue: 222,
    blurb: 'Last in, first out. Push and pop at one end.',
    costs: ['push O(1)', 'pop O(1)'],
    icon: <Icon>{box(6, 3, 12, 4.5)}{box(6, 9.5, 12, 4.5)}{box(6, 16, 12, 4.5)}</Icon>,
  },
  queue: {
    label: 'Queue', group: 'Linear', hue: 200,
    blurb: 'First in, first out. Enqueue at the rear, dequeue at the front.',
    costs: ['enqueue O(1)', 'dequeue O(1)'],
    icon: <Icon>{box(1.5, 9.5, 5.5, 5)}{box(9.2, 9.5, 5.5, 5)}{box(17, 9.5, 5.5, 5)}{line(7, 12, 9.2, 12)}{line(14.7, 12, 17, 12)}</Icon>,
  },
  deque: {
    label: 'Deque', group: 'Linear', hue: 186,
    blurb: 'A double-ended queue: push, pop and peek at both ends.',
    costs: ['both ends O(1)'],
    icon: <Icon>{box(5, 9.5, 5.5, 5)}{box(13.5, 9.5, 5.5, 5)}{line(10.5, 12, 13.5, 12)}<path d="M4 8 L1.5 12 L4 16 M20 8 L22.5 12 L20 16" /></Icon>,
  },
  list: {
    label: 'Linked list', group: 'Linear', hue: 168,
    blurb: 'Singly linked nodes from head: insert, delete, search, reverse.',
    costs: ['insert O(i)', 'search O(n)'],
    icon: <Icon>{dot(4, 12)}{dot(12, 12)}{dot(20, 12)}{line(6.4, 12, 9.6, 12)}{line(14.4, 12, 17.6, 12)}</Icon>,
  },
  doublyList: {
    label: 'Doubly linked list', group: 'Linear', hue: 152,
    blurb: 'Every node links to both neighbours: delete a node without walking.',
    costs: ['delete O(1)', 'insert O(min(i, n − i))'],
    icon: <Icon>{dot(4, 12)}{dot(12, 12)}{dot(20, 12)}{line(6.4, 10.6, 9.6, 10.6)}{line(6.4, 13.4, 9.6, 13.4)}{line(14.4, 10.6, 17.6, 10.6)}{line(14.4, 13.4, 17.6, 13.4)}</Icon>,
  },
  circularList: {
    label: 'Circular list', group: 'Linear', hue: 130,
    blurb: 'The last node links back to the first. Rotate in one step.',
    costs: ['insert O(1)', 'rotate O(1)'],
    icon: <Icon><circle cx={12} cy={12} r={7.5} className="ring" />{dot(12, 4.5)}{dot(19.5, 12)}{dot(12, 19.5)}{dot(4.5, 12)}</Icon>,
  },
  binaryTree: {
    label: 'Binary tree', group: 'Trees', hue: 262,
    blurb: 'You choose where each node goes. Four traversals, step by step.',
    costs: ['traversals O(n)'],
    icon: treeIcon(),
  },
  bst: {
    label: 'Binary search tree', group: 'Trees', hue: 278,
    blurb: 'Smaller left, larger right. Fast when balanced, a list when not.',
    costs: ['search O(h)', 'insert O(h)'],
    icon: treeIcon([0]),
  },
  avl: {
    label: 'AVL tree', group: 'Trees', hue: 296,
    blurb: 'Heights differ by at most one. LL, LR, RR and RL rotations.',
    costs: ['every operation O(log n)'],
    icon: treeIcon([0, 1, 2]),
  },
  redBlack: {
    label: 'Red-black tree', group: 'Trees', hue: 350,
    blurb: 'Balanced by color rules: recolor and rotate, the CLRS way.',
    costs: ['every operation O(log n)'],
    icon: treeIcon([1, 2]),
  },
  splay: {
    label: 'Splay tree', group: 'Trees', hue: 322,
    blurb: 'Every access is rotated to the root: zig, zig-zig and zig-zag.',
    costs: ['amortised O(log n)'],
    icon: treeIcon([0, 4]),
  },
  minHeap: {
    label: 'Min-heap', group: 'Heaps and hashing', hue: 28,
    blurb: 'A priority queue in an array: the smallest value is always on top.',
    costs: ['insert O(log n)', 'extract O(log n)'],
    icon: <Icon>{line(12, 5, 6, 13)}{line(12, 5, 18, 13)}{dot(12, 5, 2.6)}{dot(6, 13, 2.6)}{dot(18, 13, 2.6)}{box(3, 18.5, 4, 3.5)}{box(8, 18.5, 4, 3.5)}{box(13, 18.5, 4, 3.5)}{box(18, 18.5, 4, 3.5)}</Icon>,
  },
  maxHeap: {
    label: 'Max-heap', group: 'Heaps and hashing', hue: 12,
    blurb: 'The same array heap, with the largest value on top.',
    costs: ['insert O(log n)', 'extract O(log n)'],
    icon: <Icon>{line(12, 5, 6, 13)}{line(12, 5, 18, 13)}<circle cx={12} cy={5} r={3.2} className="fill" />{dot(6, 13, 2.2)}{dot(18, 13, 2.2)}{box(3, 18.5, 4, 3.5)}{box(8, 18.5, 4, 3.5)}{box(13, 18.5, 4, 3.5)}{box(18, 18.5, 4, 3.5)}</Icon>,
  },
  hashTable: {
    label: 'Hash table', group: 'Heaps and hashing', hue: 46,
    blurb: 'Seven buckets, separate chaining: one hash step, one short chain.',
    costs: ['average O(1)', 'worst O(n)'],
    icon: <Icon>{box(2, 3, 5, 4.5)}{box(2, 9.7, 5, 4.5)}{box(2, 16.5, 5, 4.5)}{line(7, 5.2, 10.5, 5.2)}{dot(12.5, 5.2, 2)}{line(14.5, 5.2, 17.5, 5.2)}{dot(19.5, 5.2, 2)}{line(7, 18.7, 10.5, 18.7)}{dot(12.5, 18.7, 2)}</Icon>,
  },
};

export const GROUPS: readonly StructureInfo['group'][] = ['Linear', 'Trees', 'Heaps and hashing'];
