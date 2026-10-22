import { useEffect, useState } from 'react';

import { loadEngine, type LoadedEngine } from './engine/loadEngine';
import { EngineLoadErrorView } from './panels/EngineLoadErrorView';
import { StackPanel } from './panels/StackPanel';

type LoadState =
  | { kind: 'loading' }
  | { kind: 'ready'; engine: LoadedEngine }
  | { kind: 'failed'; error: unknown };

export function App() {
  const [state, setState] = useState<LoadState>({ kind: 'loading' });

  useEffect(() => {
    let active = true; // ignore a result that arrives after this effect was cleaned up
    loadEngine().then(
      (engine) => active && setState({ kind: 'ready', engine }),
      (error: unknown) => active && setState({ kind: 'failed', error }),
    );
    return () => {
      active = false;
    };
  }, []);

  return (
    <main>
      <header>
        <h1>DSLab</h1>
        <p className="subtitle">Every step below is computed by the C++ engine, running as WebAssembly in this page.</p>
      </header>
      {state.kind === 'loading' && (
        <p className="loading" role="status">Loading the C++ engine (WebAssembly)…</p>
      )}
      {state.kind === 'failed' && <EngineLoadErrorView error={state.error} />}
      {state.kind === 'ready' && <StackPanel engine={state.engine} />}
    </main>
  );
}
