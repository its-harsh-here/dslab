// JSON request/response layer (docs/operation-contract.md, section 5).
// Algorithm classes never see JSON: this file turns text into a Request and a
// Response back into text.
#include "dslab/json/Protocol.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "dslab/Catalog.h"
#include "dslab/Limits.h"

namespace dslab {

namespace {

// Keys stay in the order they are written, so output matches the contract.
using Json = nlohmann::ordered_json;

// ---------------------------------------------------------------------------
// The structures, their operations and the arguments each operation takes

// Arguments an operation can take, combined with |.
enum ArgFlag : unsigned {
    kNoArgs = 0,
    kValueArg = 1,
    kIndexArg = 2,
    kNodeIdArg = 4,
    kParentIdArg = 8,
    kSideArg = 16,
    kTextArg = 32,  // import: the export document, as text
};

// The integer arguments, in the order they are checked.
struct IntegerArg {
    ArgFlag flag;
    const char* name;
};

const IntegerArg kIntegerArgs[] = {
    {kParentIdArg, "parentId"},
    {kIndexArg, "index"},
    {kValueArg, "value"},
    {kNodeIdArg, "nodeId"},
};

// The flag of the argument called 'name', or kNoArgs when there is none.
unsigned argFlag(const std::string& name) {
    if (name == "side") {
        return kSideArg;
    }
    if (name == "text") {
        return kTextArg;
    }
    for (const IntegerArg& arg : kIntegerArgs) {
        if (name == arg.name) {
            return arg.flag;
        }
    }
    return kNoArgs;
}

// Puts a checked integer argument into its field of the request.
void storeInteger(Request& request, ArgFlag flag, std::int32_t number) {
    switch (flag) {
        case kValueArg: request.value = number; break;
        case kIndexArg: request.index = number; break;
        case kNodeIdArg: request.nodeId = number; break;
        case kParentIdArg: request.parentId = number; break;
        case kSideArg:
        case kTextArg:
        case kNoArgs: break;
    }
}

struct OperationInfo {
    const char* name;
    Operation operation;
    unsigned args;
};

struct StructureInfo {
    const char* name;         // on the wire, e.g. "stack"
    const char* displayName;  // in messages, e.g. "the stack"
    StructureKind kind;
    std::vector<OperationInfo> operations;
};

const std::vector<StructureInfo>& structureTable() {
    static const std::vector<StructureInfo> table = {
        {"stack", "stack", StructureKind::Stack,
         {{"push", Operation::Push, kValueArg},
          {"pop", Operation::Pop, kNoArgs},
          {"peek", Operation::Peek, kNoArgs},
          {"clear", Operation::Clear, kNoArgs},
          {"snapshot", Operation::Snapshot, kNoArgs},
          {"catalog", Operation::Catalog, kNoArgs},
          {"undo", Operation::Undo, kNoArgs},
          {"redo", Operation::Redo, kNoArgs},
          {"export", Operation::Export, kNoArgs},
          {"import", Operation::Import, kTextArg}}},
        {"queue", "queue", StructureKind::Queue,
         {{"enqueue", Operation::Enqueue, kValueArg},
          {"dequeue", Operation::Dequeue, kNoArgs},
          {"front", Operation::Front, kNoArgs},
          {"clear", Operation::Clear, kNoArgs},
          {"snapshot", Operation::Snapshot, kNoArgs},
          {"catalog", Operation::Catalog, kNoArgs},
          {"undo", Operation::Undo, kNoArgs},
          {"redo", Operation::Redo, kNoArgs},
          {"export", Operation::Export, kNoArgs},
          {"import", Operation::Import, kTextArg}}},
        {"list", "list", StructureKind::List,
         {{"insertAt", Operation::InsertAt, kIndexArg | kValueArg},
          {"deleteNode", Operation::DeleteNode, kNodeIdArg},
          {"search", Operation::Search, kValueArg},
          {"reverse", Operation::Reverse, kNoArgs},
          {"clear", Operation::Clear, kNoArgs},
          {"snapshot", Operation::Snapshot, kNoArgs},
          {"catalog", Operation::Catalog, kNoArgs},
          {"undo", Operation::Undo, kNoArgs},
          {"redo", Operation::Redo, kNoArgs},
          {"export", Operation::Export, kNoArgs},
          {"import", Operation::Import, kTextArg}}},
        {"binaryTree", "binary tree", StructureKind::BinaryTree,
         {{"addRoot", Operation::AddRoot, kValueArg},
          {"addChild", Operation::AddChild, kParentIdArg | kSideArg | kValueArg},
          {"deleteSubtree", Operation::DeleteSubtree, kNodeIdArg},
          {"preorder", Operation::Preorder, kNoArgs},
          {"inorder", Operation::Inorder, kNoArgs},
          {"postorder", Operation::Postorder, kNoArgs},
          {"levelorder", Operation::Levelorder, kNoArgs},
          {"clear", Operation::Clear, kNoArgs},
          {"snapshot", Operation::Snapshot, kNoArgs},
          {"catalog", Operation::Catalog, kNoArgs},
          {"undo", Operation::Undo, kNoArgs},
          {"redo", Operation::Redo, kNoArgs},
          {"export", Operation::Export, kNoArgs},
          {"import", Operation::Import, kTextArg}}},
        {"avl", "AVL tree", StructureKind::Avl,
         {{"insert", Operation::Insert, kValueArg},
          {"delete", Operation::Delete, kValueArg},
          {"search", Operation::Search, kValueArg},
          {"preorder", Operation::Preorder, kNoArgs},
          {"inorder", Operation::Inorder, kNoArgs},
          {"postorder", Operation::Postorder, kNoArgs},
          {"levelorder", Operation::Levelorder, kNoArgs},
          {"clear", Operation::Clear, kNoArgs},
          {"snapshot", Operation::Snapshot, kNoArgs},
          {"catalog", Operation::Catalog, kNoArgs},
          {"undo", Operation::Undo, kNoArgs},
          {"redo", Operation::Redo, kNoArgs},
          {"export", Operation::Export, kNoArgs},
          {"import", Operation::Import, kTextArg}}},
        {"bst", "BST", StructureKind::Bst,
         {{"insert", Operation::Insert, kValueArg},
          {"delete", Operation::Delete, kValueArg},
          {"search", Operation::Search, kValueArg},
          {"preorder", Operation::Preorder, kNoArgs},
          {"inorder", Operation::Inorder, kNoArgs},
          {"postorder", Operation::Postorder, kNoArgs},
          {"levelorder", Operation::Levelorder, kNoArgs},
          {"clear", Operation::Clear, kNoArgs},
          {"snapshot", Operation::Snapshot, kNoArgs},
          {"catalog", Operation::Catalog, kNoArgs},
          {"undo", Operation::Undo, kNoArgs},
          {"redo", Operation::Redo, kNoArgs},
          {"export", Operation::Export, kNoArgs},
          {"import", Operation::Import, kTextArg}}},
        {"redBlack", "red-black tree", StructureKind::RedBlack,
         {{"insert", Operation::Insert, kValueArg},
          {"delete", Operation::Delete, kValueArg},
          {"search", Operation::Search, kValueArg},
          {"preorder", Operation::Preorder, kNoArgs},
          {"inorder", Operation::Inorder, kNoArgs},
          {"postorder", Operation::Postorder, kNoArgs},
          {"levelorder", Operation::Levelorder, kNoArgs},
          {"clear", Operation::Clear, kNoArgs},
          {"snapshot", Operation::Snapshot, kNoArgs},
          {"catalog", Operation::Catalog, kNoArgs},
          {"undo", Operation::Undo, kNoArgs},
          {"redo", Operation::Redo, kNoArgs},
          {"export", Operation::Export, kNoArgs},
          {"import", Operation::Import, kTextArg}}},
    };
    return table;
}

const StructureInfo* findStructure(const std::string& name) {
    for (const StructureInfo& info : structureTable()) {
        if (name == info.name) {
            return &info;
        }
    }
    return nullptr;
}

const StructureInfo& infoOf(StructureKind kind) {
    for (const StructureInfo& info : structureTable()) {
        if (info.kind == kind) {
            return info;
        }
    }
    return structureTable().front();
}

const OperationInfo* findOperation(const StructureInfo& structure, const std::string& name) {
    for (const OperationInfo& info : structure.operations) {
        if (name == info.name) {
            return &info;
        }
    }
    return nullptr;
}

// "a, b and c"
std::string joinNames(const std::vector<std::string>& names) {
    std::string text;
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i > 0) {
            text += (i + 1 == names.size()) ? " and " : ", ";
        }
        text += names[i];
    }
    return text;
}

