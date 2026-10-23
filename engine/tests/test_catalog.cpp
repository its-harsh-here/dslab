// The pseudocode catalog (operation-contract.md 5.6). Together with the check in
// test::run(), which looks up the step ID of every frame of every operation in
// every test, this keeps listings and frames from drifting apart.
#include <set>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include "dslab/Catalog.h"
#include "dslab/Engine.h"
#include "dslab/json/Protocol.h"
#include "support/Checkers.h"
#include "support/Run.h"

using namespace dslab;
using Json = nlohmann::ordered_json;

namespace {

const std::vector<StructureKind> kStructures = {StructureKind::Stack, StructureKind::Queue,
                                                StructureKind::List,  StructureKind::BinaryTree,
                                                StructureKind::Bst,   StructureKind::Avl,
                                                StructureKind::RedBlack};

Json catalogResponse(Engine& engine, StructureKind structure) {
    const std::string request = R"({"schemaVersion":1,"requestId":1,"structure":")" + test::wireName(structure) +
                                R"(","operation":"catalog","args":{}})";
    return Json::parse(handleJson(engine, request));
}

}  // namespace

TEST_CASE("catalog: every operation that runs an algorithm has a listing, in contract order") {
    const std::vector<std::string> trees = {"insert", "delete", "search", "preorder", "inorder", "postorder",
                                            "levelorder", "clear"};
    const std::vector<std::pair<StructureKind, std::vector<std::string>>> expected = {
        {StructureKind::Stack, {"push", "pop", "peek", "clear"}},
        {StructureKind::Queue, {"enqueue", "dequeue", "front", "clear"}},
        {StructureKind::List, {"insertAt", "deleteNode", "search", "reverse", "clear"}},
        {StructureKind::BinaryTree,
         {"addRoot", "addChild", "deleteSubtree", "preorder", "inorder", "postorder", "levelorder", "clear"}},
        {StructureKind::Bst, trees},
        {StructureKind::Avl, trees},
        {StructureKind::RedBlack, trees},
    };
    Engine engine;
    for (const auto& [structure, operations] : expected) {
        CAPTURE(test::wireName(structure));
        const Json response = catalogResponse(engine, structure);
        REQUIRE(response["result"]["status"] == "ok");
        std::vector<std::string> names;
        for (const Json& operation : response["result"]["data"]["operations"]) {
            names.push_back(operation["operation"]);
        }
        CHECK(names == operations);
    }
}

TEST_CASE("catalog: listings are well formed") {
    for (const StructureKind structure : kStructures) {
        const std::string prefix = test::wireName(structure) + ".";
        for (const OperationListing& listing : catalogFor(structure)) {
            CAPTURE(prefix);
            CAPTURE(listing.lines.front().text);
            CHECK_FALSE(listing.complexity.empty());
            REQUIRE_FALSE(listing.lines.empty());
            CHECK_FALSE(listing.lines.front().stepId.empty());  // the first line is where the operation starts
            std::set<std::string> seen;
            for (const PseudocodeLine& line : listing.lines) {
                CHECK_FALSE(line.text.empty());
                if (line.stepId.empty()) continue;
                CAPTURE(line.stepId);
                CHECK(line.stepId.rfind(prefix, 0) == 0);
                CHECK(seen.insert(line.stepId).second);  // one line per step ID, so the UI highlights one line
            }
        }
    }
}

TEST_CASE("catalog: the JSON shape of a catalog response") {
    Engine engine;
    const Json response = catalogResponse(engine, StructureKind::List);
    REQUIRE(response["result"]["status"] == "ok");

    // Like "snapshot": two frames, no change, no history entry.
    REQUIRE(response["frames"].size() == 2);
    CHECK(response["frames"][0]["stepId"] == "common.catalog.enter");
    CHECK(response["frames"][1]["stepId"] == "common.catalog.done");
    CHECK(response["history"]["undo"] == 0);

    const Json& insertAt = response["result"]["data"]["operations"][0];
    std::vector<std::string> keys;
    for (const auto& item : insertAt.items()) keys.push_back(item.key());
    CHECK(keys == std::vector<std::string>{"operation", "args", "mutates", "complexity", "pseudocode"});
    CHECK(insertAt["args"] == Json::array({"index", "value"}));
    CHECK(insertAt["mutates"] == true);
    CHECK(insertAt["pseudocode"][0] == Json{{"stepId", "list.insertAt.enter"}, {"text", "insertAt(index, value):"}});
    CHECK(insertAt["pseudocode"][3]["stepId"].is_null());  // "  if index > 0:" records no frame

    const Json binaryTree = catalogResponse(engine, StructureKind::BinaryTree);
    CHECK(binaryTree["result"]["data"]["operations"][1]["args"] == Json::array({"parentId", "side", "value"}));
    const Json bst = catalogResponse(engine, StructureKind::Bst);
    CHECK(bst["result"]["data"]["operations"][2]["operation"] == "search");
    CHECK(bst["result"]["data"]["operations"][2]["mutates"] == false);
}

TEST_CASE("catalog: the catalog operation passes the frame contract on every structure") {
    Engine engine;
    for (const StructureKind structure : kStructures) {
        const Response response = test::run(engine, structure, Operation::Catalog);
        CHECK(response.status.ok());
        CHECK(response.data.empty());  // the listings are written by the JSON layer
    }
}
