/*
This file is part of khmer, https://github.com/dib-lab/khmer/, and is
Copyright (C) 2015-2016, The Regents of the University of California.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.

    * Neither the name of the Michigan State University nor the names
      of its contributors may be used to endorse or promote products
      derived from this software without specific prior written
      permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
LICENSE (END)

Contact: khmer-project@idyll.org
*/
#ifndef CDBG_HH
#define CDBG_HH

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <limits>
#include <list>
#include <iostream>
#include <sstream>
#include <unordered_set>
#include <unordered_map>

#include "oxli.hh"
#include "kmer_hash.hh"
#include "hashtable.hh"
#include "hashgraph.hh"
#include "kmer_filters.hh"
#include "traversal.hh"
#include "assembler.hh"
#include "alphabets.hh"

#define DEBUG_CDBG
# ifdef DEBUG_CDBG
#   define pdebug(x) do { std::cout << std::endl << "@ " << __FILE__ <<\
                          ":" << __FUNCTION__ << ":" <<\
                          __LINE__  << std::endl << x << std::endl;\
                          } while (0)
# else
#   define pdebug(x) do {} while (0)
# endif

#define complement(ch) ((ch) == 'A' ? 'T' : \
                        (ch) == 'T' ? 'A' : \
                        (ch) == 'C' ? 'G' : 'C')

namespace oxli {

typedef uint64_t id_t;
#define NULL_ID ULLONG_MAX

using std::make_shared;
using std::shared_ptr;

typedef std::pair<HashIntoType, id_t> HashIDPair;
typedef std::unordered_set<HashIntoType> UHashSet;
typedef std::vector<HashIntoType> HashVector;
typedef std::unordered_map<HashIntoType, id_t> HashIDMap;
typedef std::unordered_set<id_t> IDSet;


enum compact_edge_meta_t {
    FULL,
    TIP,
    ISLAND,
    TRIVIAL
};


inline const char * edge_meta_repr(compact_edge_meta_t meta) {
    switch(meta) {
        case FULL:
            return "FULL";
        case TIP:
            return "TIP";
        case ISLAND:
            return "ISLAND";
        case TRIVIAL:
            return "TRIVIAL";
    }
}


class CompactEdgeFactory;
class CompactEdge {
    friend class CompactEdgeFactory;

public:

    const id_t in_node_id; // left and right HDN IDs
    const id_t out_node_id;
    const id_t edge_id;
    compact_edge_meta_t meta;
    std::string sequence;
    UHashSet tags;

    CompactEdge(id_t in_node_id, id_t out_node_id, id_t edge_id) : 
        in_node_id(in_node_id), out_node_id(out_node_id), 
        meta(FULL), edge_id(edge_id) {}
    
    CompactEdge(id_t in_node_id, id_t out_node_id, id_t edge_id,
                compact_edge_meta_t meta) :
        in_node_id(in_node_id), out_node_id(out_node_id),
        meta(meta), edge_id(edge_id) {}

    void add_tags(UHashSet& new_tags) {
        for (auto tag: new_tags) {
            tags.insert(tag);
        }
    }

    std::string rc_sequence() const {
        return _revcomp(sequence);
    }

    float tag_density() const {
        return (float)sequence.length() / (float)tags.size();
    }

    std::string tag_viz(WordLength K) const {
        uint64_t pos;
        std::string ret = "L=" + std::to_string(sequence.length()) + " ";
        const char * _s = sequence.c_str();

        for (pos = 0; pos < sequence.length() - K + 1; pos++) {
            if (set_contains(tags, _hash(_s+pos, K))) {
                ret += ("(" + std::to_string(pos) + ")");
            }
            ret += sequence[pos];
        }
        return ret;
    }

    friend std::ostream& operator<<(std::ostream& stream,
                                     const CompactEdge& edge) {
            stream << "<CompactEdge in_node_id=" << 
                      std::to_string(edge.in_node_id) << 
                      " out_node_id=" << 
                      std::to_string(edge.out_node_id)
                   << " length=" << edge.sequence.length()
                   << " meta=" << edge_meta_repr(edge.meta)
                   << " n_tags=" << edge.tags.size() << ">";
            return stream;
    }

};

typedef std::vector<CompactEdge> CompactEdgeVector;
typedef std::unordered_map<HashIntoType, CompactEdge*> TagEdgeMap;
typedef std::unordered_map<id_t, CompactEdge*> IDEdgeMap;
typedef std::pair<HashIntoType, CompactEdge*> TagEdgePair;
typedef std::set<TagEdgePair> TagEdgePairSet;
typedef std::set<CompactEdge*> CompactEdgeSet;

class CompactNodeFactory;
class CompactEdgeFactory : public KmerFactory {
    friend class CompactNodeFactory;
protected:

