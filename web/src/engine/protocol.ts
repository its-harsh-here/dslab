// TypeScript mirror of docs/operation-contract.md, schema version 1 (the part
// the stack slice uses). The C++ engine is the authority: these types only
// describe the JSON it sends, and every field is read-only.

export const SCHEMA_VERSION = 1;

export type NodeId = number;
export type StructureName = 'stack';
export type StackOperation = 'push' | 'pop' | 'peek' | 'clear' | 'snapshot';

export interface EngineRequest {
  readonly schemaVersion: typeof SCHEMA_VERSION;
  readonly requestId: number;
  readonly structure: StructureName;
  readonly operation: StackOperation;
  readonly args: Readonly<Record<string, number>>;
}

export interface StackNode {
  readonly id: NodeId;
  readonly value: number;
  readonly next: NodeId | null;
}

export interface StackSnapshot {
  readonly structure: 'stack';
  readonly size: number;
  readonly top: NodeId | null;
  readonly nodes: readonly StackNode[];
}

export type EventName =
  | 'begin' | 'compare' | 'move' | 'visit' | 'read' | 'found' | 'notFound'
  | 'create' | 'link' | 'unlink' | 'remove' | 'update' | 'rotate' | 'recolor'
  | 'restore' | 'reject' | 'done';

export interface Frame {
  readonly index: number;
  readonly event: EventName;
  readonly stepId: string;
  readonly explanation: string;
  readonly highlight: { readonly nodes: readonly NodeId[]; readonly edges: readonly string[] };
  readonly pointers: Readonly<Record<string, NodeId | null>>;
  readonly output: readonly number[];
  readonly pending: readonly NodeId[];
  readonly snapshot: StackSnapshot;
}

export type Result =
  | { readonly status: 'ok'; readonly data: Readonly<Record<string, number>> }
  | { readonly status: 'error'; readonly code: string; readonly message: string };

export interface EngineResponse {
  readonly schemaVersion: number;
  readonly requestId: number | null;
  readonly structure: StructureName | null;
  readonly operation: string | null;
  readonly result: Result;
  readonly snapshot: StackSnapshot | null;
  readonly frames: readonly Frame[];
  readonly metrics: {
    readonly algorithm: Readonly<Record<string, number>>;
    readonly trace: { readonly frames: number; readonly snapshotNodes: number; readonly truncated: boolean };
  };
  readonly history: { readonly undo: number; readonly redo: number } | null;
}

/** The edge ID rule shared with C++: "<fromId>-><toId>". */
export function edgeId(from: NodeId, to: NodeId): string {
  return `${from}->${to}`;
}
