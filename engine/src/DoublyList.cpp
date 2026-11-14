#include "dslab/DoublyList.h"

#include <string>

#include "dslab/Limits.h"

namespace dslab {

namespace {

std::string text(Value value) {
    return std::to_string(value);
}

// "1 node" or "3 nodes".
std::string nodeCount(int count) {
    return std::to_string(count) + (count == 1 ? " node" : " nodes");
}

// "an empty doubly linked list" or "a doubly linked list that holds 3 nodes".
std::string describe(int size) {
    return size == 0 ? "an empty doubly linked list" : "a doubly linked list that holds " + nodeCount(size);
}

}  // namespace

// insertAt(index, value):                                    doublyList.insertAt.enter
//   if size = 200: reject CAPACITY_EXCEEDED                  doublyList.insertAt.full
//   if index < 0 or index > size: reject INDEX_OUT_OF_RANGE  doublyList.insertAt.badIndex
//   if 0 < index < size:              (find the node now at 'index')
//     if index <= size / 2:
//       curr <- head                                         doublyList.insertAt.startHead
//       repeat index times: curr <- curr.next                doublyList.insertAt.walkNext
//     else:
//       curr <- tail                                         doublyList.insertAt.startTail
//       repeat size - 1 - index times: curr <- curr.prev     doublyList.insertAt.walkPrev
//   node <- new Node(value)                                  doublyList.insertAt.create
//   if head = NIL: head <- node; tail <- node                doublyList.insertAt.linkOnly
//   else if index = 0:
//     node.next <- head; head.prev <- node                   doublyList.insertAt.linkBeforeHead
//     head <- node                                           doublyList.insertAt.moveHead
//   else if index = size:
//     node.prev <- tail; tail.next <- node                   doublyList.insertAt.linkAfterTail
//     tail <- node                                           doublyList.insertAt.moveTail
//   else:
//     node.prev <- curr.prev; node.next <- curr              doublyList.insertAt.linkNode
//     curr.prev.next <- node; curr.prev <- node              doublyList.insertAt.linkNeighbours
//   return node                                              doublyList.insertAt.done
Result<NodeId> DoublyList::insertAt(int index, Value value, Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "doublyList.insertAt.enter",
                "Insert " + text(value) + " at index " + std::to_string(index) + " of " + describe(size()) + ".");

    if (size() == kMaxNodes) {
        return reject<NodeId>(trace, "doublyList.insertAt.full", ErrorCode::CapacityExceeded,
                              "The doubly linked list already holds 200 nodes, the maximum.");
    }
    if (!pool_.hasFreeId()) {
        return reject<NodeId>(trace, "doublyList.insertAt.full", ErrorCode::IdExhausted,
                              "No node IDs are left for this doubly linked list.");
    }
    if (index < 0 || index > size()) {
        const std::string where = size() == 0 ? "an empty list" : "a list of " + nodeCount(size());
        return reject<NodeId>(trace, "doublyList.insertAt.badIndex", ErrorCode::IndexOutOfRange,
                              "Index " + std::to_string(index) + " is outside 0 to " + std::to_string(size()) +
                                  " for " + where + ".");
    }

