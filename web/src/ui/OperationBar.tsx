// Above the drawing: the structure's own operation controls, the examples
// (batch, random, presets), and a status line that says why operations wait.
import {
  createContext,
  useContext,
  useRef,
  useState,
  type ButtonHTMLAttributes,
  type ChangeEvent,
  type FormEvent,
  type ReactNode,
} from 'react';

import type { Snapshot } from '../engine/protocol';
import type { Player, Step } from '../playback/player';
import type { PlayerView } from '../playback/usePlayer';
import { readInteger } from '../panels/common';
import { describeRequest } from './describe';
import { seededRandom } from './random';

/** True while new operations must wait (see isBusy in player.ts). */
export const BusyContext = createContext(false);

/**
 * A button that starts an operation: disabled while busy. Text boxes stay
 * usable, so keyboard focus is never lost when an operation starts playing.
 */
export function OpButton({ disabled, ...props }: ButtonHTMLAttributes<HTMLButtonElement>) {
  const busy = useContext(BusyContext);
  return <button type="button" {...props} disabled={busy || disabled} />;
}

export interface Preset<S extends Snapshot> {
  readonly name: string;
  readonly steps: readonly Step<S>[];
}

export interface Examples<S extends Snapshot> {
  /** One animated step per value of a comma-separated list (none for the binary tree). */
  readonly batch?: { readonly button: string; readonly step: (value: number) => Step<S> };
  /** The steps that build a random example from a seeded generator. */
  readonly random: (random: () => number, count: number) => readonly Step<S>[];
  readonly presets?: readonly Preset<S>[];
}

const MAX_BATCH = 40;
/** The engine's request limit (contract section 8): a larger file is never read. */
const MAX_FILE_BYTES = 1024 * 1024;

/** Offers 'text' as a file download. The page only saves what C++ wrote. */
function saveFile(text: string, fileName: string) {
  const url = URL.createObjectURL(new Blob([text], { type: 'application/json' }));
  const link = document.createElement('a');
  link.href = url;
  link.download = fileName;
  link.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}

export function OperationBar<S extends Snapshot>({
  title,
  player,
  view,
  controls,
  hint,
}: {
  title: string;
  player: Player<S>;
  view: PlayerView<S>;
  controls: ReactNode;
  hint?: ReactNode;
}) {
  const { state, busy } = view;
  const total = state.animation.frames.length;
  const fileInput = useRef<HTMLInputElement>(null);
  const [fileProblem, setFileProblem] = useState<string | null>(null);
  const blocked = busy || state.crash !== null;

  function handleExport() {
    // Shown without animation: export changes nothing.
    const response = player.run('export', {}, false);
    if (response?.result.status === 'ok') {
      saveFile(String(response.result.data.text), `dslab-${player.structure}.json`);
    }
  }

  // The file's text goes to C++ unread: every check of it happens there.
  async function handleFile(event: ChangeEvent<HTMLInputElement>) {
    const file = event.target.files?.[0];
    event.target.value = ''; // so the same file can be chosen again
    if (!file) return;
    if (file.size > MAX_FILE_BYTES) {
      setFileProblem(`${file.name} is larger than 1 MiB, the most the engine accepts.`);
      return;
    }
    const text = await file.text();
    const accepted = player.run('import', { text });
    setFileProblem(accepted ? null : 'The import was not started: an operation is still on screen. Jump to the result and try again.');
  }
  let status: ReactNode;
  if (state.crash) {
    status = <span className="status-error">The C++ engine stopped: {state.crash} Reload the page to start again.</span>;
  } else if (busy) {
    const what = state.shown ? describeRequest(state.shown.operation, state.shown.args) : 'the operation';
    status = (
      <>
        <span>
          {state.playing ? 'Playing' : 'Showing'} {what}: step {state.index + 1} of {total}.
          {state.queued > 0 && ` ${state.queued} more queued.`} New operations wait for the result.
        </span>
        <button type="button" className="primary" onClick={() => player.jumpToResult()} aria-keyshortcuts="End">
          Jump to result
        </button>
      </>
    );
  } else {
    status = <span>Ready. {hint}</span>;
  }

  return (
    <div className="operation-bar">
      <div className="operation-title">
        <h2>{title}</h2>
        <BusyContext.Provider value={blocked}>
          <div className="history-buttons" role="group" aria-label="Undo, redo and files">
            <OpButton onClick={() => player.run('undo')} disabled={state.undoCount === 0} aria-keyshortcuts="Control+Z"
              title={`Undo the last operation (${state.undoCount} can be undone)`}>
              Undo
            </OpButton>
            <OpButton onClick={() => player.run('redo')} disabled={state.redoCount === 0} aria-keyshortcuts="Control+Y"
              title={`Redo the last undone operation (${state.redoCount} can be redone)`}>
              Redo
            </OpButton>
            <OpButton onClick={handleExport} title="Save this structure as a JSON file">Export</OpButton>
            <OpButton onClick={() => fileInput.current?.click()} title="Replace this structure with a saved file">
              Import…
            </OpButton>
            <input ref={fileInput} type="file" accept=".json,application/json" hidden onChange={handleFile}
              aria-label="Choose a DSLab file to import" />
            <button type="button" onClick={() => player.reset()} title="Stop playback and clear this structure">
              Reset
            </button>
          </div>
        </BusyContext.Provider>
      </div>
      <BusyContext.Provider value={blocked}>{controls}</BusyContext.Provider>
      {fileProblem && <p className="input-error">{fileProblem}</p>}
      <p className="status-line" role="status">{status}</p>
    </div>
  );
}

