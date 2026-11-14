import { useEffect, useRef, useState, type MutableRefObject, type ReactNode } from 'react';

import { trackPageView } from './analytics';

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
import { STRUCTURE_INFO } from './ui/structures';
import { ThemeToggle } from './ui/ThemeToggle';
import { useKeyboardShortcuts } from './ui/useKeyboardShortcuts';
import { useSidebar, type Sidebar } from './ui/useSidebar';

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

/** The browser tab's title for a page. */
function titleOf(view: View): string {
  return view === 'home' ? 'DSLab · Data structure visualizer' : `${STRUCTURE_INFO[view].label} · DSLab`;
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

  const sidebar = useSidebar();
  const ready = state.kind === 'ready';
  const classes = ['app'];
  if (ready && !sidebar.narrow && !sidebar.open) classes.push('nav-collapsed');
  if (ready && sidebar.narrow && sidebar.open) classes.push('drawer-open');

  return (
    <div className={classes.join(' ')}>
      <header className="app-header">
        {ready && (
          <button type="button" className="icon-button sidebar-toggle" onClick={sidebar.toggle}
            aria-expanded={sidebar.open} aria-controls="structure-nav" aria-keyshortcuts="Control+B"
            title={`${sidebar.open ? 'Hide' : 'Show'} the structure list (Ctrl+B)`}>
            <svg viewBox="0 0 20 20" aria-hidden="true" focusable="false">
              <rect x={2.5} y={3.5} width={15} height={13} rx={2} />
              <line x1={7.5} y1={3.5} x2={7.5} y2={16.5} />
            </svg>
            <span className="visually-hidden">{sidebar.open ? 'Hide structure list' : 'Show structure list'}</span>
          </button>
        )}
        <div className="brand">
          <svg className="brand-mark" viewBox="0 0 32 32" aria-hidden="true" focusable="false">
            <line x1={16} y1={9} x2={9} y2={22} />
            <line x1={16} y1={9} x2={23} y2={22} />
            <circle cx={16} cy={9} r={4} />
            <circle cx={9} cy={22} r={4} />
            <circle cx={23} cy={22} r={4} />
          </svg>
          <h1>DSLab</h1>
        </div>
        <ThemeToggle />
      </header>
      {state.kind === 'loading' && (
        <div className="loading" role="status">
          <span className="spinner" aria-hidden="true" />
          Loading…
        </div>
      )}
      {state.kind === 'failed' && <EngineLoadErrorView error={state.error} />}
      {state.kind === 'ready' && <Simulator engine={state.engine} sidebar={sidebar} />}
    </div>
  );
}

function Simulator({ engine, sidebar }: { engine: LoadedEngine; sidebar: Sidebar }) {
  // Created once; the players keep each structure's state while another one is shown.
  const [players] = useState<Players>(() => createPlayers(engine));
  const [active, setActive] = useState<View>(viewFromUrl);
  const stageCommands = useRef<StageCommands | null>(null);

  // 'fromUrl': the URL already names this page (Back, Forward, an edited URL),
  // so no new history entry is added.
  function choose(view: View, fromUrl = false) {
    if (view === active) return;
    // Leaving a structure stops its playback and drops its waiting batch.
    if (active !== 'home') players[active].cancel();
    setActive(view);
    // A new history entry, so the browser's Back button returns to the previous page.
    if (!fromUrl) window.history.pushState(null, '', `#${view}`);
  }

  // Choosing a page from the drawer closes it (narrow screens).
  function chooseFromNav(view: View) {
    sidebar.closeDrawer();
    choose(view);
  }

  // The tab title follows the page; moves after the first load are reported as page views.
  const firstView = useRef(true);
  useEffect(() => {
    document.title = titleOf(active);
    if (firstView.current) {
      firstView.current = false;
      return;
    }
    trackPageView(document.title);
  }, [active]);

  // Back, Forward and hand-edited URLs change the hash without a click here.
  const chooseRef = useRef(choose);
  chooseRef.current = choose;
  useEffect(() => {
    const onHashChange = () => chooseRef.current(viewFromUrl(), true);
    window.addEventListener('hashchange', onHashChange);
    return () => window.removeEventListener('hashchange', onHashChange);
  }, []);

  return (
    <>
      <StructureNav active={active} onChoose={chooseFromNav} hidden={!sidebar.open} />
      {sidebar.narrow && sidebar.open && (
        <div className="drawer-backdrop" aria-hidden="true" onClick={sidebar.closeDrawer} />
      )}
      <main className="app-main">
        {active === 'home' ? (
          <Home onChoose={(view) => choose(view)} />
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