const char* eventName(Event event) {
    switch (event) {
        case Event::Begin: return "begin";
        case Event::Compare: return "compare";
        case Event::Move: return "move";
        case Event::Read: return "read";
        case Event::Visit: return "visit";
        case Event::Found: return "found";
        case Event::NotFound: return "notFound";
        case Event::Create: return "create";
        case Event::Link: return "link";
        case Event::Unlink: return "unlink";
        case Event::Remove: return "remove";
        case Event::Update: return "update";
        case Event::Imbalance: return "imbalance";
        case Event::FixupCase: return "fixupCase";
        case Event::Rotate: return "rotate";
        case Event::Recolor: return "recolor";
        case Event::Balanced: return "balanced";
        case Event::Restore: return "restore";
        case Event::Reject: return "reject";
        case Event::Done: return "done";
    }
    return "done";
}

const char* errorCodeName(ErrorCode code) {
    switch (code) {
        case ErrorCode::Ok: return "OK";
        case ErrorCode::BadRequest: return "BAD_REQUEST";
        case ErrorCode::UnsupportedVersion: return "UNSUPPORTED_VERSION";
        case ErrorCode::UnknownStructure: return "UNKNOWN_STRUCTURE";
        case ErrorCode::UnknownOperation: return "UNKNOWN_OPERATION";
        case ErrorCode::ValueOutOfRange: return "VALUE_OUT_OF_RANGE";
        case ErrorCode::CapacityExceeded: return "CAPACITY_EXCEEDED";
        case ErrorCode::IdExhausted: return "ID_EXHAUSTED";
        case ErrorCode::EmptyStructure: return "EMPTY_STRUCTURE";
        case ErrorCode::IndexOutOfRange: return "INDEX_OUT_OF_RANGE";
        case ErrorCode::NodeNotFound: return "NODE_NOT_FOUND";
        case ErrorCode::RootExists: return "ROOT_EXISTS";
        case ErrorCode::ChildExists: return "CHILD_EXISTS";
        case ErrorCode::DuplicateValue: return "DUPLICATE_VALUE";
        case ErrorCode::ValueNotFound: return "VALUE_NOT_FOUND";
        case ErrorCode::NothingToUndo: return "NOTHING_TO_UNDO";
        case ErrorCode::NothingToRedo: return "NOTHING_TO_REDO";
        case ErrorCode::ImportInvalid: return "IMPORT_INVALID";
        case ErrorCode::InternalError: return "INTERNAL_ERROR";
    }
    return "INTERNAL_ERROR";
}