    uint64_t n_compact_edges;
    uint64_t _n_updates;
    uint32_t tag_density;

    TagEdgeMap tags_to_edges;
    IDEdgeMap compact_edges;

public:

    CompactEdgeFactory(WordLength K) :

        KmerFactory(K), n_compact_edges(0),
        _n_updates(0) {

        tag_density = DEFAULT_TAG_DENSITY;    
    }

    uint64_t n_edges() const {
        return n_compact_edges;
    }

    uint64_t n_updates() const {
        return _n_updates;
    }

    CompactEdge* build_edge(id_t left_id, id_t right_id,
                            compact_edge_meta_t edge_meta,
                            std::string edge_sequence) {

        CompactEdge* edge = new CompactEdge(left_id, right_id,
                                            _n_updates, edge_meta);
        compact_edges[_n_updates] = edge;

        pdebug("new compact edge: \n left=" << std::to_string(left_id) 
                << std::endl << " right=" << std::to_string(right_id)
                << std::endl << " meta=" << edge_meta_repr(edge_meta)
                << std::endl << " sequence   =" << edge_sequence
                << std::endl << " rc_sequence=" << _revcomp(edge_sequence)
                << std::endl << " start   =" << edge_sequence.substr(0, _ksize+1)
                << std::endl << " rc_start=" << _revcomp(edge_sequence.substr(0, _ksize+1))
                << std::endl << " end    =" 
                << edge_sequence.substr(edge_sequence.length()-_ksize-1, _ksize+1)
                << std::endl << " rc_end =" 
                << _revcomp(edge_sequence.substr(edge_sequence.length()-_ksize-1, _ksize+1)));

        edge->sequence = edge_sequence;
        n_compact_edges++;
        _n_updates++;
        return edge;
    }

    CompactEdge* get_edge_by_id(id_t id) {
        auto search = compact_edges.find(id);
        if (search != compact_edges.end()) {
            return search->second;
        }
        return nullptr;
    }

    void delete_edge(CompactEdge * edge) {
        //pdebug("attempt edge delete @" << edge);
        if (edge != nullptr) {
            pdebug("edge not null, proceeding");
            for (auto tag: edge->tags) {
                tags_to_edges.erase(tag);
            }
            compact_edges.erase(edge->edge_id);
            delete edge;
            n_compact_edges--;
            _n_updates++;
        }
    }

    void delete_edge_by_id(id_t id) {
        CompactEdge* e = get_edge_by_id(id);
        delete_edge(e);
    }

    void delete_edge_by_tag(UHashSet& tags) {
        CompactEdge* edge = get_edge(tags);
        delete_edge(edge);
    }

    void delete_edge_by_tag(HashIntoType tag) {
        CompactEdge* edge = get_edge(tag);
        delete_edge(edge);
    }

    CompactEdge* get_edge(HashIntoType tag) const {
        //pdebug("get compact edge from tag " << tag);
        auto search = tags_to_edges.find(tag);
        if (search != tags_to_edges.end()) {
            return search->second;
        }
        return nullptr;
    }

    bool get_tag_edge_pair(HashIntoType tag, TagEdgePair& pair) const {
        auto search = tags_to_edges.find(tag);
        if (search != tags_to_edges.end()) {
            pair = *search;
            return true;
        } else {
            return false;
        }
    }

    CompactEdge* get_edge(UHashSet& tags) const {
        CompactEdge * edge = nullptr;
        for (auto tag: tags) {
            edge = get_edge(tag);
            if (edge != nullptr) {
                break;
            }
        }
        return edge;
    }

    KmerFilter get_tag_stopper(TagEdgePair& te_pair,
                               bool& found_tag) { const
        KmerFilter stopper = [&] (const Kmer& node) {
            found_tag = get_tag_edge_pair(node, te_pair);
            return found_tag;
        };

        return stopper;
    }

    void write_gml(const std::string filename,
                   const CompactNodeFactory& nodes) const;
    void write_fasta(const std::string filename) const;

};


class CompactNodeFactory;
class CompactNode {
    friend class CompactNodeFactory;
public:
    Kmer kmer;
    uint32_t count;
    const id_t node_id;
    std::string sequence;
    bool direction;

