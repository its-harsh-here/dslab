import type { TreeSnapshot } from '../engine/protocol';
import type { PanelProps } from '../ui/Workspace';
import { preset, SearchTreePanel } from './SearchTreePanel';

// The four rotation cases (docs/avl.md): the third insert unbalances the tree.
const PRESETS = [
  preset('LL: insert 30, 20, then 10', [30, 20], 'insert', 10),
  preset('RR: insert 10, 20, then 30', [10, 20], 'insert', 30),
  preset('LR: insert 30, 10, then 20', [30, 10], 'insert', 20),
  preset('RL: insert 10, 30, then 20', [10, 30], 'insert', 20),
];

export function AvlPanel(props: PanelProps<TreeSnapshot>) {
  return <SearchTreePanel {...props} title="AVL tree" shortName="AVL tree" presets={PRESETS}
    legend="h = height, bf = height(left) − height(right). Click a node to put its value in the box." />;
}
