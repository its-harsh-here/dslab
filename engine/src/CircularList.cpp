#include "dslab/CircularList.h"

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

// "an empty circular list" or "a circular list that holds 3 nodes".
std::string describe(int size) {
    return size == 0 ? "an empty circular list" : "a circular list that holds " + nodeCount(size);
}

}  // namespace

// insertFront(value):                                circularList.insertFront.enter
//   if size = 200: reject CAPACITY_EXCEEDED          circularList.insertFront.full
//   node <- new Node(value)                          circularList.insertFront.create
//   if tail = NIL:
//     node.next <- node                              circularList.insertFront.linkSelf
//     tail <- node                                   circularList.insertFront.setTail
//   else:
//     node.next <- tail.next                         circularList.insertFront.linkNext
//     tail.next <- node                              circularList.insertFront.linkTail
//   return node                                      circularList.insertFront.done
//
// insertBack(value) does exactly the same and then moves tail to the new node
// (circularList.insertBack.moveTail): the only difference between the two ends.
Result<NodeId> CircularList::insert(bool atFront, Value value, Trace& trace) {
    const std::string step = atFront ? "circularList.insertFront." : "circularList.insertBack.";
    const std::string place = atFront ? "front" : "back";

    trace.frame(snapshot(), Event::Begin, step + "enter",
                "Insert " + text(value) + " at the " + place + " of " + describe(size()) + ".");

    if (size() == kMaxNodes) {
        return reject<NodeId>(trace, step + "full", ErrorCode::CapacityExceeded,
                              "The circular list already holds 200 nodes, the maximum.");
    }
    if (!pool_.hasFreeId()) {
        return reject<NodeId>(trace, step + "full", ErrorCode::IdExhausted,
                              "No node IDs are left for this circular list.");
    }

    Node* node = pool_.create(value);  // non-owning: the pool owns every node
    trace.frame(snapshot(), Event::Create, step + "create", "Create a new node holding " + text(value) + ".",
                {{node->id}, {}}, {{"node", node->id}});

    if (tail_ == nullptr) {
        node->next = node;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), Event::Link, step + "linkSelf",
                    "The list is empty, so the new node's next points at itself: a circle of one.",
                    {{node->id}, {{node->id, node->id}}}, {{"node", node->id}});
        tail_ = node;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), Event::Link, step + "setTail", "Point tail at the new node.", {{node->id}, {}},
                    {{"node", node->id}});
    } else {
        Node* first = tail_->next;  // non-owning: the pool owns every node
        node->next = first;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), Event::Link, step + "linkNext",
                    "Point the new node's next at the first node, " + text(first->value) + ".",
                    {{node->id, first->id}, {{node->id, first->id}}}, {{"node", node->id}});
        tail_->next = node;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), Event::Link, step + "linkTail",
                    "Point the tail's next (" + text(tail_->value) + ") at the new node, which closes the circle.",
                    {{tail_->id, node->id}, {{tail_->id, node->id}}}, {{"node", node->id}});
        if (!atFront) {
            tail_ = node;
            trace.counters.linksChanged++;
            trace.frame(snapshot(), Event::Link, step + "moveTail",
                        "Move tail to the new node, so " + text(value) + " is now the last node.",
                        {{node->id}, {}}, {{"node", node->id}});
        }
    }

    trace.frame(snapshot(), Event::Done, step + "done",
                "Inserted " + text(value) + " at the " + place + "; the circular list now holds " +
                    nodeCount(size()) + ".",
                {{node->id}, {}});
    return Result<NodeId>{okStatus(), node->id};
}

