// Pieces shared by every structure panel.
import type { DataValue, EngineResponse, Snapshot } from '../engine/protocol';
import type { StructureView } from '../playback/useStructure';

// What an integer box accepts: an optional minus sign and 1 to 10 digits.
// Only the syntax is checked here; C++ checks the 32-bit range and every rule.
const INTEGER_SYNTAX = /^-?\d{1,10}$/;
export const INTEGER_HINT = 'Enter a whole number: an optional minus sign and 1 to 10 digits.';

/** The number typed in a box, or null when it is not a whole number. */
export function readInteger(text: string): number | null {
  const trimmed = text.trim();
  return INTEGER_SYNTAX.test(trimmed) ? Number(trimmed) : null;
}

export function IntegerField({
  id,
  label,
  value,
  onChange,
  invalid,
  placeholder = 'e.g. 42',
}: {
  id: string;
  label: string;
  value: string;
  onChange: (text: string) => void;
  invalid: boolean;
  placeholder?: string;
}) {
  return (
    <>
      <label htmlFor={id}>{label}</label>
      <input
        id={id}
        type="text"
        inputMode="numeric"
        autoComplete="off"
        placeholder={placeholder}
        value={value}
        aria-invalid={invalid}
        onChange={(event) => onChange(event.target.value)}
      />
    </>
  );
}

/** Below the drawing: the step's explanation, progress, traversal output and the result. */
export function StepReport<S extends Snapshot>({ view }: { view: StructureView<S> }) {
  const { frame, animation, lastResponse, crash } = view;
  const valueOf = new Map(frame.snapshot.nodes.map((node) => [node.id, node.value]));
  return (
    <>
      <p className="explanation" aria-live="polite">{frame.explanation}</p>
      <p className="step-info">
        Step {frame.index + 1} of {animation.frames.length} · {frame.event}
      </p>
      {frame.output.length > 0 && (
        <p className="output-strip">
          Output: <strong>{frame.output.join(', ')}</strong>
        </p>
      )}
      {frame.pending.length > 0 && (
        <p className="output-strip">
          Queue: {frame.pending.map((id) => valueOf.get(id) ?? `#${id}`).join(', ')}
        </p>
      )}
      <ResultLine response={lastResponse} />
      {crash && (
        <p className="error" role="alert">
          The C++ engine stopped: {crash} Reload the page to start again.
        </p>
      )}
    </>
  );
}

/** One line about the latest result: what C++ returned, or its typed error. */
function ResultLine({ response }: { response: EngineResponse }) {
  const { result, operation } = response;
  if (result.status === 'error') {
    return (
      <p className="error" role="alert">
        <strong>{result.code}</strong> {result.message}
      </p>
    );
  }
  const text = describeResult(operation, result.data);
  return text ? <p className="result">{text}</p> : null;
}

function describeResult(operation: string | null, data: Readonly<Record<string, DataValue>>): string | null {
  const nodes = (count: DataValue) => `${count} node${count === 1 ? '' : 's'}`;
  switch (operation) {
    case 'push':
    case 'enqueue':
    case 'insertAt':
      return `${operation} → created node #${data.nodeId}`;
    case 'pop':
    case 'dequeue':
    case 'deleteNode':
      return `${operation} → removed ${data.value} (node #${data.nodeId})`;
    case 'search':
      if (!data.found) return 'search → not found';
      return `search → found node #${data.nodeId}` + (typeof data.index === 'number' ? ` at index ${data.index}` : '');
    case 'reverse':
      return 'reverse → done';
    case 'peek':
    case 'front':
      return `${operation} → ${data.value} (node #${data.nodeId})`;
    case 'clear':
      return `clear → freed ${nodes(data.removedCount)}`;
    default:
      return null;
  }
}
