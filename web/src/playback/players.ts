// One player per structure, created once when the engine has loaded.
import type { LoadedEngine } from '../engine/loadEngine';
import type {
  CircularListSnapshot,
  DequeSnapshot,
  DoublyListSnapshot,
  EngineResponse,
  HashTableSnapshot,
  ListSnapshot,
  QueueSnapshot,
  Snapshot,
  StackSnapshot,
  StructureName,
  TreeSnapshot,
} from '../engine/protocol';
import type { LayoutStrategy } from '../layout/layout';
import {
  circularListLayout,
  dequeLayout,
  doublyListLayout,
  hashTableLayout,
  listLayout,
  queueLayout,
  stackLayout,
} from '../layout/linear';
import { heapLayout, redBlackLayout, treeLayout } from '../layout/tree';
import { Player } from './player';

export interface Players {
  readonly stack: Player<StackSnapshot>;
  readonly queue: Player<QueueSnapshot>;
  readonly list: Player<ListSnapshot>;
  readonly binaryTree: Player<TreeSnapshot>;
  readonly bst: Player<TreeSnapshot>;
  readonly avl: Player<TreeSnapshot>;
  readonly redBlack: Player<TreeSnapshot>;
  readonly deque: Player<DequeSnapshot>;
  readonly doublyList: Player<DoublyListSnapshot>;
  readonly circularList: Player<CircularListSnapshot>;
  readonly minHeap: Player<TreeSnapshot>;
  readonly maxHeap: Player<TreeSnapshot>;
  readonly splay: Player<TreeSnapshot>;
  readonly hashTable: Player<HashTableSnapshot>;
}

export function createPlayers(engine: LoadedEngine): Players {
  function make<S extends Snapshot>(structure: StructureName, strategy: LayoutStrategy<S>): Player<S> {
    return new Player<S>(engine.client, structure, strategy, engine.initial[structure] as EngineResponse<S>);
  }
  return {
    stack: make('stack', stackLayout),
    queue: make('queue', queueLayout),
    list: make('list', listLayout),
    binaryTree: make('binaryTree', treeLayout),
    bst: make('bst', treeLayout),
    avl: make('avl', treeLayout),
    redBlack: make('redBlack', redBlackLayout),
    deque: make('deque', dequeLayout),
    doublyList: make('doublyList', doublyListLayout),
    circularList: make('circularList', circularListLayout),
    minHeap: make('minHeap', heapLayout),
    maxHeap: make('maxHeap', heapLayout),
    splay: make('splay', treeLayout),
    hashTable: make('hashTable', hashTableLayout),
  };
}
