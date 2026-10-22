// Loads the C++ engine (WebAssembly) once per page.
//
// The .wasm URL is explicit: Vite's "?url" import gives the built asset's URL,
// which with `base: './'` is relative to the deployed files, so it also works
// under a GitHub Pages sub-path. We fetch and compile the file ourselves so
// that any failure names the exact file, URL and cause.
import createDslabEngine from '../generated/dslab_engine.mjs';
import wasmAssetUrl from '../generated/dslab_engine.wasm?url';

import { EngineClient } from './client';
import { STRUCTURES, type EngineResponse, type StructureName } from './protocol';

const WASM_FILE = 'dslab_engine.wasm';

/** Why the engine could not be loaded, with enough detail to fix it. */
export class EngineLoadError extends Error {
  constructor(
    readonly file: string,
    readonly url: string,
    readonly problem: string,
    readonly fix: string,
  ) {
    super(`${file} could not be loaded from ${url}: ${problem}`);
    this.name = 'EngineLoadError';
  }
}

export interface LoadedEngine {
  readonly client: EngineClient;
  /** Each structure's state when the page opened (read-only "snapshot" requests). */
  readonly initial: Readonly<Record<StructureName, EngineResponse>>;
}

let loading: Promise<LoadedEngine> | null = null;

/**
 * Starts loading on the first call; later calls share the same promise. It is a
 * module-level promise rather than a component effect, so React StrictMode's
 * double effects in development can never create a second engine.
 */
export function loadEngine(): Promise<LoadedEngine> {
  loading ??= load();
  return loading;
}

async function load(): Promise<LoadedEngine> {
  const url = new URL(wasmAssetUrl, document.baseURI).href;

  if (typeof WebAssembly !== 'object') {
    throw new EngineLoadError(WASM_FILE, url, 'this browser has no WebAssembly support.',
      'Use a current version of Chrome, Edge, Firefox or Safari.');
  }

  let response: Response;
  try {
    response = await fetch(url);
  } catch (error) {
    throw new EngineLoadError(WASM_FILE, url, `the request failed (${describe(error)}).`,
      'Check that the site is being served (npm run dev or npm run preview) and that the network is up.');
  }
  if (!response.ok) {
    throw new EngineLoadError(WASM_FILE, url, `the server answered HTTP ${response.status} ${response.statusText}.`,
      'The file is missing. In development run "npm run build:wasm"; for a deployment run "npm run build" ' +
        'and upload the whole dist/ folder, including dist/assets/.');
  }

  let compiled: WebAssembly.Module;
  try {
    compiled = await WebAssembly.compile(await response.arrayBuffer());
  } catch (error) {
    throw new EngineLoadError(WASM_FILE, url, `the file is not valid WebAssembly (${describe(error)}).`,
      'Open the URL in a new tab: if it shows a web page, the server is returning HTML for a missing file. ' +
        'Rebuild with "npm run build:wasm".');
  }

  let module: Awaited<ReturnType<typeof createDslabEngine>>;
  try {
    module = await startRuntime(compiled);
  } catch (error) {
    throw new EngineLoadError(WASM_FILE, url, `the engine did not start (${describe(error)}).`,
      'dslab_engine.mjs and dslab_engine.wasm come from different builds. Run "npm run build:wasm" again.');
  }

  const client = new EngineClient(new module.Engine());
  const initial = {} as Record<StructureName, EngineResponse>;
  for (const structure of STRUCTURES) {
    initial[structure] = client.send(structure, 'snapshot');
  }
  return { client, initial };
}

/** Runs Emscripten's startup with our compiled module instead of letting it fetch one. */
function startRuntime(compiled: WebAssembly.Module): ReturnType<typeof createDslabEngine> {
  // Emscripten's promise never settles if instantiation fails, so we race it
  // against our own failure signal.
  let reportFailure: (error: unknown) => void = () => {};
  const failed = new Promise<never>((_, reject) => {
    reportFailure = reject;
  });
  const started = createDslabEngine({
    instantiateWasm(imports: WebAssembly.Imports, done: (instance: WebAssembly.Instance) => void) {
      WebAssembly.instantiate(compiled, imports).then(done, reportFailure);
      return {}; // instantiation finishes asynchronously
    },
  });
  return Promise.race([started, failed]);
}

function describe(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}
