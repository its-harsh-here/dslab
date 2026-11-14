#include "dslab/BinaryHeap.h"

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

int parentOf(int i) {
    return (i - 1) / 2;
}

int leftOf(int i) {
    return 2 * i + 1;
}

int rightOf(int i) {
    return 2 * i + 2;
}

}  // namespace

BinaryHeap::BinaryHeap(StructureKind kind)
    : kind_(kind),
      isMin_(kind == StructureKind::MinHeap),
      prefix_(isMin_ ? "minHeap" : "maxHeap"),
      name_(isMin_ ? "min-heap" : "max-heap") {}

void BinaryHeap::swapSlots(int i, int j, Trace& trace) {
    Node* moved = slots_[i];  // non-owning: the pool owns every node
    slots_[i] = slots_[j];
    slots_[j] = moved;
    slots_[i]->index = i;
    slots_[j]->index = j;
    trace.counters.linksChanged += 2;  // two array slots were written
}

NodeId BinaryHeap::idAt(int slot) const {
    return slot < size() ? slots_[slot]->id : kNoNode;
}

// insert(value):                                         minHeap.insert.enter
//   if size = 200: reject CAPACITY_EXCEEDED              minHeap.insert.full
//   i <- size; A[i] <- new Node(value)                   minHeap.insert.append
//   while i > 0:                              (sift up)
//     p <- (i - 1) / 2
//     if not A[i] < A[p]: stop                           minHeap.insert.compare
//     swap A[i] and A[p]; i <- p                         minHeap.insert.swap
//   return the node                                      minHeap.insert.done
//
// A max-heap is the same with ">" for "<" (step IDs start with maxHeap).
Result<NodeId> BinaryHeap::insert(Value value, Trace& trace) {
    const std::string step = prefix_ + ".insert.";
    trace.frame(snapshot(), Event::Begin, step + "enter",
                "Insert " + text(value) + " into " +
                    (size() == 0 ? "an empty " + name_ : "a " + name_ + " that holds " + nodeCount(size())) + ".");

    if (size() == kMaxNodes) {
        return reject<NodeId>(trace, step + "full", ErrorCode::CapacityExceeded,
                              "The " + name_ + " already holds 200 nodes, the maximum.");
    }
    if (!pool_.hasFreeId()) {
        return reject<NodeId>(trace, step + "full", ErrorCode::IdExhausted,
                              "No node IDs are left for this " + name_ + ".");
    }

    Node* node = pool_.create(value);  // non-owning: the pool owns every node
    int i = size() - 1;
    node->index = i;
    slots_.push_back(node);
    trace.counters.linksChanged++;
    trace.frame(snapshot(), Event::Create, step + "append",
                "Put " + text(value) + " in the first free slot, " + std::to_string(i) +
                    ": the next leaf, left to right.",
                {{node->id}, {}}, {{"i", node->id}});

    while (i > 0) {
        const int p = parentOf(i);
        Node* parent = slots_[p];  // non-owning: the pool owns every node
        trace.counters.comparisons++;
        const bool rises = goesAbove(node->value, parent->value);
        const std::string relation = text(node->value) + (rises ? (isMin_ ? " < " : " > ") : (isMin_ ? " >= " : " <= ")) +
                                     text(parent->value);
        trace.frame(snapshot(), Event::Compare, step + "compare",
                    "Compare with the parent in slot " + std::to_string(p) + ": " + relation +
                        (rises ? ", so they must swap." : ", so the heap order holds here."),
                    {{node->id, parent->id}, {{parent->id, node->id}}}, {{"i", node->id}, {"parent", parent->id}});
        if (!rises) {
            break;
        }
        swapSlots(i, p, trace);
        i = p;
        trace.counters.nodesVisited++;
        trace.frame(snapshot(), Event::Swap, step + "swap",
                    "Swap them: " + text(node->value) + " moves up to slot " + std::to_string(i) + " and " +
                        text(parent->value) + " moves down.",
                    {{node->id, parent->id}, {{node->id, parent->id}}}, {{"i", node->id}});
    }

    trace.frame(snapshot(), Event::Done, step + "done",
                "Inserted " + text(value) + " in slot " + std::to_string(i) + "; the " + name_ + " now holds " +
                    nodeCount(size()) + ".",
                {{node->id}, {}});
    return Result<NodeId>{okStatus(), node->id};
}

