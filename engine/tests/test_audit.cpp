// Audit checks across every structure (milestone 8):
//  - stored frames are plain copies: they never alias live nodes, so later
//    operations (and freed nodes) cannot change a response already returned;
//  - invalid input of every kind changes nothing: state, ID counter, history;
//  - engines can be created, filled, reset and destroyed repeatedly, with
//    structure switching in between (run under ASan/UBSan this also catches
//    leaks and use-after-free).
#include <algorithm>
#include <random>
#include <string>
#include <tuple>
#include <vector>

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include "dslab/Engine.h"
#include "dslab/json/Protocol.h"
#include "support/Checkers.h"
#include "support/Run.h"

using namespace dslab;
using Json = nlohmann::ordered_json;

namespace {

const std::vector<StructureKind> kAll = {StructureKind::Stack, StructureKind::Queue, StructureKind::List,
                                         StructureKind::BinaryTree, StructureKind::Bst, StructureKind::Avl,
                                         StructureKind::RedBlack};

Request request(StructureKind structure, Operation operation, Value value = 0) {
    Request r;
    r.structure = structure;
    r.operation = operation;
    r.value = value;
    return r;
}

// One successful node-creating request for each structure.
Request add(StructureKind structure, Value value) {
    switch (structure) {
        case StructureKind::Stack: return request(structure, Operation::Push, value);
        case StructureKind::Queue: return request(structure, Operation::Enqueue, value);
        case StructureKind::List: return request(structure, Operation::InsertAt, value);  // index 0
        case StructureKind::BinaryTree: return request(structure, Operation::AddRoot, value);
        default: return request(structure, Operation::Insert, value);
    }
}

Json send(Engine& engine, const std::string& text) {
    return Json::parse(handleJson(engine, text));
}

std::string exportOf(Engine& engine, const std::string& structure) {
    const Json response = send(engine, R"({"schemaVersion":1,"requestId":1,"structure":")" + structure +
                                           R"(","operation":"export","args":{}})");
    return response["result"]["data"]["text"];
}

}  // namespace

TEST_CASE("audit: stored frames never alias live nodes") {
    for (const StructureKind structure : kAll) {
        CAPTURE(test::wireName(structure));
        Engine engine;
        for (const Value value : {30, 10, 50}) {
            if (structure == StructureKind::BinaryTree && value != 30) break;  // only one root
            test::run(engine, add(structure, value));
        }
        const Snapshot before = engine.snapshot(structure);
        const Response kept = test::run(engine, request(structure, Operation::Clear));
        const std::vector<Frame> copy = kept.frames;  // a separate copy, taken now

        // Free every node, create new ones (with new IDs), undo, redo: the live
        // nodes the frames once showed are gone or replaced.
        for (int round = 0; round < 3; ++round) {
            test::run(engine, add(structure, 70 + round));
            test::run(engine, request(structure, Operation::Clear));
        }
        test::run(engine, request(structure, Operation::Undo));
        test::run(engine, request(structure, Operation::Redo));

        REQUIRE(kept.frames.size() == copy.size());
        for (std::size_t i = 0; i < copy.size(); ++i) {
            CHECK(kept.frames[i].snapshot == copy[i].snapshot);
            CHECK(kept.frames[i].explanation == copy[i].explanation);
        }
        CHECK(kept.frames.front().snapshot == before);  // still the state before the clear
        CHECK(test::checkFrames(kept, before) == "");
    }
}

