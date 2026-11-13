#include "dslab/HashTable.h"

#include <string>

namespace dslab {

namespace {

std::string text(Value value) {
    return std::to_string(value);
}

// "1 node" or "3 nodes".
std::string nodeCount(int count) {
    return std::to_string(count) + (count == 1 ? " node" : " nodes");
}

// "an empty hash table" or "a hash table that holds 3 nodes".
std::string describe(int size) {
    return size == 0 ? "an empty hash table" : "a hash table that holds " + nodeCount(size);
}

}  // namespace

// C++'s % keeps the sign of the value (-3 % 7 is -3), so a negative result is
// moved into 0..6 by adding the bucket count once.
int HashTable::bucketOf(Value value) {
    const int remainder = static_cast<int>(value % kHashBuckets);
    return remainder < 0 ? remainder + kHashBuckets : remainder;
}

int HashTable::hashStep(Value value, const std::string& stepId, Trace& trace) const {
    const int bucket = bucketOf(value);
    trace.bucket = bucket;
    const Node* first = buckets_[bucket];  // non-owning: the pool owns every node
    trace.frame(snapshot(), Event::Hash, stepId,
                "bucket = " + text(value) + " mod " + std::to_string(kHashBuckets) + " = " + std::to_string(bucket) +
                    (first == nullptr ? "; that bucket is empty." : "; its chain starts with " + text(first->value) + "."),
                first == nullptr ? Highlight{} : Highlight{{first->id}, {}});
    return bucket;
}

// insert(value):                                         hashTable.insert.enter
//   if size = 200: reject CAPACITY_EXCEEDED              hashTable.insert.full
//   b <- value mod 7                                     hashTable.insert.hash
//   curr <- buckets[b]
//   while curr != NIL:
//     if curr.value = value: reject DUPLICATE_VALUE      hashTable.insert.duplicate
//     curr <- curr.next                                  hashTable.insert.next
//   node <- new Node(value)                              hashTable.insert.create
//   node.next <- buckets[b]                              hashTable.insert.linkNext
//   buckets[b] <- node                                   hashTable.insert.linkBucket
//   return node                                          hashTable.insert.done
Result<NodeId> HashTable::insert(Value value, Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "hashTable.insert.enter",
                "Insert " + text(value) + " into " + describe(size()) + ".");

    if (size() == kMaxNodes) {
        return reject<NodeId>(trace, "hashTable.insert.full", ErrorCode::CapacityExceeded,
                              "The hash table already holds 200 nodes, the maximum.");
    }
    if (!pool_.hasFreeId()) {
        return reject<NodeId>(trace, "hashTable.insert.full", ErrorCode::IdExhausted,
                              "No node IDs are left for this hash table.");
    }

    const int bucket = hashStep(value, "hashTable.insert.hash", trace);

    // The chain is checked to its end first: a set holds each value once.
    for (const Node* curr = buckets_[bucket]; curr != nullptr; curr = curr->next) {
        trace.counters.nodesVisited++;
        trace.counters.comparisons++;
        if (curr->value == value) {
            return reject<NodeId>(trace, "hashTable.insert.duplicate", ErrorCode::DuplicateValue,
                                  text(value) + " is already in the hash table, and a set does not allow duplicates.");
        }
        Highlight highlight{{curr->id}, {}};
        if (curr->next != nullptr) {
            highlight.edges.push_back(Edge{curr->id, curr->next->id});
        }
        trace.frame(snapshot(), Event::Compare, "hashTable.insert.next",
                    text(curr->value) + " is not " + text(value) + ", so follow next" +
                        (curr->next != nullptr ? " to " + text(curr->next->value) + "." : std::string(", which is NIL.")),
                    highlight, {{"curr", curr->id}});
    }

    Node* node = pool_.create(value);  // non-owning: the pool owns every node
    trace.frame(snapshot(), Event::Create, "hashTable.insert.create",
                text(value) + " is not in the chain, so create a new node holding it.", {{node->id}, {}},
                {{"node", node->id}});

    Node* oldHead = buckets_[bucket];  // non-owning: the pool owns every node
    node->next = oldHead;
    trace.counters.linksChanged++;
    if (oldHead != nullptr) {
        trace.frame(snapshot(), Event::Link, "hashTable.insert.linkNext",
                    "Point the new node's next at the chain's first node, " + text(oldHead->value) + ".",
                    {{node->id, oldHead->id}, {{node->id, oldHead->id}}}, {{"node", node->id}});
    } else {
        trace.frame(snapshot(), Event::Unlink, "hashTable.insert.linkNext",
                    "The bucket is empty, so the new node's next stays NIL.", {{node->id}, {}},
                    {{"node", node->id}});
    }

    buckets_[bucket] = node;
    trace.counters.linksChanged++;
    trace.frame(snapshot(), Event::Link, "hashTable.insert.linkBucket",
                "Point bucket " + std::to_string(bucket) + " at the new node: it is now the head of the chain.",
                {{node->id}, {}}, {{"node", node->id}});