// extract():                                             minHeap.extract.enter
//   if size = 0: reject EMPTY_STRUCTURE                  minHeap.extract.empty
//   top <- A[0]                                          minHeap.extract.take
//   if size > 1: swap A[0] and A[size - 1]               minHeap.extract.moveLast
//   remove the last slot; delete top                     minHeap.extract.remove
//   i <- 0
//   loop:                                     (sift down)
//     c <- the child of i with the smaller value; if i has no child: stop
//     if not A[c] < A[i]: stop                           minHeap.extract.compare
//     swap A[i] and A[c]; i <- c                         minHeap.extract.swap
//   return top.value                                     minHeap.extract.done
//
// A max-heap takes the larger child and uses ">" (step IDs start with maxHeap).
Result<NodeValue> BinaryHeap::extract(Trace& trace) {
    const std::string step = prefix_ + ".extract.";
    const std::string extreme = isMin_ ? "smallest" : "largest";
    trace.frame(snapshot(), Event::Begin, step + "enter",
                "Extract the " + extreme + " value from " +
                    (size() == 0 ? "an empty " + name_ : "a " + name_ + " that holds " + nodeCount(size())) + ".");

    if (size() == 0) {
        return reject<NodeValue>(trace, step + "empty", ErrorCode::EmptyStructure,
                                 "Cannot extract: the " + name_ + " is empty.");
    }

    Node* top = slots_[0];  // non-owning: the pool owns every node
    trace.counters.nodesVisited++;
    trace.frame(snapshot(), Event::Read, step + "take",
                "The root, slot 0, holds the " + extreme + " value, " + text(top->value) + ".", {{top->id}, {}},
                {{"top", top->id}});

    const int last = size() - 1;
    if (last > 0) {
        Node* mover = slots_[last];  // non-owning: the pool owns every node
        swapSlots(0, last, trace);
        trace.frame(snapshot(), Event::Swap, step + "moveLast",
                    "Swap the root with the last node, " + text(mover->value) + " in slot " + std::to_string(last) +
                        ", so the value to remove is now a leaf.",
                    {{top->id, mover->id}, {}}, {{"top", top->id}, {"i", mover->id}});
    }

    const NodeValue removed{top->id, top->value};
    slots_.pop_back();
    trace.counters.linksChanged++;
    pool_.destroy(top);  // 'top' dangles from here on and is not used again
    trace.frame(snapshot(), Event::Remove, step + "remove",
                "Remove the last slot, which held " + text(removed.value) + ", and free its node.",
                {{removed.nodeId}, {}});

    int i = 0;
    while (leftOf(i) < size()) {
        Node* node = slots_[i];  // non-owning: the pool owns every node
        int c = leftOf(i);
        if (rightOf(i) < size()) {
            trace.counters.comparisons++;
            if (goesAbove(slots_[rightOf(i)]->value, slots_[c]->value)) {
                c = rightOf(i);
            }
        }
        Node* child = slots_[c];  // non-owning: the pool owns every node
        trace.counters.comparisons++;
        const bool sinks = goesAbove(child->value, node->value);
        const std::string which = rightOf(i) < size() ? (isMin_ ? "smaller child" : "larger child") : "only child";
        trace.frame(snapshot(), Event::Compare, step + "compare",
                    "The " + which + " of " + text(node->value) + " is " + text(child->value) +
                        (sinks ? ", which must be above it: swap." : ", so the heap order holds here."),
                    {{node->id, child->id}, {{node->id, child->id}}}, {{"i", node->id}, {"child", child->id}});
        if (!sinks) {
            break;
        }
        swapSlots(i, c, trace);
        i = c;
        trace.counters.nodesVisited++;
        trace.frame(snapshot(), Event::Swap, step + "swap",
                    "Swap them: " + text(node->value) + " moves down to slot " + std::to_string(i) + " and " +
                        text(child->value) + " moves up.",
                    {{node->id, child->id}, {{child->id, node->id}}}, {{"i", node->id}});
    }

    trace.frame(snapshot(), Event::Done, step + "done",
                size() == 0 ? "Extracted " + text(removed.value) + "; the " + name_ + " is now empty."
                            : "Extracted " + text(removed.value) + "; the " + name_ + " now holds " +
                                  nodeCount(size()) + ".");
    return Result<NodeValue>{okStatus(), removed};
}

