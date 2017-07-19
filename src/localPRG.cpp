#include <iostream>
#include <fstream>
#include <algorithm>
#include <stdint.h>
#include <string>
#include <vector>
#include <tuple>
#include <set>
#include <cassert>
#include <limits>
#include <cmath>
#include "minimizer.h"
#include "localPRG.h"
#include "interval.h"
#include "localgraph.h"
#include "index.h"
#include "path.h"
#include "minihits.h"
#include "minihit.h"
#include "inthash.h"
#include "pannode.h"
#include "utils.h"
#include "vcf.h"
#include "errormessages.h"

#define assert_msg(x) !(std::cerr << "Assertion failed: " << x << std::endl)

using namespace std;

LocalPRG::LocalPRG (uint32_t i, string n, string p): next_id(0), buff(" "), next_site(5), id(i), name(n), seq(p), num_hits(2, 0)
{
    vector<uint32_t> v;
    // avoid error if a prg contains only empty space as it's sequence
    if(seq.find_first_not_of("\t\n\v\f\r") != std::string::npos)
    {
        vector<uint32_t> b = build_graph(Interval(0,seq.size()), v);
    } else {
	prg.add_node(0, "", Interval(0,0));
    }
}

bool LocalPRG::isalpha_string (const string& s )
{
    // Returns if a string s is entirely alphabetic
    for(uint32_t j=0; j<s.length(); ++j)
        if(isalpha (s[j]) == 0)
        {
	    //cout << "Found non-alpha char: " << s[j] << endl;
            return 0; //False
        }
    return 1; //True
}

string LocalPRG::string_along_path(const Path& p)
{
    //cout << p << endl;
    assert(p.start<=seq.length());
    assert(p.end<=seq.length());
    string s;
    for (deque<Interval>::const_iterator it=p.path.begin(); it!=p.path.end(); ++it)
    {
	s += seq.substr(it->start, it->length);
        //cout << s << endl;
    }
    //cout << "lengths: " << s.length() << ", " << p.length << endl;
    assert(s.length()==p.length()||assert_msg("sequence length " << s.length() << " is not equal to path length " << p.length()));
    return s;
}

vector<LocalNode*> LocalPRG::nodes_along_path(const Path& p)
{
    vector<LocalNode*> v;
    v.reserve(100);
    // for each interval of the path
    for (deque<Interval>::const_iterator it=p.path.begin(); it!=p.path.end(); ++it)
    {
	//cout << "looking at interval " << *it << endl;
        // find the appropriate node of the prg
        for (map<uint32_t, LocalNode*>::const_iterator n=prg.nodes.begin(); n!=prg.nodes.end(); ++n)
        {
            if ((it->end > n->second->pos.start and it->start < n->second->pos.end) or 
		(it->start == n->second->pos.start and it->end == n->second->pos.end) or 
		(it->start == n->second->pos.start and it->length == 0 and it == --(p.path.end()) and n!=prg.nodes.begin()))
            {
		v.push_back(n->second);
		//cout << "found node " << *(n->second) << " so return vector size is now " << v.size() << endl;
            } else if (it->end < n->second->pos.start)
            {
		//cout << "gave up" << endl;
		break;
	    } // because the local nodes are labelled in order of occurance in the linear prg string, we don't need to search after this
        }
    }
    return v;
}

