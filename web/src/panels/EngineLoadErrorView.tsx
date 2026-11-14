import { EngineLoadError } from '../engine/loadEngine';

/**
 * Shown when the app cannot start. A short message and a way out first; the
 * technical details (which file, which URL, what went wrong) are folded away
 * for whoever has to fix it.
 */
export function EngineLoadErrorView({ error }: { error: unknown }) {
  return (
    <section className="load-error" role="alert">
      <h2>DSLab could not start</h2>
      <p>Reload the page to try again. If this keeps happening, check your connection or try another browser.</p>
      <p>
        <button type="button" className="primary" onClick={() => window.location.reload()}>Reload</button>
      </p>
      <details>
        <summary>Technical details</summary>
        {error instanceof EngineLoadError ? (
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
        ) : (
          <p>{error instanceof Error ? error.message : String(error)} Run <code>npm run build:wasm</code> and reload.</p>
        )}
      </details>
    </section>
  );
}