// ---------------------------------------------------------------------------
// Reading a request

// What a response repeats from its request: only the parts that were valid.
struct Echo {
    std::optional<std::int32_t> requestId;
    std::optional<StructureKind> structure;
    const char* operation = nullptr;  // nullptr until recognised
};

struct ParsedRequest {
    Status status;  // ok, or the protocol error that stopped reading
    Echo echo;
    Request request;
};

ParsedRequest fail(ParsedRequest parsed, ErrorCode code, std::string message) {
    parsed.status = errorStatus(code, std::move(message));
    return parsed;
}

// Compact JSON text. Invalid UTF-8 is replaced instead of throwing.
std::string compact(const Json& json) {
    return json.dump(-1, ' ', false, Json::error_handler_t::replace);
}

// The member 'name' of a JSON object, or nullptr when it is missing.
const Json* member(const Json& object, const char* name) {
    const auto found = object.find(name);
    return found == object.end() ? nullptr : &*found;
}

// True when 'value' is a JSON integer that fits in a signed 32-bit int.
bool fitsInt32(const Json& value) {
    if (value.is_number_unsigned()) {
        return value.get<std::uint64_t>() <=
               static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());
    }
    if (value.is_number_integer()) {
        const std::int64_t number = value.get<std::int64_t>();
        return number >= std::numeric_limits<std::int32_t>::min() &&
               number <= std::numeric_limits<std::int32_t>::max();
    }
    return false;
}

// Text from the request is repeated in a message only when it is short,
// printable ASCII.
std::string mention(const std::string& text) {
    bool printable = text.size() <= 30;
    for (const char c : text) {
        if (c < 0x20 || c > 0x7e) {
            printable = false;
        }
    }
    return printable ? "\"" + text + "\"" : std::string("(not shown)");
}

// Reads the integer argument 'name' into 'target', or returns the protocol error.
Status readInteger(const Json& args, const char* name, std::int32_t& target) {
    const Json& value = *member(args, name);
    if (!value.is_number_integer()) {
        return errorStatus(ErrorCode::BadRequest, std::string("\"") + name + "\" must be an integer.");
    }
    if (!fitsInt32(value)) {
        return errorStatus(ErrorCode::ValueOutOfRange,
                           std::string("The ") + name + " " + compact(value) +
                               " is outside the 32-bit signed integer range.");
    }
    target = value.get<std::int32_t>();
    return okStatus();
}

// ---------------------------------------------------------------------------
// Reading an export document (operation-contract.md 5.7, rules 1 and 2). The
// engine checks every other rule (Validate.h) before anything changes.