    CompactEdge* in_edges[4] = {nullptr, nullptr, nullptr, nullptr};
    CompactEdge* out_edges[4] = {nullptr, nullptr, nullptr, nullptr};

    CompactNode(Kmer kmer, id_t node_id) : 
        kmer(kmer), count(0), node_id(node_id), direction(kmer.is_forward()) {}

    CompactNode(Kmer kmer, std::string sequence, id_t node_id) : 
        kmer(kmer), count(0), sequence(sequence), node_id(node_id),
        direction(kmer.is_forward()) {}

    friend bool operator== (const CompactNode& lhs, const CompactNode& rhs) {
        return lhs.node_id == rhs.node_id;
    }

    std::string rc_sequence() const {
        return _revcomp(sequence);
    }

    bool delete_edge(CompactEdge* edge) {
        bool deleted = false;
        if (delete_in_edge(edge)) {
            deleted = true;
        }
        if (delete_out_edge(edge)) {
            deleted = true;
        }
        return deleted;
    }

    bool delete_in_edge(CompactEdge* edge) {
        for (uint8_t i=0; i<4; i++) {
            if (in_edges[i] == edge) {
                in_edges[i] = nullptr;
                return true;
            }
        }
        return false;
    }

    void add_in_edge(const char base, CompactEdge* edge) {
        //pdebug("add in edge to " << *this << ", base=" << base
        //        << ", edge: " << *edge);
        in_edges[twobit_repr(base)] = edge;
    }

    CompactEdge* get_in_edge(const char base) {
        return in_edges[twobit_repr(base)];
    }

    bool delete_out_edge(CompactEdge* edge) {
        for (uint8_t i=0; i<4; i++) {
            if (out_edges[i] == edge) {
                out_edges[i] = nullptr;
                return true;
            }
        }
        return false;
    }

    void add_out_edge(const char base, CompactEdge* edge) {
        //pdebug("add out edge to " << *this << ", base=" << base
        //        << ", edge: " << *edge);
        out_edges[twobit_repr(base)] = edge;
    }

    CompactEdge* get_out_edge(const char base) {
        return out_edges[twobit_repr(base)];
    }

    uint8_t degree() const {
        return out_degree() + in_degree();
    }

    uint8_t out_degree() const {
        uint8_t acc = 0;
        for (auto edge: out_edges) {
            if (edge != nullptr) {
                acc++;
            }
        }
        return acc;
    }

    uint8_t in_degree() const {
        uint8_t acc = 0;
        for (auto edge: in_edges) {
            if (edge != nullptr) {
                acc++;
            }
        }
        return acc;
    }

    friend std::ostream& operator<<(std::ostream& stream,
                                     const CompactNode& node) {
            stream << "<CompactNode ID=" << node.node_id << " Kmer=" << node.kmer.kmer_u
                   << " Sequence=" << node.sequence
                   << " rc_Sequence=" << node.rc_sequence()
                   << " Count=" << node.count << " in_degree=" 
                   << std::to_string(node.in_degree())
                   << " out_degree=" << std::to_string(node.out_degree()) << ">";
            return stream;
    }

    std::string edges_repr() {
        std::ostringstream os;
        os << *this << std::endl << "\tin_edges:" << std::endl;
        for (auto b : alphabets::DNA_SIMPLE) {
            CompactEdge* e = get_in_edge(b);
            if (e != nullptr) {
                os << "\t " << b << "=" << *e << std::endl;
            }
        }
        os << "\tout_edges:" << std::endl;
        for (auto b : alphabets::DNA_SIMPLE) {
            CompactEdge* e = get_out_edge(b);
            if (e != nullptr) {
                os << "\t " << b << "=" << *e << std::endl;
            }
        }
        return os.str();
    }
};

typedef std::vector<CompactNode> CompactNodeVector;

class CompactNodeFactory : public KmerFactory {
    friend class CompactEdgeFactory;
protected:

    // map from HDN hashes to CompactNode IDs
    HashIDMap kmer_id_map;
    // linear storage for CompactNodes
    CompactNodeVector compact_nodes;
    uint64_t n_compact_nodes;
    uint64_t _n_updates;

public:
    CompactNodeFactory(WordLength K) : 
        KmerFactory(K), n_compact_nodes(0),
        _n_updates(0) {}

    uint64_t n_nodes() const {
        return n_compact_nodes;
    }
    
    uint64_t n_updates() const {
        return _n_updates;
    }