// deleteNode(id):                                        circularList.deleteNode.enter
//   node <- the selected node
//   if there is none: reject NODE_NOT_FOUND              circularList.deleteNode.missing
//   if node.next = node: tail <- NIL                     circularList.deleteNode.onlyNode
//   else:
//     prev <- tail                                       circularList.deleteNode.start
//     while prev.next is not node: prev <- prev.next     circularList.deleteNode.walk
//     prev.next <- node.next                             circularList.deleteNode.bypass
//     if node = tail: tail <- prev                       circularList.deleteNode.moveTail
//   delete node                                          circularList.deleteNode.remove
//   return                                               circularList.deleteNode.done
Result<NodeValue> CircularList::deleteNode(NodeId id, Trace& trace) {
    // Which node the user selected (support work, like a pointer the caller already holds).
    Node* node = pool_.find(id);  // non-owning: the pool owns every node
    trace.frame(snapshot(), Event::Begin, "circularList.deleteNode.enter",
                (node != nullptr ? "Delete the selected node #" + std::to_string(id) + " (" + text(node->value) + ")"
                                 : "Delete node #" + std::to_string(id)) +
                    " from " + describe(size()) + ".");

    if (node == nullptr) {
        return reject<NodeValue>(trace, "circularList.deleteNode.missing", ErrorCode::NodeNotFound,
                                 "There is no node with ID " + std::to_string(id) + " in this list.");
    }

    if (node->next == node) {
        tail_ = nullptr;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), Event::Unlink, "circularList.deleteNode.onlyNode",
                    text(node->value) + " is the only node, so tail becomes NIL and the circle is gone.",
                    {{node->id}, {}}, {{"node", node->id}});
    } else {
        Node* prev = tail_;  // non-owning: the pool owns every node
        trace.counters.nodesVisited++;
        trace.frame(snapshot(), Event::Move, "circularList.deleteNode.start",
                    "Start at the tail, " + text(prev->value) + ", and look for the node just before #" +
                        std::to_string(id) + ".",
                    {{prev->id}, {}}, {{"prev", prev->id}});
        while (prev->next != node) {
            const NodeId from = prev->id;
            prev = prev->next;
            trace.counters.nodesVisited++;
            trace.frame(snapshot(), Event::Move, "circularList.deleteNode.walk",
                        "Its next is not the selected node, so move to " + text(prev->value) + ".",
                        {{prev->id}, {{from, prev->id}}}, {{"prev", prev->id}});
        }

        prev->next = node->next;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), Event::Link, "circularList.deleteNode.bypass",
                    "Point " + text(prev->value) + "'s next past the node, at " + text(prev->next->value) + ".",
                    {{prev->id, node->id, prev->next->id}, {{prev->id, prev->next->id}}},
                    {{"prev", prev->id}, {"node", node->id}});

        if (node == tail_) {
            tail_ = prev;
            trace.counters.linksChanged++;
            trace.frame(snapshot(), Event::Link, "circularList.deleteNode.moveTail",
                        "The node was the tail, so tail moves back to " + text(prev->value) + ".",
                        {{prev->id, node->id}, {}}, {{"prev", prev->id}, {"node", node->id}});
        }
    }

    const NodeValue removed{node->id, node->value};
    pool_.destroy(node);  // 'node' dangles from here on and is not used again
    trace.frame(snapshot(), Event::Remove, "circularList.deleteNode.remove",
                "Free the node that held " + text(removed.value) + ".", {{removed.nodeId}, {}});

    trace.frame(snapshot(), Event::Done, "circularList.deleteNode.done",
                size() == 0 ? "Deleted " + text(removed.value) + "; the circular list is now empty."
                            : "Deleted " + text(removed.value) + "; the circular list now holds " +
                                  nodeCount(size()) + ".");
    return Result<NodeValue>{okStatus(), removed};
}

// search(value):                                         circularList.search.enter
//   if tail != NIL:
//     curr <- tail.next; index <- 0        (the first node)
//     repeat:
//       if curr.value = value: found                     circularList.search.found
//       curr <- curr.next; index <- index + 1            circularList.search.next
//     until curr = tail.next               (back at the start: there is no NIL to stop at)
//   not found                                            circularList.search.notFound
//   return                                               circularList.search.done
Result<ListSearchResult> CircularList::search(Value value, Trace& trace) const {
    trace.frame(snapshot(), Event::Begin, "circularList.search.enter",
                "Search " + describe(size()) + " for " + text(value) + ", starting at the first node (tail.next).");

    ListSearchResult result;
    if (tail_ != nullptr) {
        const Node* first = tail_->next;  // non-owning: the pool owns every node
        const Node* curr = first;         // non-owning: the pool owns every node
        int index = 0;
        do {
            trace.counters.nodesVisited++;
            trace.counters.comparisons++;
            if (curr->value == value) {
                result = ListSearchResult{true, curr->id, index};
                trace.frame(snapshot(), Event::Found, "circularList.search.found",
                            "Index " + std::to_string(index) + " holds " + text(value) + ": found.",
                            {{curr->id}, {}}, {{"curr", curr->id}});
                break;
            }
            trace.frame(snapshot(), Event::Compare, "circularList.search.next",
                        "Index " + std::to_string(index) + " holds " + text(curr->value) + ", not " + text(value) +
                            ", so move to the next node.",
                        {{curr->id}, {{curr->id, curr->next->id}}}, {{"curr", curr->id}});
            curr = curr->next;
            index = index + 1;
        } while (curr != first);
    }

    if (result.found) {
        trace.frame(snapshot(), Event::Done, "circularList.search.done",
                    "Search finished: " + text(value) + " is at index " + std::to_string(result.index) + ".",
                    {{result.nodeId}, {}});
    } else {
        trace.frame(snapshot(), Event::NotFound, "circularList.search.notFound",
                    size() == 0 ? "The list is empty, so " + text(value) + " is not in it."
                                : "Back at the first node after a full circle: " + text(value) + " is not in the list.");
        trace.frame(snapshot(), Event::Done, "circularList.search.done",
                    "Search finished: " + text(value) + " was not found.");
    }
    return Result<ListSearchResult>{okStatus(), result};
}