// "" when 'object' has exactly the fields 'expected' (in any order), else a
// sentence naming the first missing or unknown one. 'owner' starts it.
std::string exactFields(const Json& object, const std::vector<std::string>& expected, const std::string& owner) {
    for (const std::string& key : expected) {
        if (member(object, key.c_str()) == nullptr) {
            return owner + " has no " + mention(key) + " field.";
        }
    }
    for (const auto& item : object.items()) {
        bool known = false;
        for (const std::string& key : expected) {
            known = known || item.key() == key;
        }
        if (!known) {
            return owner + " has an unknown field, " + mention(item.key()) + ".";
        }
    }
    return "";
}

// "a stack", "a BST", "an AVL tree": every display name takes "an" exactly
// when it starts with a vowel letter.
std::string withArticle(const std::string& name) {
    const bool vowel = !name.empty() && std::string("AEIOUaeiou").find(name[0]) != std::string::npos;
    return (vowel ? "an " : "a ") + name;
}

// A link or entry pointer: null (NIL) or a node ID from 1 to 2147483647.
bool readLink(const Json& value, NodeId& target) {
    if (value.is_null()) {
        target = kNoNode;
        return true;
    }
    if (!fitsInt32(value) || value.get<std::int64_t>() < 1) {
        return false;
    }
    target = value.get<std::int32_t>();
    return true;
}

// A color field: "red" or "black".
bool readColor(const Json& value, Color& target) {
    if (value == "red" || value == "black") {
        target = (value == "red") ? Color::Red : Color::Black;
        return true;
    }
    return false;
}

// The entry pointers a snapshot of this structure has (5.4).
std::vector<std::string> entryNames(StructureKind structure) {
    switch (structure) {
        case StructureKind::Stack: return {"top"};
        case StructureKind::Queue: return {"front", "rear"};
        case StructureKind::List: return {"head"};
        default: return {"root"};
    }
}

// Reads one node of the document into 'view'; "" or the problem.
std::string decodeNode(const Json& node, std::size_t position, StructureKind structure, NodeView& view) {
    if (!node.is_object()) {
        return "Node number " + std::to_string(position + 1) + " in \"nodes\" is not a JSON object.";
    }
    const Json* id = member(node, "id");
    const std::string owner = (id != nullptr && fitsInt32(*id)) ? "Node " + compact(*id)
                                                                : "Node number " + std::to_string(position + 1);
    std::vector<std::string> fields = {"id", "value"};
    if (isLinear(structure)) {
        fields.push_back("next");
    } else {
        fields.insert(fields.end(), {"left", "right", "parent"});
    }
    if (structure == StructureKind::Avl) fields.insert(fields.end(), {"height", "balance"});
    if (structure == StructureKind::RedBlack) fields.push_back("color");
    const std::string problem = exactFields(node, fields, owner);
    if (!problem.empty()) return problem;

    if (!fitsInt32(*id)) return owner + "'s id must be a 32-bit integer.";
    view.id = id->get<std::int32_t>();
    if (!fitsInt32(*member(node, "value"))) return owner + "'s value must be a 32-bit integer.";
    view.value = member(node, "value")->get<std::int32_t>();
    for (const std::string name : {"next", "left", "right", "parent"}) {
        const Json* link = member(node, name.c_str());
        if (link == nullptr) continue;
        NodeId target = kNoNode;
        if (!readLink(*link, target)) return owner + "'s " + name + " must be null or a node ID from 1 to 2147483647.";
        if (name == "next") view.next = target;
        if (name == "left") view.left = target;
        if (name == "right") view.right = target;
        if (name == "parent") view.parent = target;
    }
    if (structure == StructureKind::Avl) {
        if (!fitsInt32(*member(node, "height")) || !fitsInt32(*member(node, "balance"))) {
            return owner + "'s height and balance must be integers.";
        }
        view.height = member(node, "height")->get<std::int32_t>();
        view.balance = member(node, "balance")->get<std::int32_t>();
    }
    if (structure == StructureKind::RedBlack && !readColor(*member(node, "color"), view.color)) {
        return owner + "'s color must be \"red\" or \"black\".";
    }
    return "";
}

