import { useState } from 'react';

import type { TreeSnapshot } from '../engine/protocol';
import type { PanelProps } from '../ui/Workspace';
import { preset, SearchTreePanel } from './SearchTreePanel';

// One preset per fix-up case (docs/red-black.md, "Presets").
const PRESETS = [
  preset('Insert case 1: red uncle', [30, 20, 40], 'insert', 10),
  preset('Insert case 2: inner child', [30, 10], 'insert', 20),
  preset('Insert case 3: outer child', [30, 20], 'insert', 10),
  preset('Delete case 1: red sibling', [10, 20, 30, 40, 60, 70], 'delete', 10),
  preset('Delete case 2: black sibling, black children', [10, 20, 30, 40, 70, 90, 60, 50], 'delete', 10),
  preset('Delete case 3: near child red', [10, 20, 40, 30], 'delete', 10),
  preset('Delete case 4: far child red', [10, 20, 30, 40], 'delete', 10),
];

export function RedBlackPanel(props: PanelProps<TreeSnapshot>) {
  const [showNil, setShowNil] = useState(false);
  return (
    <SearchTreePanel {...props} title="Red-black tree" shortName="Red-black tree" presets={PRESETS} showNil={showNil}
      legend="R = red, B = black; every NIL leaf is black. Click a node to put its value in the box."
      options={
        <label className="toggle">
          <input type="checkbox" checked={showNil} onChange={(event) => setShowNil(event.target.checked)} />
          Show NIL leaves
        </label>
      } />
  );
}
