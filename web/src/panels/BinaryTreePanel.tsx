import { useState, type FormEvent } from 'react';

import type { NodeId, TreeSnapshot } from '../engine/protocol';
import type { Step } from '../playback/player';
import { usePlayer } from '../playback/usePlayer';
import { TreeSvg } from '../render/TreeSvg';
import { OpButton } from '../ui/OperationBar';
import { randomInt } from '../ui/random';
import { Workspace, type PanelProps } from '../ui/Workspace';
import { INTEGER_HINT, IntegerField, readInteger } from './common';
import { randomValues } from './examples';
import { TraversalButtons } from './TraversalButtons';

/**
 * A random shape: a root, then each value under a random node that still has a
 * free child link (read from C++'s state when the request is sent). Only the
 * input is random; C++ checks every rule.
 */
function randomTree(random: () => number, count: number): Step<TreeSnapshot>[] {
  const [first, ...rest] = randomValues(random, count);
  return [
    { operation: 'clear', animate: false },
    { operation: 'addRoot', args: { value: first }, animate: false },
    ...rest.map((value): Step<TreeSnapshot> => ({
      operation: 'addChild',
      animate: false,
      args: (latest) => {
        const open = latest.nodes.filter((node) => node.left === null || node.right === null);
        const parent = open[randomInt(random, 0, open.length - 1)];
        const both = parent.left === null && parent.right === null;
        const side = both ? (random() < 0.5 ? 'left' : 'right') : parent.left === null ? 'left' : 'right';
        return { parentId: parent.id, side, value };
      },
    })),
  ];
}

export function BinaryTreePanel({ player, catalog, stageCommands }: PanelProps<TreeSnapshot>) {
  const view = usePlayer(player);
  const [input, setInput] = useState('');
  const [inputError, setInputError] = useState(false);
  // The selected node is remembered by its stable ID, never by its value.
  const [selectedId, setSelectedId] = useState<NodeId | null>(null);
  const selected = view.state.latest.nodes.find((node) => node.id === selectedId) ?? null;

  function readValue(): number | null {
    const value = readInteger(input);
    setInputError(value === null);
    return value;
  }

  function handleAddRoot(event: FormEvent) {
    event.preventDefault();
    const value = readValue();
    if (value !== null) player.run('addRoot', { value });
  }

  function handleAddChild(side: 'left' | 'right') {
    const value = readValue();
    if (value !== null && selected) player.run('addChild', { parentId: selected.id, side, value });
  }

  const { snapshot } = view.frame;
  const label = snapshot.size === 0 ? 'Empty binary tree' : `Binary tree with ${snapshot.size} node${snapshot.size === 1 ? '' : 's'}`;

  return (
    <Workspace title="Binary tree" view={view} player={player} catalog={catalog} stageCommands={stageCommands}
      selectedId={selectedId}
      hint={selected ? `Selected: node #${selected.id} (value ${selected.value}); new children go under it.`
        : 'Click a node to select it; new children go under the selected node.'}
      controls={
        <>
          <form className="controls" onSubmit={handleAddRoot} noValidate>
            <IntegerField id="tree-value" label="Value" value={input} onChange={setInput} invalid={inputError} />
            <OpButton type="submit">Add root</OpButton>
            <OpButton onClick={() => handleAddChild('left')} disabled={!selected}>Add left child</OpButton>
            <OpButton onClick={() => handleAddChild('right')} disabled={!selected}>Add right child</OpButton>
            <OpButton onClick={() => selected && player.run('deleteSubtree', { nodeId: selected.id })} disabled={!selected}>
              Delete selected subtree
            </OpButton>
            <OpButton onClick={() => player.run('clear')}>Clear</OpButton>
          </form>
          <TraversalButtons player={player} />
          {inputError && <p className="input-error">{INTEGER_HINT}</p>}
        </>
      }
      // No batch: an ordinary binary tree has no insert, the user chooses every position.
      examples={{ random: randomTree }}
      drawing={(viewBox) => (
        <TreeSvg frame={view.frame} layout={view.layout} viewBox={viewBox} label={label} selectedId={selectedId}
          onSelect={(id) => setSelectedId((current) => (current === id ? null : id))} />
      )}
    />
  );
}
