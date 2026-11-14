import { useState } from 'react';

import type { DequeSnapshot, NodeId } from '../engine/protocol';
import { usePlayer } from '../playback/usePlayer';
import { ChainSvg } from '../render/ChainSvg';
import { OpButton } from '../ui/OperationBar';
import { Workspace, type PanelProps } from '../ui/Workspace';
import { INTEGER_HINT, IntegerField, readInteger } from './common';
import { animated, randomValues, setUp } from './examples';

export function DequePanel({ player, catalog, stageCommands }: PanelProps<DequeSnapshot>) {
  const view = usePlayer(player);
  const [input, setInput] = useState('');
  const [inputError, setInputError] = useState(false);
  const [selectedId, setSelectedId] = useState<NodeId | null>(null);

  function push(operation: 'pushFront' | 'pushBack') {
    const value = readInteger(input);
    setInputError(value === null);
    if (value !== null && player.run(operation, { value })) setInput('');
  }

  const { snapshot } = view.frame;
  const label = snapshot.size === 0 ? 'Empty deque' : `Deque with ${snapshot.size} node${snapshot.size === 1 ? '' : 's'}`;

  return (
    <Workspace title="Deque (double-ended queue)" view={view} player={player} catalog={catalog} stageCommands={stageCommands}
      selectedId={selectedId} hint="Every node links both ways, so both ends work in O(1). Click a node to see its properties."
      controls={
        <>
          <form className="controls" onSubmit={(event) => { event.preventDefault(); push('pushBack'); }} noValidate>
            <IntegerField id="deque-value" label="Value" value={input} onChange={setInput} invalid={inputError} />
            <OpButton onClick={() => push('pushFront')}>Push front</OpButton>
            <OpButton type="submit">Push back</OpButton>
            <OpButton onClick={() => player.run('popFront')}>Pop front</OpButton>
            <OpButton onClick={() => player.run('popBack')}>Pop back</OpButton>
            <OpButton onClick={() => player.run('peekFront')}>Peek front</OpButton>
            <OpButton onClick={() => player.run('peekBack')}>Peek back</OpButton>
            <OpButton onClick={() => player.run('clear')}>Clear</OpButton>
          </form>
          {inputError && <p className="input-error">{INTEGER_HINT}</p>}
        </>
      }
      examples={{
        batch: { button: 'Push back all', step: (value) => animated('pushBack', value) },
        random: (random, count) => setUp('pushBack', randomValues(random, count)),
        presets: [
          {
            name: 'Push at both ends, then pop at both ends',
            steps: [
              ...setUp<DequeSnapshot>('pushBack', [20, 30]),
              animated<DequeSnapshot>('pushFront', 10),
              animated<DequeSnapshot>('pushBack', 40),
              { operation: 'popFront', animate: true },
              { operation: 'popBack', animate: true },
            ],
          },
        ],
      }}
      drawing={(viewBox) => (
        <ChainSvg frame={view.frame} layout={view.layout} viewBox={viewBox} label={label} doubly
          entry={{ name: 'FRONT', target: snapshot.front }} exit={{ name: 'REAR', target: snapshot.rear }}
          selectedId={selectedId} onSelect={(id) => setSelectedId((current) => (current === id ? null : id))} />
      )}
    />
  );
}
