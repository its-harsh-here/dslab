#!/usr/bin/env python3
"""Writes web/e2e/fixtures/parity-commands.jsonl: the requests that the
native-versus-Wasm test (web/e2e/parity.spec.ts) sends to both builds.

One request per line, exactly as sent. Deterministic (fixed seed), so the file
only changes when this script does. Every deliberately invalid request is
wrapped in an "export" of the same structure before and after it, so the test
can check that it changed nothing, the ID counter included.

Usage: python3 scripts/make-parity-commands.py
"""
import json
import os
import random

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'web', 'e2e', 'fixtures', 'parity-commands.jsonl')
FIXTURES = os.path.join(ROOT, 'engine', 'tests', 'fixtures')

lines = []
request_id = 0


def raw(text):
    lines.append(text)


def send(structure, operation, **args):
    global request_id
    request_id += 1
    raw(json.dumps({"schemaVersion": 1, "requestId": request_id, "structure": structure,
                    "operation": operation, "args": args}, separators=(',', ':')))


def invalid(structure, text_or_operation, **args):
    """A request expected to fail, between two exports of the same structure."""
    send(structure, 'export')
    if text_or_operation.startswith('{') or ' ' in text_or_operation:
        raw(text_or_operation)
    else:
        send(structure, text_or_operation, **args)
    send(structure, 'export')


rng = random.Random(2026)
LINEAR = {'stack': ('push', 'pop'), 'queue': ('enqueue', 'dequeue')}

# Linear structures: random pushes and pops, undo/redo, clear.
for structure, (add, remove) in LINEAR.items():
    for _ in range(80):
        r = rng.random()
        if r < 0.55:
            send(structure, add, value=rng.randint(-100, 100))
        elif r < 0.8:
            send(structure, remove)
        elif r < 0.9:
            send(structure, rng.choice(['undo', 'redo']))
        else:
            send(structure, 'peek' if structure == 'stack' else 'front')
    send(structure, 'clear')
    invalid(structure, remove)                                  # empty
    invalid(structure, add, value=2147483648)                   # out of range (protocol)
    invalid(structure, add)                                     # missing argument
    invalid(structure, 'redo')                                  # nothing to redo after clear

# Singly linked list.
for _ in range(100):
    r = rng.random()
    if r < 0.5:
        send('list', 'insertAt', index=rng.randint(0, 6), value=rng.randint(0, 9))
    elif r < 0.7:
        send('list', 'deleteNode', nodeId=rng.randint(1, 60))
    elif r < 0.85:
        send('list', 'search', value=rng.randint(0, 9))
    elif r < 0.92:
        send('list', 'reverse')
    else:
        send('list', rng.choice(['undo', 'redo']))
invalid('list', 'insertAt', index=-1, value=1)
invalid('list', 'deleteNode', nodeId=99999)
invalid('list', 'insertAt', index=0, value="1")

# Ordinary binary tree: a root, then children under random existing IDs.
send('binaryTree', 'addRoot', value=1)
for i in range(2, 60):
    send('binaryTree', 'addChild', parentId=rng.randint(1, i - 1), side=rng.choice(['left', 'right']), value=i)
for order in ['preorder', 'inorder', 'postorder', 'levelorder']:
    send('binaryTree', order)
send('binaryTree', 'deleteSubtree', nodeId=2)
invalid('binaryTree', 'addRoot', value=5)                       # root exists
invalid('binaryTree', 'addChild', parentId=99999, side='left', value=1)
invalid('binaryTree', 'addChild', parentId=1, side='middle', value=1)

# Search trees: random inserts and deletes, searches, traversals, undo/redo.
for structure in ['bst', 'avl', 'redBlack']:
    for _ in range(140):
        r = rng.random()
        value = rng.randint(0, 80)
        if r < 0.5:
            send(structure, 'insert', value=value)
        elif r < 0.8:
            send(structure, 'delete', value=value)
        elif r < 0.9:
            send(structure, 'search', value=value)
        elif r < 0.95:
            send(structure, rng.choice(['inorder', 'levelorder']))
        else:
            send(structure, rng.choice(['undo', 'redo']))
    invalid(structure, 'delete', value=12345)                   # absent
    invalid(structure, 'insert', value=None)                    # wrong type (protocol)
    invalid(structure, 'import', text='{')                      # not JSON

# Imports of the C++ test fixtures: valid ones succeed, invalid ones change nothing.
for name in sorted(os.listdir(FIXTURES)):
    structure = name.split('-')[0]
    with open(os.path.join(FIXTURES, name)) as f:
        text = f.read()
    if name.endswith('-valid.json'):
        send(structure, 'import', text=text)
        send(structure, 'export')
        send(structure, 'undo')
        send(structure, 'redo')
    else:
        invalid(structure, 'import', text=text)

# The worst real response: a 200-node red-black tree and its level-order traversal.
send('redBlack', 'clear')
for value in range(200):
    send('redBlack', 'insert', value=value)
invalid('redBlack', 'insert', value=1000)                       # capacity
send('redBlack', 'levelorder')

# A degenerate 200-node BST (one long right spine): the traversals and clear
# recurse 200 deep, which checks the Wasm stack (contract section 8).
send('bst', 'clear')
for value in range(200):
    send('bst', 'insert', value=value)
for order in ['preorder', 'inorder', 'postorder']:
    send('bst', order)
send('bst', 'search', value=199)
send('bst', 'clear')

# The structures added after milestone 9. They come after everything above,
# so the earlier requests stay exactly as they were.

