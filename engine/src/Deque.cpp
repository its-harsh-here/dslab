#include "dslab/Deque.h"

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

// "an empty deque" or "a deque that holds 3 nodes".
std::string describe(int size) {
    return size == 0 ? "an empty deque" : "a deque that holds " + nodeCount(size);
}

}  // namespace

// pushFront(value):                                  deque.pushFront.enter
//   if size = 200: reject CAPACITY_EXCEEDED          deque.pushFront.full
//   node <- new Node(value)                          deque.pushFront.create
//   if front = NIL: rear <- node                     deque.pushFront.linkEmpty
//   else: node.next <- front; front.prev <- node     deque.pushFront.link
//   front <- node                                    deque.pushFront.moveEnd
//   return node                                      deque.pushFront.done
//
// pushBack(value) is the mirror image: "front" and "rear" change places, and
// so do "next" and "prev". Its step IDs start with deque.pushBack.
Result<NodeId> Deque::push(End end, Value value, Trace& trace) {
    const bool atFront = (end == End::Front);
    const std::string step = atFront ? "deque.pushFront." : "deque.pushBack.";
    const std::string endName = atFront ? "front" : "rear";
    const std::string otherName = atFront ? "rear" : "front";

    trace.frame(snapshot(), Event::Begin, step + "enter",
                "Push " + text(value) + " at the " + endName + " of " + describe(size()) + ".");

    if (size() == kMaxNodes) {
        return reject<NodeId>(trace, step + "full", ErrorCode::CapacityExceeded,
                              "The deque already holds 200 nodes, the maximum.");
    }
    if (!pool_.hasFreeId()) {
        return reject<NodeId>(trace, step + "full", ErrorCode::IdExhausted,
                              "No node IDs are left for this deque.");
    }

    Node* node = pool_.create(value);  // non-owning: the pool owns every node
    trace.frame(snapshot(), Event::Create, step + "create",
                "Create a new node holding " + text(value) + "; its prev and next are NIL.",
                {{node->id}, {}}, {{"node", node->id}});

    Node* old = atFront ? front_ : rear_;  // non-owning: the node now at this end
    if (old == nullptr) {
        (atFront ? rear_ : front_) = node;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), Event::Link, step + "linkEmpty",
                    "The deque was empty, so " + otherName + " points to the new node too.",
                    {{node->id}, {}}, {{"node", node->id}});
    } else {
        if (atFront) {
            node->next = old;
            old->prev = node;
        } else {
            node->prev = old;
            old->next = node;
        }
        trace.counters.linksChanged += 2;
        const Edge forward = atFront ? Edge{node->id, old->id} : Edge{old->id, node->id};
        const Edge backward{forward.to, forward.from};
        trace.frame(snapshot(), Event::Link, step + "link",
                    "Link the new node and the old " + endName + ", " + text(old->value) +
                        ", to each other in both directions.",
                    {{node->id, old->id}, {forward, backward}}, {{"node", node->id}});
    }

    (atFront ? front_ : rear_) = node;
    trace.counters.linksChanged++;
    trace.frame(snapshot(), Event::Link, step + "moveEnd",
                "Move " + endName + " to the new node, so " + text(value) + " is now at the " + endName + ".",
                {{node->id}, {}}, {{"node", node->id}});

    trace.frame(snapshot(), Event::Done, step + "done",
                "Pushed " + text(value) + " at the " + endName + "; the deque now holds " + nodeCount(size()) + ".",
                {{node->id}, {}});
    return Result<NodeId>{okStatus(), node->id};
}

// popFront():                                        deque.popFront.enter
//   if front = NIL: reject EMPTY_STRUCTURE           deque.popFront.empty
//   node <- front                                    deque.popFront.take
//   front <- node.next                               deque.popFront.moveEnd
//   if front = NIL: rear <- NIL                      deque.popFront.clearOther
//   else: front.prev <- NIL                          deque.popFront.unlink
//   value <- node.value
//   delete node                                      deque.popFront.remove
//   return value                                     deque.popFront.done
//
// popBack() is the mirror image (rear <- node.prev, and so on). Because nodes
// store prev, it is O(1) too: a singly linked queue would have to walk to the
// node before the rear.
Result<NodeValue> Deque::pop(End end, Trace& trace) {
    const bool atFront = (end == End::Front);
    const std::string step = atFront ? "deque.popFront." : "deque.popBack.";
    const std::string endName = atFront ? "front" : "rear";
    const std::string otherName = atFront ? "rear" : "front";

    trace.frame(snapshot(), Event::Begin, step + "enter",
                "Pop from the " + endName + " of " + describe(size()) + ".");

    Node* node = atFront ? front_ : rear_;  // non-owning: the pool owns every node
    if (node == nullptr) {
        return reject<NodeValue>(trace, step + "empty", ErrorCode::EmptyStructure,
                                 "Cannot pop from the " + endName + ": the deque is empty.");
    }
    trace.counters.nodesVisited++;
    trace.frame(snapshot(), Event::Move, step + "take",
                "Take the " + endName + " node, which holds " + text(node->value) + ".", {{node->id}, {}},
                {{"node", node->id}});

    Node* neighbour = atFront ? node->next : node->prev;  // non-owning: the new end, or NIL
    (atFront ? front_ : rear_) = neighbour;
    trace.counters.linksChanged++;
    if (neighbour == nullptr) {
        trace.frame(snapshot(), Event::Unlink, step + "moveEnd",
                    "Move " + endName + " to the neighbouring node, which is NIL.", {{node->id}, {}},
                    {{"node", node->id}});
        (atFront ? rear_ : front_) = nullptr;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), Event::Unlink, step + "clearOther",
                    "That was the only node, so " + otherName + " becomes NIL too.", {{node->id}, {}},
                    {{"node", node->id}});
    } else {
        trace.frame(snapshot(), Event::Link, step + "moveEnd",
                    "Move " + endName + " to the neighbouring node, " + text(neighbour->value) + ".",
                    {{node->id, neighbour->id}, {}}, {{"node", node->id}});
        (atFront ? neighbour->prev : neighbour->next) = nullptr;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), Event::Unlink, step + "unlink",
                    "Set " + text(neighbour->value) + "'s " + (atFront ? "prev" : "next") +
                        " to NIL, so nothing points at the old " + endName + " node any more.",
                    {{node->id, neighbour->id}, {}}, {{"node", node->id}});
    }

    const NodeValue removed{node->id, node->value};
    pool_.destroy(node);  // 'node' dangles from here on and is not used again
    trace.frame(snapshot(), Event::Remove, step + "remove",
                "Free the old " + endName + " node, which held " + text(removed.value) + ".",
                {{removed.nodeId}, {}});

    trace.frame(snapshot(), Event::Done, step + "done",
                size() == 0 ? "Popped " + text(removed.value) + "; the deque is now empty."
                            : "Popped " + text(removed.value) + "; the deque now holds " + nodeCount(size()) + ".");
    return Result<NodeValue>{okStatus(), removed};
}

