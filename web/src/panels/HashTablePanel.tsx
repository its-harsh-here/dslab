import { useState, type FormEvent } from 'react';

import type { HashTableSnapshot, NodeId } from '../engine/protocol';
import { usePlayer } from '../playback/usePlayer';
import { HashSvg } from '../render/HashSvg';
import { OpButton } from '../ui/OperationBar';
import { distinctValues } from '../ui/random';
import { Workspace, type PanelProps } from '../ui/Workspace';
import { INTEGER_HINT, IntegerField, readInteger } from './common';
import { animated, setUp } from './examples';

export function HashTablePanel({ player, catalog, stageCommands }: PanelProps<HashTableSnapshot>) {
  const view = usePlayer(player);
  const [input, setInput] = useState('');
  const [inputError, setInputError] = useState(false);
  // Selection is by stable ID; delete and search are keyed by value, so
  // selecting a node copies its value into the box.
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
  const buckets = snapshot.buckets.length;
  const label = snapshot.size === 0 ? 'Empty hash table' : `Hash table with ${snapshot.size} node${snapshot.size === 1 ? '' : 's'} in ${buckets} buckets`;
  const load = buckets > 0 ? (snapshot.size / buckets).toFixed(2) : '0';

  return (
    <Workspace title="Hash table (separate chaining)" view={view} player={player} catalog={catalog} stageCommands={stageCommands}
      selectedId={selectedId}
      hint={selected ? `Selected: node #${selected.id} (value ${selected.value}); its value is in the box.`
        : `Load factor ${load} (${snapshot.size} values in ${buckets} buckets). Click a node to put its value in the box.`}
      controls={
        <>
          <form className="controls" onSubmit={handleInsert} noValidate>
            <IntegerField id="hashtable-value" label="Value" value={input} onChange={setInput} invalid={inputError} />
            <OpButton type="submit">Insert</OpButton>
            <OpButton onClick={() => runWithValue('delete')}>Delete</OpButton>
            <OpButton onClick={() => runWithValue('search')}>Search</OpButton>
            <OpButton onClick={() => player.run('clear')}>Clear</OpButton>
          </form>
          {inputError && <p className="input-error">{INTEGER_HINT}</p>}
        </>
      }
      examples={{
        batch: { button: 'Insert all', step: (value) => animated('insert', value) },
        random: (random, count) => setUp('insert', distinctValues(random, count)),
        presets: [
          {
            name: 'Collisions: three values in one bucket',
            steps: [...setUp<HashTableSnapshot>('insert', [3, 12, 20]), animated<HashTableSnapshot>('insert', 10),
              animated<HashTableSnapshot>('insert', 17), animated<HashTableSnapshot>('insert', 24)],
          },
          {
            name: 'Search looks at one chain only',
            steps: [...setUp<HashTableSnapshot>('insert', [1, 8, 15, 22, 2, 9, 5, 13, 27, 34]), animated<HashTableSnapshot>('search', 8)],
          },
          {
            name: 'Delete from the middle of a chain',
            steps: [...setUp<HashTableSnapshot>('insert', [4, 11, 18, 25, 6, 14]), animated<HashTableSnapshot>('delete', 11)],
          },
        ],
      }}
      drawing={(viewBox) => (
        <HashSvg frame={view.frame} layout={view.layout} viewBox={viewBox} label={label}
          selectedId={selectedId} onSelect={handleSelect} />
      )}
    />
  );
}
