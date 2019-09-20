#include "denovo_discovery/path_builder.h"

PathBuilder::PathBuilder(LocalAssemblyGraph* graph, const Node* start_node,
    const Node* end_node, uint32_t max_path_length)
    : graph(graph)
    , tree(graph->depth_first_search_from(*start_node))
    , start_node(start_node)
    , end_node(end_node)
    , max_path_length(max_path_length)
{
    if (this->is_start_and_end_connected()) {
        end_node_distances
            = this->graph->breadth_first_search_from(*this->end_node, true);
    }
}

bool PathBuilder::is_start_and_end_connected() const
{
    return this->tree.find(this->graph->toString(*this->end_node)) == this->tree.end();
}