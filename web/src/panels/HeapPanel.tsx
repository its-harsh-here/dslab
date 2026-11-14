// The page of a binary heap. The min-heap and the max-heap are two structures
// in C++ (two objects of one class); this page serves both.
import { useState, type FormEvent } from 'react';

import type { NodeId, TreeSnapshot } from '../engine/protocol';
import { usePlayer } from '../playback/usePlayer';
import { TreeSvg } from '../render/TreeSvg';
import { OpButton } from '../ui/OperationBar';
import { Workspace, type PanelProps } from '../ui/Workspace';
import { INTEGER_HINT, IntegerField, readInteger } from './common';
import { animated, randomValues, setUp } from './examples';

export function HeapPanel({ player, catalog, stageCommands, kind }: PanelProps<TreeSnapshot> & { kind: 'min' | 'max' }) {
  const view = usePlayer(player);
  const [input, setInput] = useState('');
  const [inputError, setInputError] = useState(false);
  const [selectedId, setSelectedId] = useState<NodeId | null>(null);
  const name = kind === 'min' ? 'Min-heap' : 'Max-heap';
  const extreme = kind === 'min' ? 'smallest' : 'largest';

  function handleInsert(event: FormEvent) {
    event.preventDefault();
    const value = readInteger(input);
    setInputError(value === null);
    if (value !== null && player.run('insert', { value })) setInput('');
  }

  const { snapshot } = view.frame;
  const label = snapshot.size === 0 ? `Empty ${name.toLowerCase()}` : `${name} with ${snapshot.size} node${snapshot.size === 1 ? '' : 's'}`;
  // Sorted inputs are the worst case for sifting: every insert climbs to the root.
  const climbing = kind === 'min' ? [50, 40, 30, 20, 10] : [10, 20, 30, 40, 50];

  return (
    <Workspace title={`${name} (priority queue)`} view={view} player={player} catalog={catalog} stageCommands={stageCommands}
      selectedId={selectedId}
      hint={`The ${extreme} value is always at the root. The tree is really the array drawn under it. Click a node to see its properties.`}
      controls={
        <>
          <form className="controls" onSubmit={handleInsert} noValidate>
            <IntegerField id={`${kind}heap-value`} label="Value" value={input} onChange={setInput} invalid={inputError} />
            <OpButton type="submit">Insert</OpButton>
            <OpButton onClick={() => player.run('extract')}>{kind === 'min' ? 'Extract min' : 'Extract max'}</OpButton>
            <OpButton onClick={() => player.run('peek')}>Peek</OpButton>
            <OpButton onClick={() => player.run('clear')}>Clear</OpButton>
          </form>
          {inputError && <p className="input-error">{INTEGER_HINT}</p>}
        </>
      }
      examples={{
        batch: { button: 'Insert all', step: (value) => animated('insert', value) },
        random: (random, count) => setUp('insert', randomValues(random, count)),
        presets: [
          {
            name: 'Every insert sifts up to the root',
            steps: [{ operation: 'clear', animate: false }, ...climbing.map((value) => animated<TreeSnapshot>('insert', value))],
          },
          {
            name: `Extract the ${extreme}: the last node sifts down`,
            steps: [...setUp<TreeSnapshot>('insert', [40, 10, 30, 70, 20, 60, 50, 90, 80]), { operation: 'extract', animate: true }],
          },
          {
            name: 'Heap sort: extract until empty',
            steps: [
              ...setUp<TreeSnapshot>('insert', [30, 10, 50, 20, 40]),
              ...Array.from({ length: 5 }, () => ({ operation: 'extract', animate: true }) as const),
            ],
          },
        ],
      }}
      drawing={(viewBox) => (
        <TreeSvg frame={view.frame} layout={view.layout} viewBox={viewBox} label={label} heapArray
          selectedId={selectedId} onSelect={(id) => setSelectedId((current) => (current === id ? null : id))} />
      )}
    />
  );
}