// Reads the export document 'text' for 'structure' into 'out'. Returns "" or,
// when rule 1 or 2 is broken, one sentence saying what is wrong.
std::string decodeExport(const std::string& text, StructureKind structure, Snapshot& out) {
    const Json document = Json::parse(text, nullptr, /*allow_exceptions=*/false);
    if (document.is_discarded()) return "The file is not valid JSON.";
    if (!document.is_object()) return "The file must hold one JSON object.";
    std::string problem = exactFields(document, {"format", "formatVersion", "nextId", "snapshot"}, "The file");
    if (!problem.empty()) return problem;
    if (*member(document, "format") != "dslab-structure") {
        return "This is not a DSLab export: \"format\" must be \"dslab-structure\".";
    }
    const Json& version = *member(document, "formatVersion");
    if (version != 1) {
        return "The file has format version " + compact(version) + ", but this engine reads version 1.";
    }
    const Json& nextId = *member(document, "nextId");
    if (!nextId.is_number_integer() || nextId.get<std::int64_t>() < 1 || nextId.get<std::int64_t>() > 2147483648LL) {
        return "nextId must be a whole number from 1 to 2147483648.";
    }

    const Json& snapshot = *member(document, "snapshot");
    if (!snapshot.is_object()) return "\"snapshot\" must be a JSON object.";
    const Json* name = member(snapshot, "structure");
    if (name == nullptr || !name->is_string()) return "The snapshot must name its structure.";
    const StructureInfo& expected = infoOf(structure);
    if (*name != expected.name) {
        const StructureInfo* found = findStructure(name->get<std::string>());
        return found != nullptr ? "The file holds " + withArticle(found->displayName) + ", but this is the " +
                                      expected.displayName + "."
                                : "The file's structure, " + mention(name->get<std::string>()) + ", is unknown.";
    }
    std::vector<std::string> fields = {"structure", "size"};
    for (const std::string& entry : entryNames(structure)) fields.push_back(entry);
    if (structure == StructureKind::RedBlack) fields.push_back("nilColor");
    fields.push_back("nodes");
    problem = exactFields(snapshot, fields, "The snapshot");
    if (!problem.empty()) return problem;

    out = Snapshot{};
    out.structure = structure;
    out.nextId = nextId.get<std::int64_t>();
    if (!fitsInt32(*member(snapshot, "size"))) return "size must be a whole number.";
    out.size = member(snapshot, "size")->get<std::int32_t>();
    for (const std::string& entry : entryNames(structure)) {
        NodeId target = kNoNode;
        if (!readLink(*member(snapshot, entry.c_str()), target)) {
            return entry + " must be null or a node ID from 1 to 2147483647.";
        }
        if (entry == "top") out.top = target;
        if (entry == "front") out.front = target;
        if (entry == "rear") out.rear = target;
        if (entry == "head") out.head = target;
        if (entry == "root") out.root = target;
    }
    if (structure == StructureKind::RedBlack && !readColor(*member(snapshot, "nilColor"), out.nilColor)) {
        return "nilColor must be \"red\" or \"black\".";
    }
    const Json& nodes = *member(snapshot, "nodes");
    if (!nodes.is_array()) return "\"nodes\" must be a list.";
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        NodeView view;
        problem = decodeNode(nodes[i], i, structure, view);
        if (!problem.empty()) return problem;
        out.nodes.push_back(view);
    }
    return "";
}

