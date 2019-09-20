//
// Created by michael on 20/9/19.
//

#ifndef PANDORA_PATH_BUILDER_H
#define PANDORA_PATH_BUILDER_H

#include <denovo_discovery/local_assembly.h>
class PathBuilder {
protected:
    LocalAssemblyGraph* graph;
    DfsTree tree;
    const Node* start_node;
    const Node* end_node;
    const uint32_t max_path_length;
    BfsDistanceMap end_node_distances;
    PathBuilder()
        : start_node(nullptr)
        , end_node(nullptr)
        , max_path_length(0)
    {
    }

public:
    PathBuilder(LocalAssemblyGraph* graph, const Node* start_node, const Node* end_node,
        uint32_t max_path_length);
    virtual ~PathBuilder() {}
    virtual bool is_start_and_end_connected() const;
};

#endif // PANDORA_PATH_BUILDER_H
