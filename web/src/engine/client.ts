import {
  SCHEMA_VERSION,
  type EngineRequest,
  type EngineResponse,
  type StackOperation,
  type StructureName,
} from './protocol';

/** The one method of the C++ Embind class "Engine" that this page uses. */
export interface EngineHandle {
  handle(requestJson: string): string;
}

/**
 * Sends versioned JSON requests to the C++ engine and parses its answers.
 * It never interprets or changes structure data: C++ owns the state.
 */
export class EngineClient {
  private nextRequestId = 1;

  constructor(private readonly engine: EngineHandle) {}

  send(structure: StructureName, operation: StackOperation, args: Record<string, number> = {}): EngineResponse {
    const request: EngineRequest = {
      schemaVersion: SCHEMA_VERSION,
      requestId: this.nextRequestId++,
      structure,
      operation,
      args,
    };
    // A synchronous call into WebAssembly: Embind copies the string both ways.
    const response = JSON.parse(this.engine.handle(JSON.stringify(request))) as EngineResponse;
    if (response.schemaVersion !== SCHEMA_VERSION) {
      throw new Error(
        `The engine answered with schema version ${response.schemaVersion}, but this page speaks version ` +
          `${SCHEMA_VERSION}. An old cached dslab_engine.wasm may be in use: reload the page without the cache.`,
      );
    }
    return response;
  }
}
