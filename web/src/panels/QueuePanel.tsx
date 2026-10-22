import { useState, type FormEvent } from 'react';

import type { LoadedEngine } from '../engine/loadEngine';
import type { QueueSnapshot } from '../engine/protocol';
import { queueLayout } from '../layout/linear';
import { useStructure } from '../playback/useStructure';
import { ChainSvg } from '../render/ChainSvg';
import { INTEGER_HINT, IntegerField, readInteger, StepReport } from './common';

export function QueuePanel({ engine }: { engine: LoadedEngine }) {
  const view = useStructure<QueueSnapshot>(engine, 'queue', queueLayout);
  const [input, setInput] = useState('');
  const [inputError, setInputError] = useState(false);

  function handleEnqueue(event: FormEvent) {
    event.preventDefault();
    const value = readInteger(input);
    setInputError(value === null);
    if (value === null) return;
    view.send('enqueue', { value });
    setInput('');
  }

  const { snapshot } = view.frame;
  const label =
    snapshot.size === 0 ? 'Empty queue' : `Queue with ${snapshot.size} node${snapshot.size === 1 ? '' : 's'}`;

  return (
    <section className="structure-panel" aria-labelledby="queue-heading">
      <h2 id="queue-heading">Queue</h2>
      <form className="controls" onSubmit={handleEnqueue} noValidate>
        <IntegerField id="queue-value" label="Value" value={input} onChange={setInput} invalid={inputError} />
        <button type="submit">Enqueue</button>
        <button type="button" onClick={() => view.send('dequeue')}>Dequeue</button>
        <button type="button" onClick={() => view.send('front')}>Front</button>
        <button type="button" onClick={() => view.send('clear')}>Clear</button>
      </form>
      {inputError && <p className="input-error">{INTEGER_HINT}</p>}

      <div className="stage">
        <ChainSvg
          frame={view.frame}
          layout={view.layout}
          viewBox={view.animation.viewBox}
          entry={{ name: 'FRONT', target: snapshot.front }}
          exit={{ name: 'REAR', target: snapshot.rear }}
          label={label}
        />
      </div>
      <StepReport view={view} />
    </section>
  );
}