    trace.frame(snapshot(), Event::Done, "hashTable.insert.done",
                "Inserted " + text(value) + " in bucket " + std::to_string(bucket) + "; the table now holds " +
                    nodeCount(size()) + ".",
                {{node->id}, {}});
    return Result<NodeId>{okStatus(), node->id};
}

// delete(value):                                         hashTable.delete.enter
//   b <- value mod 7                                     hashTable.delete.hash
//   prev <- NIL; curr <- buckets[b]
//   while curr != NIL and curr.value != value:
//     prev <- curr; curr <- curr.next                    hashTable.delete.next
//   if curr = NIL: reject VALUE_NOT_FOUND                hashTable.delete.notFound
//                                                        hashTable.delete.found
//   if prev = NIL: buckets[b] <- curr.next               hashTable.delete.unlinkHead
//   else: prev.next <- curr.next                         hashTable.delete.bypass
//   delete curr                                          hashTable.delete.remove
//   return                                               hashTable.delete.done
Result<NodeValue> HashTable::remove(Value value, Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "hashTable.delete.enter",
                "Delete " + text(value) + " from " + describe(size()) + ".");

    const int bucket = hashStep(value, "hashTable.delete.hash", trace);

    Node* prev = nullptr;            // non-owning: the pool owns every node
    Node* curr = buckets_[bucket];   // non-owning: the pool owns every node
    while (curr != nullptr) {
        trace.counters.nodesVisited++;
        trace.counters.comparisons++;
        if (curr->value == value) {
            break;
        }
        Highlight highlight{{curr->id}, {}};
        if (curr->next != nullptr) {
            highlight.edges.push_back(Edge{curr->id, curr->next->id});
        }
        trace.frame(snapshot(), Event::Compare, "hashTable.delete.next",
                    text(curr->value) + " is not " + text(value) + ", so follow next" +
                        (curr->next != nullptr ? " to " + text(curr->next->value) + "." : std::string(", which is NIL.")),
                    highlight, {{"curr", curr->id}});
        prev = curr;
        curr = curr->next;
    }
    if (curr == nullptr) {
        return reject<NodeValue>(trace, "hashTable.delete.notFound", ErrorCode::ValueNotFound,
                                 text(value) + " is not in the hash table, so there is nothing to delete.");
    }

    std::vector<NamedPointer> pointers;
    if (prev != nullptr) {
        pointers.push_back({"prev", prev->id});
    }
    pointers.push_back({"curr", curr->id});
    trace.frame(snapshot(), Event::Found, "hashTable.delete.found",
                "Found " + text(value) + " in bucket " + std::to_string(bucket) + ".", {{curr->id}, {}}, pointers);

    Node* after = curr->next;  // non-owning: the pool owns every node
    if (prev == nullptr) {
        buckets_[bucket] = after;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), after != nullptr ? Event::Link : Event::Unlink, "hashTable.delete.unlinkHead",
                    "It is the head of the chain, so bucket " + std::to_string(bucket) + " now points to " +
                        (after != nullptr ? text(after->value) : std::string("NIL")) + ".",
                    {{curr->id}, {}}, pointers);
    } else {
        prev->next = after;
        trace.counters.linksChanged++;
        Highlight highlight{{prev->id, curr->id}, {}};
        if (after != nullptr) {
            highlight.edges.push_back(Edge{prev->id, after->id});
        }
        trace.frame(snapshot(), after != nullptr ? Event::Link : Event::Unlink, "hashTable.delete.bypass",
                    "Point " + text(prev->value) + "'s next past it, at " +
                        (after != nullptr ? text(after->value) : std::string("NIL")) + ".",
                    highlight, pointers);
    }

    const NodeValue removed{curr->id, curr->value};
    pool_.destroy(curr);  // 'curr' dangles from here on and is not used again
    trace.frame(snapshot(), Event::Remove, "hashTable.delete.remove",
                "Free the node that held " + text(removed.value) + ".", {{removed.nodeId}, {}});

    trace.frame(snapshot(), Event::Done, "hashTable.delete.done",
                size() == 0 ? "Deleted " + text(value) + "; the hash table is now empty."
                            : "Deleted " + text(value) + "; the hash table now holds " + nodeCount(size()) + ".");
    return Result<NodeValue>{okStatus(), removed};
}

