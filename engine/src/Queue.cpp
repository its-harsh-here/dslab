#include "dslab/Queue.h"

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

// "an empty queue" or "a queue that holds 3 nodes".
std::string describe(int size) {
    return size == 0 ? "an empty queue" : "a queue that holds " + nodeCount(size);
}

}  // namespace

// enqueue(value):                                queue.enqueue.enter
//   if size = 200: reject CAPACITY_EXCEEDED      queue.enqueue.full
//   node <- new Node(value)                      queue.enqueue.create
//   if rear = NIL: front <- node                 queue.enqueue.linkFront
//   else: rear.next <- node                      queue.enqueue.linkRear
//   rear <- node                                 queue.enqueue.moveRear
//   return node                                  queue.enqueue.done
Result<NodeId> Queue::enqueue(Value value, Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "queue.enqueue.enter",
                "Enqueue " + text(value) + " at the back of " + describe(size()) + ".");

    if (size() == kMaxNodes) {
        return reject<NodeId>(trace, "queue.enqueue.full", ErrorCode::CapacityExceeded,
                              "The queue already holds 200 nodes, the maximum.");
    }
    if (!pool_.hasFreeId()) {
        return reject<NodeId>(trace, "queue.enqueue.full", ErrorCode::IdExhausted,
                              "No node IDs are left for this queue.");
    }

    Node* node = pool_.create(value);  // non-owning: the pool owns every node
    trace.frame(snapshot(), Event::Create, "queue.enqueue.create",
                "Create a new node holding " + text(value) + "; its next is NIL.", {{node->id}, {}},
                {{"node", node->id}});

    if (rear_ == nullptr) {
        front_ = node;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), Event::Link, "queue.enqueue.linkFront",
                    "The queue was empty, so front points to the new node.", {{node->id}, {}},
                    {{"node", node->id}});
    } else {
        rear_->next = node;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), Event::Link, "queue.enqueue.linkRear",
                    "Link the current rear, " + text(rear_->value) + ", to the new node.",
                    {{rear_->id, node->id}, {{rear_->id, node->id}}}, {{"node", node->id}});
    }

    rear_ = node;
    trace.counters.linksChanged++;
    trace.frame(snapshot(), Event::Link, "queue.enqueue.moveRear",
                "Move rear to the new node, so " + text(value) + " is now at the back.",
                {{node->id}, {}}, {{"node", node->id}});

    trace.frame(snapshot(), Event::Done, "queue.enqueue.done",
                "Enqueued " + text(value) + "; the queue now holds " + nodeCount(size()) + ".",
                {{node->id}, {}});
    return Result<NodeId>{okStatus(), node->id};
}

// dequeue():                                     queue.dequeue.enter
//   if front = NIL: reject EMPTY_STRUCTURE       queue.dequeue.empty
//   node <- front                                queue.dequeue.take
//   front <- node.next                           queue.dequeue.moveFront
//   if front = NIL: rear <- NIL                  queue.dequeue.clearRear
//   value <- node.value
//   delete node                                  queue.dequeue.remove
//   return value                                 queue.dequeue.done
Result<NodeValue> Queue::dequeue(Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "queue.dequeue.enter",
                "Dequeue from the front of " + describe(size()) + ".");

    if (front_ == nullptr) {
        return reject<NodeValue>(trace, "queue.dequeue.empty", ErrorCode::EmptyStructure,
                                 "Cannot dequeue: the queue is empty.");
    }

    Node* node = front_;  // non-owning: the pool owns every node
    trace.counters.nodesVisited++;
    trace.frame(snapshot(), Event::Move, "queue.dequeue.take",
                "Take the front node, which holds " + text(node->value) + ".", {{node->id}, {}},
                {{"node", node->id}});

    front_ = node->next;
    trace.counters.linksChanged++;
    if (front_ != nullptr) {
        trace.frame(snapshot(), Event::Link, "queue.dequeue.moveFront",
                    "Move front to the next node, " + text(front_->value) + ".",
                    {{node->id, front_->id}, {{node->id, front_->id}}}, {{"node", node->id}});
    } else {
        trace.frame(snapshot(), Event::Unlink, "queue.dequeue.moveFront",
                    "Move front to the next node, which is NIL.", {{node->id}, {}},
                    {{"node", node->id}});
        rear_ = nullptr;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), Event::Unlink, "queue.dequeue.clearRear",
                    "The queue is now empty, so rear becomes NIL too.", {{node->id}, {}},
                    {{"node", node->id}});
    }

    const NodeValue removed{node->id, node->value};
    pool_.destroy(node);  // 'node' dangles from here on and is not used again
    trace.frame(snapshot(), Event::Remove, "queue.dequeue.remove",
                "Free the old front node, which held " + text(removed.value) + ".",
                {{removed.nodeId}, {}});

    trace.frame(snapshot(), Event::Done, "queue.dequeue.done",
                size() == 0 ? "Dequeued " + text(removed.value) + "; the queue is now empty."
                            : "Dequeued " + text(removed.value) + "; the queue now holds " +
                                  nodeCount(size()) + ".");
    return Result<NodeValue>{okStatus(), removed};
}