vector<Interval> LocalPRG::split_by_site(const Interval& i)
{
    // Splits interval by next_site based on substring of seq in the interval
    //cout << "splitting by site " << next_site << " in interval " << i << endl;

    // Split first by var site
    vector<Interval> v;
    v.reserve(4);
    string::size_type k = i.start;
    string d = buff + to_string(next_site) + buff;
    string::size_type j = seq.find(d, k);
    while (j!=string::npos and j+d.size()<=i.end) {
        v.push_back(Interval(k, j));
        k = j + d.size();
        j = seq.find(d, k);
    }

    if (j!=string::npos and j<i.end and j+d.size()>i.end) {
        v.push_back(Interval(k, j));
    } else if (j!=string::npos and j+d.size()==i.end) {
        v.push_back(Interval(k, j));
	if (seq.find(buff,j+d.size())==j+d.size()){
	    v.push_back(Interval(j+d.size(), j+d.size()));
	}
    } else {
	v.push_back(Interval(k, i.end));
    }

    assert(v[0].start >= i.start);
    for(uint32_t l=1; l!=v.size(); ++l)
    {
        assert(v[l-1].end <= v[l].start|| assert_msg(v[l-1].end << ">" << v[l].start << " giving overlapping intervals  " << v[l-1] << " and " << v[l]));
    }
    assert(v.back().end <= i.end);

    // then split by var site + 1
    vector<Interval> w;
    w.reserve(20);
    d = buff + to_string(next_site+1) + buff;
    for(uint32_t l=0; l!=v.size(); ++l)
    {
	k = v[l].start;
	j = seq.find(d, k);
        while (j!=string::npos and j+d.size()<=v[l].end) {
            w.push_back(Interval(k, j));
            k = j + d.size();
            j = seq.find(d, k);
    	}
        if (j!=string::npos and j<v[l].end and j+d.size()>v[l].end) {
            w.push_back(Interval(k, j));
	} else if (j!=string::npos and j+d.size()==v[l].end) {
	    w.push_back(Interval(k, j));
	    if (seq.find(buff,j+d.size())==j+d.size()){
                v.push_back(Interval(j+d.size(), j+d.size()));
            }
        } else {
	    w.push_back(Interval(k, v[l].end));
	}
    }
    if (v.size() == w.size() && v.size() == 3)
    {
	cout << "There was something dodgy with var site " << next_site << ": found no separated alternates.";
	cout << " I'm going to assume for now that this is as a result of straggly ends of sequences which don't align nicely,";
	cout << " but you should check this. To handle, add an empty interval alternate." << endl;
	vector<Interval> x;
	for(uint32_t l=0; l!=w.size()-1; ++l)
	{
	    x.push_back(w[l]);
	}
	x.push_back(Interval(w[w.size()-2].end, w[w.size()-2].end));
	x.push_back(w[w.size()-1]);
        w = x;
    }

    assert(w[0].start >= i.start);
    for(uint32_t l=1; l!=w.size(); ++l)
    {
	assert(w[l-1].end <= w[l].start|| assert_msg(w[l-1].end << ">" << w[l].start << " giving overlapping intervals  " << w[l-1] << " and " << w[l] << " when splitting seq :" << seq.substr(i.start, i.length)));
    }
    assert(w.back().end <= i.end);
    return w;
}

vector<uint32_t> LocalPRG::build_graph(const Interval& i, const vector<uint32_t>& from_ids, uint32_t current_level)
{
    // we will return the ids on the ends of any stretches of graph added
    vector<uint32_t> end_ids;
    end_ids.reserve(20);

    // save the start id, so can add 0, and the last id to the index at level 0 at the end
    uint32_t start_id = next_id;

    // add nodes
    string s = seq.substr(i.start, i.length); //check length correct with this end...
    if (isalpha_string(s)) // should return true for empty string too
    {
	prg.add_node(next_id, s, i);
	// add edges from previous part of graph to start of this interval
        for (uint32_t j=0; j!=from_ids.size(); j++)
        {
            prg.add_edge(from_ids[j], next_id);
        }
	end_ids.push_back(next_id);
	next_id++;
    } else {
	// split by next var site
	vector<Interval> v = split_by_site(i); // should have length at least 4
	if(v.size()<(uint)4)
        {
	    cerr << SPLIT_SITE_WRONG_SIZE_ERROR << endl;
            cerr << "Size of partition based on site " << next_site << " is " << v.size() << endl;
            exit(-1);
        }
	next_site += 2;
	// add first interval (should be alpha)
	s = seq.substr(v[0].start, v[0].length);
	if (! (isalpha_string(s)))
        {
            cerr << SPLIT_SITE_ERROR << endl;
            cerr << "After splitting by site " << next_site << " do not have alphabetic sequence before var site: " << v[0] << endl;
            exit(-1);
        }
	prg.add_node(next_id, s, v[0]);
	// add edges from previous part of graph to start of this interval
        for (uint32_t j=0; j!=from_ids.size(); j++)
        {
            prg.add_edge(from_ids[j], next_id);
        }

	vector<uint32_t> mid_ids;
        mid_ids.reserve(20);
	mid_ids.push_back(next_id);
	next_id++;
	// add (recurring as necessary) middle intervals
	for (uint32_t j=1; j!=v.size() - 1; j++)
	{
	    vector<uint32_t> w = build_graph(v[j], mid_ids, current_level+1);
	    end_ids.insert(end_ids.end(), w.begin(), w.end());
	}
	// add end interval
	end_ids = build_graph(v.back(), end_ids, current_level);
    }
    if (start_id == 0)
    {
	assert(end_ids.size()==1);
    }
    return end_ids;
}

