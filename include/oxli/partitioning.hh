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
#ifndef PARTITIONING_HH 
#define PARTITIONING_HH

#include <functional>
#include <memory>

#include "oxli.hh"
#include "kmer_hash.hh"
#include "hashtable.hh"
#include "hashgraph.hh"
#include "kmer_filters.hh"
#include "traversal.hh"

#ifndef DEBUG_SP
#define DEBUG_SP 0
#endif

namespace oxli
{


template <typename T>
class GuardedKmerMap {

    private:

        uint32_t lock;

    public:

        // Filter should be owned exclusively by GuardedKmerMap
        std::unique_ptr<Nodegraph> filter;
        std::map<HashIntoType, T> data;
        
        explicit GuardedKmerMap(WordLength ksize,
                                unsigned short n_tables,
                                uint64_t max_table_size): lock(0)
        {
            std::vector<uint64_t> table_sizes = get_n_primes_near_x(n_tables, max_table_size);
            filter = std::unique_ptr<Nodegraph>(new Nodegraph(ksize, table_sizes));
        }

        T get(HashIntoType kmer) const {
            if (filter->get_count(kmer)) {
                auto search = data.find(kmer);
                if (search != data.end()) {
                    return search->second;
                }
            }
            
            return NULL;
        }

        T get_threadsafe(HashIntoType kmer) const {
            if (filter->get_count(kmer)) {
                acquire_lock();
                auto search = data.find(kmer);
                if (search != data.end()) {
                    release_lock();
                    return search->second;
                }
                release_lock();
            }

            return NULL;
        }

        void set(HashIntoType kmer, T item) {
            filter->count(kmer);
            data[kmer] = item;
        }

        void set_threadsafe(HashIntoType kmer, T item) {
            acquire_lock();
            set(kmer, item);
            release_lock();
        }

        bool contains(HashIntoType kmer) const {
            return get(kmer) != NULL;
        }

        uint64_t size() const {
            return data.size();
        }

        inline void acquire_lock() {
            while(!__sync_bool_compare_and_swap( &lock, 0, 1));
        }

        inline void release_lock() {
            __sync_bool_compare_and_swap( &lock, 1, 0);
        }

};


class Component;
typedef std::shared_ptr<Component> ComponentPtr;


class ComponentPtrCompare {
    public:
        bool operator() (const ComponentPtr& lhs, const ComponentPtr& rhs) const;
};


class ComponentPtrCompare;
typedef std::set<ComponentPtr, ComponentPtrCompare> ComponentPtrSet;
typedef GuardedKmerMap<ComponentPtr> GuardedKmerCompMap;


class Component {

    private:
        
        static uint64_t n_created;
        static uint64_t n_destroyed;

    public:

        const uint64_t component_id;
        std::set<HashIntoType> tags;

        explicit Component(): component_id(n_created) {
            n_created++;
        }

        explicit Component(uint64_t component_id): component_id(component_id) {
            n_created++;
        }

        ~Component() {
            n_destroyed++;
        }

        void merge(ComponentPtrSet other_comps) {
            for (auto other : other_comps) {
                if (*other == *this) {
                    continue;
                }
                this->add_tag(other->tags);
            }
        }

        uint64_t get_n_created() const {
            return n_created;
        }

        uint64_t get_n_destroyed() const {
            return n_destroyed;
        }

        void add_tag(HashIntoType tag) {
            tags.insert(tag);
        }

        void add_tag(std::set<HashIntoType>& new_tags) {
            for (auto tag: new_tags) {
                add_tag(tag);
            }
        }

        uint64_t get_n_tags() const {
            return tags.size();
        }

        friend bool operator==(const Component& lhs, const Component& rhs) {
            return lhs.component_id == rhs.component_id;
        }

        friend bool operator<(const Component& lhs, const Component& rhs) {
            return lhs.component_id < rhs.component_id;
        }

        friend std::ostream& operator<< (std::ostream& stream, const Component& comp);
};


class StreamingPartitioner {

    private:
    
        uint32_t _tag_density;

        // We're not graph's owner, simply an observer.
        // Unforunately our ownership policies elsewhere are a mess
        Hashgraph * graph;
        //std::weak_ptr<Hashgraph> graph;
        // We should exclusively own tag_component_map.
        std::shared_ptr<GuardedKmerCompMap> tag_component_map;
        std::shared_ptr<ComponentPtrSet> components;
        uint32_t components_lock;
        uint64_t n_consumed;

    public:

        explicit StreamingPartitioner(Hashgraph * graph, 
                                      uint32_t tag_density=DEFAULT_TAG_DENSITY);

        uint64_t consume(const std::string& seq);
        uint64_t consume_pair(const std::string& first,
                          const std::string& second);
        uint64_t consume_and_connect_tags(const std::string& seq,
                                      std::set<HashIntoType>& tags);
        void create_and_connect_components(std::set<HashIntoType>& tags);

        uint64_t consume_fasta(std::string const &filename);
        void map_tags_to_component(std::set<HashIntoType>& tags, ComponentPtr& comp);
        void add_component(ComponentPtr comp);
        void find_connected_tags(KmerQueue& node_q,
                                 std::set<HashIntoType>& found_tags,
                                 std::set<HashIntoType>& seen,
                                 bool truncate=false) const;

        uint64_t get_n_components() const {
            return components->size();
        }

        uint64_t get_n_tags() const {
            return tag_component_map->size();
        }

        uint64_t get_n_consumed() const {
            return n_consumed;
        }

        uint32_t get_tag_density() const {
            return _tag_density;
        }

        void merge_components(ComponentPtr& root, ComponentPtrSet& comps);

        ComponentPtr get_tag_component(HashIntoType tag) const;
        ComponentPtr get_tag_component(std::string& tag) const;
        
        ComponentPtr get_nearest_component(Kmer kmer) const;
        ComponentPtr get_nearest_component(std::string& kmer) const;

        std::weak_ptr<ComponentPtrSet> get_component_set() const {
            return std::weak_ptr<ComponentPtrSet>(components);
        }

        std::weak_ptr<GuardedKmerCompMap> get_tag_component_map() const {
            return std::weak_ptr<GuardedKmerCompMap>(tag_component_map);
        }

        inline void acquire_components() {
            while(!__sync_bool_compare_and_swap( &components_lock, 0, 1));
        }

        inline void release_components() {
            __sync_bool_compare_and_swap( &components_lock, 1, 0);
        }
};


}

#endif
