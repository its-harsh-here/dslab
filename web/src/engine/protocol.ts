// TypeScript mirror of docs/operation-contract.md, schema version 1. The C++
// engine is the authority: these types only describe the JSON it sends, and
// every field is read-only.

export const SCHEMA_VERSION = 1;

/** The structures this build of the engine implements, in tab order. */
export const STRUCTURES = ['stack', 'queue', 'list', 'binaryTree', 'bst', 'avl', 'redBlack'] as const;
export type StructureName = (typeof STRUCTURES)[number];

export type NodeId = number;

/** Stack, queue and list nodes. */
export interface LinearNode {
  readonly id: NodeId;
  readonly value: number;
  readonly next: NodeId | null;
}

export interface StackSnapshot {
  readonly structure: 'stack';
  readonly size: number;
  readonly top: NodeId | null;
  readonly nodes: readonly LinearNode[];
}

export interface QueueSnapshot {
  readonly structure: 'queue';
  readonly size: number;
  readonly front: NodeId | null;
  readonly rear: NodeId | null;
  readonly nodes: readonly LinearNode[];
}

export interface ListSnapshot {
  readonly structure: 'list';
  readonly size: number;
  readonly head: NodeId | null;
  readonly nodes: readonly LinearNode[];
}

/**
 * Binary tree, BST, AVL and red-black nodes. Only AVL nodes have a height and
 * balance; only red-black nodes have a color.
 */
export interface TreeNode {
  readonly id: NodeId;
  readonly value: number;
  readonly left: NodeId | null;
  readonly right: NodeId | null;
  readonly parent: NodeId | null;
  readonly height?: number;
  /** height(left) - height(right), from the stored heights. */
  readonly balance?: number;
  readonly color?: 'red' | 'black';
}

export interface TreeSnapshot {
  readonly structure: 'binaryTree' | 'bst' | 'avl' | 'redBlack';
  readonly size: number;
  readonly root: NodeId | null;
  /** Red-black only: the NIL sentinel's color. Links to NIL are null. */
  readonly nilColor?: 'black';
  readonly nodes: readonly TreeNode[];
}

export type Snapshot = StackSnapshot | QueueSnapshot | ListSnapshot | TreeSnapshot;

export type EventName =
  | 'begin' | 'compare' | 'move' | 'visit' | 'read' | 'found' | 'notFound'
  | 'create' | 'link' | 'unlink' | 'remove' | 'update' | 'imbalance' | 'fixupCase' | 'rotate' | 'balanced'
  | 'recolor' | 'restore' | 'reject' | 'done';

/** The algorithm's own work (contract 6.5); in a frame, the work done so far. */
export interface Counters {
  readonly comparisons: number;
  readonly nodesVisited: number;
  readonly linksChanged: number;
  readonly rotations: number;
  readonly recolors: number;
}

export interface Frame<S extends Snapshot = Snapshot> {
  readonly index: number;
  readonly event: EventName;
  readonly stepId: string;
  readonly explanation: string;
  readonly highlight: { readonly nodes: readonly NodeId[]; readonly edges: readonly string[] };
  readonly pointers: Readonly<Record<string, NodeId | null>>;
  readonly output: readonly number[];
  readonly pending: readonly NodeId[];
  readonly counters: Counters;
  readonly snapshot: S;
}

export type DataValue = number | boolean | null | readonly number[];

export type Result =
  | { readonly status: 'ok'; readonly data: Readonly<Record<string, DataValue>> }
  | { readonly status: 'error'; readonly code: string; readonly message: string };

export interface EngineResponse<S extends Snapshot = Snapshot> {
  readonly schemaVersion: number;
  readonly requestId: number | null;
  readonly structure: StructureName | null;
  readonly operation: string | null;
  readonly result: Result;
  readonly snapshot: S | null;
  readonly frames: readonly Frame<S>[];
  readonly metrics: {
    readonly algorithm: Counters;
    readonly trace: { readonly frames: number; readonly snapshotNodes: number; readonly truncated: boolean };
  };
  readonly history: { readonly undo: number; readonly redo: number } | null;
}

/** Operation arguments: integers, plus "side" ("left" or "right") for the binary tree. */
export type Args = Readonly<Record<string, number | string>>;

export interface EngineRequest {
  readonly schemaVersion: typeof SCHEMA_VERSION;
  readonly requestId: number;
  readonly structure: StructureName;
  readonly operation: string;
  readonly args: Args;
}

/** The edge ID rule shared with C++: "<fromId>-><toId>". */
export function edgeId(from: NodeId, to: NodeId): string {
  return `${from}->${to}`;
}

/** One line of pseudocode; `stepId` is null for a line that records no frame. */
export interface CatalogLine {
  readonly stepId: string | null;
  readonly text: string;
}

/** One operation's entry in the C++ catalog (contract 5.6). */
export interface CatalogOperation {
  readonly operation: string;
  readonly args: readonly string[];
  readonly mutates: boolean;
  readonly complexity: string;
  readonly pseudocode: readonly CatalogLine[];
}