vector<Path> LocalPRG::shift(Path p)
{
    // returns all paths of the same length which have been shifted by one position along prg graph
    Path q;
    q = p.subpath(1,p.length()-1);
    vector<LocalNode*> n;
    vector<Path> return_paths;
    deque<Path> short_paths = {q};
    vector<Path> k_paths;
    Interval i;
    bool non_terminus;
    //uint exp_num_return_seqs = 0;

    // first find extensions of the path
    while (short_paths.size() > 0)
    {
	p = short_paths.front();
	n = nodes_along_path(p);
	short_paths.pop_front();
	
	// if we can extend within the same localnode, do
	if (p.end < n.back()->pos.end)
        {
            p.path.back().end += 1;
	    p.end += 1;
	    p.path.back().length += 1;
	    k_paths.push_back(p);    
	} else if (p.end != (--(prg.nodes.end()))->second->pos.end) {
	    for (uint i=0; i!=n.back()->outNodes.size(); ++i)
	    {
		//exp_num_return_seqs += 1;
		short_paths.push_back(p);
		short_paths.back().add_end_interval(Interval(n.back()->outNodes[i]->pos.start, n.back()->outNodes[i]->pos.start));
	    }
	}
    }

    // now check if by adding null nodes we reach the end of the prg
    for (uint i=0; i!= k_paths.size(); ++i)
    {
	short_paths = {k_paths[i]};
	non_terminus = false; // assume there all extensions are terminal i.e. reach end or prg

	while (short_paths.size() > 0)
	{
	    p = short_paths.front();
            n = nodes_along_path(p);
            short_paths.pop_front();
	    
	    if (n.back()->pos.end==(--(prg.nodes.end()))->second->pos.end)
            {
                return_paths.push_back(p);
	    } else if (n.back()->pos.end==p.end) {
                for (uint i=0; i!=n.back()->outNodes.size(); ++i)
		{
		    if (n.back()->outNodes[i]->pos.length == 0)
		    {
                        short_paths.push_back(p);
                        short_paths.back().add_end_interval(n.back()->outNodes[i]->pos);
		    } else {
			non_terminus = true;
		    }
		}
            } else {
		non_terminus = true;
	    }
	}
	if (non_terminus == true)
	{
	    return_paths.push_back(k_paths[i]);
	}
    }

    return return_paths;
}

