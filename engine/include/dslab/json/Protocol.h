// The single JSON entry point (docs/operation-contract.md, section 5).
// The Wasm bindings and the native tests both call it, so they see exactly the
// same text. This layer is the only engine code that knows about JSON.
#pragma once

#include <string>

#include "dslab/Engine.h"

namespace dslab {

// Reads one request (JSON text, schema version 1), runs it on 'engine' and
// returns the response as compact JSON text. Never throws.
std::string handleJson(Engine& engine, const std::string& requestText);

}  // namespace dslab
