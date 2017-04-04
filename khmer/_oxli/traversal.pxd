from libcpp.memory cimport unique_ptr

from wrapper cimport CpTraverser, CpHashgraph

cdef class Traverser:
    cdef unique_ptr[CpTraverser] _this
    cdef CpHashgraph * _graph_ptr



