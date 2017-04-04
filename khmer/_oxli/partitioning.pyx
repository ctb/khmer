# cython: c_string_type=unicode, c_string_encoding=utf8
import cython
from cython.operator cimport dereference as deref, preincrement as inc

from libcpp cimport bool
from libcpp.string cimport string
from libcpp.vector cimport vector
from libcpp.map cimport map
from libcpp.set cimport set
from libcpp.queue cimport queue
from libcpp.memory cimport unique_ptr, weak_ptr, shared_ptr, make_shared
from libcpp.utility cimport pair

from libc.stdint cimport uint32_t, uint8_t, uint64_t
from libc.limits cimport UINT_MAX

from libc.stdint cimport uintptr_t
from libc.stdio cimport FILE, fopen, fwrite, fclose, stdout, stderr, fprintf

import json
import os

from wrapper cimport *
from .._khmer import Countgraph
from .._khmer import Nodegraph
from khmer import load_countgraph, load_nodegraph

cdef class Component:

    def __cinit__(self, Component other=None):
        if other is not None:
            self._this.reset(other._this.get())

    @property 
    def component_id(self):
        return deref(self._this).component_id

    @property 
    def _n_created(self):
        return deref(self._this).get_n_created()

    @property 
    def _n_destroyed(self):
        return deref(self._this).get_n_destroyed()

    def __len__(self):
        return deref(self._this).get_n_tags()

    def __iter__(self):
        cdef HashIntoType tag
        for tag in deref(self._this).tags:
            yield tag

    def __hash__(self):
        return <uintptr_t>self._this.get()

    def __richcmp__(x, y, op):
        if op == 2:
            return x.component_id == y.component_id
        else:
            raise NotImplementedError('Operator not available.')

    @staticmethod
    cdef vector[BoundedCounterType] _tag_counts(ComponentPtr comp, CpHashgraph* graph):
        cdef uint64_t n_tags = deref(comp).get_n_tags()
        cdef vector[BoundedCounterType] counts
        counts = vector[BoundedCounterType](n_tags)
        cdef int idx
        cdef uint64_t tag
        for idx, tag in enumerate(deref(comp).tags):
            counts[idx] = deref(graph).get_count(tag)
        return counts

    @staticmethod
    def tag_counts(Component component, graph):
        cdef CpHashgraph* graph_ptr = get_hashgraph_ptr(graph)
        return Component._tag_counts(component._this, graph_ptr)

    @staticmethod
    cdef float _mean_tag_count(ComponentPtr comp, CpHashgraph * graph):
        cdef uint64_t n_tags = deref(comp).get_n_tags()
        cdef float acc = 0
        cdef uint64_t tag
        for tag in deref(comp).tags:
            acc += <float>deref(graph).get_count(tag)
        return acc / <float>n_tags

    cdef void save(self, FILE* fp):
        cdef HashIntoType tag
        cdef int i

        fprintf(fp, "{\"component_id\": %llu, \"tags\": [", deref(self._this).component_id)
        for i, tag in enumerate(deref(self._this).tags):
            if i != 0:
                fprintf(fp, ",")
            fprintf(fp, "%llu", tag)
        fprintf(fp, "]}")
    
    @staticmethod
    cdef ComponentPtr load(uint64_t component_id, list tags):
        cdef ComponentPtr comp
        cdef HashIntoType tag
        cdef int i, N = len(tags)
        comp.reset(new CpComponent(component_id))
        for i in range(N):
            tag = tags[i]
            deref(comp).add_tag(tag)
        return comp
    
    @staticmethod
    cdef Component wrap(ComponentPtr ptr):
        cdef Component comp = Component()
        comp._this = ptr
        return comp


