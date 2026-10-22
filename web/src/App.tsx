import { useEffect, useState, type ReactNode } from 'react';

import { loadEngine, type LoadedEngine } from './engine/loadEngine';
import { STRUCTURES, type StructureName } from './engine/protocol';
import { BinaryTreePanel } from './panels/BinaryTreePanel';
import { EngineLoadErrorView } from './panels/EngineLoadErrorView';
import { ListPanel } from './panels/ListPanel';
import { QueuePanel } from './panels/QueuePanel';
import { StackPanel } from './panels/StackPanel';

type LoadState =
  | { kind: 'loading' }
  | { kind: 'ready'; engine: LoadedEngine }
  | { kind: 'failed'; error: unknown };

const TAB_LABELS: Record<StructureName, string> = {
  stack: 'Stack',
  queue: 'Queue',
  list: 'Linked list',
  binaryTree: 'Binary tree',
};

function panelFor(structure: StructureName, engine: LoadedEngine): ReactNode {
  switch (structure) {
    case 'stack':
      return <StackPanel engine={engine} />;
    case 'queue':
      return <QueuePanel engine={engine} />;
    case 'list':
      return <ListPanel engine={engine} />;
    case 'binaryTree':
      return <BinaryTreePanel engine={engine} />;
  }
}

/** The tab named in the URL (e.g. "#queue"), so a tab can be linked to directly. */
function tabFromUrl(): StructureName {
  const name = window.location.hash.slice(1);
  return (STRUCTURES as readonly string[]).includes(name) ? (name as StructureName) : 'stack';
}

export function App() {
  const [state, setState] = useState<LoadState>({ kind: 'loading' });
  const [active, setActive] = useState<StructureName>(tabFromUrl);

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

  function choose(structure: StructureName) {
    setActive(structure);
    window.history.replaceState(null, '', `#${structure}`);
  }

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
      {state.kind === 'ready' && (
        <>
          <div className="tabs" role="tablist" aria-label="Data structures">
            {STRUCTURES.map((structure) => (
              <button key={structure} type="button" role="tab" id={`tab-${structure}`}
                aria-selected={active === structure} aria-controls={`panel-${structure}`}
                onClick={() => choose(structure)}>
                {TAB_LABELS[structure]}
              </button>
            ))}
          </div>
          {/* Every panel stays mounted, so each keeps its picture while hidden. */}
          {STRUCTURES.map((structure) => (
            <div key={structure} role="tabpanel" id={`panel-${structure}`} aria-labelledby={`tab-${structure}`}
              hidden={active !== structure}>
              {panelFor(structure, state.engine)}
            </div>
          ))}
        </>
      )}
    </main>
  );
}