TEST_CASE("audit: invalid input of every kind changes nothing") {
    const std::vector<std::pair<std::string, std::vector<std::string>>> cases = {
        {"stack", {R"("operation":"push","args":{})", R"("operation":"push","args":{"value":"7"})",
                   R"("operation":"push","args":{"value":2147483648})", R"("operation":"push","args":{"value":1,"index":0})"}},
        {"queue", {R"("operation":"enqueue","args":{"value":-2147483649})", R"("operation":"enqueue","args":{"value":1.5})"}},
        {"list", {R"("operation":"insertAt","args":{"index":5,"value":1})", R"("operation":"insertAt","args":{"index":-1,"value":1})",
                  R"("operation":"deleteNode","args":{"nodeId":999})", R"("operation":"deleteNode","args":{"nodeId":1.5})"}},
        {"binaryTree", {R"("operation":"addChild","args":{"parentId":999,"side":"left","value":1})",
                        R"("operation":"addChild","args":{"parentId":1,"side":"up","value":1})",
                        R"("operation":"deleteSubtree","args":{"nodeId":999})"}},
        {"bst", {R"("operation":"delete","args":{"value":12345})", R"("operation":"insert","args":{"value":null})"}},
        {"avl", {R"("operation":"delete","args":{"value":12345})", R"("operation":"rotate","args":{})"}},
        {"redBlack", {R"("operation":"delete","args":{"value":12345})", R"("operation":"import","args":{"text":"{"})",
                      R"("operation":"import","args":{"text":5})"}},
    };
    const std::vector<std::string> everywhere = {
        R"("operation":"redo","args":{})",
        R"("operation":"import","args":{"text":"{\"format\":\"dslab-structure\",\"formatVersion\":9}"})",
        R"("operation":"insert","args":{"value":1}, "extra":true)",
    };
    for (const auto& [structure, bodies] : cases) {
        CAPTURE(structure);
        Engine engine;
        // Some content first, so that "unchanged" means something.
        for (const char* setup : {R"("operation":"clear","args":{})"}) {
            send(engine, R"({"schemaVersion":1,"requestId":1,"structure":")" + structure + "\"," + setup + "}");
        }
        const StructureKind kind = [&] {
            for (const StructureKind k : kAll) if (test::wireName(k) == structure) return k;
            return StructureKind::Stack;
        }();
        test::run(engine, add(kind, 40));
        if (kind != StructureKind::BinaryTree) test::run(engine, add(kind, 60));

        std::vector<std::string> all = bodies;
        all.insert(all.end(), everywhere.begin(), everywhere.end());
        all.push_back("not json at all");
        all.push_back(R"({"schemaVersion":2,"requestId":1,"structure":")" + structure + R"(","operation":"clear","args":{}})");
        for (const std::string& body : all) {
            CAPTURE(body);
            const std::string text = body.front() == '{' || body.front() == 'n'
                                         ? body
                                         : R"({"schemaVersion":1,"requestId":1,"structure":")" + structure + "\"," + body + "}";
            const std::string exportBefore = exportOf(engine, structure);
            const Json historyBefore = send(engine, R"({"schemaVersion":1,"requestId":1,"structure":")" + structure +
                                                        R"(","operation":"snapshot","args":{}})")["history"];
            const Json response = send(engine, text);
            CHECK(response["result"]["status"] == "error");
            CHECK(exportOf(engine, structure) == exportBefore);  // nodes, links and the ID counter
            if (response["history"].is_object()) CHECK(response["history"] == historyBefore);
            CHECK(test::checkSnapshot(engine.snapshot(kind)) == "");
        }
    }
}

TEST_CASE("audit: errors on an empty structure change nothing") {
    Engine engine;
    for (const auto& [structure, operation, code] :
         std::vector<std::tuple<StructureKind, Operation, ErrorCode>>{
             {StructureKind::Stack, Operation::Pop, ErrorCode::EmptyStructure},
             {StructureKind::Stack, Operation::Peek, ErrorCode::EmptyStructure},
             {StructureKind::Queue, Operation::Dequeue, ErrorCode::EmptyStructure},
             {StructureKind::Queue, Operation::Front, ErrorCode::EmptyStructure},
             {StructureKind::Bst, Operation::Delete, ErrorCode::ValueNotFound},
             {StructureKind::Avl, Operation::Undo, ErrorCode::NothingToUndo},
             {StructureKind::RedBlack, Operation::Redo, ErrorCode::NothingToRedo}}) {
        CAPTURE(test::wireName(structure));
        test::checkRejected(engine, request(structure, operation), code);  // state, counter and history unchanged
        CHECK(engine.snapshot(structure).nextId == 1);
    }
}

TEST_CASE("audit: when node IDs run out, every node-creating operation is rejected and changes nothing") {
    for (const StructureKind structure : kAll) {
        CAPTURE(test::wireName(structure));
        Engine engine;
        // Reachable only through a file: the counter at its maximum (2147483647 was the last ID).
        Snapshot full;
        full.structure = structure;
        full.nextId = 2147483648LL;
        full.nilColor = structure == StructureKind::RedBlack ? Color::Black : Color::None;
        if (structure == StructureKind::BinaryTree) {
            // A root, so that addChild has a parent.
            NodeView root;
            root.id = 2147483647;
            root.value = 1;
            full.nodes.push_back(root);
            full.size = 1;
            full.root = root.id;
        }
        Request import = request(structure, Operation::Import);
        import.importSnapshot = full;
        REQUIRE(test::run(engine, import).status.ok());

        Request create = structure == StructureKind::BinaryTree ? request(structure, Operation::AddChild, 2) : add(structure, 2);
        create.parentId = 2147483647;
        test::checkRejected(engine, create, ErrorCode::IdExhausted);
        CHECK(engine.snapshot(structure).nextId == 2147483648LL);
    }
}

TEST_CASE("audit: traversal sentences use the right article") {
    Engine engine;
    for (const Value value : {2, 1, 3}) test::run(engine, request(StructureKind::Avl, Operation::Insert, value));
    const Response traversal = test::run(engine, request(StructureKind::Avl, Operation::Inorder));
    CHECK(traversal.frames.front().explanation == "In-order traversal of an AVL tree that holds 3 nodes.");
    const Response empty = test::run(engine, request(StructureKind::Bst, Operation::Preorder));
    CHECK(empty.frames.front().explanation == "Pre-order traversal of an empty BST.");
}

