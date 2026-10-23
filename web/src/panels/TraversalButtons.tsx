import type { Snapshot } from '../engine/protocol';
import type { Player } from '../playback/player';
import { OpButton } from '../ui/OperationBar';

/** The four traversal buttons, shared by every tree. */
export function TraversalButtons<S extends Snapshot>({ player }: { player: Player<S> }) {
  return (
    <div className="controls" role="group" aria-label="Traversals">
      <span className="group-label">Traverse</span>
      <OpButton onClick={() => player.run('preorder')}>Pre-order</OpButton>
      <OpButton onClick={() => player.run('inorder')}>In-order</OpButton>
      <OpButton onClick={() => player.run('postorder')}>Post-order</OpButton>
      <OpButton onClick={() => player.run('levelorder')}>Level-order</OpButton>
    </div>
  );
}