    const int oldSize = size();
    Node* curr = nullptr;  // non-owning: the node now at 'index' (middle inserts only)
    if (index > 0 && index < oldSize) {
        if (index <= oldSize / 2) {
            curr = head_;
            trace.counters.nodesVisited++;
            trace.frame(snapshot(), Event::Move, "doublyList.insertAt.startHead",
                        "Index " + std::to_string(index) + " is nearer the head, so start at the head, " +
                            text(curr->value) + ".",
                        {{curr->id}, {}}, {{"curr", curr->id}});
            for (int position = 1; position <= index; ++position) {
                const NodeId from = curr->id;
                curr = curr->next;
                trace.counters.nodesVisited++;
                trace.frame(snapshot(), Event::Move, "doublyList.insertAt.walkNext",
                            "Follow next to " + text(curr->value) + ", which is index " + std::to_string(position) + ".",
                            {{curr->id}, {{from, curr->id}}}, {{"curr", curr->id}});
            }
        } else {
            curr = tail_;
            trace.counters.nodesVisited++;
            trace.frame(snapshot(), Event::Move, "doublyList.insertAt.startTail",
                        "Index " + std::to_string(index) + " is nearer the tail, so start at the tail, " +
                            text(curr->value) + ".",
                        {{curr->id}, {}}, {{"curr", curr->id}});
            for (int position = oldSize - 2; position >= index; --position) {
                const NodeId from = curr->id;
                curr = curr->prev;
                trace.counters.nodesVisited++;
                trace.frame(snapshot(), Event::Move, "doublyList.insertAt.walkPrev",
                            "Follow prev to " + text(curr->value) + ", which is index " + std::to_string(position) + ".",
                            {{curr->id}, {{from, curr->id}}}, {{"curr", curr->id}});
            }
        }
    }

    Node* node = pool_.create(value);  // non-owning: the pool owns every node
    std::vector<NamedPointer> pointers;
    if (curr != nullptr) {
        pointers.push_back({"curr", curr->id});
    }
    pointers.push_back({"node", node->id});
    trace.frame(snapshot(), Event::Create, "doublyList.insertAt.create",
                "Create a new node holding " + text(value) + "; its prev and next are NIL.", {{node->id}, {}},
                pointers);

    if (head_ == nullptr) {
        head_ = node;
        tail_ = node;
        trace.counters.linksChanged += 2;
        trace.frame(snapshot(), Event::Link, "doublyList.insertAt.linkOnly",
                    "The list was empty, so head and tail both point to the new node.", {{node->id}, {}},
                    pointers);
    } else if (index == 0) {
        Node* oldHead = head_;  // non-owning: the pool owns every node
        node->next = oldHead;
        oldHead->prev = node;
        trace.counters.linksChanged += 2;
        trace.frame(snapshot(), Event::Link, "doublyList.insertAt.linkBeforeHead",
                    "Link the new node and the old head, " + text(oldHead->value) + ", to each other.",
                    {{node->id, oldHead->id}, {{node->id, oldHead->id}, {oldHead->id, node->id}}}, pointers);
        head_ = node;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), Event::Link, "doublyList.insertAt.moveHead",
                    "Move head to the new node, so " + text(value) + " is now at index 0.", {{node->id}, {}},
                    pointers);
    } else if (index == oldSize) {
        Node* oldTail = tail_;  // non-owning: the pool owns every node
        node->prev = oldTail;
        oldTail->next = node;
        trace.counters.linksChanged += 2;
        trace.frame(snapshot(), Event::Link, "doublyList.insertAt.linkAfterTail",
                    "Link the old tail, " + text(oldTail->value) + ", and the new node to each other.",
                    {{oldTail->id, node->id}, {{oldTail->id, node->id}, {node->id, oldTail->id}}}, pointers);
        tail_ = node;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), Event::Link, "doublyList.insertAt.moveTail",
                    "Move tail to the new node, so " + text(value) + " is now the last node.", {{node->id}, {}},
                    pointers);
    } else {
        Node* before = curr->prev;  // non-owning: the pool owns every node
        node->prev = before;
        node->next = curr;
        trace.counters.linksChanged += 2;
        trace.frame(snapshot(), Event::Link, "doublyList.insertAt.linkNode",
                    "Point the new node's prev at " + text(before->value) + " and its next at " +
                        text(curr->value) + ".",
                    {{node->id, before->id, curr->id}, {{node->id, before->id}, {node->id, curr->id}}}, pointers);
        before->next = node;
        curr->prev = node;
        trace.counters.linksChanged += 2;
        trace.frame(snapshot(), Event::Link, "doublyList.insertAt.linkNeighbours",
                    "Point " + text(before->value) + "'s next and " + text(curr->value) +
                        "'s prev at the new node: it is linked in.",
                    {{node->id, before->id, curr->id}, {{before->id, node->id}, {curr->id, node->id}}}, pointers);
    }

    trace.frame(snapshot(), Event::Done, "doublyList.insertAt.done",
                "Inserted " + text(value) + " at index " + std::to_string(index) + "; the list now holds " +
                    nodeCount(size()) + ".",
                {{node->id}, {}});
    return Result<NodeId>{okStatus(), node->id};
}