// rotate():                                              circularList.rotate.enter
//   if tail != NIL: tail <- tail.next                    circularList.rotate.moveTail
//   return                                               circularList.rotate.done
//
// The old first node is now the last one. No link between nodes changes.
Status CircularList::rotate(Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "circularList.rotate.enter",
                "Rotate " + describe(size()) + ": the first node goes to the back.");

    if (tail_ != nullptr) {
        const NodeId from = tail_->id;
        tail_ = tail_->next;
        trace.counters.linksChanged++;
        trace.counters.nodesVisited++;
        trace.frame(snapshot(), Event::Link, "circularList.rotate.moveTail",
                    "Move tail one step along the circle, to " + text(tail_->value) + "; no link between nodes changes.",
                    {{tail_->id}, {{from, tail_->id}}});
    }

    trace.frame(snapshot(), Event::Done, "circularList.rotate.done",
                size() < 2 ? "A circular list of " + nodeCount(size()) + " looks the same after a rotation."
                           : "Rotated: " + text(tail_->value) + " is now last and " + text(tail_->next->value) +
                                 " is first.");
    return okStatus();
}

// clear():                                               circularList.clear.enter
//   while tail != NIL:
//     first <- tail.next
//     if first = tail: tail <- NIL
//     else: tail.next <- first.next
//     delete first                                       circularList.clear.remove
//   return removed                                       circularList.clear.done
Result<int> CircularList::clear(Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "circularList.clear.enter", "Clear " + describe(size()) + ".");

    int removed = 0;
    while (tail_ != nullptr) {
        Node* first = tail_->next;  // non-owning: the pool owns every node
        trace.counters.nodesVisited++;
        if (first == tail_) {
            tail_ = nullptr;
        } else {
            tail_->next = first->next;
        }
        trace.counters.linksChanged++;

        const NodeValue freed{first->id, first->value};
        pool_.destroy(first);  // 'first' dangles from here on and is not used again
        removed = removed + 1;
        trace.frame(snapshot(), Event::Remove, "circularList.clear.remove",
                    "Unlink the first node, " + text(freed.value) + ", from the circle and free it.",
                    {{freed.nodeId}, {}});
    }

    trace.frame(snapshot(), Event::Done, "circularList.clear.done",
                removed == 0 ? std::string("The circular list was already empty, so nothing was freed.")
                             : "Cleared " + nodeCount(removed) + "; the circular list is now empty.");
    return Result<int>{okStatus(), removed};
}

Snapshot CircularList::snapshot() const {
    Snapshot copy;
    copy.structure = StructureKind::CircularList;
    copy.size = pool_.size();
    copy.nextId = pool_.nextId();
    copy.tail = (tail_ != nullptr) ? tail_->id : kNoNode;
    for (const std::unique_ptr<Node>& node : pool_.nodes()) {
        const NodeId next = (node->next != nullptr) ? node->next->id : kNoNode;
        copy.nodes.push_back(NodeView{node->id, node->value, next});
    }
    return copy;
}

// restore(snapshot): create every node with its ID and value, then set the links.
void CircularList::restore(const Snapshot& snapshot) {
    pool_.reset(snapshot.nextId);
    for (const NodeView& view : snapshot.nodes) {
        pool_.createWithId(view.id, view.value);
    }
    for (const NodeView& view : snapshot.nodes) {
        pool_.find(view.id)->next = pool_.find(view.next);
    }
    tail_ = pool_.find(snapshot.tail);
}

}  // namespace dslab
