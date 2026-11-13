// A hash table of integers with separate chaining (it stores a set: no
// duplicates):
//
//     bucket 0: NIL
//     bucket 1: [15] -> [8] -> [1] -> NIL       bucket(value) = value mod 7,
//     bucket 2: [9] -> NIL                      made non-negative
//     ...
//     bucket 6: [13] -> NIL
//
// An array of kHashBuckets head pointers; each bucket is a singly linked chain
// of the values that hash to it. insert, delete and search first compute the
// bucket (one step), then walk only that chain, so they cost O(1 + chain
// length): O(1) on average while the table is not overloaded, O(n) if every
// value lands in one bucket. A new value is linked at the head of its chain.
// The number of buckets is fixed (no rehashing), so chains simply grow.
//
// Every operation records frames into a Trace while it runs (rule 7) and
// changes nothing when it rejects (rule 8).
#pragma once

#include <string>

#include "dslab/Limits.h"
#include "dslab/NodePool.h"
#include "dslab/Result.h"
#include "dslab/Snapshot.h"
#include "dslab/Trace.h"
#include "dslab/Types.h"

namespace dslab {

// What a hash table search found.
struct HashSearchResult {
    bool found = false;
    NodeId nodeId = kNoNode;  // kNoNode when not found
    int bucket = 0;           // the bucket the value hashes to, found or not
};

class HashTable {
public:
    HashTable() = default;

    // Nodes link to each other by address, so a table is never copied or moved.
    HashTable(const HashTable&) = delete;
    HashTable& operator=(const HashTable&) = delete;
    HashTable(HashTable&&) = delete;
    HashTable& operator=(HashTable&&) = delete;

    // The bucket a value belongs in: value mod kHashBuckets, from 0 to kHashBuckets - 1.
    static int bucketOf(Value value);

    Result<NodeId> insert(Value value, Trace& trace);     // data: the new node's ID
    Result<NodeValue> remove(Value value, Trace& trace);  // "delete"; data: the removed node
    Result<HashSearchResult> search(Value value, Trace& trace) const;
    Result<int> clear(Trace& trace);  // data: how many nodes were freed

    // A copy of every node, in ascending ID order, and of the bucket array.
    Snapshot snapshot() const;

    // Replaces the whole structure with 'snapshot' (undo, redo, import). Only
    // called with a snapshot that passed validateSnapshot(). No frames.
    void restore(const Snapshot& snapshot);

    int size() const { return pool_.size(); }

private:
    struct Node {
        NodeId id = kNoNode;
        Value value = 0;
        Node* next = nullptr;  // non-owning: the pool owns every node
    };

    // Computes the bucket of 'value', remembers it in the trace (so every
    // later frame shows it) and records the Hash frame under 'stepId'.
    int hashStep(Value value, const std::string& stepId, Trace& trace) const;

    // Records a Reject frame and returns the matching error.
    template <typename T>
    Result<T> reject(Trace& trace, const char* stepId, ErrorCode code,
                     const std::string& message) const {
        trace.frame(snapshot(), Event::Reject, stepId, message);
        return Result<T>{errorStatus(code, message), T{}};
    }

    NodePool<Node> pool_;
    // The bucket array: the first node of each chain, or nullptr.
    // Non-owning: the pool owns every node.
    Node* buckets_[kHashBuckets] = {};
};

}  // namespace dslab
