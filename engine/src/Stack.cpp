#include "dslab/Stack.h"

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

// "an empty stack" or "a stack that holds 3 nodes".
std::string describe(int size) {
    return size == 0 ? "an empty stack" : "a stack that holds " + nodeCount(size);
}

}  // namespace

// push(value):                                   stack.push.enter
//   if size = 200: reject CAPACITY_EXCEEDED      stack.push.full
//   node <- new Node(value)                      stack.push.create
//   node.next <- top                             stack.push.linkNext
//   top <- node                                  stack.push.moveTop
//   return node                                  stack.push.done
Result<NodeId> Stack::push(Value value, Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "stack.push.enter",
                "Push " + text(value) + " onto " + describe(size()) + ".");

    if (size() == kMaxNodes) {
        return reject<NodeId>(trace, "stack.push.full", ErrorCode::CapacityExceeded,
                              "The stack already holds 200 nodes, the maximum.");
    }
    if (!pool_.hasFreeId()) {
        return reject<NodeId>(trace, "stack.push.full", ErrorCode::IdExhausted,
                              "No node IDs are left for this stack.");
    }

    Node* node = pool_.create(value);  // non-owning: the pool owns every node
    trace.frame(snapshot(), Event::Create, "stack.push.create",
                "Create a new node holding " + text(value) + ".", {{node->id}, {}},
                {{"node", node->id}});

    node->next = top_;
    trace.counters.linksChanged++;
    if (top_ != nullptr) {
        trace.frame(snapshot(), Event::Link, "stack.push.linkNext",
                    "Point the new node's next at the current top, " + text(top_->value) + ".",
                    {{node->id, top_->id}, {{node->id, top_->id}}}, {{"node", node->id}});
    } else {
        trace.frame(snapshot(), Event::Unlink, "stack.push.linkNext",
                    "Point the new node's next at the current top, which is NIL.",
                    {{node->id}, {}}, {{"node", node->id}});
    }

    top_ = node;
    trace.counters.linksChanged++;
    trace.frame(snapshot(), Event::Link, "stack.push.moveTop",
                "Move top to the new node, so " + text(value) + " is now on top.",
                {{node->id}, {}}, {{"node", node->id}});

    trace.frame(snapshot(), Event::Done, "stack.push.done",
                "Pushed " + text(value) + "; the stack now holds " + nodeCount(size()) + ".",
                {{node->id}, {}});
    return Result<NodeId>{okStatus(), node->id};
}

// pop():                                         stack.pop.enter
//   if top = NIL: reject EMPTY_STRUCTURE         stack.pop.empty
//   node <- top                                  stack.pop.take
//   top <- node.next                             stack.pop.moveTop
//   value <- node.value
//   delete node                                  stack.pop.remove
//   return value                                 stack.pop.done
Result<NodeValue> Stack::pop(Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "stack.pop.enter", "Pop from " + describe(size()) + ".");

    if (top_ == nullptr) {
        return reject<NodeValue>(trace, "stack.pop.empty", ErrorCode::EmptyStructure,
                                 "Cannot pop: the stack is empty.");
    }

    Node* node = top_;  // non-owning: the pool owns every node
    trace.counters.nodesVisited++;
    trace.frame(snapshot(), Event::Move, "stack.pop.take",
                "Take the top node, which holds " + text(node->value) + ".", {{node->id}, {}},
                {{"node", node->id}});

    top_ = node->next;
    trace.counters.linksChanged++;
    if (top_ != nullptr) {
        trace.frame(snapshot(), Event::Link, "stack.pop.moveTop",
                    "Move top to the next node, " + text(top_->value) + ".",
                    {{node->id, top_->id}, {{node->id, top_->id}}}, {{"node", node->id}});
    } else {
        trace.frame(snapshot(), Event::Unlink, "stack.pop.moveTop",
                    "Move top to the next node, which is NIL.", {{node->id}, {}},
                    {{"node", node->id}});
    }

    const NodeValue removed{node->id, node->value};
    pool_.destroy(node);  // 'node' dangles from here on and is not used again
    trace.frame(snapshot(), Event::Remove, "stack.pop.remove",
                "Free the old top node, which held " + text(removed.value) + ".",
                {{removed.nodeId}, {}});

    trace.frame(snapshot(), Event::Done, "stack.pop.done",
                size() == 0 ? "Popped " + text(removed.value) + "; the stack is now empty."
                            : "Popped " + text(removed.value) + "; the stack now holds " +
                                  nodeCount(size()) + ".");
    return Result<NodeValue>{okStatus(), removed};
}