// search(value):                                         hashTable.search.enter
//   b <- value mod 7                                     hashTable.search.hash
//   curr <- buckets[b]
//   while curr != NIL:
//     if curr.value = value: found                       hashTable.search.found
//     curr <- curr.next                                  hashTable.search.next
//   not found                                            hashTable.search.notFound
//   return                                               hashTable.search.done
// Only one chain is looked at, however many values the table holds.
Result<HashSearchResult> HashTable::search(Value value, Trace& trace) const {
    trace.frame(snapshot(), Event::Begin, "hashTable.search.enter",
                "Search " + describe(size()) + " for " + text(value) + ".");

    HashSearchResult result;
    result.bucket = hashStep(value, "hashTable.search.hash", trace);

    for (const Node* curr = buckets_[result.bucket]; curr != nullptr; curr = curr->next) {
        trace.counters.nodesVisited++;
        trace.counters.comparisons++;
        if (curr->value == value) {
            result.found = true;
            result.nodeId = curr->id;
            trace.frame(snapshot(), Event::Found, "hashTable.search.found",
                        "Found " + text(value) + " after " + std::to_string(trace.counters.comparisons) +
                            (trace.counters.comparisons == 1 ? " comparison." : " comparisons."),
                        {{curr->id}, {}}, {{"curr", curr->id}});
            break;
        }
        Highlight highlight{{curr->id}, {}};
        if (curr->next != nullptr) {
            highlight.edges.push_back(Edge{curr->id, curr->next->id});
        }
        trace.frame(snapshot(), Event::Compare, "hashTable.search.next",
                    text(curr->value) + " is not " + text(value) + ", so follow next" +
                        (curr->next != nullptr ? " to " + text(curr->next->value) + "." : std::string(", which is NIL.")),
                    highlight, {{"curr", curr->id}});
    }

    if (result.found) {
        trace.frame(snapshot(), Event::Done, "hashTable.search.done",
                    "Search finished: " + text(value) + " is in bucket " + std::to_string(result.bucket) +
                        "; no other bucket was looked at.",
                    {{result.nodeId}, {}});
    } else {
        trace.frame(snapshot(), Event::NotFound, "hashTable.search.notFound",
                    "Reached the end of bucket " + std::to_string(result.bucket) + "'s chain: " + text(value) +
                        " is not in the table.");
        trace.frame(snapshot(), Event::Done, "hashTable.search.done",
                    "Search finished: " + text(value) + " was not found; only bucket " +
                        std::to_string(result.bucket) + " was looked at.");
    }
    return Result<HashSearchResult>{okStatus(), result};
}

// clear():                                               hashTable.clear.enter
//   for b <- 0 to 6:
//     while buckets[b] != NIL:
//       node <- buckets[b]; buckets[b] <- node.next
//       delete node                                      hashTable.clear.remove
//   return removed                                       hashTable.clear.done
Result<int> HashTable::clear(Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "hashTable.clear.enter", "Clear " + describe(size()) + ".");

    int removed = 0;
    for (int bucket = 0; bucket < kHashBuckets; ++bucket) {
        while (buckets_[bucket] != nullptr) {
            Node* node = buckets_[bucket];  // non-owning: the pool owns every node
            trace.counters.nodesVisited++;
            buckets_[bucket] = node->next;
            trace.counters.linksChanged++;

            const NodeValue freed{node->id, node->value};
            pool_.destroy(node);  // 'node' dangles from here on and is not used again
            removed = removed + 1;
            trace.bucket = bucket;
            trace.frame(snapshot(), Event::Remove, "hashTable.clear.remove",
                        "Unlink " + text(freed.value) + " from the head of bucket " + std::to_string(bucket) +
                            " and free it.",
                        {{freed.nodeId}, {}});
        }
    }

    trace.bucket = -1;
    trace.frame(snapshot(), Event::Done, "hashTable.clear.done",
                removed == 0 ? std::string("The hash table was already empty, so nothing was freed.")
                             : "Cleared " + nodeCount(removed) + "; every bucket is now empty.");
    return Result<int>{okStatus(), removed};
}

Snapshot HashTable::snapshot() const {
    Snapshot copy;
    copy.structure = StructureKind::HashTable;
    copy.size = pool_.size();
    copy.nextId = pool_.nextId();
    for (const Node* first : buckets_) {
        copy.buckets.push_back(first != nullptr ? first->id : kNoNode);
    }
    for (const std::unique_ptr<Node>& node : pool_.nodes()) {
        const NodeId next = (node->next != nullptr) ? node->next->id : kNoNode;
        copy.nodes.push_back(NodeView{node->id, node->value, next});
    }
    return copy;
}

// restore(snapshot): create every node with its ID and value, then set the
// links and the bucket array.
void HashTable::restore(const Snapshot& snapshot) {
    pool_.reset(snapshot.nextId);
    for (const NodeView& view : snapshot.nodes) {
        pool_.createWithId(view.id, view.value);
    }
    for (const NodeView& view : snapshot.nodes) {
        pool_.find(view.id)->next = pool_.find(view.next);
    }
    for (int bucket = 0; bucket < kHashBuckets; ++bucket) {
        buckets_[bucket] = pool_.find(snapshot.buckets[static_cast<std::size_t>(bucket)]);
    }
}

}  // namespace dslab