// The first wording slip in 'sentence', or "": "a" before a vowel or "an"
// before a consonant, "1 nodes" or "N node", a doubled word, a double space.
std::string wordingSlip(const std::string& sentence) {
    if (sentence.find("  ") != std::string::npos) return "double space";
    std::vector<std::string> words;
    std::string word;
    for (const char c : sentence + " ") {
        if (c == ' ') {
            if (!word.empty()) words.push_back(word);
            word.clear();
        } else {
            word += c;
        }
    }
    const auto vowel = [](char c) { return std::string("AEIOUaeiou").find(c) != std::string::npos; };
    const auto number = [](const std::string& w) {
        return !w.empty() && std::all_of(w.begin(), w.end(), [](char c) { return c >= '0' && c <= '9'; });
    };
    for (std::size_t i = 0; i + 1 < words.size(); ++i) {
        const std::string& a = words[i];
        const std::string& b = words[i + 1];
        if ((a == "a" || a == "A") && vowel(b[0])) return "\"a\" before " + b;
        if ((a == "an" || a == "An") && !vowel(b[0])) return "\"an\" before " + b;
        const std::string noun = b.substr(0, b.find_first_of(".,;:"));
        if (a == "1" && noun == "nodes") return "1 nodes";
        if (number(a) && a != "1" && noun == "node") return a + " node";
        if (a == b) return "doubled word " + a;
    }
    return "";
}

TEST_CASE("audit: no generated sentence has a wording slip") {
    // The checker itself, on known slips.
    CHECK(wordingSlip("Traversal of a AVL tree.") == "\"a\" before AVL");
    CHECK(wordingSlip("It holds 1 nodes.") == "1 nodes");
    CHECK(wordingSlip("It holds 3 node.") == "3 node");
    CHECK(wordingSlip("Insert the the value.") == "doubled word the");
    CHECK(wordingSlip("Traversal of an AVL tree that holds 1 node.") == "");

    // Every explanation and error message from random operations on every structure.
    std::mt19937 random(8007);
    Engine engine;
    int checked = 0;
    for (int step = 0; step < 3000; ++step) {
        const StructureKind structure = kAll[random() % kAll.size()];
        const unsigned kind = random() % 10;
        Request r = kind < 6   ? add(structure, static_cast<Value>(random() % 40))
                    : kind < 7 ? request(structure, Operation::Undo)
                    : kind < 8 ? request(structure, Operation::Clear)
                    : structure == StructureKind::Stack || structure == StructureKind::Queue || structure == StructureKind::List
                        ? request(structure, Operation::Export)
                        : request(structure, random() % 2 == 0 ? Operation::Levelorder : Operation::Inorder);
        if (structure == StructureKind::BinaryTree && kind < 6 && engine.snapshot(structure).size > 0) {
            r = request(structure, Operation::AddChild, static_cast<Value>(step));
            r.parentId = engine.snapshot(structure).nodes.back().id;
            r.side = random() % 2 == 0 ? Side::Left : Side::Right;
        }
        const Response response = test::run(engine, r);
        std::vector<std::string> sentences;
        for (const Frame& frame : response.frames) sentences.push_back(frame.explanation);
        if (!response.status.ok()) sentences.push_back(response.status.message);
        for (const std::string& sentence : sentences) {
            CAPTURE(sentence);
            CHECK(wordingSlip(sentence) == "");
            ++checked;
        }
    }
    CHECK(checked > 10000);
}

TEST_CASE("audit: repeated create, fill, reset and destroy across all structures") {
    std::mt19937 random(8006);
    for (int round = 0; round < 60; ++round) {
        CAPTURE(round);
        Engine engine;  // created and destroyed every round
        for (int step = 0; step < 40; ++step) {
            // Switch structure on every step.
            const StructureKind structure = kAll[random() % kAll.size()];
            const unsigned kind = random() % 10;
            const Value value = static_cast<Value>(random() % 50);
            Request r = kind < 5 ? add(structure, value)
                        : kind < 7 ? request(structure, Operation::Clear)
                        : kind < 8 ? request(structure, Operation::Undo)
                        : kind < 9 ? request(structure, Operation::Redo)
                                   : request(structure, Operation::Export);
            test::run(engine, r);
        }
        // Reset everything (clear each structure), then import one exported state back.
        for (const StructureKind structure : kAll) {
            const Snapshot before = engine.snapshot(structure);
            test::run(engine, request(structure, Operation::Clear));
            Request back = request(structure, Operation::Import);
            back.importSnapshot = before;
            REQUIRE(test::run(engine, back).status.ok());
            CHECK(engine.snapshot(structure) == before);
        }
    }
}