ParsedRequest parseRequest(const std::string& text) {
    ParsedRequest parsed;
    if (text.size() > kMaxRequestBytes) {
        return fail(parsed, ErrorCode::BadRequest, "The request is larger than 1 MiB.");
    }

    const Json json = Json::parse(text, nullptr, /*allow_exceptions=*/false);
    if (json.is_discarded()) {
        return fail(parsed, ErrorCode::BadRequest, "The request is not valid JSON.");
    }
    if (!json.is_object()) {
        return fail(parsed, ErrorCode::BadRequest, "The request must be a JSON object.");
    }

    // The version comes first, so a request from a newer schema is recognised
    // as such before its fields are judged.
    const Json* version = member(json, "schemaVersion");
    if (version == nullptr || !version->is_number_integer()) {
        return fail(parsed, ErrorCode::BadRequest, "The request needs \"schemaVersion\": 1.");
    }
    if (*version != 1) {
        return fail(parsed, ErrorCode::UnsupportedVersion,
                    "Schema version " + compact(*version) +
                        " is not supported; this engine speaks version 1.");
    }

    for (const auto& item : json.items()) {
        const std::string& key = item.key();
        if (key != "schemaVersion" && key != "requestId" && key != "structure" &&
            key != "operation" && key != "args") {
            return fail(parsed, ErrorCode::BadRequest,
                        "The request has an unknown field " + mention(key) + ".");
        }
    }
    for (const char* name : {"requestId", "structure", "operation", "args"}) {
        if (member(json, name) == nullptr) {
            return fail(parsed, ErrorCode::BadRequest,
                        std::string("The request is missing the field \"") + name + "\".");
        }
    }

    const Json& requestId = *member(json, "requestId");
    if (!fitsInt32(requestId) || requestId.get<std::int64_t>() < 0) {
        return fail(parsed, ErrorCode::BadRequest,
                    "\"requestId\" must be an integer from 0 to 2147483647.");
    }
    parsed.request.requestId = requestId.get<std::int32_t>();
    parsed.echo.requestId = parsed.request.requestId;

    const Json& structureName = *member(json, "structure");
    if (!structureName.is_string()) {
        return fail(parsed, ErrorCode::BadRequest, "\"structure\" must be a string.");
    }
    const StructureInfo* structure = findStructure(structureName.get<std::string>());
    if (structure == nullptr) {
        std::vector<std::string> known;
        for (const StructureInfo& info : structureTable()) {
            known.push_back(info.name);
        }
        return fail(parsed, ErrorCode::UnknownStructure,
                    "Unknown structure " + mention(structureName.get<std::string>()) +
                        "; this engine has " + joinNames(known) + " so far.");
    }
    parsed.request.structure = structure->kind;
    parsed.echo.structure = structure->kind;

    const Json& operationName = *member(json, "operation");
    if (!operationName.is_string()) {
        return fail(parsed, ErrorCode::BadRequest, "\"operation\" must be a string.");
    }
    const OperationInfo* operation = findOperation(*structure, operationName.get<std::string>());
    if (operation == nullptr) {
        std::vector<std::string> known;
        for (const OperationInfo& info : structure->operations) {
            known.push_back(info.name);
        }
        return fail(parsed, ErrorCode::UnknownOperation,
                    std::string("The ") + structure->displayName + " has no operation " +
                        mention(operationName.get<std::string>()) + "; it has " +
                        joinNames(known) + ".");
    }
    parsed.request.operation = operation->operation;
    parsed.echo.operation = operation->name;

    const Json& args = *member(json, "args");
    if (!args.is_object()) {
        return fail(parsed, ErrorCode::BadRequest, "\"args\" must be an object.");
    }
    for (const auto& item : args.items()) {
        if ((operation->args & argFlag(item.key())) == 0) {
            return fail(parsed, ErrorCode::BadRequest,
                        std::string(operation->name) + " has no argument " + mention(item.key()) + ".");
        }
    }
    for (const IntegerArg& arg : kIntegerArgs) {
        if ((operation->args & arg.flag) == 0) {
            continue;
        }
        if (member(args, arg.name) == nullptr) {
            return fail(parsed, ErrorCode::BadRequest,
                        std::string(operation->name) + " needs the argument \"" + arg.name + "\".");
        }
        std::int32_t number = 0;
        const Status status = readInteger(args, arg.name, number);
        if (!status.ok()) {
            parsed.status = status;
            return parsed;
        }
        storeInteger(parsed.request, arg.flag, number);
    }
    if ((operation->args & kSideArg) != 0) {
        const Json* side = member(args, "side");
        if (side == nullptr) {
            return fail(parsed, ErrorCode::BadRequest,
                        std::string(operation->name) + " needs the argument \"side\".");
        }
        if (*side != "left" && *side != "right") {
            return fail(parsed, ErrorCode::BadRequest, "\"side\" must be \"left\" or \"right\".");
        }
        parsed.request.side = (*side == "left") ? Side::Left : Side::Right;
    }
    if ((operation->args & kTextArg) != 0) {
        const Json* text = member(args, "text");
        if (text == nullptr) {
            return fail(parsed, ErrorCode::BadRequest,
                        std::string(operation->name) + " needs the argument \"text\".");
        }
        if (!text->is_string()) {
            return fail(parsed, ErrorCode::BadRequest, "\"text\" must be a string.");
        }
        // A document that cannot be read is not a protocol error: import itself
        // rejects it (IMPORT_INVALID), with begin and reject frames.
        parsed.request.importProblem =
            decodeExport(text->get<std::string>(), structure->kind, parsed.request.importSnapshot);
    }

    parsed.status = okStatus();
    return parsed;
}

// ---------------------------------------------------------------------------
// Writing a response

Json nodeOrNull(NodeId id) {
    return id == kNoNode ? Json(nullptr) : Json(id);
}

// Red-black colors on the wire: "red" or "black".
const char* colorName(Color color) {
    return color == Color::Red ? "red" : "black";
}