// deleteNode(id):                                        doublyList.deleteNode.enter
//   node <- the selected node
//   if there is none: reject NODE_NOT_FOUND              doublyList.deleteNode.missing
//                                                        doublyList.deleteNode.found
//   if node.prev = NIL: head <- node.next                doublyList.deleteNode.moveHead
//   else: node.prev.next <- node.next                    doublyList.deleteNode.bypassNext
//   if node.next = NIL: tail <- node.prev                doublyList.deleteNode.moveTail
//   else: node.next.prev <- node.prev                    doublyList.deleteNode.bypassPrev
//   delete node                                          doublyList.deleteNode.remove
//   return                                               doublyList.deleteNode.done
//
// No walk: the node itself knows its neighbours, so this is O(1).
Result<NodeValue> DoublyList::deleteNode(NodeId id, Trace& trace) {
    // Which node the user selected (support work, like a pointer the caller already holds).
    Node* node = pool_.find(id);  // non-owning: the pool owns every node
    trace.frame(snapshot(), Event::Begin, "doublyList.deleteNode.enter",
                (node != nullptr ? "Delete the selected node #" + std::to_string(id) + " (" + text(node->value) + ")"
                                 : "Delete node #" + std::to_string(id)) +
                    " from " + describe(size()) + ".");

    if (node == nullptr) {
        return reject<NodeValue>(trace, "doublyList.deleteNode.missing", ErrorCode::NodeNotFound,
                                 "There is no node with ID " + std::to_string(id) + " in this list.");
    }
    trace.counters.nodesVisited++;
    trace.frame(snapshot(), Event::Found, "doublyList.deleteNode.found",
                "The node holds " + text(node->value) + "; its prev and next links lead straight to its neighbours.",
                {{node->id}, {}}, {{"node", node->id}});

    Node* before = node->prev;  // non-owning: the pool owns every node
    Node* after = node->next;   // non-owning: the pool owns every node

    if (before == nullptr) {
        head_ = after;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), after != nullptr ? Event::Link : Event::Unlink, "doublyList.deleteNode.moveHead",
                    after != nullptr ? "It is the head, so move head to the next node, " + text(after->value) + "."
                                     : std::string("It is the head, so move head to the next node, which is NIL."),
                    {{node->id}, {}}, {{"node", node->id}});
    } else {
        before->next = after;
        trace.counters.linksChanged++;
        Highlight highlight{{node->id, before->id}, {}};
        if (after != nullptr) {
            highlight.edges.push_back(Edge{before->id, after->id});
        }
        trace.frame(snapshot(), after != nullptr ? Event::Link : Event::Unlink, "doublyList.deleteNode.bypassNext",
                    "Point " + text(before->value) + "'s next past the node, at " +
                        (after != nullptr ? text(after->value) : std::string("NIL")) + ".",
                    highlight, {{"node", node->id}});
    }

    if (after == nullptr) {
        tail_ = before;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), before != nullptr ? Event::Link : Event::Unlink, "doublyList.deleteNode.moveTail",
                    before != nullptr ? "It is the tail, so move tail back to " + text(before->value) + "."
                                      : std::string("It is the tail too, so tail becomes NIL."),
                    {{node->id}, {}}, {{"node", node->id}});
    } else {
        after->prev = before;
        trace.counters.linksChanged++;
        Highlight highlight{{node->id, after->id}, {}};
        if (before != nullptr) {
            highlight.edges.push_back(Edge{after->id, before->id});
        }
        trace.frame(snapshot(), before != nullptr ? Event::Link : Event::Unlink, "doublyList.deleteNode.bypassPrev",
                    "Point " + text(after->value) + "'s prev past the node, at " +
                        (before != nullptr ? text(before->value) : std::string("NIL")) + ".",
                    highlight, {{"node", node->id}});
    }

    const NodeValue removed{node->id, node->value};
    pool_.destroy(node);  // 'node' dangles from here on and is not used again
    trace.frame(snapshot(), Event::Remove, "doublyList.deleteNode.remove",
                "Free the node that held " + text(removed.value) + ".", {{removed.nodeId}, {}});

    trace.frame(snapshot(), Event::Done, "doublyList.deleteNode.done",
                size() == 0 ? "Deleted " + text(removed.value) + "; the list is now empty."
                            : "Deleted " + text(removed.value) + " without walking; the list now holds " +
                                  nodeCount(size()) + ".");
    return Result<NodeValue>{okStatus(), removed};
}