    // protected linear creation of CompactNode
    // they should never be deleted, so this is straightforward
    CompactNode* build_node(Kmer hdn) {
        pdebug("new compact node from " << hdn);
        CompactNode * v = get_node_by_kmer(hdn);
        if (v == nullptr) {
            compact_nodes.emplace_back(hdn, n_compact_nodes);
            n_compact_nodes++;
            v = &(compact_nodes.back());
            v->sequence = _revhash(hdn, _ksize);
            kmer_id_map[hdn] = v->node_id;
            _n_updates++;
            pdebug("Allocate: " << *v);
        }
        return v;
    }

    CompactNode* get_node_by_kmer(HashIntoType hdn) {
        auto search = kmer_id_map.find(hdn);
        if (search != kmer_id_map.end()) {
            id_t ID = search->second;
            return &(compact_nodes[ID]);
        }
        return nullptr;
    }

    CompactNode* get_node_by_id(id_t id) {
        if (id >= compact_nodes.size()) {
            return nullptr;
        }
        return &(compact_nodes[id]);
    }

    CompactNode* get_or_build_node(Kmer hdn) {
        CompactNode* v = get_node_by_kmer(hdn);
        if (v != nullptr) {
            v->count += 1;
        } else {
            v = build_node(hdn);
            v->count = 1;
        }
        return v;
    }

    std::vector<CompactNode*> get_nodes(const std::string& sequence) {
        //pdebug("get compact node IDs");
        KmerIterator kmers(sequence.c_str(), _ksize);
        std::vector<CompactNode*> nodes;

        CompactNode* node;

        while(!kmers.done()) {
            Kmer kmer = kmers.next();

            node = get_node_by_kmer(kmer);
            if (node != nullptr) {
                nodes.push_back(node);
            }
        }

        return nodes;
    }

    void unlink_edge(CompactEdge* edge) {
        pdebug("unlink edge " << *edge);
        CompactNode *left, *right;
        left = get_node_by_id(edge->in_node_id);
        right = get_node_by_id(edge->out_node_id);
        if (left != nullptr) {
            // be lazy for now and use bidirectional delete
            left->delete_edge(edge);
            _n_updates++;
        }
        if (right != nullptr) {
            right->delete_edge(edge);
            _n_updates++;
        }
    }

    bool is_rc_from_left(CompactNode* v, std::string& sequence) const {
        /* Check if sequence shares same canonical orientation with
         * v when coming from graph left, assuming sequence
         * does NOT include v.
         */
        const char * node_kmer = v->sequence.c_str();
        const char * _sequence = sequence.c_str();
        return strncmp(node_kmer, 
                       _sequence + sequence.size()-_ksize+1,
                       _ksize - 1) != 0;
    }

    bool get_pivot_from_left(CompactNode* v,
                             std::string& sequence,
                             char& pivot_base) const {
        /* Check if sequence shared same canonical
         * orientation with v from graph left, assuming
         * sequence includes v
         */
        const char * node_kmer = v->sequence.c_str();
        const char * _segment = sequence.c_str();
        pivot_base = _segment[sequence.size()-_ksize-1];
        if (strncmp(node_kmer, 
                    _segment+sequence.size()-_ksize, 
                    _ksize-1) == 0) {
            // same canonical orientation
            return false;
        } else {
            // must have opposite canonical orientation
            pivot_base = complement(pivot_base);
            return true;
        }
    }

    bool add_edge_from_left(CompactNode* v, CompactEdge* e) {
        char pivot_base;
        if (!get_pivot_from_left(v, e->sequence, pivot_base)) {
            // same canonical orientation
            pdebug("add in edge " << *e << " to node " << *v << " from " << pivot_base);
            v->add_in_edge(pivot_base, e);
            _n_updates++;
            return false;
        } else {
            // must have opposite canonical orientation
            pdebug("add out edge " << *e << " to node " << *v << " from " << pivot_base);
            v->add_out_edge(pivot_base, e);
            _n_updates++;
            return true;
        }
    }


    bool get_edge_from_left(CompactNode* v,
                            CompactEdge* &result_edge,
                            std::string& sequence) const {
        char pivot_base;
        if (!get_pivot_from_left(v, sequence, pivot_base)) {
            result_edge = v->get_in_edge(pivot_base);
            return false;
        } else {
            result_edge = v->get_out_edge(pivot_base);
            return true;
        }
    }

