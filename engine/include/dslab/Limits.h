// Every numeric bound of the engine, in one place
// (docs/operation-contract.md, section 8).
#pragma once

#include <cstddef>

namespace dslab {

// At most this many nodes in one structure.
constexpr int kMaxNodes = 200;

// At most this many frames in one operation's trace. The worst real
// operation needs 2n + 2 = 402 frames, so the cap is never reached.
constexpr int kMaxFramesPerOperation = 512;

// At most this many history snapshots per structure, undo and redo together.
// 14 structures x 50 x 200 nodes is at most 140,000 node copies (a few MB).
constexpr int kMaxHistory = 50;

// The hash table has this many buckets; bucket(value) = value mod kHashBuckets
// (made non-negative). A small prime, so chains are easy to see and to check.
constexpr int kHashBuckets = 7;

// Requests longer than this are rejected with BAD_REQUEST.
constexpr std::size_t kMaxRequestBytes = 1024 * 1024;

}  // namespace dslab