// search(value):                                         doublyList.search.enter
//   curr <- head; index <- 0
//   while curr != NIL:
//     if curr.value = value: found                       doublyList.search.found
//     curr <- curr.next; index <- index + 1              doublyList.search.next
//   not found                                            doublyList.search.notFound
//   return                                               doublyList.search.done
Result<ListSearchResult> DoublyList::search(Value value, Trace& trace) const {
    trace.frame(snapshot(), Event::Begin, "doublyList.search.enter",
                "Search " + describe(size()) + " for " + text(value) + ", starting at the head.");

    ListSearchResult result;
    int index = 0;
    const Node* curr = head_;  // non-owning: the pool owns every node
    while (curr != nullptr) {
        trace.counters.nodesVisited++;
        trace.counters.comparisons++;
        if (curr->value == value) {
            result = ListSearchResult{true, curr->id, index};
            trace.frame(snapshot(), Event::Found, "doublyList.search.found",
                        "Index " + std::to_string(index) + " holds " + text(value) + ": found.", {{curr->id}, {}},
                        {{"curr", curr->id}});
            break;
        }
        Highlight highlight{{curr->id}, {}};
        if (curr->next != nullptr) {
            highlight.edges.push_back(Edge{curr->id, curr->next->id});
        }
        trace.frame(snapshot(), Event::Compare, "doublyList.search.next",
                    "Index " + std::to_string(index) + " holds " + text(curr->value) + ", not " + text(value) +
                        ", so move to the next node.",
                    highlight, {{"curr", curr->id}});
        curr = curr->next;
        index = index + 1;
    }

    if (result.found) {
        trace.frame(snapshot(), Event::Done, "doublyList.search.done",
                    "Search finished: " + text(value) + " is at index " + std::to_string(result.index) + ".",
                    {{result.nodeId}, {}});
    } else {
        trace.frame(snapshot(), Event::NotFound, "doublyList.search.notFound",
                    "Reached NIL: " + text(value) + " is not in the list.");
        trace.frame(snapshot(), Event::Done, "doublyList.search.done",
                    "Search finished: " + text(value) + " was not found.");
    }
    return Result<ListSearchResult>{okStatus(), result};
}

