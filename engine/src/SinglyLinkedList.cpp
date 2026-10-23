#include "dslab/SinglyLinkedList.h"

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

// "an empty list" or "a list that holds 3 nodes".
std::string describe(int size) {
    return size == 0 ? "an empty list" : "a list that holds " + nodeCount(size);
}

// "#3 (30)": duplicates are allowed, so the ID says which node is meant.
template <typename Node>
std::string named(const Node* node) {
    return "#" + std::to_string(node->id) + " (" + text(node->value) + ")";
}

}  // namespace

// insertAt(index, value):                                   list.insertAt.enter
//   if size = 200: reject CAPACITY_EXCEEDED                 list.insertAt.full
//   if index < 0 or index > size: reject INDEX_OUT_OF_RANGE list.insertAt.badIndex
//   if index = 0:
//     node <- new Node(value)                               list.insertAt.create
//     node.next <- head                                     list.insertAt.linkNext
//     head <- node                                          list.insertAt.linkHead
//   else:
//     prev <- head                                          list.insertAt.start
//     repeat index - 1 times: prev <- prev.next             list.insertAt.walk
//     node <- new Node(value)                               list.insertAt.create
//     node.next <- prev.next                                list.insertAt.linkNext
//     prev.next <- node                                     list.insertAt.linkPrev
//   return node                                             list.insertAt.done
Result<NodeId> SinglyLinkedList::insertAt(int index, Value value, Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "list.insertAt.enter",
                "Insert " + text(value) + " at index " + std::to_string(index) + " of " +
                    describe(size()) + ".");

    if (size() == kMaxNodes) {
        return reject<NodeId>(trace, "list.insertAt.full", ErrorCode::CapacityExceeded,
                              "The list already holds 200 nodes, the maximum.");
    }
    if (!pool_.hasFreeId()) {
        return reject<NodeId>(trace, "list.insertAt.full", ErrorCode::IdExhausted,
                              "No node IDs are left for this list.");
    }
    if (index < 0 || index > size()) {
        const std::string where = size() == 0 ? "an empty list" : "a list of " + nodeCount(size());
        return reject<NodeId>(trace, "list.insertAt.badIndex", ErrorCode::IndexOutOfRange,
                              "Index " + std::to_string(index) + " is outside 0 to " +
                                  std::to_string(size()) + " for " + where + ".");
    }

    Node* node = nullptr;  // non-owning: the pool owns every node
    if (index == 0) {
        node = pool_.create(value);
        trace.frame(snapshot(), Event::Create, "list.insertAt.create",
                    "Create a new node holding " + text(value) + " to go in front of the head.",
                    {{node->id}, {}}, {{"node", node->id}});

        node->next = head_;
        trace.counters.linksChanged++;
        if (head_ != nullptr) {
            trace.frame(snapshot(), Event::Link, "list.insertAt.linkNext",
                        "Point the new node's next at the current head, " + text(head_->value) + ".",
                        {{node->id, head_->id}, {{node->id, head_->id}}}, {{"node", node->id}});
        } else {
            trace.frame(snapshot(), Event::Unlink, "list.insertAt.linkNext",
                        "Point the new node's next at the current head, which is NIL.",
                        {{node->id}, {}}, {{"node", node->id}});
        }

        head_ = node;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), Event::Link, "list.insertAt.linkHead",
                    "Move head to the new node, so " + text(value) + " is now at index 0.",
                    {{node->id}, {}}, {{"node", node->id}});
    } else {
        Node* prev = head_;  // non-owning: the pool owns every node
        trace.counters.nodesVisited++;
        trace.frame(snapshot(), Event::Move, "list.insertAt.start",
                    "Start at the head, " + text(prev->value) + ", which is index 0.",
                    {{prev->id}, {}}, {{"prev", prev->id}});
        for (int position = 1; position < index; ++position) {
            const NodeId from = prev->id;
            prev = prev->next;
            trace.counters.nodesVisited++;
            trace.frame(snapshot(), Event::Move, "list.insertAt.walk",
                        "Move to the next node, " + text(prev->value) + ", which is index " +
                            std::to_string(position) + ".",
                        {{prev->id}, {{from, prev->id}}}, {{"prev", prev->id}});
        }

        node = pool_.create(value);
        trace.frame(snapshot(), Event::Create, "list.insertAt.create",
                    "Create a new node holding " + text(value) + " to go after " +
                        text(prev->value) + ".",
                    {{node->id}, {}}, {{"prev", prev->id}, {"node", node->id}});

        node->next = prev->next;
        trace.counters.linksChanged++;
        if (node->next != nullptr) {
            trace.frame(snapshot(), Event::Link, "list.insertAt.linkNext",
                        "Point the new node's next at " + text(node->next->value) +
                            ", the node after " + text(prev->value) + ".",
                        {{node->id, node->next->id}, {{node->id, node->next->id}}},
                        {{"prev", prev->id}, {"node", node->id}});
        } else {
            trace.frame(snapshot(), Event::Unlink, "list.insertAt.linkNext",
                        "Point the new node's next at NIL, because " + text(prev->value) +
                            " is the last node.",
                        {{node->id}, {}}, {{"prev", prev->id}, {"node", node->id}});
        }

        prev->next = node;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), Event::Link, "list.insertAt.linkPrev",
                    "Point " + text(prev->value) + "'s next at the new node, so " + text(value) +
                        " is now at index " + std::to_string(index) + ".",
                    {{prev->id, node->id}, {{prev->id, node->id}}},
                    {{"prev", prev->id}, {"node", node->id}});
    }

    trace.frame(snapshot(), Event::Done, "list.insertAt.done",
                "Inserted " + text(value) + " at index " + std::to_string(index) +
                    "; the list now holds " + nodeCount(size()) + ".",
                {{node->id}, {}});
    return Result<NodeId>{okStatus(), node->id};
}