void LocalPRG::minimizer_sketch (Index* idx, const uint32_t w, const uint32_t k)
{
    cout << now() << "Sketch PRG " << name << " which has " << prg.nodes.size() << " nodes" << endl;

    // clean up after any previous runs
    // although note we can't clear the index because it is also added to by other LocalPRGs
    kmer_prg.clear();

    // declare variables
    vector<Path> walk_paths, shift_paths, v;
    deque<KmerNode*> current_leaves, end_leaves;
    deque<vector<Path>> shifts;
    deque<Interval> d;
    Path kmer_path;
    string kmer;
    uint64_t smallest;
    pair<uint64_t, uint64_t> kh;
    KmerHash hash;
    uint num_kmers_added = 0;
    KmerNode *kn, *new_kn;
    vector<KmerNode*>::iterator found;
    vector<LocalNode*> n;
    bool mini_found_in_window;
    size_t num_AT=0;

    // create a null start node in the kmer graph
    d = {Interval(0,0)};
    kmer_path.initialize(d);
    kn = kmer_prg.add_node(kmer_path);
    num_kmers_added += 1;

    // if this is a null prg, return the null kmergraph
    if (prg.nodes.size() == 1 and prg.nodes[0]->pos.length < k)
    {
	return;
    }

    // find first w,k minimizers
    walk_paths = prg.walk(prg.nodes.begin()->second->id, 0, w+k-1);
    for (uint i=0; i!=walk_paths.size(); ++i)
    {
        // find minimizer for this path 
        smallest = std::numeric_limits<uint64_t>::max();
	mini_found_in_window = false;
        for (uint32_t j = 0; j != w; j++)
        {
            kmer_path = walk_paths[i].subpath(j,k);
            if (kmer_path.path.size() > 0)
            {
                kmer = string_along_path(kmer_path);
                kh = hash.kmerhash(kmer, k);
                smallest = min(smallest, min(kh.first, kh.second));
            }
        }
        for (uint32_t j = 0; j != w; j++)
        {
	    kmer_path = walk_paths[i].subpath(j,k);
            if (kmer_path.path.size() > 0)
            {       
                kmer = string_along_path(kmer_path);
                kh = hash.kmerhash(kmer, k);
		n = nodes_along_path(kmer_path);
	
                if (prg.walk(n.back()->id, n.back()->pos.end, w+k-1).size() == 0)
                {
                    while (kmer_path.end >= n.back()->pos.end and n.back()->outNodes.size() == 1 and n.back()->outNodes[0]->pos.length == 0)
                    {
                        kmer_path.add_end_interval(n.back()->outNodes[0]->pos);
                        n.push_back(n.back()->outNodes[0]);
                    }
                }
	
                if (kh.first == smallest or kh.second == smallest)
                {
		    found = find_if(kmer_prg.nodes.begin(), kmer_prg.nodes.end(), condition(kmer_path));
                    if (found == kmer_prg.nodes.end())
                    {
		        // add to index, kmer_prg
		        idx->add_record(min(kh.first, kh.second), id, kmer_path, (kh.first<=kh.second));
		        num_AT = std::count(kmer.begin(), kmer.end(), 'A') + std::count(kmer.begin(), kmer.end(), 'T');
		        kn = kmer_prg.add_node_with_kh(kmer_path, min(kh.first, kh.second), num_AT);	
		        num_kmers_added += 1;
		        if (mini_found_in_window == false)
		        {
                            kmer_prg.add_edge(kmer_prg.nodes[0]->path, kmer_path);
		        }
		        mini_found_in_window = true;
		        current_leaves.push_back(kn);
		    }
		}
	    }
	}
    }

    // while we have intermediate leaves of the kmergraph, for each in turn, explore the neighbourhood
    // in the prg to find the next minikmers as you walk the prg
    while (current_leaves.size()>0)
    {
	kn = current_leaves.front();
	current_leaves.pop_front();
	assert(kn->khash < std::numeric_limits<uint64_t>::max());

        // find all paths which are this kmernode shifted by one place along the graph
	shift_paths = shift(kn->path);
	if (shift_paths.size() == 0)
	{
	    //assert(kn->path.start == 0); not true for a too short test, would be true if all paths long enough to have at least 2 minikmers on...
	    end_leaves.push_back(kn);
	}
	for (uint i=0; i!=shift_paths.size(); ++i)
	{
	    v = {shift_paths[i]};
	    shifts.push_back(v);
	}
	shift_paths.clear();

	while(shifts.size() > 0)
	{
	    v = shifts.front();
	    shifts.pop_front();	    
	    assert(v.back().length() == k);
	    kmer = string_along_path(v.back());
            kh = hash.kmerhash(kmer, k);
	    if (min(kh.first, kh.second) <= kn->khash) {
		// found next minimizer
		found = find_if(kmer_prg.nodes.begin(), kmer_prg.nodes.end(), condition(v.back()));
		if (found == kmer_prg.nodes.end())
		{
		    idx->add_record(min(kh.first, kh.second), id, v.back(), (kh.first<=kh.second));
		    num_AT = std::count(kmer.begin(), kmer.end(), 'A') + std::count(kmer.begin(), kmer.end(), 'T');
                    new_kn = kmer_prg.add_node_with_kh(v.back(), min(kh.first, kh.second), num_AT);
                    kmer_prg.add_edge(kn, new_kn);
		    if (v.back().end == (--(prg.nodes.end()))->second->pos.end)
		    {
			end_leaves.push_back(new_kn);
		    } else if (find(current_leaves.begin(), current_leaves.end(), new_kn) == current_leaves.end()){
                        current_leaves.push_back(new_kn);
		    }
                    num_kmers_added += 1;
		} else {
		    kmer_prg.add_edge(kn, *found);
		    if (v.back().end == (--(prg.nodes.end()))->second->pos.end)
                    {
                        end_leaves.push_back(*found);
                    } else if (find(current_leaves.begin(), current_leaves.end(), *found) == current_leaves.end()) {
                        current_leaves.push_back(*found);
                    }
		}
            } else if (v.size() == w) {
		// the old minimizer has dropped out the window, minimizer the w new kmers
		smallest = std::numeric_limits<uint64_t>::max();
		mini_found_in_window = false;
                for (uint j = 0; j != w; j++)
                {
                    kmer = string_along_path(v[j]);
                    kh = hash.kmerhash(kmer, k);
                    smallest = min(smallest, min(kh.first, kh.second));
		    //cout << min(kh.first, kh.second) << " ";
                }
		for (uint j = 0; j != w; j++)
                {
		    kmer = string_along_path(v[j]);
                    kh = hash.kmerhash(kmer, k);
		    if (kh.first == smallest or kh.second == smallest)
		    {
			    found = find_if(kmer_prg.nodes.begin(), kmer_prg.nodes.end(), condition(v[j]));
			    if (found == kmer_prg.nodes.end())
			    {
			        idx->add_record(min(kh.first, kh.second), id, v[j], (kh.first<=kh.second));
				num_AT = std::count(kmer.begin(), kmer.end(), 'A') + std::count(kmer.begin(), kmer.end(), 'T');
                                new_kn = kmer_prg.add_node_with_kh(v[j], min(kh.first, kh.second), num_AT);

				// if there is more than one mini in the window, edge should go to the first, and from the first to the second
				if (mini_found_in_window == false)
                                {
                                    kmer_prg.add_edge(kn, new_kn);
                                }
                                mini_found_in_window = true;

				if (v.back().end == (--(prg.nodes.end()))->second->pos.end)
                                {
                                    end_leaves.push_back(new_kn);
                                } else if (find(current_leaves.begin(), current_leaves.end(), new_kn) == current_leaves.end()){
                                    current_leaves.push_back(new_kn);
                                }
                                num_kmers_added += 1;
			    } else {
				if (mini_found_in_window == false)
				{
				    kmer_prg.add_edge(kn, *found);
				}
				mini_found_in_window = true;
				if (v.back().end == (--(prg.nodes.end()))->second->pos.end)
                                {
                                    end_leaves.push_back(*found);
                                } else if (find(current_leaves.begin(), current_leaves.end(), *found) == current_leaves.end()){
                                    current_leaves.push_back(*found);
                                }
			    }
		    }
		}
	    } else if (v.back().end == (--(prg.nodes.end()))->second->pos.end) {
                end_leaves.push_back(kn);
	    } else {
		shift_paths = shift(v.back());
                for (uint i=0; i!=shift_paths.size(); ++i)
                {
		    shifts.push_back(v);
		    shifts.back().push_back(shift_paths[i]);
                }
                shift_paths.clear();
	    }
	}
    }

    // create a null end node, and for each end leaf add an edge to this terminus
    assert(end_leaves.size()>0);
    d = {Interval((--(prg.nodes.end()))->second->pos.end,(--(prg.nodes.end()))->second->pos.end)};
    kmer_path.initialize(d);
    kn = kmer_prg.add_node(kmer_path);
    num_kmers_added += 1;
    for (uint i=0; i!=end_leaves.size(); ++i)
    {
        kmer_prg.add_edge(end_leaves[i], kn);
    }

    // print, check and return
    kmer_prg.sort_topologically();
    kmer_prg.check(num_kmers_added);
    return;
}