// peekFront():                                       deque.peekFront.enter
//   if front = NIL: reject EMPTY_STRUCTURE           deque.peekFront.empty
//   value <- front.value                             deque.peekFront.read
//   return value                                     deque.peekFront.done
//
// peekBack() reads rear instead.
Result<NodeValue> Deque::peek(End end, Trace& trace) {
    const bool atFront = (end == End::Front);
    const std::string step = atFront ? "deque.peekFront." : "deque.peekBack.";
    const std::string endName = atFront ? "front" : "rear";

    trace.frame(snapshot(), Event::Begin, step + "enter",
                "Look at the " + endName + " of " + describe(size()) + ".");

    const Node* node = atFront ? front_ : rear_;  // non-owning: the pool owns every node
    if (node == nullptr) {
        return reject<NodeValue>(trace, step + "empty", ErrorCode::EmptyStructure,
                                 "Cannot read the " + endName + ": the deque is empty.");
    }

    const NodeValue seen{node->id, node->value};
    trace.frame(snapshot(), Event::Read, step + "read",
                "The " + endName + " node holds " + text(seen.value) + ".", {{seen.nodeId}, {}});
    trace.frame(snapshot(), Event::Done, step + "done",
                "The " + endName + " value is " + text(seen.value) + "; the deque is unchanged.",
                {{seen.nodeId}, {}});
    return Result<NodeValue>{okStatus(), seen};
}

// clear():                                           deque.clear.enter
//   removed <- 0
//   while front != NIL:
//     node <- front
//     front <- node.next
//     if front = NIL: rear <- NIL
//     else: front.prev <- NIL
//     delete node                                    deque.clear.remove
//     removed <- removed + 1
//   return removed                                   deque.clear.done
Result<int> Deque::clear(Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "deque.clear.enter", "Clear " + describe(size()) + ".");

    int removed = 0;
    while (front_ != nullptr) {
        Node* node = front_;  // non-owning: the pool owns every node
        trace.counters.nodesVisited++;
        front_ = node->next;
        trace.counters.linksChanged++;
        if (front_ == nullptr) {
            rear_ = nullptr;  // never leave rear pointing at a freed node
        } else {
            front_->prev = nullptr;  // nor the new front's prev
        }
        trace.counters.linksChanged++;

        const NodeValue freed{node->id, node->value};
        pool_.destroy(node);  // 'node' dangles from here on and is not used again
        removed = removed + 1;
        trace.frame(snapshot(), Event::Remove, "deque.clear.remove",
                    "Move front past " + text(freed.value) + " and free that node.", {{freed.nodeId}, {}});
    }

    trace.frame(snapshot(), Event::Done, "deque.clear.done",
                removed == 0 ? std::string("The deque was already empty, so nothing was freed.")
                             : "Cleared " + nodeCount(removed) + "; the deque is now empty.");
    return Result<int>{okStatus(), removed};
}

Snapshot Deque::snapshot() const {
    Snapshot copy;
    copy.structure = StructureKind::Deque;
    copy.size = pool_.size();
    copy.nextId = pool_.nextId();
    copy.front = (front_ != nullptr) ? front_->id : kNoNode;
    copy.rear = (rear_ != nullptr) ? rear_->id : kNoNode;
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
void Deque::restore(const Snapshot& snapshot) {
    pool_.reset(snapshot.nextId);
    for (const NodeView& view : snapshot.nodes) {
        pool_.createWithId(view.id, view.value);
    }
    for (const NodeView& view : snapshot.nodes) {
        Node* node = pool_.find(view.id);  // non-owning: the pool owns every node
        node->next = pool_.find(view.next);
        node->prev = pool_.find(view.prev);
    }
    front_ = pool_.find(snapshot.front);
    rear_ = pool_.find(snapshot.rear);
}

}  // namespace dslab
