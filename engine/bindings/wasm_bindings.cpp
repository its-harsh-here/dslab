// The Embind facade: the only C++ object JavaScript ever holds
// (docs/architecture.md, 2.4). One method takes a JSON request (schema
// version 1) and returns the JSON response; nothing else crosses the boundary.
#include <string>

#include <emscripten/bind.h>

#include "dslab/Engine.h"
#include "dslab/json/Protocol.h"

namespace {

// Owns the engine for as long as JavaScript keeps its handle (the page lifetime).
class WasmEngine {
public:
    std::string handle(const std::string& requestJson) {
        return dslab::handleJson(engine_, requestJson);
    }

private:
    dslab::Engine engine_;
};

}  // namespace

EMSCRIPTEN_BINDINGS(dslab) {
    emscripten::class_<WasmEngine>("Engine")
        .constructor<>()
        .function("handle", &WasmEngine::handle);
}