// Only the fields of this structure (operation-contract.md, 5.4).
Json snapshotJson(const Snapshot& snapshot) {
    const bool linear = isLinear(snapshot.structure);
    Json nodes = Json::array();
    for (const NodeView& node : snapshot.nodes) {
        Json item = Json::object();
        item["id"] = node.id;
        item["value"] = node.value;
        if (linear) {
            item["next"] = nodeOrNull(node.next);
        } else {
            item["left"] = nodeOrNull(node.left);
            item["right"] = nodeOrNull(node.right);
            item["parent"] = nodeOrNull(node.parent);
            if (snapshot.structure == StructureKind::Avl) {
                item["height"] = node.height;
                item["balance"] = node.balance;
            }
            if (snapshot.structure == StructureKind::RedBlack) {
                item["color"] = colorName(node.color);
            }
        }
        nodes.push_back(std::move(item));
    }

    Json json = Json::object();
    json["structure"] = infoOf(snapshot.structure).name;
    json["size"] = snapshot.size;
    switch (snapshot.structure) {
        case StructureKind::Stack:
            json["top"] = nodeOrNull(snapshot.top);
            break;
        case StructureKind::Queue:
            json["front"] = nodeOrNull(snapshot.front);
            json["rear"] = nodeOrNull(snapshot.rear);
            break;
        case StructureKind::List:
            json["head"] = nodeOrNull(snapshot.head);
            break;
        case StructureKind::BinaryTree:
        case StructureKind::Bst:
        case StructureKind::Avl:
            json["root"] = nodeOrNull(snapshot.root);
            break;
        case StructureKind::RedBlack:
            json["root"] = nodeOrNull(snapshot.root);
            json["nilColor"] = colorName(snapshot.nilColor);
            break;
    }
    json["nodes"] = std::move(nodes);
    return json;
}

Json countersJson(const Counters& counters) {
    Json json = Json::object();
    json["comparisons"] = counters.comparisons;
    json["nodesVisited"] = counters.nodesVisited;
    json["linksChanged"] = counters.linksChanged;
    json["rotations"] = counters.rotations;
    json["recolors"] = counters.recolors;
    return json;
}

Json frameJson(const Frame& frame) {
    Json highlightNodes = Json::array();
    for (const NodeId id : frame.highlight.nodes) {
        highlightNodes.push_back(id);
    }
    Json highlightEdges = Json::array();
    for (const Edge& edge : frame.highlight.edges) {
        highlightEdges.push_back(std::to_string(edge.from) + "->" + std::to_string(edge.to));
    }
    Json highlight = Json::object();
    highlight["nodes"] = std::move(highlightNodes);
    highlight["edges"] = std::move(highlightEdges);

    Json pointers = Json::object();
    for (const NamedPointer& pointer : frame.pointers) {
        pointers[pointer.name] = nodeOrNull(pointer.node);
    }

    Json json = Json::object();
    json["index"] = frame.index;
    json["event"] = eventName(frame.event);
    json["stepId"] = frame.stepId;
    json["explanation"] = frame.explanation;
    json["highlight"] = std::move(highlight);
    json["pointers"] = std::move(pointers);
    json["output"] = frame.output;
    json["pending"] = frame.pending;
    json["counters"] = countersJson(frame.counters);
    json["snapshot"] = snapshotJson(frame.snapshot);
    return json;
}

Json dataValueJson(const DataField& field) {
    switch (field.kind) {
        case DataField::Kind::Integer: return Json(field.integer);
        case DataField::Kind::Boolean: return Json(field.boolean);
        case DataField::Kind::Null: return Json(nullptr);
        case DataField::Kind::IntegerList: return Json(field.list);
    }
    return Json(nullptr);
}

// The argument names of an operation, in the order a reader expects them.
Json argNames(unsigned args) {
    Json names = Json::array();
    if ((args & kParentIdArg) != 0) names.push_back("parentId");
    if ((args & kSideArg) != 0) names.push_back("side");
    if ((args & kIndexArg) != 0) names.push_back("index");
    if ((args & kValueArg) != 0) names.push_back("value");
    if ((args & kNodeIdArg) != 0) names.push_back("nodeId");
    return names;
}

// The data of a "catalog" result (operation-contract.md, 5.6): every listing
// of the structure, in the order of its operation table.
Json catalogJson(StructureKind structure) {
    Json operations = Json::array();
    for (const OperationInfo& info : infoOf(structure).operations) {
        const OperationListing* listing = findListing(structure, info.operation);
        if (listing == nullptr) {
            continue;  // snapshot and catalog run no algorithm
        }
        Json lines = Json::array();
        for (const PseudocodeLine& line : listing->lines) {
            Json item = Json::object();
            item["stepId"] = line.stepId.empty() ? Json(nullptr) : Json(line.stepId);
            item["text"] = line.text;
            lines.push_back(std::move(item));
        }
        Json item = Json::object();
        item["operation"] = info.name;
        item["args"] = argNames(info.args);
        item["mutates"] = isMutating(info.operation);
        item["complexity"] = listing->complexity;
        item["pseudocode"] = std::move(lines);
        operations.push_back(std::move(item));
    }
    Json data = Json::object();
    data["operations"] = std::move(operations);
    return data;
}

// The export document (operation-contract.md 5.7), 2-space indented (D27).
std::string exportText(const Snapshot& snapshot) {
    Json document = Json::object();
    document["format"] = "dslab-structure";
    document["formatVersion"] = 1;
    document["nextId"] = snapshot.nextId;
    document["snapshot"] = snapshotJson(snapshot);
    return document.dump(2, ' ', false, Json::error_handler_t::replace) + "\n";
}

