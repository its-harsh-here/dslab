import { EngineLoadError } from '../engine/loadEngine';

/** Explains a failed engine load: which file, which URL, what went wrong, what to do. */
export function EngineLoadErrorView({ error }: { error: unknown }) {
  if (!(error instanceof EngineLoadError)) {
    return (
      <section className="load-error" role="alert">
        <h2>The C++ engine could not be started</h2>
        <p>{error instanceof Error ? error.message : String(error)}</p>
        <p>Run <code>npm run build:wasm</code> and reload the page.</p>
      </section>
    );
  }
  return (
    <section className="load-error" role="alert">
      <h2>The C++ engine could not be loaded</h2>
      <dl>
        <dt>File</dt>
        <dd><code>{error.file}</code></dd>
        <dt>URL</dt>
        <dd><a href={error.url}>{error.url}</a></dd>
        <dt>Problem</dt>
        <dd>{error.problem}</dd>
        <dt>How to fix it</dt>
        <dd>{error.fix}</dd>
      </dl>
    </section>
  );
}
