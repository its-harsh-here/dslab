import type { TreeSnapshot } from '../engine/protocol';
import type { PanelProps } from '../ui/Workspace';
import { animated, setUp } from './examples';
import { preset, SearchTreePanel } from './SearchTreePanel';

const PRESETS = [
  // 50's successor, 60, is deeper than its right child: it is detached, then moved up.
  preset('Delete with a deeper successor', [50, 30, 70, 20, 40, 60, 80, 65], 'delete', 50),
  {
    name: 'Sorted inserts (a degenerate tree)',
    steps: [...setUp<TreeSnapshot>('insert', []), ...[10, 20, 30, 40, 50].map((value) => animated<TreeSnapshot>('insert', value))],
  },
];

export function BstPanel(props: PanelProps<TreeSnapshot>) {
  return <SearchTreePanel {...props} title="Binary search tree" shortName="BST" presets={PRESETS}
    legend="Click a node to put its value in the box." />;
}