cdef class StreamingPartitioner:

    def __cinit__(self, graph, tag_density=None):
        self._graph_ptr =  get_hashgraph_ptr(graph)
        if self._graph_ptr == NULL:
            raise ValueError('Must take an object with Hashgraph *')
        self.graph = graph

        if tag_density is None:
            self._this.reset(new CpStreamingPartitioner(self._graph_ptr))
        else:
            self._this.reset(new CpStreamingPartitioner(self._graph_ptr, tag_density))

        self._tag_component_map = deref(self._this).get_tag_component_map()
        self._components = deref(self._this).get_component_set()
        self.n_consumed = 0

    def consume(self, sequence):
        self.n_consumed += 1
        return deref(self._this).consume(sequence.encode('utf-8'))

    def consume_pair(self, first, second):
        self.n_consumed += 2
        return deref(self._this).consume_pair(first.encode('utf-8'),
                                              second.encode('utf-8'))

    def consume_fasta(self, filename):
        return deref(self._this).consume_fasta(filename.encode('utf-8'))

    def get_tag_component(self, kmer):
        cdef ComponentPtr compptr
        compptr = deref(self._this).get_tag_component(kmer.encode('utf-8'))
        if compptr == NULL:
            return None
        else:
            return Component.wrap(compptr)

    def get_nearest_component(self, kmer):
        cdef ComponentPtr compptr
        compptr = deref(self._this).get_nearest_component(kmer.encode('utf-8'))
        if compptr == NULL:
            return None
        else:
            return Component.wrap(compptr)

    def components(self):
        cdef shared_ptr[ComponentPtrSet] locked
        cdef ComponentPtr cmpptr
        lockedptr = self._components.lock()
        if lockedptr:
            for cmpptr in deref(lockedptr):
                yield Component.wrap(cmpptr)
        else:
            raise MemoryError("Can't locked underlying Component set")

    def tag_components(self):
        cdef shared_ptr[CpGuardedKmerCompMap] locked
        cdef pair[HashIntoType,ComponentPtr] cpair
        locked = self._tag_component_map.lock()
        if locked:
            for cpair in deref(locked).data:
                yield cpair.first, Component.wrap(cpair.second)
        else:
            raise MemoryError("Can't lock underlying Component set")

    def write_components(self, filename):
        cdef FILE* fp
        fp = fopen(filename.encode('utf-8'), 'wb')
        if fp == NULL:
            raise IOError('Can\'t open file.')
        
        cdef ComponentPtr cmpptr
        cdef shared_ptr[ComponentPtrSet] lockedptr
        lockedptr = self._components.lock()

        if lockedptr:      
            for cmpptr in deref(lockedptr):
                fprintf(fp, "%llu,%llu,%f\n", 
                        deref(cmpptr).component_id,
                        deref(cmpptr).get_n_tags(),
                        Component._mean_tag_count(cmpptr, self._graph_ptr))
        fclose(fp)

    def save(self, filename):
        graph_filename = '{0}.graph'.format(filename)
        comp_filename = '{0}.json'.format(filename)
        bytes_graph_filename = graph_filename.encode('utf-8')
        cdef char * c_graph_filename = bytes_graph_filename
        self.graph.save(graph_filename)

        cdef FILE* fp = fopen(comp_filename.encode('utf-8'), 'w')
        if fp == NULL:
            raise IOError('Can\'t open file.')

        fprintf(fp, "{\"graph\": \"%s\",\n\"n_components\": %llu,\n",
                c_graph_filename, deref(self._this).get_n_components())
        fprintf(fp, "\"n_tags\": %llu,\n", deref(self._this).get_n_tags())
        fprintf(fp, "\"components\": [\n")

        cdef Component comp
        cdef int i
        cdef shared_ptr[ComponentPtrSet] locked
        locked = self._components.lock()
        if locked:
            for i, comp in enumerate(self.components()):
                if i != 0:
                    fprintf(fp, ",\n")
                comp.save(fp)
        fprintf(fp, "\n]}")
        fclose(fp)

    @staticmethod
    def load(filename):

        with open(filename) as fp:
            data = json.load(fp)
        directory = os.path.dirname(filename)

        cdef object graph
        graph_filename = os.path.join(directory, data['graph'])
        try:
            graph = load_countgraph(graph_filename)
            print('Loading', graph_filename, 'as CountGraph')
        except OSError as e:
            # maybe it was a nodegraph instead
            graph = load_nodegraph(graph_filename)
            print('Loading', graph_filename, 'as NodeGraph')

        partitioner = StreamingPartitioner(graph)
        cdef ComponentPtr comp_ptr
        for comp_info in data['components']:
            comp_ptr = Component.load(comp_info['component_id'],
                                      comp_info['tags'])
            deref(partitioner._this).add_component(comp_ptr)
        return partitioner

    @property
    def component_dict(self):
        return {comp.component_id: comp for comp in self.components()}

    @property 
    def n_components(self):
        return deref(self._this).get_n_components()
        
    @property
    def n_tags(self):
        return deref(self._this).get_n_tags()

    @property
    def tag_density(self):
        return deref(self._this).get_tag_density()