// deleteNode(id):                                           list.deleteNode.enter
//   if no node has this ID: reject NODE_NOT_FOUND           list.deleteNode.missing
//   if head is the node:
//     node <- head                                          list.deleteNode.atHead
//     head <- node.next                                     list.deleteNode.moveHead
//   else:
//     prev <- head                                          list.deleteNode.start
//     while prev.next is not the node: prev <- prev.next    list.deleteNode.walk
//     node <- prev.next                                     list.deleteNode.found
//     prev.next <- node.next                                list.deleteNode.bypass
//   delete node                                             list.deleteNode.remove
//   return                                                  list.deleteNode.done
Result<NodeValue> SinglyLinkedList::deleteNode(NodeId id, Trace& trace) {
    // Which node the user selected (support work, like a pointer the caller already holds).
    const Node* selected = pool_.find(id);  // non-owning: the pool owns every node
    trace.frame(snapshot(), Event::Begin, "list.deleteNode.enter",
                (selected != nullptr ? "Delete the selected node " + named(selected)
                                     : "Delete node #" + std::to_string(id)) +
                    " from " + describe(size()) + ".");

    if (selected == nullptr) {
        return reject<NodeValue>(trace, "list.deleteNode.missing", ErrorCode::NodeNotFound,
                                 "There is no node with ID " + std::to_string(id) + " in this list.");
    }

    Node* node = nullptr;  // non-owning: the pool owns every node
    if (head_->id == id) {
        node = head_;
        trace.counters.nodesVisited++;
        trace.frame(snapshot(), Event::Found, "list.deleteNode.atHead",
                    "The selected node, " + text(node->value) + ", is the head.", {{node->id}, {}},
                    {{"node", node->id}});

        head_ = node->next;
        trace.counters.linksChanged++;
        if (head_ != nullptr) {
            trace.frame(snapshot(), Event::Link, "list.deleteNode.moveHead",
                        "Move head to the next node, " + text(head_->value) + ".",
                        {{node->id, head_->id}, {{node->id, head_->id}}}, {{"node", node->id}});
        } else {
            trace.frame(snapshot(), Event::Unlink, "list.deleteNode.moveHead",
                        "Move head to the next node, which is NIL.", {{node->id}, {}},
                        {{"node", node->id}});
        }
    } else {
        Node* prev = head_;  // non-owning: the pool owns every node
        trace.counters.nodesVisited++;
        trace.frame(snapshot(), Event::Move, "list.deleteNode.start",
                    "Start at the head, " + text(prev->value) + ", and look for the node just before #" +
                        std::to_string(id) + ".",
                    {{prev->id}, {}}, {{"prev", prev->id}});
        while (prev->next->id != id) {
            const NodeId from = prev->id;
            prev = prev->next;
            trace.counters.nodesVisited++;
            trace.frame(snapshot(), Event::Move, "list.deleteNode.walk",
                        "Its next is not the selected node, so move to " + text(prev->value) + ".",
                        {{prev->id}, {{from, prev->id}}}, {{"prev", prev->id}});
        }

        node = prev->next;
        trace.counters.nodesVisited++;
        trace.frame(snapshot(), Event::Found, "list.deleteNode.found",
                    "The node after " + text(prev->value) + " is the selected node, " +
                        text(node->value) + ".",
                    {{prev->id, node->id}, {{prev->id, node->id}}},
                    {{"prev", prev->id}, {"node", node->id}});

        prev->next = node->next;
        trace.counters.linksChanged++;
        if (prev->next != nullptr) {
            trace.frame(snapshot(), Event::Link, "list.deleteNode.bypass",
                        "Point " + text(prev->value) + "'s next past it, at " +
                            text(prev->next->value) + ".",
                        {{prev->id, prev->next->id}, {{prev->id, prev->next->id}}},
                        {{"prev", prev->id}, {"node", node->id}});
        } else {
            trace.frame(snapshot(), Event::Unlink, "list.deleteNode.bypass",
                        "Point " + text(prev->value) + "'s next at NIL, because " +
                            text(node->value) + " was the last node.",
                        {{prev->id, node->id}, {}}, {{"prev", prev->id}, {"node", node->id}});
        }
    }

    const NodeValue removed{node->id, node->value};
    pool_.destroy(node);  // 'node' dangles from here on and is not used again
    trace.frame(snapshot(), Event::Remove, "list.deleteNode.remove",
                "Free the selected node, which held " + text(removed.value) + ".",
                {{removed.nodeId}, {}});

    trace.frame(snapshot(), Event::Done, "list.deleteNode.done",
                size() == 0 ? "Deleted " + text(removed.value) + "; the list is now empty."
                            : "Deleted " + text(removed.value) + "; the list now holds " +
                                  nodeCount(size()) + ".");
    return Result<NodeValue>{okStatus(), removed};
}