vector<LocalNode*> LocalPRG::localnode_path_from_kmernode_path(vector<KmerNode*> kmernode_path, uint w)
{
    cout << now() << "Convert kmernode path to localnode path" << endl;
    vector<LocalNode*> localnode_path, kmernode, walk_path;
    vector<Path> walk_paths;
    for (uint i=0; i!=kmernode_path.size(); ++i)
    {
	if (kmernode_path[i] == kmer_prg.nodes.back())
	{
	    break;
	}
        kmernode = nodes_along_path(kmernode_path[i]->path);

	// if the start of the new localnode path is after the end of the previous, join up WLOG with top path
        while (localnode_path.size() > 0 and localnode_path.back()->outNodes.size() > 0 and kmernode[0]->id > localnode_path.back()->outNodes[0]->id)
        {
            localnode_path.push_back(localnode_path.back()->outNodes[0]);
        }
	// if new the localnodes in kmernode overlap old ones, truncate localnode_path
	while (localnode_path.size() > 0 and kmernode[0]->id <= localnode_path.back()->id)
	{
	    localnode_path.pop_back();
	}
	localnode_path.insert(localnode_path.end(), kmernode.begin(), kmernode.end());
    }

    // extend to beginning of graph if possible
    bool overlap;
    if (localnode_path[0]->id != 0)
    {	
	walk_paths = prg.walk(0,0,w);
	for (uint i=0; i!= walk_paths.size(); ++i)
	{
	    walk_path = nodes_along_path(walk_paths[i]);
	    // does it overlap
	    uint n=0, m=0;
	    overlap = false;	
	    for (uint j=0; j!= walk_path.size(); ++j)
	    {
		if (walk_path[j] == localnode_path[n])
		{
		    if (overlap == false)
		    {
			m=j;
		    }
		    overlap = true;
		    if (n+1 >= localnode_path.size())
		    {
			break;
		    } else {
			++n;
		    }
		} else if (overlap == true) {
		    overlap = false;
		    break;
		}
	    }
	    if (overlap == true)
	    {
		localnode_path.insert(localnode_path.begin(), walk_path.begin(), walk_path.begin()+m);
		break;
	    }
	}
	if (localnode_path[0]->id != 0)
	{
	    cout << "localnode path still does not start with 0" << endl;
	}
    }

    // extend to end of graph if possible
    if (localnode_path.back()->id != prg.nodes.size()-1)
    {
        walk_paths = prg.walk_back(prg.nodes.size()-1,seq.length(),w);
        for (uint i=0; i!= walk_paths.size(); ++i)
        {
            walk_path = nodes_along_path(walk_paths[i]);

            // does it overlap
            uint n=localnode_path.size(), m=0;
            overlap = false;
            for (uint j=walk_path.size(); j!= 0; --j)
            {   
                if (walk_path[j-1] == localnode_path[n-1])
                {   
                    if (overlap == false)
                    {
                        m=j;
                    }
                    overlap = true;
                    if (n-1 == 0)
                    {
                        break;
                    } else {
                        --n;
                    }
                } else if (overlap == true) {
                    overlap = false;
                    break;
                }
            }
            if (overlap == true)
            {
                localnode_path.insert(localnode_path.end(), walk_path.begin()+m, walk_path.end());
                break;
            }
        }
	if (localnode_path.back()->id != prg.nodes.size()-1)
        {
            cout << "localnode path still does not end with " << prg.nodes.size()-1 << endl;
        }
    }

    return localnode_path;
}

