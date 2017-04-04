#include "khmer/_cpy_smallcounttable.hh"
#include "oxli/hashtable.hh"

using namespace oxli;

namespace khmer {

PyTypeObject khmer_KSmallCounttable_Type
CPYCHECKER_TYPE_OBJECT_FOR_TYPEDEF("khmer_KSmallCounttable_Object")
= {
    PyVarObject_HEAD_INIT(NULL, 0) /* init & ob_size */
    "_khmer.SmallCounttable",             /* tp_name */
    sizeof(khmer_KSmallCounttable_Object), /* tp_basicsize */
    0,                             /* tp_itemsize */
    0,                             /*tp_dealloc*/
    0,              /*tp_print*/
    0,              /*tp_getattr*/
    0,              /*tp_setattr*/
    0,              /*tp_compare*/
    0,              /*tp_repr*/
    0,              /*tp_as_number*/
    0,              /*tp_as_sequence*/
    0,              /*tp_as_mapping*/
    0,              /*tp_hash */
    0,              /*tp_call*/
    0,              /*tp_str*/
    0,              /*tp_getattro*/
    0,              /*tp_setattro*/
    0,              /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,       /*tp_flags*/
    "SmallCounttable object",           /* tp_doc */
    0,                       /* tp_traverse */
    0,                       /* tp_clear */
    0,                       /* tp_richcompare */
    0,                       /* tp_weaklistoffset */
    0,                       /* tp_iter */
    0,                       /* tp_iternext */
    0,                       /* tp_methods */
    0,                       /* tp_members */
    0,                       /* tp_getset */
    0,                       /* tp_base */
    0,                       /* tp_dict */
    0,                       /* tp_descr_get */
    0,                       /* tp_descr_set */
    0,                       /* tp_dictoffset */
    0,                       /* tp_init */
    0,                       /* tp_alloc */
    khmer_smallcounttable_new,    /* tp_new */
};


//
// khmer_smallcounttable_new
//

PyObject* khmer_smallcounttable_new(PyTypeObject * type, PyObject * args,
        PyObject * kwds)
{
    khmer_KSmallCounttable_Object * self;

    self = (khmer_KSmallCounttable_Object *)type->tp_alloc(type, 0);

    if (self != NULL) {
        WordLength k = 0;
        PyListObject * sizes_list_o = NULL;

        if (!PyArg_ParseTuple(args, "bO!", &k, &PyList_Type, &sizes_list_o)) {
            Py_DECREF(self);
            return NULL;
        }

        std::vector<uint64_t> sizes;
        if (!convert_Pytablesizes_to_vector(sizes_list_o, sizes)) {
            Py_DECREF(self);
            return NULL;
        }

        try {
            self->counttable = new SmallCounttable(k, sizes);
        } catch (std::bad_alloc &e) {
            Py_DECREF(self);
            return PyErr_NoMemory();
        }
        self->khashtable.hashtable =
            dynamic_cast<Hashtable*>(self->counttable);
    }

    return (PyObject *) self;
}

}
