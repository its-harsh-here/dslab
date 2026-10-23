// The page of a binary search tree, AVL tree or red-black tree: the same
// operations (insert, delete and search by value, traversals, clear); each
// structure adds its own presets and drawing options.
import { useState, type FormEvent, type ReactNode } from 'react';

import type { NodeId, TreeSnapshot } from '../engine/protocol';
import { usePlayer } from '../playback/usePlayer';
import { TreeSvg } from '../render/TreeSvg';
import { OpButton, type Preset } from '../ui/OperationBar';
import { distinctValues } from '../ui/random';
import { Workspace, type PanelProps } from '../ui/Workspace';
import { INTEGER_HINT, IntegerField, readInteger } from './common';
import { animated, setUp } from './examples';
import { TraversalButtons } from './TraversalButtons';

interface SearchTreeProps extends PanelProps<TreeSnapshot> {
  readonly title: string;
  /** "BST", "AVL tree": used in the drawing's accessible name. */
  readonly shortName: string;
  readonly presets: readonly Preset<TreeSnapshot>[];
  /** What clicking a node shows, and other notes. */
  readonly legend: string;
  /** Drawing options next to the operations (the red-black NIL toggle). */
  readonly options?: ReactNode;
  readonly showNil?: boolean;
}

export function SearchTreePanel({ player, catalog, stageCommands, title, shortName, presets, legend, options, showNil }: SearchTreeProps) {
  const view = usePlayer(player);
  const [input, setInput] = useState('');
  const [inputError, setInputError] = useState(false);
  // Selection is by stable ID; delete and search are keyed by value
  // (decision D12), so selecting a node copies its value into the box.
  const [selectedId, setSelectedId] = useState<NodeId | null>(null);
  const selected = view.state.latest.nodes.find((node) => node.id === selectedId) ?? null;

  function runWithValue(operation: 'insert' | 'delete' | 'search') {
    const value = readInteger(input);
    setInputError(value === null);
    if (value !== null) player.run(operation, { value });
  }

  function handleInsert(event: FormEvent) {
    event.preventDefault();
    runWithValue('insert');
  }

  function handleSelect(id: NodeId) {
    const next = selectedId === id ? null : id;
    setSelectedId(next);
    const node = view.frame.snapshot.nodes.find((candidate) => candidate.id === next);
    if (node) setInput(String(node.value));
  }

  const { snapshot } = view.frame;
  const label = snapshot.size === 0 ? `Empty ${shortName}` : `${shortName} with ${snapshot.size} node${snapshot.size === 1 ? '' : 's'}`;
  const prefix = title.replace(/[^a-z]/gi, '').toLowerCase();

  return (
    <Workspace title={title} view={view} player={player} catalog={catalog} stageCommands={stageCommands}
      selectedId={selectedId}
      hint={selected ? `Selected: node #${selected.id} (value ${selected.value}); its value is in the box.` : legend}
      controls={
        <>
          <form className="controls" onSubmit={handleInsert} noValidate>
            <IntegerField id={`${prefix}-value`} label="Value" value={input} onChange={setInput} invalid={inputError} />
            <OpButton type="submit">Insert</OpButton>
            <OpButton onClick={() => runWithValue('delete')}>Delete</OpButton>
            <OpButton onClick={() => runWithValue('search')}>Search</OpButton>
            <OpButton onClick={() => player.run('clear')}>Clear</OpButton>
            {options}
          </form>
          <TraversalButtons player={player} />
          {inputError && <p className="input-error">{INTEGER_HINT}</p>}
        </>
      }
      examples={{
        batch: { button: 'Insert all', step: (value) => animated('insert', value) },
        random: (random, count) => setUp('insert', distinctValues(random, count)),
        presets,
      }}
      drawing={(viewBox) => (
        <TreeSvg frame={view.frame} layout={view.layout} viewBox={viewBox} label={label} selectedId={selectedId}
          onSelect={handleSelect} showNil={showNil} />
      )}
    />
  );
}

/** A preset: build `setup` without animation, then animate one operation. */
export function preset(name: string, setup: readonly number[], operation: 'insert' | 'delete', value: number): Preset<TreeSnapshot> {
  return { name, steps: [...setUp<TreeSnapshot>('insert', setup), animated<TreeSnapshot>(operation, value)] };
}