    bool is_rc_from_right(CompactNode* v,
                          std::string& sequence) const {
        /* Check if sequence shared same canonical
         * orientation with v from graph right, assuming
         * sequence does NOT include v
         */
        const char * node_kmer = v->sequence.c_str();
        const char * _sequence = sequence.c_str();
        return strncmp(node_kmer+1, _sequence, _ksize-1) != 0;
    }

    bool get_pivot_from_right(CompactNode* v,
                              std::string& sequence,
                              char& pivot_base) const {
        /* Find the "pivot base" between sequence and v
         * when sequence is from graph right, assuming
         * v contained in sequence
         */
        const char * node_kmer = v->sequence.c_str();
        const char * _segment = sequence.c_str();
        pivot_base = _segment[_ksize];
        if (strncmp(node_kmer+1, _segment+1, _ksize-1) == 0) {
            // same canonical orientation
            return false;
        } else {
            // must have opposite canonical orientation
            pivot_base = complement(pivot_base);
            return true;
        }
    }

    bool add_edge_from_right(CompactNode* v, CompactEdge* e) {
        char pivot_base;
        if (!get_pivot_from_right(v, e->sequence, pivot_base)) {
            pdebug("add out edge " << *e << " to node " << *v << " from " << pivot_base);
            v->add_out_edge(pivot_base, e);
            _n_updates++;
            return false;
        } else {
            pdebug("add in edge " << *e << " to node " << *v << " from " << pivot_base);
            v->add_in_edge(pivot_base, e);
            _n_updates++;
            return true;
        }
    }

    bool get_edge_from_right(CompactNode* v,
                             CompactEdge* &result_edge,
                             std::string& sequence) const {
        char pivot_base;
        if (!get_pivot_from_right(v, sequence, pivot_base)) {
            result_edge = v->get_out_edge(pivot_base);
            return false;
        } else {
            result_edge = v->get_in_edge(pivot_base);
            return true;
        }

    }
};


class StreamingCompactor : public KmerFactory
{

protected:

    // map from tags to CompactEdges
    CompactNodeFactory nodes;
    CompactEdgeFactory edges;

    uint64_t n_sequences_added;

public:

    shared_ptr<Hashgraph> graph;
    
    StreamingCompactor(shared_ptr<Hashgraph> graph) :
        KmerFactory(graph->ksize()),
        nodes(graph->ksize()), edges(graph->ksize()),
        n_sequences_added(0), graph(graph)
    {
    }

    compact_edge_meta_t deduce_edge_meta(CompactNode* in, CompactNode* out) {
        compact_edge_meta_t edge_meta;
        if (in == nullptr && out == nullptr) {
           edge_meta = ISLAND;
        } else if ((out == nullptr) != (in == nullptr))  {
            edge_meta = TIP;
        } else {
            edge_meta = FULL;
        }
        return edge_meta;
    }

    uint64_t n_nodes() const {
        return nodes.n_nodes();
    }

    uint64_t n_edges() const {
        return edges.n_edges();
    }

    uint64_t n_updates() const {
        return nodes.n_updates() + edges.n_updates();
    }

    void report() const {
        std::cout << std::endl << "REPORT: StreamingCompactor(@" << this << " with "
            << "Hashgraph @" << graph.get() << ")" << std::endl;
        std::cout << "  * " << n_nodes() << " cDBG nodes (HDNs)" << std::endl;
        std::cout << "  * " << n_edges() << " cDBG edges" << std::endl;
        std::cout << "  * " << n_sequences_added << " sequences added" << std::endl;
    }


    CompactNode* get_node_by_kmer(Kmer hdn) {
        return nodes.get_node_by_kmer(hdn);
    }

    CompactNode* get_node_by_id(id_t id) {
        return nodes.get_node_by_id(id);
    }

    std::vector<CompactNode*> get_nodes(const std::string& sequence) {
        return nodes.get_nodes(sequence);
    }

    CompactEdge* get_edge(HashIntoType tag) const {
        return edges.get_edge(tag);
    }

    bool get_tag_edge_pair(HashIntoType tag, TagEdgePair& pair) const {
        return edges.get_tag_edge_pair(tag, pair);
    }

    CompactEdge* get_edge(UHashSet& tags) const {
        return edges.get_edge(tags);
    }

    uint64_t consume_sequence(const std::string& sequence) {
        uint64_t prev_n_kmers = graph->n_unique_kmers();
        graph->consume_string(sequence);
        return graph->n_unique_kmers() - prev_n_kmers;
    }

