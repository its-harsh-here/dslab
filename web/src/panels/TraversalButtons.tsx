import type { Args } from '../engine/protocol';

/** The four traversal buttons, shared by the binary tree and the BST. */
export function TraversalButtons({ send }: { send: (operation: string, args?: Args) => void }) {
  return (
    <div className="controls traversals">
      <span className="group-label">Traverse:</span>
      <button type="button" onClick={() => send('preorder')}>Pre-order</button>
      <button type="button" onClick={() => send('inorder')}>In-order</button>
      <button type="button" onClick={() => send('postorder')}>Post-order</button>
      <button type="button" onClick={() => send('levelorder')}>Level-order</button>
    </div>
  );
}