// search(value):                                            list.search.enter
//   curr <- head; index <- 0
//   while curr != NIL:
//     if curr.value = value: return found                   list.search.found
//     curr <- curr.next; index <- index + 1                 list.search.next
//   return not found                                        list.search.notFound
//                                                           list.search.done
Result<ListSearchResult> SinglyLinkedList::search(Value value, Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "list.search.enter",
                "Search " + describe(size()) + " for " + text(value) + ".");

    ListSearchResult result;
    const Node* curr = head_;  // non-owning: the pool owns every node
    int index = 0;
    while (curr != nullptr) {
        trace.counters.nodesVisited++;
        trace.counters.comparisons++;
        if (curr->value == value) {
            result = ListSearchResult{true, curr->id, index};
            trace.frame(snapshot(), Event::Found, "list.search.found",
                        "Found " + text(value) + " at index " + std::to_string(index) + ".",
                        {{curr->id}, {}}, {{"curr", curr->id}});
            break;
        }
        Highlight highlight{{curr->id}, {}};
        if (curr->next != nullptr) {
            highlight.edges.push_back(Edge{curr->id, curr->next->id});
        }
        trace.frame(snapshot(), Event::Compare, "list.search.next",
                    text(curr->value) + " is not " + text(value) + ", so move to the next node.",
                    highlight, {{"curr", curr->id}});
        curr = curr->next;
        index = index + 1;
    }

    if (!result.found) {
        trace.frame(snapshot(), Event::NotFound, "list.search.notFound",
                    "Reached NIL: " + text(value) + " is not in the list.", {}, {{"curr", kNoNode}});
        trace.frame(snapshot(), Event::Done, "list.search.done",
                    "Search finished: " + text(value) + " is not in the list.");
    } else {
        trace.frame(snapshot(), Event::Done, "list.search.done",
                    "Search finished: " + text(value) + " is at index " +
                        std::to_string(result.index) + ".",
                    {{result.nodeId}, {}});
    }
    return Result<ListSearchResult>{okStatus(), result};
}