export function ExamplesBar<S extends Snapshot>({ player, examples, busy }: { player: Player<S>; examples: Examples<S>; busy: boolean }) {
  const [batchText, setBatchText] = useState('');
  const [batchError, setBatchError] = useState<string | null>(null);
  const [seedText, setSeedText] = useState('1');
  const [countText, setCountText] = useState('8');
  const [randomError, setRandomError] = useState<string | null>(null);
  const [presetIndex, setPresetIndex] = useState(0);
  const presets = examples.presets ?? [];

  function runBatch(event: FormEvent) {
    event.preventDefault();
    if (!examples.batch) return;
    const items = batchText.split(',').map((item) => item.trim()).filter((item) => item !== '');
    const values = items.map(readInteger);
    if (items.length === 0 || values.some((value) => value === null)) {
      setBatchError('Enter whole numbers separated by commas, for example 10, 20, 30.');
      return;
    }
    if (values.length > MAX_BATCH) {
      setBatchError(`At most ${MAX_BATCH} values at a time.`);
      return;
    }
    setBatchError(null);
    const batch = examples.batch;
    player.runSteps(values.map((value) => batch.step(value as number)));
  }

  function runRandom(event: FormEvent) {
    event.preventDefault();
    const seed = readInteger(seedText);
    const count = readInteger(countText);
    if (seed === null || count === null || count < 1 || count > 30) {
      setRandomError('The seed is a whole number; the size is 1 to 30.');
      return;
    }
    setRandomError(null);
    player.runSteps(examples.random(seededRandom(seed), count));
  }

  // Open on wide screens; on a phone it starts closed so the drawing stays near the top.
  const [open] = useState(() => window.matchMedia('(min-width: 900px)').matches);

  return (
    <BusyContext.Provider value={busy}>
      <details className="examples-box" open={open}>
        <summary>Examples: batch, random{presets.length > 0 ? ', presets' : ''}</summary>
        <div className="examples">
          {examples.batch && (
            <form className="example" onSubmit={runBatch} noValidate>
              <label htmlFor="batch-values">Batch</label>
              <input id="batch-values" type="text" inputMode="numeric" autoComplete="off" placeholder="10, 20, 30"
                value={batchText} onChange={(event) => setBatchText(event.target.value)} aria-invalid={batchError !== null}
                aria-describedby={batchError ? 'batch-error' : undefined} />
              <OpButton type="submit">{examples.batch.button}</OpButton>
            </form>
          )}
          <form className="example" onSubmit={runRandom} noValidate>
            <label htmlFor="random-seed">Random, seed</label>
            <input id="random-seed" className="short" type="text" inputMode="numeric" autoComplete="off" value={seedText}
              onChange={(event) => setSeedText(event.target.value)} aria-invalid={randomError !== null} />
            <label htmlFor="random-size">size</label>
            <input id="random-size" className="short" type="text" inputMode="numeric" autoComplete="off" value={countText}
              onChange={(event) => setCountText(event.target.value)} aria-invalid={randomError !== null} />
            <OpButton type="submit">Build</OpButton>
          </form>
          {presets.length > 0 && (
            <form className="example" onSubmit={(event) => { event.preventDefault(); player.runSteps(presets[presetIndex].steps); }}>
              <label htmlFor="preset">Preset</label>
              <select id="preset" value={presetIndex} onChange={(event) => setPresetIndex(Number(event.target.value))}>
                {presets.map((preset, index) => (
                  <option key={preset.name} value={index}>{preset.name}</option>
                ))}
              </select>
              <OpButton type="submit">Run</OpButton>
            </form>
          )}
        </div>
        {batchError && <p className="input-error" id="batch-error">{batchError}</p>}
        {randomError && <p className="input-error">{randomError}</p>}
      </details>
    </BusyContext.Provider>
  );
}
