#ifndef __PANREAD_H_INCLUDED__   // if panread.h hasn't been included yet...
#define __PANREAD_H_INCLUDED__

#include <vector>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include "minihits.h"
#include "pangenome/ns.cpp"

class pangenome::Read {
public:
    const uint32_t id; // corresponding the the read id
    vector<NodePtr> nodes;
    vector<bool> node_orientations;
    std::unordered_map<uint32_t,std::set<MinimizerHitPtr, pComp_path>> hits; // from node id to cluster of hits against that node in this read

    Read(const uint32_t);
    void add_hits(const uint32_t, const std::set<MinimizerHitPtr, pComp>&);

    uint find_position(const vector<uint16_t>&, const vector<bool>&, const uint16_t min_overlap=1);

    void remove_node(NodePtr);
    void remove_node(vector<NodePtr>::iterator);
    // remove nodes
    void replace_node(vector<NodePtr>::iterator, NodePtr);
    // replace nodes

    bool operator == (const Read& y) const;
    bool operator != (const Read& y) const;
    bool operator < (const Read& y) const;

    friend std::ostream& operator<< (std::ostream& out, const Read& r);
    friend class pangenome::Graph;
    friend class pangenome::Node;
};

#endif