void LocalPRG::update_covg_with_hit(MinimizerHit* mh)
{
    bool added = false;
    // update the covg in the kmer_prg
    for (uint i=0; i!=kmer_prg.nodes.size(); ++i)
    {
	if (kmer_prg.nodes[i]->path == mh->prg_path)
	{
	    kmer_prg.nodes[i]->covg[mh->strand] += 1;
	    added = true;
	    break;
	}
    }
    num_hits[mh->strand] += 1;
    assert(added == true || assert_msg("could not find kmernode corresponding to " << *mh));
}

void LocalPRG::write_max_path_to_fasta(const string& filepath, const vector<LocalNode*>& lmp, const float& ppath)
{
    ofstream handle;
    handle.open (filepath);

    handle << ">" << name << "\tlog P(data|sequence)=" << ppath  << endl;
    for (uint j = 0; j!= lmp.size(); ++j)
    {
        handle << lmp[j]->seq;
    }
    handle << endl;

    handle.close();
    return;
}

void LocalPRG::build_vcf()
{
    cout << now() << "Build VCF for prg " << name << endl;
    assert(prg.nodes.size()>0); //otherwise empty nodes -> segfault

    vector<LocalNode*> varpath;
    varpath.reserve(100);
    vector<LocalNode*> toppath, bottompath;
    toppath.reserve(100);
    toppath.push_back(prg.nodes[0]);
    bottompath.reserve(100);

    deque<vector<LocalNode*>> paths;
    paths.push_back(toppath);

    vector<vector<LocalNode*>> alts;
    alts.reserve(100);

    int level = 0, max_level=0;
    uint pos=prg.nodes[0]->pos.length;
    string vartype = "GRAPHTYPE=SIMPLE", ref = "", alt = "";

    // do until we reach the end of the localPRG
    while (toppath.back()->outNodes.size() > 0 or toppath.size() > 1)
    {	
	//cout << "extend toppath from id " << toppath.back()->id << endl;
	// extend the top path until level 0 is reached again
	while(level>0 or toppath.size() == 1)
	{
            // first update the level we are at
            if (toppath.back()->outNodes.size() > 1)
            {
                level += 1;
            } else {
                level -= 1;
            }
	    max_level = max(level, max_level);
            assert(level >= 0);

            // update vartype if complex
            if (level > 1)
            {
                vartype = "GRAPHTYPE=COMPLEX";
            }

	    //extend if necessary
	    if (level>0 or toppath.size() == 1)
	    {
	        toppath.push_back(toppath.back()->outNodes[0]);
	    }
	}

    	// next collect all the alts
	while(paths.size() > 0)
	{
	    varpath = paths[0];
	    paths.pop_front();
	    if (varpath.back()->outNodes[0]->id == toppath.back()->outNodes[0]->id)
	    {
		alts.push_back(varpath);
	    } else {
		for (uint j=0; j!=varpath.back()->outNodes.size(); ++j)
		{
		    paths.push_back(varpath);
		    paths.back().push_back(varpath.back()->outNodes[j]);
		}
	    }

	    // if have too many alts, just give bottom path 
	    if ((paths.size() > 100 and max_level > 2) or paths.size() > 1000)
	    {
		paths.clear();
		alts.clear();
		bottompath.push_back(toppath[0]);
		while(bottompath.back()->outNodes.size()>0 and bottompath.back()->outNodes[0]->id != toppath.back()->outNodes[0]->id)
		{
		    bottompath.push_back(bottompath.back()->outNodes.back());
		}
		alts.push_back(bottompath);	
		vartype = "GRAPHTYPE=TOO_MANY_ALTS";
		break;
	    }
	}

        // define ref sequence
        for (uint j=1; j< toppath.size(); ++j)
        {
            ref += toppath[j]->seq;
        }

        // add each alt to the vcf
	for (uint i=0; i<alts.size(); ++i)
	{
	    for (uint j=1; j<alts[i].size(); ++j)
	    {
		alt += alts[i][j]->seq;
	    }

	    if (ref != alt)
	    {
                vcf.add_record(name, pos, ref, alt, ".", vartype);
	    }
	    alt = "";
	}

	// clear up
	pos += ref.length() + toppath.back()->outNodes[0]->pos.length;
        vartype = "GRAPHTYPE=SIMPLE";
        ref = "";
        alt = "";
	max_level = 0;
        toppath = {toppath.back()->outNodes[0]};
	bottompath.clear();
        alts.clear();
        paths.push_back(toppath);
	if (toppath.back()->outNodes.size() == 0)
        {
            break;
	}
    }
    //cout << "sort vcf" << endl;
    sort(vcf.records.begin(), vcf.records.end());
    return;
}

