// The page of one structure: operation controls and examples above, the
// drawing in the centre, playback and history below, and the explanation,
// pseudocode, node properties, cost and text view on the right. Each structure
// supplies only its controls, examples and drawing.
import type { MutableRefObject, ReactNode } from 'react';

import type { CatalogOperation, NodeId, Snapshot } from '../engine/protocol';
import type { ViewBox } from '../layout/layout';
import type { Player } from '../playback/player';
import type { PlayerView } from '../playback/usePlayer';
import { HistoryLog } from './HistoryLog';
import { ExamplesBar, OperationBar, type Examples } from './OperationBar';
import { PlayerBar } from './PlayerBar';
import { CostPanel, ExplanationPanel, NodeProperties, PseudocodePanel, TextView } from './SidePanels';
import { Stage, type StageCommands } from './Stage';
import { SHORTCUTS } from './useKeyboardShortcuts';

/** What every structure page gets from the app. */
export interface PanelProps<S extends Snapshot> {
  readonly player: Player<S>;
  readonly catalog: readonly CatalogOperation[];
  readonly stageCommands: MutableRefObject<StageCommands | null>;
}

interface WorkspaceProps<S extends Snapshot> extends PanelProps<S> {
  readonly title: string;
  readonly view: PlayerView<S>;
  /** The structure's own operation controls (use OpButton for anything that runs an operation). */
  readonly controls: ReactNode;
  /** One line shown when the structure is ready, e.g. what clicking a node does. */
  readonly hint?: ReactNode;
  readonly examples: Examples<S>;
  readonly drawing: (viewBox: ViewBox) => ReactNode;
  readonly selectedId: NodeId | null;
}

export function Workspace<S extends Snapshot>(props: WorkspaceProps<S>) {
  const { player, view, catalog, stageCommands, selectedId } = props;
  const { state, frame } = view;
  const operation = state.response.operation ?? '';
  const listing = catalog.find((entry) => entry.operation === operation);

  return (
    <div className="workspace">
      <section className="ws-controls" aria-label="Operations">
        <OperationBar title={props.title} player={player} view={view} controls={props.controls} hint={props.hint} />
        <ExamplesBar player={player} examples={props.examples} busy={view.busy || state.crash !== null} />
      </section>

      <section className="ws-stage" aria-label="Drawing">
        <Stage content={state.animation.viewBox} focus={state.animation.focus} commands={stageCommands}>
          {props.drawing}
        </Stage>
      </section>

      <section className="ws-player" aria-label="Playback">
        <PlayerBar player={player} view={view} />
      </section>

      <aside className="ws-side" aria-label="Details of this step">
        <ExplanationPanel view={view} />
        <PseudocodePanel catalog={catalog} operation={operation} stepId={frame.stepId} />
        <NodeProperties snapshot={frame.snapshot} selectedId={selectedId} />
        <CostPanel structure={player.structure} frame={frame} total={state.response.metrics.algorithm}
          frames={state.response.metrics.trace.frames} snapshotNodes={state.response.metrics.trace.snapshotNodes}
          complexity={listing?.complexity ?? null} />
        <TextView snapshot={frame.snapshot} />
        <details className="panel shortcuts">
          <summary>Keyboard shortcuts</summary>
          <dl>
            {SHORTCUTS.map(([keys, action]) => (
              <div key={keys}>
                <dt>{keys}</dt>
                <dd>{action}</dd>
              </div>
            ))}
          </dl>
        </details>
      </aside>

      <section className="ws-history" aria-label="History">
        <HistoryLog history={state.history} />
      </section>
    </div>
  );
}