// peek():                                                minHeap.peek.enter
//   if size = 0: reject EMPTY_STRUCTURE                  minHeap.peek.empty
//   value <- A[0].value                                  minHeap.peek.read
//   return value                                         minHeap.peek.done
Result<NodeValue> BinaryHeap::peek(Trace& trace) const {
    const std::string step = prefix_ + ".peek.";
    const std::string extreme = isMin_ ? "smallest" : "largest";
    trace.frame(snapshot(), Event::Begin, step + "enter",
                "Look at the " + extreme + " value of " +
                    (size() == 0 ? "an empty " + name_ : "a " + name_ + " that holds " + nodeCount(size())) + ".");

    if (size() == 0) {
        return reject<NodeValue>(trace, step + "empty", ErrorCode::EmptyStructure,
                                 "Cannot peek: the " + name_ + " is empty.");
    }

    const NodeValue top{slots_[0]->id, slots_[0]->value};
    trace.frame(snapshot(), Event::Read, step + "read",
                "The root, slot 0, holds " + text(top.value) + ": no search is needed.", {{top.nodeId}, {}});
    trace.frame(snapshot(), Event::Done, step + "done",
                "The " + extreme + " value is " + text(top.value) + "; the " + name_ + " is unchanged.",
                {{top.nodeId}, {}});
    return Result<NodeValue>{okStatus(), top};
}

// clear():                                               minHeap.clear.enter
//   while size > 0:
//     remove the last slot and delete its node           minHeap.clear.remove
//   return removed                                       minHeap.clear.done
//
// Removing the last leaf never breaks the heap order, so no sifting is needed.
Result<int> BinaryHeap::clear(Trace& trace) {
    const std::string step = prefix_ + ".clear.";
    trace.frame(snapshot(), Event::Begin, step + "enter",
                "Clear " + (size() == 0 ? "an empty " + name_ : "a " + name_ + " that holds " + nodeCount(size())) + ".");

    int removed = 0;
    while (size() > 0) {
        Node* node = slots_.back();  // non-owning: the pool owns every node
        trace.counters.nodesVisited++;
        const NodeValue freed{node->id, node->value};
        const int slot = node->index;
        slots_.pop_back();
        trace.counters.linksChanged++;
        pool_.destroy(node);  // 'node' dangles from here on and is not used again
        removed = removed + 1;
        trace.frame(snapshot(), Event::Remove, step + "remove",
                    "Remove the last slot, " + std::to_string(slot) + ", which held " + text(freed.value) + ".",
                    {{freed.nodeId}, {}});
    }

    trace.frame(snapshot(), Event::Done, step + "done",
                removed == 0 ? "The " + name_ + " was already empty, so nothing was freed."
                             : "Cleared " + nodeCount(removed) + "; the " + name_ + " is now empty.");
    return Result<int>{okStatus(), removed};
}

// The links are not stored: they follow from the slots (see the header).
Snapshot BinaryHeap::snapshot() const {
    Snapshot copy;
    copy.structure = kind_;
    copy.size = pool_.size();
    copy.nextId = pool_.nextId();
    copy.root = idAt(0);
    for (const std::unique_ptr<Node>& node : pool_.nodes()) {
        NodeView view;
        view.id = node->id;
        view.value = node->value;
        view.index = node->index;
        view.left = idAt(leftOf(node->index));
        view.right = idAt(rightOf(node->index));
        view.parent = node->index == 0 ? kNoNode : idAt(parentOf(node->index));
        copy.nodes.push_back(view);
    }
    return copy;
}

// restore(snapshot): create every node, then put each one in its slot.
void BinaryHeap::restore(const Snapshot& snapshot) {
    pool_.reset(snapshot.nextId);
    slots_.assign(snapshot.nodes.size(), nullptr);
    for (const NodeView& view : snapshot.nodes) {
        Node* node = pool_.createWithId(view.id, view.value);  // non-owning: the pool owns every node
        node->index = view.index;
        slots_[static_cast<std::size_t>(view.index)] = node;
    }
}

}  // namespace dslab
