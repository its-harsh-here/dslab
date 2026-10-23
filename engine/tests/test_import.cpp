// Export and import (operation-contract.md 2.8 and 5.7), through the JSON entry
// point, as the page uses them. A valid file restores the structure exactly
// (IDs, values, links, colors, heights and the ID counter); an invalid file is
// rejected with a message naming the broken rule and changes nothing at all.
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include "dslab/Engine.h"
#include "dslab/json/Protocol.h"
#include "support/Checkers.h"

using namespace dslab;
using Json = nlohmann::ordered_json;

namespace {

std::string readFixture(const std::string& name) {
    std::ifstream file(std::string(DSLAB_FIXTURES_DIR) + "/" + name);
    REQUIRE_MESSAGE(file.good(), "cannot open fixture " << name);
    std::stringstream text;
    text << file.rdbuf();
    return text.str();
}

Json send(Engine& engine, const std::string& structure, const std::string& operation, const Json& args = Json::object()) {
    Json request = Json::object();
    request["schemaVersion"] = 1;
    request["requestId"] = 1;
    request["structure"] = structure;
    request["operation"] = operation;
    request["args"] = args;
    const Json response = Json::parse(handleJson(engine, request.dump()), nullptr, false);
    REQUIRE_FALSE(response.is_discarded());
    return response;
}

Json importText(Engine& engine, const std::string& structure, const std::string& text) {
    Json args = Json::object();
    args["text"] = text;
    return send(engine, structure, "import", args);
}

std::string exportText(Engine& engine, const std::string& structure) {
    const Json response = send(engine, structure, "export");
    REQUIRE(response["result"]["status"] == "ok");
    return response["result"]["data"]["text"].get<std::string>();
}

std::vector<std::string> eventsOf(const Json& response) {
    std::vector<std::string> names;
    for (const Json& frame : response["frames"]) names.push_back(frame["event"]);
    return names;
}

StructureKind kindOf(const std::string& structure) {
    if (structure == "stack") return StructureKind::Stack;
    if (structure == "queue") return StructureKind::Queue;
    if (structure == "list") return StructureKind::List;
    if (structure == "binaryTree") return StructureKind::BinaryTree;
    if (structure == "bst") return StructureKind::Bst;
    if (structure == "avl") return StructureKind::Avl;
    return StructureKind::RedBlack;
}

// A document for a linear structure: 'entries' are its entry pointers, each
// node is {id, value, next}.
std::string linearDocument(const std::string& structure, long long nextId, const Json& entries, const Json& nodes) {
    Json snapshot = Json::object();
    snapshot["structure"] = structure;
    snapshot["size"] = nodes.size();
    for (const auto& item : entries.items()) snapshot[item.key()] = item.value();
    snapshot["nodes"] = nodes;
    Json document = Json::object();
    document["format"] = "dslab-structure";
    document["formatVersion"] = 1;
    document["nextId"] = nextId;
    document["snapshot"] = snapshot;
    return document.dump(2);
}

Json linearNode(int id, int value, const Json& next) {
    return Json{{"id", id}, {"value", value}, {"next", next}};
}

// A valid fixture with one change applied.
std::string changed(const std::string& fixture, void (*change)(Json&)) {
    Json document = Json::parse(readFixture(fixture));
    change(document);
    return document.dump(2);
}

// Some content, so that "nothing changed" means something.
void fillWithSomething(Engine& engine, const std::string& structure) {
    const std::string add = structure == "stack"        ? "push"
                            : structure == "queue"      ? "enqueue"
                            : structure == "list"       ? "insertAt"
                            : structure == "binaryTree" ? "addRoot"
                                                        : "insert";
    Json args = Json::object();
    args["value"] = 77;
    if (structure == "list") args["index"] = 0;
    REQUIRE(send(engine, structure, add, args)["result"]["status"] == "ok");
    if (structure != "binaryTree") {
        args["value"] = 88;
        REQUIRE(send(engine, structure, add, args)["result"]["status"] == "ok");
    }
}

}  // namespace

