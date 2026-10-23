import { useEffect, useRef, useState, type MutableRefObject, type ReactNode } from 'react';

import { loadEngine, type LoadedEngine } from './engine/loadEngine';
import { STRUCTURES, type Snapshot, type StructureName } from './engine/protocol';
import { AvlPanel } from './panels/AvlPanel';
import { BinaryTreePanel } from './panels/BinaryTreePanel';
import { BstPanel } from './panels/BstPanel';
import { EngineLoadErrorView } from './panels/EngineLoadErrorView';
import { ListPanel } from './panels/ListPanel';
import { QueuePanel } from './panels/QueuePanel';
import { RedBlackPanel } from './panels/RedBlackPanel';
import { StackPanel } from './panels/StackPanel';
import type { Player } from './playback/player';
import { createPlayers, type Players } from './playback/players';
import type { StageCommands } from './ui/Stage';
import { StructureNav } from './ui/StructureNav';
import { useKeyboardShortcuts } from './ui/useKeyboardShortcuts';

type LoadState =
  | { kind: 'loading' }
  | { kind: 'ready'; engine: LoadedEngine }
  | { kind: 'failed'; error: unknown };

/** The structure named in the URL (e.g. "#queue"), so a structure can be linked to directly. */
function structureFromUrl(): StructureName {
  const name = window.location.hash.slice(1);
  return (STRUCTURES as readonly string[]).includes(name) ? (name as StructureName) : 'stack';
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
        <h1>DSLab</h1>
        <p className="subtitle">Every step is computed by the C++ engine, running as WebAssembly in this page.</p>
      </header>
      {state.kind === 'loading' && <p className="loading" role="status">Loading the C++ engine (WebAssembly)…</p>}
      {state.kind === 'failed' && <EngineLoadErrorView error={state.error} />}
      {state.kind === 'ready' && <Simulator engine={state.engine} />}
    </div>
  );
}

function Simulator({ engine }: { engine: LoadedEngine }) {
  // Created once; the players keep each structure's state while another one is shown.
  const [players] = useState<Players>(() => createPlayers(engine));
  const [active, setActive] = useState<StructureName>(structureFromUrl);
  const stageCommands = useRef<StageCommands | null>(null);
  useKeyboardShortcuts(players[active] as Player<Snapshot>, stageCommands);

  function choose(structure: StructureName) {
    if (structure === active) return;
    // Leaving a structure stops its playback and drops its waiting batch.
    players[active].cancel();
    setActive(structure);
    window.history.replaceState(null, '', `#${structure}`);
  }

  return (
    <>
      <StructureNav active={active} onChoose={choose} />
      <main className="app-main">
        {/* Keyed by structure: each page starts with fresh local state (inputs, selection, camera). */}
        <Page key={active} structure={active} players={players} engine={engine} stageCommands={stageCommands} />
      </main>
    </>
  );
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
  }
}