# Deque: both ends.
for _ in range(90):
    r = rng.random()
    if r < 0.5:
        send('deque', rng.choice(['pushFront', 'pushBack']), value=rng.randint(-100, 100))
    elif r < 0.78:
        send('deque', rng.choice(['popFront', 'popBack']))
    elif r < 0.9:
        send('deque', rng.choice(['peekFront', 'peekBack']))
    else:
        send('deque', rng.choice(['undo', 'redo']))
send('deque', 'clear')
invalid('deque', 'popFront')                                    # empty
invalid('deque', 'peekBack')                                    # empty
invalid('deque', 'pushBack', value=-2147483649)                 # out of range (protocol)
invalid('deque', 'pushFront')                                   # missing argument

# Doubly linked list.
for _ in range(110):
    r = rng.random()
    if r < 0.5:
        send('doublyList', 'insertAt', index=rng.randint(0, 8), value=rng.randint(0, 9))
    elif r < 0.7:
        send('doublyList', 'deleteNode', nodeId=rng.randint(1, 60))
    elif r < 0.85:
        send('doublyList', 'search', value=rng.randint(0, 9))
    elif r < 0.92:
        send('doublyList', 'reverse')
    else:
        send('doublyList', rng.choice(['undo', 'redo']))
invalid('doublyList', 'insertAt', index=-1, value=1)
invalid('doublyList', 'deleteNode', nodeId=99999)
invalid('doublyList', 'insertAt', index=0, value=1.5)

# Circular list.
for _ in range(110):
    r = rng.random()
    if r < 0.45:
        send('circularList', rng.choice(['insertFront', 'insertBack']), value=rng.randint(0, 9))
    elif r < 0.62:
        send('circularList', 'deleteNode', nodeId=rng.randint(1, 50))
    elif r < 0.78:
        send('circularList', 'search', value=rng.randint(0, 9))
    elif r < 0.92:
        send('circularList', 'rotate')
    else:
        send('circularList', rng.choice(['undo', 'redo']))
invalid('circularList', 'deleteNode', nodeId=99999)
invalid('circularList', 'rotate', value=1)                      # unknown argument

# Heaps.
for structure in ['minHeap', 'maxHeap']:
    for _ in range(120):
        r = rng.random()
        if r < 0.55:
            send(structure, 'insert', value=rng.randint(-50, 50))
        elif r < 0.82:
            send(structure, 'extract')
        elif r < 0.92:
            send(structure, 'peek')
        else:
            send(structure, rng.choice(['undo', 'redo']))
    send(structure, 'export')
    send(structure, 'clear')
    invalid(structure, 'extract')                               # empty
    invalid(structure, 'peek')                                  # empty
    invalid(structure, 'delete', value=1)                       # not a heap operation
    # A file whose order is wrong for this heap: a child beats its parent.
    low, high = (1, 9) if structure == 'maxHeap' else (9, 1)
    invalid(structure, 'import', text=json.dumps({
        "format": "dslab-structure", "formatVersion": 1, "nextId": 3,
        "snapshot": {"structure": structure, "size": 2, "root": 1, "nodes": [
            {"id": 1, "value": low, "index": 0, "left": 2, "right": None, "parent": None},
            {"id": 2, "value": high, "index": 1, "left": None, "right": None, "parent": 1}]}}))

# Splay tree: searches change the tree too.
for _ in range(160):
    r = rng.random()
    value = rng.randint(0, 60)
    if r < 0.42:
        send('splay', 'insert', value=value)
    elif r < 0.64:
        send('splay', 'delete', value=value)
    elif r < 0.88:
        send('splay', 'search', value=value)
    elif r < 0.94:
        send('splay', rng.choice(['inorder', 'levelorder']))
    else:
        send('splay', rng.choice(['undo', 'redo']))
invalid('splay', 'delete', value=12345)                         # absent
invalid('splay', 'insert', value=None)                          # wrong type (protocol)

# A 200-node splay tree built from sorted inserts is one long spine: searching
# its deepest value is the longest splay there is (199 rotations).
send('splay', 'clear')
for value in range(200):
    send('splay', 'insert', value=value)
send('splay', 'search', value=0)
send('splay', 'search', value=199)
invalid('splay', 'insert', value=1000)                          # capacity
send('splay', 'clear')

# Hash table: negative values too, and a bucket holding every value.
for _ in range(160):
    r = rng.random()
    value = rng.randint(-40, 40)
    if r < 0.5:
        send('hashTable', 'insert', value=value)
    elif r < 0.72:
        send('hashTable', 'delete', value=value)
    elif r < 0.92:
        send('hashTable', 'search', value=value)
    else:
        send('hashTable', rng.choice(['undo', 'redo']))
send('hashTable', 'export')
invalid('hashTable', 'delete', value=12345)                     # absent
invalid('hashTable', 'insert', value="7")                       # wrong type (protocol)
send('hashTable', 'clear')
for value in range(0, 1400, 7):                                 # 200 values, all in bucket 0
    send('hashTable', 'insert', value=value)
send('hashTable', 'search', value=0)
invalid('hashTable', 'insert', value=7)                         # full (capacity comes before duplicate)
send('hashTable', 'insert', value=-2147483648)                  # rejected: full
send('hashTable', 'clear')
send('hashTable', 'insert', value=-2147483648)                  # bucket 5
send('hashTable', 'insert', value=2147483647)

# Protocol errors that never reach a structure.
for text in ['not json', '[1,2]', '{"schemaVersion":2,"requestId":1,"structure":"stack","operation":"pop","args":{}}',
             '{"schemaVersion":1,"requestId":1,"structure":"heap","operation":"pop","args":{}}',
             '{"schemaVersion":1,"requestId":1,"structure":"stack","operation":"sort","args":{}}']:
    raw(text)

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, 'w') as f:
    f.write('\n'.join(lines) + '\n')
print(f'wrote {len(lines)} requests to {os.path.relpath(OUT, ROOT)}')