TEST_CASE("import: a valid file of each tree type restores it exactly") {
    for (const std::string structure : {"binaryTree", "bst", "avl", "redBlack"}) {
        CAPTURE(structure);
        const std::string text = readFixture(structure + "-valid.json");
        Engine engine;
        const Json response = importText(engine, structure, text);
        REQUIRE(response["result"]["status"] == "ok");
        CHECK(response["result"]["data"]["nodeCount"] == 4);
        CHECK(eventsOf(response) == std::vector<std::string>{"begin", "restore", "done"});
        CHECK(test::checkSnapshot(engine.snapshot(kindOf(structure))) == "");  // the independent checker agrees

        // Exporting gives the file back, field for field (IDs, colors, heights, counter).
        CHECK(Json::parse(exportText(engine, structure)) == Json::parse(text));

        // The ID counter came from the file: the next node gets ID 5.
        Json args = Json::object();
        if (structure == "binaryTree") {
            args = Json{{"parentId", 3}, {"side", "left"}, {"value", 99}};
        } else {
            args["value"] = 99;
        }
        const Json added = send(engine, structure, structure == "binaryTree" ? "addChild" : "insert", args);
        REQUIRE(added["result"]["status"] == "ok");
        CHECK(added["result"]["data"]["nodeId"] == 5);
    }
}

TEST_CASE("import: export then import round-trips every structure exactly") {
    Engine source;
    for (const Json& request : Json::parse(R"([
        ["stack", "push", {"value": 5}], ["stack", "push", {"value": 6}], ["stack", "pop", {}], ["stack", "push", {"value": 7}],
        ["queue", "enqueue", {"value": 1}], ["queue", "enqueue", {"value": 2}], ["queue", "dequeue", {}],
        ["list", "insertAt", {"index": 0, "value": 3}], ["list", "insertAt", {"index": 1, "value": 4}], ["list", "reverse", {}],
        ["binaryTree", "addRoot", {"value": 1}], ["binaryTree", "addChild", {"parentId": 1, "side": "right", "value": 2}],
        ["bst", "insert", {"value": 50}], ["bst", "insert", {"value": 30}], ["bst", "insert", {"value": 70}], ["bst", "delete", {"value": 50}],
        ["avl", "insert", {"value": 30}], ["avl", "insert", {"value": 20}], ["avl", "insert", {"value": 10}], ["avl", "insert", {"value": 5}],
        ["redBlack", "insert", {"value": 10}], ["redBlack", "insert", {"value": 20}], ["redBlack", "insert", {"value": 30}],
        ["redBlack", "insert", {"value": 15}], ["redBlack", "delete", {"value": 30}]
    ])")) {
        REQUIRE(send(source, request[0], request[1], request[2])["result"]["status"] == "ok");
    }
    for (const std::string structure : {"stack", "queue", "list", "binaryTree", "bst", "avl", "redBlack"}) {
        CAPTURE(structure);
        const std::string text = exportText(source, structure);
        Engine target;
        REQUIRE(importText(target, structure, text)["result"]["status"] == "ok");
        CHECK(exportText(target, structure) == text);  // byte for byte
        // The C++ states are equal, ID counter included.
        CHECK(target.snapshot(kindOf(structure)) == source.snapshot(kindOf(structure)));
    }
}

TEST_CASE("import: export changes nothing and adds no history entry") {
    Engine engine;
    fillWithSomething(engine, "bst");
    const Json before = send(engine, "bst", "snapshot");
    const Json response = send(engine, "bst", "export");
    CHECK(eventsOf(response) == std::vector<std::string>{"begin", "done"});
    CHECK(response["snapshot"] == before["snapshot"]);
    CHECK(response["history"] == before["history"]);
    const Json document = Json::parse(response["result"]["data"]["text"].get<std::string>());
    CHECK(document["format"] == "dslab-structure");
    CHECK(document["formatVersion"] == 1);
    CHECK(document["nextId"] == 3);
    CHECK(document["snapshot"] == before["snapshot"]);
}

