import { useEffect, useRef, useState, type MutableRefObject, type ReactNode } from 'react';

import { loadEngine, type LoadedEngine } from './engine/loadEngine';
import { STRUCTURES, type Snapshot, type StructureName } from './engine/protocol';
import { AvlPanel } from './panels/AvlPanel';
import { BinaryTreePanel } from './panels/BinaryTreePanel';
import { BstPanel } from './panels/BstPanel';
import { CircularListPanel } from './panels/CircularListPanel';
import { DequePanel } from './panels/DequePanel';
import { DoublyListPanel } from './panels/DoublyListPanel';
import { EngineLoadErrorView } from './panels/EngineLoadErrorView';
import { HashTablePanel } from './panels/HashTablePanel';
import { HeapPanel } from './panels/HeapPanel';
import { ListPanel } from './panels/ListPanel';
import { QueuePanel } from './panels/QueuePanel';
import { RedBlackPanel } from './panels/RedBlackPanel';
import { SplayPanel } from './panels/SplayPanel';
import { StackPanel } from './panels/StackPanel';
import type { Player } from './playback/player';
import { createPlayers, type Players } from './playback/players';
import type { StageCommands } from './ui/Stage';
import { Home } from './ui/Home';
import { StructureNav, type View } from './ui/StructureNav';
import { ThemeToggle } from './ui/ThemeToggle';
import { useKeyboardShortcuts } from './ui/useKeyboardShortcuts';

type LoadState =
  | { kind: 'loading' }
  | { kind: 'ready'; engine: LoadedEngine }
  | { kind: 'failed'; error: unknown };

/**
 * The page named in the URL: a structure (e.g. "#queue"), so one can be linked
 * to directly, or the overview when the URL names none.
 */
function viewFromUrl(): View {
  const name = window.location.hash.slice(1);
  return (STRUCTURES as readonly string[]).includes(name) ? (name as StructureName) : 'home';
}

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
    <div className="app">
      <header className="app-header">
        <div className="brand">
          <svg className="brand-mark" viewBox="0 0 32 32" aria-hidden="true" focusable="false">
            <line x1={16} y1={8} x2={8} y2={22} />
            <line x1={16} y1={8} x2={24} y2={22} />
            <circle className="m0" cx={16} cy={8} r={4.5} />
            <circle className="m1" cx={8} cy={22} r={4.5} />
            <circle className="m2" cx={24} cy={22} r={4.5} />
          </svg>
          <h1>DSLab</h1>
        </div>
        <p className="subtitle">Data structures, step by step. C++ engine, running in your browser.</p>
        <ThemeToggle />
      </header>
      {state.kind === 'loading' && (
        <p className="loading" role="status">
          <span className="loader" aria-hidden="true"><i /><i /><i /></span>
          Loading the C++ engine (WebAssembly)…
        </p>
      )}
      {state.kind === 'failed' && <EngineLoadErrorView error={state.error} />}
      {state.kind === 'ready' && <Simulator engine={state.engine} />}
    </div>
  );
}

function Simulator({ engine }: { engine: LoadedEngine }) {
  // Created once; the players keep each structure's state while another one is shown.
  const [players] = useState<Players>(() => createPlayers(engine));
  const [active, setActive] = useState<View>(viewFromUrl);
  const stageCommands = useRef<StageCommands | null>(null);

  function choose(view: View) {
    if (view === active) return;
    // Leaving a structure stops its playback and drops its waiting batch.
    if (active !== 'home') players[active].cancel();
    setActive(view);
    window.history.replaceState(null, '', `#${view}`);
  }

  // The brand link and hand-edited URLs change the hash without a click here.
  const chooseRef = useRef(choose);
  chooseRef.current = choose;
  useEffect(() => {
    const onHashChange = () => chooseRef.current(viewFromUrl());
    window.addEventListener('hashchange', onHashChange);
    return () => window.removeEventListener('hashchange', onHashChange);
  }, []);

  return (
    <>
      <StructureNav active={active} onChoose={choose} />
      <main className="app-main">
        {active === 'home' ? (
          <Home onChoose={choose} />
        ) : (
          <>
            <Shortcuts player={players[active] as Player<Snapshot>} stageCommands={stageCommands} />
            {/* Keyed by structure: each page starts with fresh local state (inputs, selection, camera). */}
            <Page key={active} structure={active} players={players} engine={engine} stageCommands={stageCommands} />
          </>
        )}
      </main>
    </>
  );
}

/** The keyboard listener of the structure on screen (none on the overview). */
function Shortcuts({ player, stageCommands }: { player: Player<Snapshot>; stageCommands: MutableRefObject<StageCommands | null> }) {
  useKeyboardShortcuts(player, stageCommands);
  return null;
}

function Page({ structure, players, engine, stageCommands }: {
  structure: StructureName;
  players: Players;
  engine: LoadedEngine;
  stageCommands: MutableRefObject<StageCommands | null>;
}): ReactNode {
  const common = { catalog: engine.catalogs[structure], stageCommands };
  switch (structure) {
    case 'stack':
      return <StackPanel player={players.stack} {...common} />;
    case 'queue':
      return <QueuePanel player={players.queue} {...common} />;
    case 'list':
      return <ListPanel player={players.list} {...common} />;
    case 'binaryTree':
      return <BinaryTreePanel player={players.binaryTree} {...common} />;
    case 'bst':
      return <BstPanel player={players.bst} {...common} />;
    case 'avl':
      return <AvlPanel player={players.avl} {...common} />;
    case 'redBlack':
      return <RedBlackPanel player={players.redBlack} {...common} />;
    case 'deque':
      return <DequePanel player={players.deque} {...common} />;
    case 'doublyList':
      return <DoublyListPanel player={players.doublyList} {...common} />;
    case 'circularList':
      return <CircularListPanel player={players.circularList} {...common} />;
    case 'minHeap':
      return <HeapPanel player={players.minHeap} kind="min" {...common} />;
    case 'maxHeap':
      return <HeapPanel player={players.maxHeap} kind="max" {...common} />;
    case 'splay':
      return <SplayPanel player={players.splay} {...common} />;
    case 'hashTable':
      return <HashTablePanel player={players.hashTable} {...common} />;
  }
}