// reverse():                                             doublyList.reverse.enter
//   curr <- head
//   while curr != NIL:
//     swap curr.prev and curr.next                       doublyList.reverse.flip
//     curr <- curr.prev        (the old next)
//   swap head and tail                                   doublyList.reverse.swapEnds
//   return                                               doublyList.reverse.done
Status DoublyList::reverse(Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "doublyList.reverse.enter", "Reverse " + describe(size()) + ".");

    Node* curr = head_;  // non-owning: the pool owns every node
    while (curr != nullptr) {
        trace.counters.nodesVisited++;
        Node* oldNext = curr->next;  // non-owning: the pool owns every node
        curr->next = curr->prev;
        curr->prev = oldNext;
        trace.counters.linksChanged += 2;
        trace.frame(snapshot(), Event::Link, "doublyList.reverse.flip",
                    "Swap the prev and next links of " + text(curr->value) + ".", {{curr->id}, {}},
                    {{"curr", curr->id}});
        curr = oldNext;
    }

    if (head_ != tail_) {
        Node* oldHead = head_;  // non-owning: the pool owns every node
        head_ = tail_;
        tail_ = oldHead;
        trace.counters.linksChanged += 2;
        trace.frame(snapshot(), Event::Link, "doublyList.reverse.swapEnds",
                    "Swap head and tail: head is now " + text(head_->value) + " and tail is " + text(tail_->value) + ".",
                    {{head_->id, tail_->id}, {}});
    }

    trace.frame(snapshot(), Event::Done, "doublyList.reverse.done",
                size() < 2 ? "A list of " + nodeCount(size()) + " reads the same in both directions."
                           : "Reversed the list of " + nodeCount(size()) + ".");
    return okStatus();
}

// clear():                                               doublyList.clear.enter
//   while head != NIL:
//     node <- head; head <- node.next
//     if head = NIL: tail <- NIL else head.prev <- NIL
//     delete node                                        doublyList.clear.remove
//   return removed                                       doublyList.clear.done
Result<int> DoublyList::clear(Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "doublyList.clear.enter", "Clear " + describe(size()) + ".");

    int removed = 0;
    while (head_ != nullptr) {
        Node* node = head_;  // non-owning: the pool owns every node
        trace.counters.nodesVisited++;
        head_ = node->next;
        trace.counters.linksChanged++;
        if (head_ == nullptr) {
            tail_ = nullptr;  // never leave tail pointing at a freed node
        } else {
            head_->prev = nullptr;  // nor the new head's prev
        }
        trace.counters.linksChanged++;

        const NodeValue freed{node->id, node->value};
        pool_.destroy(node);  // 'node' dangles from here on and is not used again
        removed = removed + 1;
        trace.frame(snapshot(), Event::Remove, "doublyList.clear.remove",
                    "Move head past " + text(freed.value) + " and free that node.", {{freed.nodeId}, {}});
    }

    trace.frame(snapshot(), Event::Done, "doublyList.clear.done",
                removed == 0 ? std::string("The list was already empty, so nothing was freed.")
                             : "Cleared " + nodeCount(removed) + "; the list is now empty.");
    return Result<int>{okStatus(), removed};
}

Snapshot DoublyList::snapshot() const {
    Snapshot copy;
    copy.structure = StructureKind::DoublyList;
    copy.size = pool_.size();
    copy.nextId = pool_.nextId();
    copy.head = (head_ != nullptr) ? head_->id : kNoNode;
    copy.tail = (tail_ != nullptr) ? tail_->id : kNoNode;
    for (const std::unique_ptr<Node>& node : pool_.nodes()) {
        NodeView view;
        view.id = node->id;
        view.value = node->value;
        view.next = (node->next != nullptr) ? node->next->id : kNoNode;
        view.prev = (node->prev != nullptr) ? node->prev->id : kNoNode;
        copy.nodes.push_back(view);
    }
    return copy;
}

// restore(snapshot): create every node with its ID and value, then set the
// links. pool_.find(kNoNode) is nullptr, which is how NIL is stored here.
void DoublyList::restore(const Snapshot& snapshot) {
    pool_.reset(snapshot.nextId);
    for (const NodeView& view : snapshot.nodes) {
        pool_.createWithId(view.id, view.value);
    }
    for (const NodeView& view : snapshot.nodes) {
        Node* node = pool_.find(view.id);  // non-owning: the pool owns every node
        node->next = pool_.find(view.next);
        node->prev = pool_.find(view.prev);
    }
    head_ = pool_.find(snapshot.head);
    tail_ = pool_.find(snapshot.tail);
}

}  // namespace dslab
