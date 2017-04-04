from libcpp.memory cimport unique_ptr
from libcpp.string import string

from wrapper cimport CpLinearAssembler, CpCompactingAssembler, CpHashgraph, CpKmer
from hashing cimport Kmer


cdef class LinearAssembler:
    cdef unique_ptr[CpLinearAssembler] _this

    cdef public object graph
    cdef CpHashgraph * _graph_ptr

    cdef public object stop_filter
    cdef CpHashgraph * _stop_filter_ptr
    
    cdef str _assemble(self, Kmer start)
    cdef str _assemble_left(self, Kmer start)
    cdef str _assemble_right(self, Kmer start)


cdef class CompactingAssembler(LinearAssembler):
    pass


