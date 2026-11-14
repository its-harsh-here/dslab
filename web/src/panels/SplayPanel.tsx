import type { TreeSnapshot } from '../engine/protocol';
import type { PanelProps } from '../ui/Workspace';
import { animated, setUp } from './examples';
import { preset, SearchTreePanel } from './SearchTreePanel';

const PRESETS = [
  // Sorted inserts leave a spine; one search for the deepest value folds it in half.
  {
    name: 'Zig-zig: search the deepest value of a spine',
    steps: [...setUp<TreeSnapshot>('insert', [10, 20, 30, 40, 50, 60, 70]), animated<TreeSnapshot>('search', 10)],
  },
  preset('Zig-zag: insert between a node and its parent', [10, 30], 'insert', 20),
  {
    name: 'A repeated search is one comparison',
    steps: [
      ...setUp<TreeSnapshot>('insert', [50, 30, 70, 20, 40, 60, 80]),
      animated<TreeSnapshot>('search', 20),
      animated<TreeSnapshot>('search', 20),
    ],
  },
  preset('Delete, then splay the parent', [50, 30, 70, 20, 40, 60, 80, 65], 'delete', 30),
];

export function SplayPanel(props: PanelProps<TreeSnapshot>) {
  return <SearchTreePanel {...props} title="Splay tree" shortName="Splay tree" presets={PRESETS}
    legend="Every insert, search and delete rotates the node it reached to the root. Click a node to put its value in the box." />;
}