TEST_CASE("import: an invalid file is rejected with a clear message and changes nothing") {
    struct Case {
        const char* rule;
        std::string structure;
        std::string text;
        std::string message;  // the message must contain this
    };
    const std::string stackTop1 = linearDocument("stack", 3, Json{{"top", 2}},
                                                 Json::array({linearNode(1, 10, nullptr), linearNode(2, 20, 1)}));
    const std::vector<Case> cases = {
        // Rule 1: JSON, format, version, structure.
        {"1 not JSON", "bst", "{", "The file is not valid JSON."},
        {"1 not an object", "bst", "[1, 2]", "The file must hold one JSON object."},
        {"1 wrong format", "bst", changed("bst-valid.json", [](Json& d) { d["format"] = "other"; }),
         "This is not a DSLab export"},
        {"1 wrong version", "bst", changed("bst-valid.json", [](Json& d) { d["formatVersion"] = 2; }),
         "The file has format version 2, but this engine reads version 1."},
        {"1 another structure", "bst", stackTop1, "The file holds a stack, but this is the BST."},
        {"1 another structure (an)", "bst", readFixture("avl-valid.json"), "The file holds an AVL tree, but this is the BST."},
        {"1 unknown structure", "bst", changed("bst-valid.json", [](Json& d) { d["snapshot"]["structure"] = "heap"; }),
         "The file's structure, \"heap\", is unknown."},
        // Rule 2: exactly the right fields, with the right types.
        {"2 missing field", "avl", readFixture("avl-missing-field.json"), "Node 2 has no \"balance\" field."},
        {"2 unknown node field", "bst", readFixture("bst-extra-field.json"), "Node 4 has an unknown field, \"height\"."},
        {"2 unknown file field", "bst", changed("bst-valid.json", [](Json& d) { d["comment"] = "hi"; }),
         "The file has an unknown field, \"comment\"."},
        {"2 value not an integer", "bst", changed("bst-valid.json", [](Json& d) { d["snapshot"]["nodes"][0]["value"] = "ten"; }),
         "Node 1's value must be a 32-bit integer."},
        {"2 link 0", "bst", changed("bst-valid.json", [](Json& d) { d["snapshot"]["nodes"][1]["left"] = 0; }),
         "Node 2's left must be null or a node ID from 1 to 2147483647."},
        {"2 not a color", "redBlack", readFixture("redBlack-bad-color.json"), "Node 4's color must be \"red\" or \"black\"."},
        {"2 nextId not a number", "bst", changed("bst-valid.json", [](Json& d) { d["nextId"] = "five"; }),
         "nextId must be a whole number from 1 to 2147483648."},
        // Rule 3: count, size, IDs.
        {"3 too many nodes", "stack",
         [] {
             Json nodes = Json::array();
             for (int id = 1; id <= 201; ++id) nodes.push_back(linearNode(id, id, id == 1 ? Json(nullptr) : Json(id - 1)));
             return linearDocument("stack", 202, Json{{"top", 201}}, nodes);
         }(),
         "The file lists 201 nodes, but a structure holds at most 200."},
        {"3 size mismatch", "stack", [&] { Json d = Json::parse(stackTop1); d["snapshot"]["size"] = 3; return d.dump(); }(),
         "The file says size 3 but lists 2 nodes."},
        {"3 repeated ID", "stack",
         linearDocument("stack", 3, Json{{"top", 2}}, Json::array({linearNode(2, 10, nullptr), linearNode(2, 20, 2)})),
         "Node ID 2 appears twice."},
        {"3 IDs out of order", "stack",
         linearDocument("stack", 3, Json{{"top", 1}}, Json::array({linearNode(2, 10, nullptr), linearNode(1, 20, 2)})),
         "Nodes must be listed in ascending ID order, but node 1 comes after node 2."},
        {"3 ID 0", "stack", linearDocument("stack", 3, Json{{"top", nullptr}}, Json::array({linearNode(0, 10, nullptr)})),
         "Node ID 0 is not allowed: IDs start at 1."},
        // Rule 10: the ID counter.
        {"10 counter too small", "stack", [&] { Json d = Json::parse(stackTop1); d["nextId"] = 2; return d.dump(); }(),
         "nextId is 2, but it must be above the largest node ID (2) and at most 2147483648."},
        // Rule 4: links name listed nodes.
        {"4 link to a missing node", "binaryTree", readFixture("binaryTree-missing-node.json"),
         "Node 2's right points to node 9, which is not in the file."},
        {"4 entry to a missing node", "stack", [&] { Json d = Json::parse(stackTop1); d["snapshot"]["top"] = 9; return d.dump(); }(),
         "top points to node 9, which is not in the file."},
        // Rule 5: linear chains.
        {"5 entry NIL with nodes", "stack", [&] { Json d = Json::parse(stackTop1); d["snapshot"]["top"] = nullptr; return d.dump(); }(),
         "top must be null exactly when there are no nodes."},
        {"5 cycle", "stack",
         linearDocument("stack", 3, Json{{"top", 2}}, Json::array({linearNode(1, 10, 2), linearNode(2, 20, 1)})),
         "Following next from top comes back to node 2: the links form a cycle."},
        {"5 unreachable node", "list",
         linearDocument("list", 4, Json{{"head", 1}},
                        Json::array({linearNode(1, 10, 2), linearNode(2, 20, nullptr), linearNode(3, 30, nullptr)})),
         "Node 3 cannot be reached from head."},
        {"5 wrong rear", "queue",
         linearDocument("queue", 3, Json{{"front", 1}, {"rear", 1}}, Json::array({linearNode(1, 10, 2), linearNode(2, 20, nullptr)})),
         "rear is node 1, but the last node of the chain is node 2."},
        // Rule 6: tree shape.
        {"6 two parents", "binaryTree", readFixture("binaryTree-two-parents.json"), "Node 4 has two parents: node 2 and node 3."},
        {"6 unreachable cycle", "binaryTree", readFixture("binaryTree-unreachable-cycle.json"),
         "Node 4 cannot be reached from the root."},
        {"6 wrong parent", "binaryTree", readFixture("binaryTree-wrong-parent.json"),
         "Node 4 says it has parent node 3, but node 2 links to it."},
        {"6 root with a parent", "bst", readFixture("bst-root-with-parent.json"),
         "Node 1 says it has parent node 3, but it is the root."},
        // Rule 7: BST order.
        {"7 order", "bst", readFixture("bst-order.json"),
         "In-order values must increase, but 25 (node 4) comes before 20 (node 1)."},
        {"7 duplicate", "bst", readFixture("bst-duplicate.json"),
         "The value 30 appears twice (nodes 4 and 3), but a BST has no duplicates."},
        // Rule 8: AVL heights and balances.
        {"8 height", "avl", readFixture("avl-wrong-height.json"), "Node 3 stores height 3, but its real height is 2."},
        {"8 balance field", "avl", readFixture("avl-wrong-balance.json"),
         "Node 3 stores balance 0, but height(left) - height(right) is 1."},
        {"8 unbalanced", "avl", readFixture("avl-unbalanced.json"),
         "Node 1 has balance +2, but an AVL tree allows only -1, 0 or +1."},
        // Rule 9: red-black properties.
        {"9 red root", "redBlack", readFixture("redBlack-red-root.json"),
         "The root, node 1, is red, but a red-black root must be black."},
        {"9 red under red", "redBlack", readFixture("redBlack-red-red.json"), "Node 3 is red and so is its child, node 4."},
        {"9 black heights", "redBlack", readFixture("redBlack-black-height.json"),
         "The paths below node 1 have different numbers of black nodes."},
        {"9 red NIL", "redBlack", readFixture("redBlack-red-nil.json"),
         "nilColor must be black: every NIL leaf of a red-black tree is black."},
    };

    for (const Case& c : cases) {
        CAPTURE(c.rule);
        Engine engine;
        fillWithSomething(engine, c.structure);
        const Json before = send(engine, c.structure, "snapshot");
        const std::string exportBefore = exportText(engine, c.structure);  // includes the ID counter

        const Json response = importText(engine, c.structure, c.text);
        REQUIRE(response["result"]["status"] == "error");
        CHECK(response["result"]["code"] == "IMPORT_INVALID");
        const std::string message = response["result"]["message"];
        CHECK(message.find(c.message) != std::string::npos);
        CHECK(message.size() <= 140);
        CHECK(message.back() == '.');

        // An operation error: begin, then reject, whose sentence is the message.
        CHECK(eventsOf(response) == std::vector<std::string>{"begin", "reject"});
        CHECK(response["frames"].back()["explanation"] == message);
        // Nothing changed: state, ID counter and history.
        CHECK(response["snapshot"] == before["snapshot"]);
        CHECK(response["history"] == before["history"]);
        CHECK(exportText(engine, c.structure) == exportBefore);
    }
}

TEST_CASE("import: a file over 1 MiB is a protocol error and changes nothing") {
    Engine engine;
    fillWithSomething(engine, "bst");
    const std::string exportBefore = exportText(engine, "bst");
    const Json response = importText(engine, "bst", std::string(1024 * 1024, ' '));
    CHECK(response["result"]["code"] == "BAD_REQUEST");
    CHECK(response["frames"].empty());
    CHECK(exportText(engine, "bst") == exportBefore);
}