// peek():                                        stack.peek.enter
//   if top = NIL: reject EMPTY_STRUCTURE         stack.peek.empty
//   value <- top.value                           stack.peek.read
//   return value                                 stack.peek.done
Result<NodeValue> Stack::peek(Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "stack.peek.enter",
                "Peek at the top of " + describe(size()) + ".");

    if (top_ == nullptr) {
        return reject<NodeValue>(trace, "stack.peek.empty", ErrorCode::EmptyStructure,
                                 "Cannot peek: the stack is empty.");
    }

    const NodeValue top{top_->id, top_->value};
    trace.frame(snapshot(), Event::Read, "stack.peek.read",
                "The top node holds " + text(top.value) + ".", {{top.nodeId}, {}});

    trace.frame(snapshot(), Event::Done, "stack.peek.done",
                "Peeked " + text(top.value) + "; the stack is unchanged.", {{top.nodeId}, {}});
    return Result<NodeValue>{okStatus(), top};
}

// clear():                                       stack.clear.enter
//   removed <- 0
//   while top != NIL:
//     node <- top
//     top <- node.next
//     delete node                                stack.clear.remove
//     removed <- removed + 1
//   return removed                               stack.clear.done
Result<int> Stack::clear(Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "stack.clear.enter", "Clear " + describe(size()) + ".");

    int removed = 0;
    while (top_ != nullptr) {
        Node* node = top_;  // non-owning: the pool owns every node
        trace.counters.nodesVisited++;
        top_ = node->next;
        trace.counters.linksChanged++;

        const NodeValue freed{node->id, node->value};
        pool_.destroy(node);  // 'node' dangles from here on and is not used again
        removed = removed + 1;
        trace.frame(snapshot(), Event::Remove, "stack.clear.remove",
                    "Move top past " + text(freed.value) + " and free that node.",
                    {{freed.nodeId}, {}});
    }

    trace.frame(snapshot(), Event::Done, "stack.clear.done",
                removed == 0 ? std::string("The stack was already empty, so nothing was freed.")
                             : "Cleared " + nodeCount(removed) + "; the stack is now empty.");
    return Result<int>{okStatus(), removed};
}

Snapshot Stack::snapshot() const {
    Snapshot copy;
    copy.structure = StructureKind::Stack;
    copy.size = pool_.size();
    copy.nextId = pool_.nextId();
    copy.top = (top_ != nullptr) ? top_->id : kNoNode;
    for (const std::unique_ptr<Node>& node : pool_.nodes()) {
        const NodeId next = (node->next != nullptr) ? node->next->id : kNoNode;
        copy.nodes.push_back(NodeView{node->id, node->value, next});
    }
    return copy;
}

// restore(snapshot): create every node with its ID and value, then set the
// links. pool_.find(kNoNode) is nullptr, which is how NIL is stored here.
void Stack::restore(const Snapshot& snapshot) {
    pool_.reset(snapshot.nextId);
    for (const NodeView& view : snapshot.nodes) {
        pool_.createWithId(view.id, view.value);
    }
    for (const NodeView& view : snapshot.nodes) {
        pool_.find(view.id)->next = pool_.find(view.next);
    }
    top_ = pool_.find(snapshot.top);
}

}  // namespace dslab
