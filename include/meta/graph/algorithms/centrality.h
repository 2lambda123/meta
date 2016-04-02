/**
 * @file centrality.h
 * @author Sean Massung
 *
 * All files in META are dual-licensed under the MIT and NCSA licenses. For more
 * details, consult the file LICENSE.mit and LICENSE.ncsa in the root of the
 * project.
 */

#ifndef META_GRAPH_ALGORITHMS_CENTRALITY_H_
#define META_GRAPH_ALGORITHMS_CENTRALITY_H_

#include <mutex>
#include "meta/graph/undirected_graph.h"
#include "meta/graph/directed_graph.h"

#include <vector>

namespace meta
{
namespace graph
{
namespace algorithms
{
using centrality_result = std::vector<std::pair<node_id, double>>;

/**
 * Find the degree centrality of each node in the graph, which is simply the
 * number of adjacent links.
 * @param g
 * @return a collection of (id, centrality) pairs
 */
template <class Graph>
centrality_result degree_centrality(const Graph& g);

/**
 * Find the PageRank centrality of each node in the graph via power iteration.
 * @see http://ilpubs.stanford.edu:8090/422/1/1999-66.pdf
 * @param g
 * @param damp The dampening (smoothing) factor
 * @param max_iters The maximum number of iterations to run the power iteration
 * @return a collection of (id, centrality) pairs
 */
template <class DirectedGraph>
centrality_result page_rank_centrality(const DirectedGraph& g,
                                       double damp = 0.85,
                                       uint64_t max_iters = 100);

/**
 * Find the Personalized PageRank centrality of each node in the graph via
 * simulation with respect to a particular node.
 * @param g
 * @param center The node that the PageRank is personalized for (i.e., where the
 * random jumping takes the surfer)
 * @param damp The dampening (smoothing) factor, which should usually be lower
 * than the PageRank dampening factor (0.85)
 * @param num_passes The number of times to run through the entire network
 * @return a collection of (id, centrality) pairs
 */
template <class DirectedGraph>
centrality_result personalized_page_rank(const DirectedGraph& g, node_id center,
                                         double damp = 0.66,
                                         uint64_t num_passes = 3.0);

/**
 * Find the betweenness centrality of each node in the graph using the algorithm
 * from Ulrik Brandes, 2001. This function is parallelized as it takes some time
 * to compute on large networks.
 * @see http://www.inf.uni-konstanz.de/algo/publications/b-fabc-01.pdf
 * @param g
 * @return a collection of (id, centrality) pairs
 */
template <class Graph>
centrality_result betweenness_centrality(const Graph& g);

/**
 * Finds the eigenvector centrality of each node (i.e. "prestige") using power
 * iteration.
 * @param g
 * @param max_iters The maximum number of iterations to run the power iteration
 * @return a collection of (id, centrality) pairs
 */
template <class Graph>
centrality_result eigenvector_centrality(const Graph& g,
                                         uint64_t max_iters = 100);

/**
 * Contains helper functions used by the centrality measures.
 */
namespace internal
{
/**
 * Helper function for betweenness_centrality.
 */
template <class Graph>
void betweenness_step(const Graph& g, centrality_result& cb, node_id n,
                      std::mutex& calc_mut);
}
}
}
}

#include "meta/graph/algorithms/centrality.tcc"
#endif
