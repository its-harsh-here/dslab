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

# Protocol errors that never reach a structure.
for text in ['not json', '[1,2]', '{"schemaVersion":2,"requestId":1,"structure":"stack","operation":"pop","args":{}}',
             '{"schemaVersion":1,"requestId":1,"structure":"heap","operation":"pop","args":{}}',
             '{"schemaVersion":1,"requestId":1,"structure":"stack","operation":"sort","args":{}}']:
    raw(text)

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, 'w') as f:
    f.write('\n'.join(lines) + '\n')
print(f'wrote {len(lines)} requests to {os.path.relpath(OUT, ROOT)}')
