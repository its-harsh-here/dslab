// Native command runner for the native-versus-Wasm comparison (web/e2e/parity.spec.ts).
// Reads one JSON request per line from standard input, sends each to one Engine
// through handleJson (exactly what the Wasm build's Engine.handle does) and
// writes one response per line to standard output.
#include <iostream>
#include <string>

#include "dslab/Engine.h"
#include "dslab/json/Protocol.h"

int main() {
    dslab::Engine engine;
    std::string line;
    while (std::getline(std::cin, line)) {
        std::cout << dslab::handleJson(engine, line) << '\n';
    }
    return 0;
}
