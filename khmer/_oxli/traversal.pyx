from libcpp.memory cimport unique_ptr
from cython.operator cimport dereference as deref

from wrapper cimport *
from hashing cimport Kmer
from hashing import Kmer


cdef class Traverser:

    def __cinit__(self, graph):
        self._graph_ptr = get_hashgraph_ptr(graph)
        if self._graph_ptr == NULL:
            raise ValueError('Must take an object with Hashgraph *')
        
        self._this.reset(new CpTraverser(self._graph_ptr))

    def neighbors(self, node):
        cdef Kmer kmer
        if not isinstance(node, Kmer):
            kmer = Kmer(node)
        else:
            kmer = node

        cdef KmerQueue kmer_q
        cdef CpKmer neighbor
        cdef Kmer pyneighbor
        deref(self._this).traverse(deref(kmer._this.get()), kmer_q)
        while(kmer_q.empty() == 0):
            neighbor = kmer_q.front()
            pyneighbor = Kmer.wrap(new CpKmer(neighbor), deref(self._graph_ptr).ksize())
            if pyneighbor.is_forward != kmer.is_forward:
                pyneighbor.reverse_complement()
            yield pyneighbor
            kmer_q.pop()

    def right_neighbors(self, node):
        cdef Kmer kmer
        if not isinstance(node, Kmer):
            kmer = Kmer(node)
        else:
            kmer = node

        cdef KmerQueue kmer_q
        cdef CpKmer neighbor
        cdef Kmer pyneighbor
        deref(self._this).traverse_right(deref(kmer._this.get()), kmer_q)
        while(kmer_q.empty() == 0):
            neighbor = kmer_q.front()
            pyneighbor = Kmer.wrap(new CpKmer(neighbor), deref(self._graph_ptr).ksize())
            if pyneighbor.is_forward != kmer.is_forward:
                pyneighbor.reverse_complement()
            yield pyneighbor
            kmer_q.pop()

    def left_neighbors(self, node):
        cdef Kmer kmer
        if not isinstance(node, Kmer):
            kmer = Kmer(node)
        else:
            kmer = node

        cdef KmerQueue kmer_q
        cdef CpKmer neighbor
        cdef Kmer pyneighbor
        deref(self._this).traverse_left(deref(kmer._this.get()), kmer_q)
        while(kmer_q.empty() == 0):
            neighbor = kmer_q.front()
            pyneighbor = Kmer.wrap(new CpKmer(neighbor), deref(self._graph_ptr).ksize())
            if pyneighbor.is_forward != kmer.is_forward:
                pyneighbor.reverse_complement()
            yield pyneighbor
            kmer_q.pop()

    def degree(self, node):
        cdef Kmer kmer
        if not isinstance(node, Kmer):
            kmer = Kmer(node)
        else:
            kmer = node

        return deref(self._this).degree(deref(kmer._this.get()))
    
    def left_degree(self, node):
        cdef Kmer kmer
        if not isinstance(node, Kmer):
            kmer = Kmer(node)
        else:
            kmer = node

        return deref(self._this).degree_left(deref(kmer._this.get()))

    def right_degree(self, node):
        cdef Kmer kmer
        if not isinstance(node, Kmer):
            kmer = Kmer(node)
        else:
            kmer = node

        return deref(self._this).degree_right(deref(kmer._this.get()))
