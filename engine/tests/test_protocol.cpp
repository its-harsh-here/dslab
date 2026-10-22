// The JSON entry point: golden files copied from docs/operation-contract.md,
// and every protocol error code.
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include "dslab/Engine.h"
#include "dslab/json/Protocol.h"

using namespace dslab;
using Json = nlohmann::ordered_json;

namespace {

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    REQUIRE_MESSAGE(file.good(), "cannot open " << path);
    std::stringstream text;
    text << file.rdbuf();
    return text.str();
}

// Sends one request and parses the answer, which must always be valid JSON.
Json send(Engine& engine, const std::string& requestText) {
    const Json response = Json::parse(handleJson(engine, requestText), nullptr, false);
    REQUIRE_FALSE(response.is_discarded());
    return response;
}

std::string pushRequest(const std::string& valueText, int requestId = 1) {
    return R"({"schemaVersion":1,"requestId":)" + std::to_string(requestId) +
           R"(,"structure":"stack","operation":"push","args":{"value":)" + valueText + "}}";
}

}  // namespace

TEST_CASE("golden files from the operation contract") {
    // Compared as ordered JSON: key order matters, whitespace does not.
    for (const char* name : {"stack-push-30.json", "stack-push-value-out-of-range.json"}) {
        CAPTURE(name);
        const Json golden = Json::parse(readFile(std::string(DSLAB_GOLDEN_DIR) + "/" + name));

        Engine engine;
        for (const Json& setup : golden["setup"]) {
            REQUIRE(send(engine, setup.dump())["result"]["status"] == "ok");
        }
        const Json actual = send(engine, golden["request"].dump());
        CHECK_MESSAGE(actual == golden["response"],
                      "expected:\n" << golden["response"].dump(2) << "\nactual:\n" << actual.dump(2));
    }
}

TEST_CASE("protocol errors return a typed code, no frames and no change") {
    struct Case {
        const char* name;
        std::string request;
        const char* code;
        bool structureKnown;
    };
    const std::vector<Case> cases = {
        {"not JSON", "{", "BAD_REQUEST", false},
        {"not an object", "[1, 2]", "BAD_REQUEST", false},
        {"invalid UTF-8", "\xff\xfe", "BAD_REQUEST", false},
        {"missing version",
         R"({"requestId":1,"structure":"stack","operation":"pop","args":{}})", "BAD_REQUEST", false},
        {"version as text",
         R"({"schemaVersion":"1","requestId":1,"structure":"stack","operation":"pop","args":{}})",
         "BAD_REQUEST", false},
        {"future version",
         R"({"schemaVersion":2,"requestId":1,"structure":"stack","operation":"pop","args":{}})",
         "UNSUPPORTED_VERSION", false},
        {"unknown top-level field",
         R"({"schemaVersion":1,"requestId":1,"structure":"stack","operation":"pop","args":{},"x":0})",
         "BAD_REQUEST", false},
        {"missing args", R"({"schemaVersion":1,"requestId":1,"structure":"stack","operation":"pop"})",
         "BAD_REQUEST", false},
        {"negative requestId",
         R"({"schemaVersion":1,"requestId":-1,"structure":"stack","operation":"pop","args":{}})",
         "BAD_REQUEST", false},
        {"structure not yet built",
         R"({"schemaVersion":1,"requestId":1,"structure":"bst","operation":"insert","args":{"value":1}})",
         "UNKNOWN_STRUCTURE", false},
        {"operation not yet built",
         R"({"schemaVersion":1,"requestId":1,"structure":"stack","operation":"undo","args":{}})",
         "UNKNOWN_OPERATION", true},
        {"argument on pop",
         R"({"schemaVersion":1,"requestId":1,"structure":"stack","operation":"pop","args":{"value":1}})",
         "BAD_REQUEST", true},
        {"extra argument on push",
         R"({"schemaVersion":1,"requestId":1,"structure":"stack","operation":"push","args":{"value":1,"index":0}})",
         "BAD_REQUEST", true},
        {"push without a value",
         R"({"schemaVersion":1,"requestId":1,"structure":"stack","operation":"push","args":{}})",
         "BAD_REQUEST", true},
        {"fractional value", pushRequest("1.5"), "BAD_REQUEST", true},
        {"value as text", pushRequest("\"7\""), "BAD_REQUEST", true},
        {"value just above the range", pushRequest("2147483648"), "VALUE_OUT_OF_RANGE", true},
        {"value just below the range", pushRequest("-2147483649"), "VALUE_OUT_OF_RANGE", true},
        {"request over 1 MiB", pushRequest("1") + std::string(1024 * 1024, ' '), "BAD_REQUEST",
         false},
    };

    for (const Case& c : cases) {
        CAPTURE(c.name);
        Engine engine;
        send(engine, pushRequest("10"));  // some state that must survive the error
        const Json before = send(engine, R"({"schemaVersion":1,"requestId":2,"structure":"stack","operation":"snapshot","args":{}})");

        const Json response = send(engine, c.request);
        CHECK(response["schemaVersion"] == 1);
        CHECK(response["result"]["status"] == "error");
        CHECK(response["result"]["code"] == c.code);
        CHECK(response["frames"] == Json::array());
        if (c.structureKnown) {
            CHECK(response["structure"] == "stack");
            CHECK(response["snapshot"] == before["snapshot"]);
            CHECK(response["history"] == before["history"]);
        } else {
            CHECK(response["snapshot"].is_null());
            CHECK(response["history"].is_null());
        }

        // Nothing changed, and no node ID was used: the next node gets ID 2.
        const Json next = send(engine, pushRequest("20", 3));
        CHECK(next["result"]["data"]["nodeId"] == 2);
    }
}

TEST_CASE("the smallest and largest 32-bit values are accepted") {
    Engine engine;
    CHECK(send(engine, pushRequest("-2147483648"))["result"]["status"] == "ok");
    CHECK(send(engine, pushRequest("2147483647"))["result"]["status"] == "ok");
}

TEST_CASE("valid parts of a request are echoed, others are null") {
    Engine engine;
    const Json ok = send(engine, pushRequest("5", 41));
    CHECK(ok["requestId"] == 41);
    CHECK(ok["structure"] == "stack");
    CHECK(ok["operation"] == "push");

    const Json unknownOperation = send(
        engine, R"({"schemaVersion":1,"requestId":42,"structure":"stack","operation":"fly","args":{}})");
    CHECK(unknownOperation["requestId"] == 42);
    CHECK(unknownOperation["structure"] == "stack");
    CHECK(unknownOperation["operation"].is_null());

    const Json broken = send(engine, "not json");
    CHECK(broken["requestId"].is_null());
    CHECK(broken["structure"].is_null());
}