    uint64_t consume_sequence_and_update(const std::string& sequence) {
        if (consume_sequence(sequence) > 0) {
            return update_compact_dbg(sequence);
        }
        return 0;
    }

    bool validate_segment(CompactNode* root_node, CompactNode* other_node,
                          CompactEdge* edge, std::string& sequence) {
        pdebug("validating " << *root_node << " with  " << *edge << ", " 
              << sequence << " and other node ID=" << 
              ((other_node != nullptr) ? other_node->node_id : NULL_ID));
        bool edge_valid = true;
        if (edge->meta == TIP) {
            if (other_node != nullptr) {
                edge_valid = false;
            }
            if (!((edge->in_node_id == root_node->node_id ||
                   edge->out_node_id == root_node->node_id) &&
                  edge->sequence.length() == sequence.length())) {
                edge_valid = false;
            }
        } else if (edge->meta == FULL) {
            if (other_node == nullptr) {
                edge_valid = false;
            } else {
                bool nodes_match;
                nodes_match = (edge->in_node_id == root_node->node_id && 
                               edge->out_node_id == other_node->node_id) ||
                              (edge->out_node_id == root_node->node_id &&
                               edge->in_node_id == other_node->node_id);
                if (!nodes_match) {
                    edge_valid = false;
                }
            }
        }
        pdebug("valid? = " << edge_valid);
        return edge_valid;
    }

    /* Update a compact dbg where there are no induced
     * HDNs
     */
    uint64_t update_compact_dbg_linear(const std::string& sequence) {
        pdebug("no induced HDNs, update linear...");
        uint64_t n_ops_before = n_updates();
        Kmer root_kmer = graph->build_kmer(sequence.substr(0, _ksize));

        CompactingAT<TRAVERSAL_LEFT> lcursor(graph.get(), root_kmer);
        CompactingAT<TRAVERSAL_RIGHT> rcursor(graph.get(), root_kmer);
        CompactingAssembler cassem(graph.get());

        std::string left_seq = cassem._assemble_directed(lcursor);
        std::string right_seq = cassem._assemble_directed(rcursor);
        std::string segment_seq = left_seq + right_seq.substr(_ksize);

        CompactNode *left_node = nullptr, *right_node = nullptr;
        left_node = nodes.get_node_by_kmer(lcursor.cursor);
        right_node = nodes.get_node_by_kmer(rcursor.cursor);
        
        CompactEdge *left_edge = nullptr, *right_edge = nullptr;
        if (left_node != nullptr) {
            nodes.get_edge_from_right(left_node, left_edge, segment_seq);
        }
        if (right_node != nullptr) {
            nodes.get_edge_from_left(right_node, right_edge, segment_seq);
        }
        
        if (left_edge != nullptr) {
            nodes.unlink_edge(left_edge);
            edges.delete_edge(left_edge);
        }
        if (right_edge != nullptr) {
            nodes.unlink_edge(right_edge);
            edges.delete_edge(right_edge);
        }

        compact_edge_meta_t edge_meta = deduce_edge_meta(left_node, right_node);
        if (edge_meta == ISLAND) { // don't deal with islands for now
            return n_updates() - n_ops_before;
        }
        id_t left_id, right_id;
        left_id = (left_node != nullptr) ? left_node->node_id : NULL_ID;
        right_id = (right_node != nullptr) ? right_node->node_id : NULL_ID;
        CompactEdge *new_edge = edges.build_edge(left_id, right_id,
                                                 edge_meta, segment_seq);
        if (left_node != nullptr) {
            nodes.add_edge_from_right(left_node, new_edge);
        }
        if (right_node != nullptr) {
            nodes.add_edge_from_left(right_node, new_edge);
        }
        
        return n_updates() - n_ops_before;
    }