// front():                                       queue.front.enter
//   if front = NIL: reject EMPTY_STRUCTURE       queue.front.empty
//   value <- front.value                         queue.front.read
//   return value                                 queue.front.done
Result<NodeValue> Queue::front(Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "queue.front.enter",
                "Look at the front of " + describe(size()) + ".");

    if (front_ == nullptr) {
        return reject<NodeValue>(trace, "queue.front.empty", ErrorCode::EmptyStructure,
                                 "Cannot read the front: the queue is empty.");
    }

    const NodeValue first{front_->id, front_->value};
    trace.frame(snapshot(), Event::Read, "queue.front.read",
                "The front node holds " + text(first.value) + ".", {{first.nodeId}, {}});

    trace.frame(snapshot(), Event::Done, "queue.front.done",
                "The front value is " + text(first.value) + "; the queue is unchanged.",
                {{first.nodeId}, {}});
    return Result<NodeValue>{okStatus(), first};
}

// clear():                                       queue.clear.enter
//   removed <- 0
//   while front != NIL:
//     node <- front
//     front <- node.next
//     if front = NIL: rear <- NIL
//     delete node                                queue.clear.remove
//     removed <- removed + 1
//   return removed                               queue.clear.done
Result<int> Queue::clear(Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "queue.clear.enter", "Clear " + describe(size()) + ".");

    int removed = 0;
    while (front_ != nullptr) {
        Node* node = front_;  // non-owning: the pool owns every node
        trace.counters.nodesVisited++;
        front_ = node->next;
        trace.counters.linksChanged++;
        if (front_ == nullptr) {
            rear_ = nullptr;  // never leave rear pointing at a freed node
            trace.counters.linksChanged++;
        }

        const NodeValue freed{node->id, node->value};
        pool_.destroy(node);  // 'node' dangles from here on and is not used again
        removed = removed + 1;
        trace.frame(snapshot(), Event::Remove, "queue.clear.remove",
                    "Move front past " + text(freed.value) + " and free that node.",
                    {{freed.nodeId}, {}});
    }

    trace.frame(snapshot(), Event::Done, "queue.clear.done",
                removed == 0 ? std::string("The queue was already empty, so nothing was freed.")
                             : "Cleared " + nodeCount(removed) + "; the queue is now empty.");
    return Result<int>{okStatus(), removed};
}

Snapshot Queue::snapshot() const {
    Snapshot copy;
    copy.structure = StructureKind::Queue;
    copy.size = pool_.size();
    copy.front = (front_ != nullptr) ? front_->id : kNoNode;
    copy.rear = (rear_ != nullptr) ? rear_->id : kNoNode;
    for (const std::unique_ptr<Node>& node : pool_.nodes()) {
        const NodeId next = (node->next != nullptr) ? node->next->id : kNoNode;
        copy.nodes.push_back(NodeView{node->id, node->value, next});
    }
    return copy;
}

}  // namespace dslab
