#ifndef __DBNODE_H_INCLUDED__ // if de_bruijn/node.h hasn't been included yet...
#define __DBNODE_H_INCLUDED__

#include <cstdint>
#include <deque>
#include <unordered_set>
#include <memory>
#include "de_bruijn/ns.cpp"

class debruijn::Node {
public:
    uint32_t id;
    std::deque<uint_least32_t> hashed_node_ids;
    std::unordered_multiset<uint32_t> read_ids;
    std::unordered_set<uint32_t> out_nodes;
    std::unordered_set<uint32_t> in_nodes;

    Node(const uint32_t, const std::deque<uint_least32_t>&, const uint32_t);

    bool operator==(const Node& y) const;

    bool operator!=(const Node& y) const;

    friend std::ostream& operator<<(std::ostream&, const Node&);
};

#endif