    uint64_t update_compact_dbg(const std::string& sequence) {
        pdebug("update cDBG from " << sequence);
        n_sequences_added++;
        uint64_t n_ops_before = n_updates();

        // first gather up all k-mers that could have been disturbed --
        // k-mers in the read, and the neighbors of the flanking nodes
        KmerIterator kmers(sequence.c_str(), _ksize);
        KmerQueue disturbed_kmers;
        Kmer kmer = kmers.next();
        CompactingAT<TRAVERSAL_LEFT> lcursor(graph.get(), kmer);
        lcursor.neighbors(disturbed_kmers);
        while(!kmers.done()) {
            kmer = kmers.next();
            disturbed_kmers.push_back(kmer);
        }
        CompactingAT<TRAVERSAL_RIGHT> rcursor(graph.get(), kmer);
        rcursor.neighbors(disturbed_kmers);
        
        pdebug(disturbed_kmers.size() << " k-mers disturbed" << std::endl);
        
                // find the induced HDNs in the disturbed k-mers
        KmerSet induced_hdns;
        KmerSet disturbed_hdns;
        while(!disturbed_kmers.empty()) {
            Kmer kmer = disturbed_kmers.back();
            disturbed_kmers.pop_back();
            uint8_t l_degree, r_degree;
            l_degree = lcursor.degree(kmer);
            r_degree = rcursor.degree(kmer);
            if(l_degree > 1 || r_degree > 1) {
                pdebug("found HDN... " << kmer);
                CompactNode* hdn = nodes.get_or_build_node(kmer);
                if (hdn->count == 1) { // just created
                    induced_hdns.insert(kmer);
                } else if (hdn->degree() != (l_degree + r_degree)) {
                    induced_hdns.insert(kmer);
                } else {
                    disturbed_hdns.insert(kmer);
                }
            }
        }
        pdebug(induced_hdns.size() << " induced HDNs");

        /* If there are no induced HDNs, we must have extended
         * a tip or merged two tips into a linear segment */
        if (induced_hdns.size() == 0 && disturbed_hdns.size() == 0) {
            return update_compact_dbg_linear(sequence);
        } else if (induced_hdns.size() == 0) {
            induced_hdns.insert(disturbed_hdns.begin(), disturbed_hdns.end());
        }

        /* Update from all induced HDNs
         */
        CompactingAssembler cassem(graph.get());
        KmerQueue neighbors;
        while(!induced_hdns.empty()) {
            Kmer root_kmer = *induced_hdns.begin();
            induced_hdns.erase(root_kmer);

            CompactNode* root_node = nodes.get_node_by_kmer(root_kmer);
            char root_front = root_node->sequence.front();
            char root_back = root_node->sequence.back();
            pdebug("searching from induced HDN: " << root_node->edges_repr());

            // check left (in) edges
            lcursor.neighbors(root_kmer, neighbors);
            pdebug("checking " << neighbors.size() << " left neighbors");
            while(!neighbors.empty()) {
                Kmer neighbor = neighbors.back();
                neighbors.pop_back();
                lcursor.cursor = neighbor;

                TagEdgePair tag_pair;
                bool found_tag = false;

                lcursor.push_filter(edges.get_tag_stopper(tag_pair, found_tag));
                std::string segment_seq = cassem._assemble_directed(lcursor);
                if (nodes.is_rc_from_left(root_node, segment_seq)) {
                    segment_seq = segment_seq  + complement(root_front);
                } else {
                    segment_seq = segment_seq + root_back;
                }
                pdebug("assembled segment: " << segment_seq << " length: " << 
                       segment_seq.length());

                // first check for a segment going this direction from root
                CompactEdge* segment_edge = nullptr;
                nodes.get_edge_from_left(root_node, segment_edge, segment_seq);

                CompactNode* left_node = nodes.get_node_by_kmer(lcursor.cursor);
                CompactEdge* left_out_edge = nullptr;
                if (left_node != nullptr) {
                    pdebug("found existing left node: " << *left_node);
                    nodes.get_edge_from_right(left_node, left_out_edge, segment_seq);
                }

                // validate edge leaving root if it exists
                if (segment_edge != nullptr && left_out_edge != nullptr) {
                    pdebug("found edges leaving root and left node");
                    
                    if (segment_edge == left_out_edge && 
                        validate_segment(root_node, left_node, 
                                         segment_edge, segment_seq)) {
                        continue;
                    } else {
                        nodes.unlink_edge(segment_edge);
                        nodes.unlink_edge(left_out_edge);
                        edges.delete_edge(segment_edge);
                        edges.delete_edge(left_out_edge);
                    }
                } else if (left_out_edge != nullptr) {
                    // there was no edge from root, must be bad
                    pdebug("edge from left invalid, delete");
                    nodes.unlink_edge(left_out_edge);
                    edges.delete_edge(left_out_edge);
                } else if (segment_edge != nullptr) {
                    pdebug("found end leaving root node");
                    if (validate_segment(root_node, left_node,
                                         segment_edge, segment_seq)) {
                        continue;
                    } else {
                        pdebug("edge from root invalid, delete");
                        nodes.unlink_edge(segment_edge);
                        edges.delete_edge(segment_edge);
                    }
                }
                
                /*
                 * Should also keep a set of pair<Kmer,Kmer> to track resolved
                 * segments
                 */

                // not needed until tags used again
                //segment_seq = cassem._assemble_directed(lcursor) +
                //              segment_seq.substr(_ksize);

                // construct the compact edge
                compact_edge_meta_t edge_meta = (left_node == nullptr) 
                                                ? TIP : FULL;
                edge_meta = (segment_seq.length() == _ksize + 1 && edge_meta == FULL)
                            ? TRIVIAL : edge_meta;

                if (edge_meta == FULL || edge_meta == TRIVIAL) {
                    segment_edge = edges.build_edge(left_node->node_id, 
                                                    root_node->node_id,
                                                    edge_meta, 
                                                    segment_seq);
                    nodes.add_edge_from_right(left_node, segment_edge);
                } else {
                    segment_edge = edges.build_edge(NULL_ID, 
                                                    root_node->node_id,
                                                    edge_meta, 
                                                    segment_seq);
                }

                nodes.add_edge_from_left(root_node, segment_edge);
            }

            // now the right neighbors...
            rcursor.neighbors(root_kmer, neighbors);
            pdebug("checking " << neighbors.size() << " right neighbors");
            while(!neighbors.empty()) {
                Kmer neighbor = neighbors.back();
                neighbors.pop_back();
                rcursor.cursor = neighbor;
                pdebug("right neighbor: " << neighbor.repr(_ksize));

                TagEdgePair tag_pair;
                bool found_tag = false;

                rcursor.push_filter(edges.get_tag_stopper(tag_pair, found_tag));
                std::string segment_seq = cassem._assemble_directed(rcursor);
                if (nodes.is_rc_from_right(root_node, segment_seq)) {
                    segment_seq = complement(root_back) + segment_seq;
                } else {
                    segment_seq = root_front + segment_seq;
                }
                pdebug("assembled segment: " << segment_seq << " length: " << 
                       segment_seq.length());
                // first check for a segment going this direction from root
                CompactEdge* segment_edge = nullptr;
                nodes.get_edge_from_right(root_node, segment_edge, segment_seq);

                CompactNode* right_node = nodes.get_node_by_kmer(rcursor.cursor);
                CompactEdge* right_in_edge = nullptr;
                if (right_node != nullptr) {
                    nodes.get_edge_from_left(right_node, right_in_edge, segment_seq);
                }

                // validate edge leaving root if it exists
                if (segment_edge != nullptr && right_in_edge != nullptr) {

                    
                    if (segment_edge == right_in_edge && 
                        validate_segment(root_node, right_node, 
                                         segment_edge, segment_seq)) {
                        continue;
                    } else {
                        nodes.unlink_edge(segment_edge);
                        nodes.unlink_edge(right_in_edge);
                        edges.delete_edge(segment_edge);
                        edges.delete_edge(right_in_edge);
                    }
                } else if (right_in_edge != nullptr) {
                    // there was no edge from root, must be bad
                    pdebug("edge from left invalid, delete");
                    nodes.unlink_edge(right_in_edge);
                    edges.delete_edge(right_in_edge);
                } else if (segment_edge != nullptr) {
                    if (validate_segment(root_node, right_node,
                                         segment_edge, segment_seq)) {
                        continue;
                    } else {
                        pdebug("edge from root invalid, delete");
                        nodes.unlink_edge(segment_edge);
                        edges.delete_edge(segment_edge);
                    }
                }

                compact_edge_meta_t edge_meta = (right_node == nullptr) ?
                                                  TIP : FULL;
                edge_meta = (segment_seq.length() == _ksize + 1 && edge_meta == FULL)
                            ? TRIVIAL : edge_meta;

                if (edge_meta == FULL || edge_meta == TRIVIAL) {
                    segment_edge = edges.build_edge(root_node->node_id, 
                                                    right_node->node_id,
                                                    edge_meta, 
                                                    segment_seq);
                    nodes.add_edge_from_left(right_node, segment_edge);
                } else {
                    segment_edge = edges.build_edge(root_node->node_id,
                                                    NULL_ID,
                                                    edge_meta, 
                                                    segment_seq);
                }

                nodes.add_edge_from_right(root_node, segment_edge);
            }

        }

        return n_updates() - n_ops_before;

    } // update_compact_dbg

    void write_gml(const std::string filename) const {
        edges.write_gml(filename, nodes);
    }

    void write_fasta(const std::string filename) const {
        edges.write_fasta(filename);
    }

};



}


#endif
