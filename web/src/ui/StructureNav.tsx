// The structure selector: a vertical list on wide screens, a wrapping row on narrow ones.
import { STRUCTURES, type StructureName } from '../engine/protocol';

export const STRUCTURE_LABELS: Record<StructureName, string> = {
  stack: 'Stack',
  queue: 'Queue',
  list: 'Linked list',
  binaryTree: 'Binary tree',
  bst: 'Binary search tree',
  avl: 'AVL tree',
  redBlack: 'Red-black tree',
};

/** Shorter names for the narrow layout, where the selector is a row of buttons. */
const SHORT_LABELS: Record<StructureName, string> = {
  stack: 'Stack',
  queue: 'Queue',
  list: 'List',
  binaryTree: 'Binary tree',
  bst: 'BST',
  avl: 'AVL',
  redBlack: 'Red-black',
};

export function StructureNav({ active, onChoose }: { active: StructureName; onChoose: (structure: StructureName) => void }) {
  return (
    <nav className="structure-nav" aria-label="Data structures">
      <ul>
        {STRUCTURES.map((structure) => (
          <li key={structure}>
            <button type="button" aria-current={structure === active ? 'page' : undefined} onClick={() => onChoose(structure)}>
              <span className="wide-only">{STRUCTURE_LABELS[structure]}</span>
              <span className="narrow-only">{SHORT_LABELS[structure]}</span>
            </button>
          </li>
        ))}
      </ul>
    </nav>
  );
}