void LocalPRG::add_sample_to_vcf(const vector<LocalNode*>& lmp)
{
    cout << now() << "Update VCF with sample path" << endl;
    assert(prg.nodes.size()>0); //otherwise empty nodes -> segfault

    vector<LocalNode*> toppath;
    toppath.reserve(100);
    toppath.push_back(prg.nodes[0]);

    uint lmp_range_start = 0, lmp_range_end = 0, pos=toppath[0]->pos.length;
    int level = 0;
    string ref = "", alt = "";

    // if prg has only one node, simple case
    if(prg.nodes.size() == 1)
    {
	vcf.samples.push_back("sample");
    }

    // do until we reach the end of the localPRG
    while (toppath.back()->outNodes.size() > 0 or toppath.size() > 1)
    {
	//cout << "toppath ends with node " << toppath.back()->id << endl;
        // first update the level we are at
        if (toppath.back()->outNodes.size() > 1)
        {
            level += 1;
        } else {
            level -= 1;
        }
        assert(level >= 0);
	//cout << "level is " << level << endl;

        // next, if we are at level 0, then we have reached the end of a varsite
        // so add it to the vcf
        if (level == 0)
	{
	    // first find the range of the maxpath which corresponds to this varsite
	    assert(toppath.size() > 0 || assert_msg("toppath has size " << toppath.size() << " when it should have entries!"));
            assert(toppath.back()->outNodes.size() > 0 || assert_msg("node " << toppath.back()->id << " has no outnodes!"));
            assert(lmp_range_end < lmp.size() || assert_msg("lmp_range_end = " << lmp_range_end << " but lmp.size() = " << lmp.size() << " which should be bigger"));

	    while (lmp[lmp_range_end]->id < toppath.back()->outNodes[0]->id and lmp_range_end < lmp.size()-1)
            {
                //cout << "lmp_range_end = " << lmp_range_end << " and alt end id is " << lmp[lmp_range_end]->id << " < " << toppath.back()->outNodes[0]->id << endl;
                lmp_range_end += 1;
            }

	    if (((toppath.size() > 0) and (lmp[lmp_range_start]->id != toppath[0]->id)) or // ref allele and var allele don't start at the same node
		(lmp[lmp_range_end]->id != toppath.back()->outNodes[0]->id)) // ref allele and var allele don't end at the same node
	    {
		cout << now() << "ERROR adding sample to VCF - start or end of varsite did not agree" << endl;
		cout << now() << "Starts : " << lmp[lmp_range_start]->id << " vs " << toppath[0]->id << endl;
		cout << now() << "Ends : " << lmp[lmp_range_end]->id << " vs " << toppath.back()->outNodes[0]->id << endl;
		vcf.samples.clear();
		return;
	    }

	    // add to vcf
	    for (uint j=1; j< toppath.size(); ++j)
	    {
		ref += toppath[j]->seq;
	    }

	    for (uint j=lmp_range_start+1; j< lmp_range_end; ++j)
            {
                alt += lmp[j]->seq;
            }

	    vcf.add_sample_gt(name, pos, ref, alt);
	
	    // prepare variables for next increment
	    assert(lmp_range_end+1 <= lmp.size() || assert_msg("lmp_range_end+1 = " << lmp_range_end+1 << " but lmp.size() = " << lmp.size() << " which is bigger"));
	    for (uint j=1; j < toppath.size(); ++j)
            {
                pos += toppath[j]->pos.length;
            }
	    pos += lmp[lmp_range_end]->pos.length;

            lmp_range_start = lmp_range_end;
	    toppath.clear();
	    ref = "";
	    alt = "";
	}

        // finally, extend the toppath, or end
        if (toppath.size() == 0)
	{
	    toppath.push_back(lmp[lmp_range_start]);
	} else if (toppath.back()->outNodes.size() == 0)
        {
	    assert(toppath.size() <= 1);
	    break;
	} else {
            toppath.push_back(toppath.back()->outNodes[0]);
	}
    }

    return;
}

