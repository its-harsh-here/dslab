// Short English descriptions of requests and results for the panels and the
// history log. They only format what C++ returned.
import type { Args, DataValue, Result } from '../engine/protocol';

/** "insert 25", "addChild #3 left 7", "insertAt [2] 40", "clear". */
export function describeRequest(operation: string, args: Args): string {
  const parts: string[] = [operation];
  if (typeof args.parentId === 'number') parts.push(`#${args.parentId}`);
  if (typeof args.side === 'string') parts.push(args.side);
  if (typeof args.index === 'number') parts.push(`[${args.index}]`);
  if (typeof args.nodeId === 'number') parts.push(`#${args.nodeId}`);
  if (typeof args.value === 'number') parts.push(String(args.value));
  return parts.join(' ');
}

/** One line about a result: what C++ returned, or its typed error. */
export function describeResult(operation: string | null, result: Result): string {
  if (result.status === 'error') return `${result.code}: ${result.message}`;
  return describeData(operation, result.data) ?? 'done';
}

function describeData(operation: string | null, data: Readonly<Record<string, DataValue>>): string | null {
  const nodes = (count: DataValue) => `${count} node${count === 1 ? '' : 's'}`;
  switch (operation) {
    case 'push':
    case 'enqueue':
    case 'insertAt':
    case 'addRoot':
    case 'addChild':
    case 'insert':
      return `created node #${data.nodeId}`;
    case 'pop':
    case 'dequeue':
    case 'deleteNode':
    case 'delete':
      return `removed ${data.value} (node #${data.nodeId})`;
    case 'search':
      if (!data.found) return 'not found';
      return `found node #${data.nodeId}` + (typeof data.index === 'number' ? ` at index ${data.index}` : '');
    case 'reverse':
      return 'reversed';
    case 'deleteSubtree':
      return `freed ${nodes(data.removedCount)}: #${(data.removedIds as readonly number[]).join(', #')}`;
    case 'preorder':
    case 'inorder':
    case 'postorder':
    case 'levelorder': {
      const values = data.values as readonly number[];
      return values.length > 0 ? values.join(', ') : 'the tree is empty';
    }
    case 'peek':
    case 'front':
      return `${data.value} (node #${data.nodeId})`;
    case 'clear':
      return `freed ${nodes(data.removedCount)}`;
    case 'undo':
      return 'undone';
    case 'redo':
      return 'redone';
    case 'export':
      return 'exported to a file';
    case 'import':
      return `imported ${nodes(data.nodeCount)}`;
    default:
      return null;
  }
}

/** Event names as words: "fixupCase" → "fix-up case". */
export function eventLabel(event: string): string {
  const words: Record<string, string> = {
    notFound: 'not found',
    fixupCase: 'fix-up case',
  };
  return words[event] ?? event;
}