// reverse():                                                list.reverse.enter
//   prev <- NIL; curr <- head
//   while curr != NIL:
//     next <- curr.next
//     curr.next <- prev                                     list.reverse.flip
//     prev <- curr; curr <- next
//   head <- prev                                            list.reverse.moveHead
//   return                                                  list.reverse.done
Status SinglyLinkedList::reverse(Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "list.reverse.enter", "Reverse " + describe(size()) + ".");

    Node* prev = nullptr;   // non-owning: the pool owns every node
    Node* curr = head_;     // non-owning: the pool owns every node
    while (curr != nullptr) {
        Node* next = curr->next;  // non-owning: the pool owns every node
        trace.counters.nodesVisited++;
        curr->next = prev;
        trace.counters.linksChanged++;

        const NodeId prevId = (prev != nullptr) ? prev->id : kNoNode;
        const NodeId nextId = (next != nullptr) ? next->id : kNoNode;
        if (prev == nullptr) {
            trace.frame(snapshot(), Event::Unlink, "list.reverse.flip",
                        "Point " + text(curr->value) + "'s next back at NIL; it will be the last node.",
                        {{curr->id}, {}}, {{"prev", prevId}, {"curr", curr->id}, {"next", nextId}});
        } else {
            trace.frame(snapshot(), Event::Link, "list.reverse.flip",
                        "Point " + text(curr->value) + "'s next back at " + text(prev->value) + ".",
                        {{curr->id, prev->id}, {{curr->id, prev->id}}},
                        {{"prev", prevId}, {"curr", curr->id}, {"next", nextId}});
        }
        prev = curr;
        curr = next;
    }

    head_ = prev;
    trace.counters.linksChanged++;
    if (head_ != nullptr) {
        trace.frame(snapshot(), Event::Link, "list.reverse.moveHead",
                    "Move head to " + text(head_->value) + ", the old last node.", {{head_->id}, {}},
                    {{"prev", head_->id}});
        trace.frame(snapshot(), Event::Done, "list.reverse.done",
                    "Reversed the list; " + text(head_->value) + " is now the head.", {{head_->id}, {}});
    } else {
        trace.frame(snapshot(), Event::Unlink, "list.reverse.moveHead",
                    "The list is empty, so head stays NIL.");
        trace.frame(snapshot(), Event::Done, "list.reverse.done",
                    "Nothing to reverse: the list is empty.");
    }
    return okStatus();
}

// clear():                                                  list.clear.enter
//   removed <- 0
//   while head != NIL:
//     node <- head
//     head <- node.next
//     delete node                                           list.clear.remove
//     removed <- removed + 1
//   return removed                                          list.clear.done
Result<int> SinglyLinkedList::clear(Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "list.clear.enter", "Clear " + describe(size()) + ".");

    int removed = 0;
    while (head_ != nullptr) {
        Node* node = head_;  // non-owning: the pool owns every node
        trace.counters.nodesVisited++;
        head_ = node->next;
        trace.counters.linksChanged++;

        const NodeValue freed{node->id, node->value};
        pool_.destroy(node);  // 'node' dangles from here on and is not used again
        removed = removed + 1;
        trace.frame(snapshot(), Event::Remove, "list.clear.remove",
                    "Move head past " + text(freed.value) + " and free that node.",
                    {{freed.nodeId}, {}});
    }

    trace.frame(snapshot(), Event::Done, "list.clear.done",
                removed == 0 ? std::string("The list was already empty, so nothing was freed.")
                             : "Cleared " + nodeCount(removed) + "; the list is now empty.");
    return Result<int>{okStatus(), removed};
}

Snapshot SinglyLinkedList::snapshot() const {
    Snapshot copy;
    copy.structure = StructureKind::List;
    copy.size = pool_.size();
    copy.nextId = pool_.nextId();
    copy.head = (head_ != nullptr) ? head_->id : kNoNode;
    for (const std::unique_ptr<Node>& node : pool_.nodes()) {
        const NodeId next = (node->next != nullptr) ? node->next->id : kNoNode;
        copy.nodes.push_back(NodeView{node->id, node->value, next});
    }
    return copy;
}

// restore(snapshot): create every node with its ID and value, then set the
// links. pool_.find(kNoNode) is nullptr, which is how NIL is stored here.
void SinglyLinkedList::restore(const Snapshot& snapshot) {
    pool_.reset(snapshot.nextId);
    for (const NodeView& view : snapshot.nodes) {
        pool_.createWithId(view.id, view.value);
    }
    for (const NodeView& view : snapshot.nodes) {
        pool_.find(view.id)->next = pool_.find(view.next);
    }
    head_ = pool_.find(snapshot.head);
}

}  // namespace dslab