// 'written' is set for "catalog" and "export": their data is JSON that this
// layer writes itself (the listings, the export document).
Json resultJson(const Response& response, const std::optional<Json>& written) {
    Json json = Json::object();
    if (response.status.ok()) {
        Json data = Json::object();
        for (const DataField& field : response.data) {
            data[field.name] = dataValueJson(field);
        }
        if (written) {
            data = *written;
        }
        json["status"] = "ok";
        json["data"] = std::move(data);
    } else {
        json["status"] = "error";
        json["code"] = errorCodeName(response.status.code);
        json["message"] = response.status.message;
    }
    return json;
}

Json metricsJson(const Response& response) {
    int snapshotNodes = 0;
    for (const Frame& frame : response.frames) {
        snapshotNodes += static_cast<int>(frame.snapshot.nodes.size());
    }

    Json trace = Json::object();
    trace["frames"] = response.frames.size();
    trace["snapshotNodes"] = snapshotNodes;
    trace["truncated"] = response.truncated;

    Json json = Json::object();
    json["algorithm"] = countersJson(response.counters);
    json["trace"] = std::move(trace);
    return json;
}

// Appends "name":value to an object that is being written by hand.
void addField(std::string& out, const char* name, const Json& value) {
    if (out.size() > 1) {
        out += ',';
    }
    out += compact(Json(name));
    out += ':';
    out += compact(value);
}

// The response is written field by field, and the frames one at a time, so a
// JSON tree of the whole response (which can be several MB) is never built.
std::string writeResponse(const Echo& echo, const Response& response, const std::optional<Json>& written) {
    const bool knownStructure = echo.structure.has_value();

    std::string out = "{";
    addField(out, "schemaVersion", Json(1));
    addField(out, "requestId", echo.requestId ? Json(*echo.requestId) : Json(nullptr));
    addField(out, "structure",
             knownStructure ? Json(infoOf(*echo.structure).name) : Json(nullptr));
    addField(out, "operation", echo.operation != nullptr ? Json(echo.operation) : Json(nullptr));
    addField(out, "result", resultJson(response, written));
    addField(out, "snapshot", knownStructure ? snapshotJson(response.snapshot) : Json(nullptr));

    out += ",\"frames\":[";
    for (std::size_t i = 0; i < response.frames.size(); ++i) {
        if (i > 0) {
            out += ',';
        }
        out += compact(frameJson(response.frames[i]));
    }
    out += ']';

    addField(out, "metrics", metricsJson(response));
    Json history = Json(nullptr);
    if (knownStructure) {
        history = Json::object();
        history["undo"] = response.undoCount;
        history["redo"] = response.redoCount;
    }
    addField(out, "history", history);
    out += '}';
    return out;
}

// Used only if something unexpected throws (operation-contract.md, 3.3).
const char* const kInternalErrorResponse =
    "{\"schemaVersion\":1,\"requestId\":null,\"structure\":null,\"operation\":null,"
    "\"result\":{\"status\":\"error\",\"code\":\"INTERNAL_ERROR\","
    "\"message\":\"The engine hit an unexpected internal error.\"},"
    "\"snapshot\":null,\"frames\":[],"
    "\"metrics\":{\"algorithm\":{\"comparisons\":0,\"nodesVisited\":0,\"linksChanged\":0,"
    "\"rotations\":0,\"recolors\":0},\"trace\":{\"frames\":0,\"snapshotNodes\":0,"
    "\"truncated\":false}},\"history\":null}";

}  // namespace

std::string handleJson(Engine& engine, const std::string& requestText) {
    try {
        const ParsedRequest parsed = parseRequest(requestText);

        Response response;
        std::optional<Json> written;
        if (parsed.status.ok()) {
            response = engine.apply(parsed.request);
            if (parsed.request.operation == Operation::Catalog) {
                written = catalogJson(parsed.request.structure);
            }
            if (parsed.request.operation == Operation::Export) {
                Json data = Json::object();
                data["text"] = exportText(response.snapshot);
                written = data;
            }
        } else {
            // Protocol error: no frames; describe the structure if it was recognised.
            response.status = parsed.status;
            if (parsed.echo.structure) {
                response.snapshot = engine.snapshot(*parsed.echo.structure);
                response.undoCount = engine.undoCount(*parsed.echo.structure);
                response.redoCount = engine.redoCount(*parsed.echo.structure);
            }
        }
        return writeResponse(parsed.echo, response, written);
    } catch (...) {
        // Answer instead of letting the exception abort the WebAssembly module.
        return kInternalErrorResponse;
    }
}

}  // namespace dslab