void LocalPRG::find_path_and_variants(const string& prefix, uint w, bool max_path, bool min_path, bool output_vcf)
{
    string new_name = name;
    std::replace(new_name.begin(),new_name.end(), ' ', '_');

    vector<KmerNode*> kmp;
    kmp.reserve(800);
    vector<LocalNode*> lmp;
    lmp.reserve(100);
    float ppath;

    if (max_path == true)
    {
        ppath = kmer_prg.find_max_path(kmp);
    	lmp = localnode_path_from_kmernode_path(kmp, w);

    	write_max_path_to_fasta(prefix + "." + new_name + "_kmlp.fasta", lmp, ppath);
	
    	cout << "localPRG ids on max likelihood path: "; 
    	for (uint i=0; i!=lmp.size(); ++i)
    	{
	    cout << lmp[i]->id << " ";
        }
    	cout << endl;

	if (output_vcf == true)
        {
            build_vcf();
    	    add_sample_to_vcf(lmp);
    	    vcf.save(prefix + "." + new_name + ".kmlp.vcf", true, true, true, true, true, true, true);
	}
   }

    if (min_path == true)
    {
	kmp.clear();
	lmp.clear();
	vcf.clear();

	ppath = kmer_prg.find_min_path(kmp);
	lmp = localnode_path_from_kmernode_path(kmp, w);
	
    	write_max_path_to_fasta(prefix + "." + new_name + "_kminp.fasta", lmp, ppath);

    	cout << "localPRG node ids on min path: ";
    	for (uint i=0; i!=lmp.size(); ++i)
    	{
            cout << lmp[i]->id << " ";
    	}
    	cout << endl;

	if (output_vcf == true)
        {
            build_vcf();
    	    add_sample_to_vcf(lmp);
    	    vcf.save(prefix + "." + new_name + ".kminp.vcf", true, true, true, true, true, true, true);
	}
    }
    return;
}

std::ostream& operator<< (std::ostream & out, LocalPRG const& data) {
    out << data.name;
    return out ;
}

