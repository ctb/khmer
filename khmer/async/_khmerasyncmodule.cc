//
// This file is part of khmer, http://github.com/ged-lab/khmer/, and is
// Copyright (C) Michigan State University, 2009-2013. It is licensed under
// the three-clause BSD license; see doc/LICENSE.txt.
// Contact: khmer-project@idyll.org
//

//
// A module for Python that exports khmer C++ library functions.
//

// Must be first.
#include <Python.h>

#include "hashtable.hh"

#include "_khmerasyncmodule.hh"
#include "../_khmermodule.hh"

using namespace khmer;

////////////////////
//
// AsyncSequenceProcessor
//
////////////////////

static void khmer_asyncseqproc_dealloc(PyObject *);
static int khmer_asyncseqproc_init(khmer_AsyncSequenceProcessorObject * self, 
        PyObject * args, PyObject * kwds);
static PyObject * khmer_asyncseqproc_new(PyTypeObject * type, 
        PyObject * args, PyObject * kwds);
static PyObject * asyncseqproc_start(PyObject * self, PyObject * args);
static PyObject * asyncseqproc_stop(PyObject * self, PyObject * args);
static PyObject * asyncseqproc_processed_iter(PyObject * self);
static PyObject * asyncseqproc_processed_iternext(PyObject * self);
static PyObject * asyncseqproc_n_parsed(PyObject * self, PyObject * args);
static PyObject * asyncseqproc_n_processed(PyObject * self, PyObject * args);
static PyObject * asyncseqproc_check_exception(PyObject * self, PyObject * args);
static PyObject * asyncseqproc_queue_load(PyObject * self, PyObject * args);

static PyMethodDef khmer_asyncseqproc_methods[] = {
    { "processed", (PyCFunction)asyncseqproc_processed_iter, METH_NOARGS, "Iterator over processed reads" },
    { "stop", asyncseqproc_stop, METH_VARARGS, "Stop processors, join threads" },
    { "start", asyncseqproc_start, METH_VARARGS, "Start processors" },
    { "n_processed", asyncseqproc_n_processed, METH_NOARGS, "Number of reads processed" },
    { "n_parsed", asyncseqproc_n_parsed, METH_NOARGS, "Number of reads parsed" },
    { "queue_load", asyncseqproc_queue_load, METH_NOARGS, "Apprx. items on writer, reader, output queues" },
    { "check_exception", asyncseqproc_check_exception, METH_NOARGS, "Check async exception state" },
    {NULL, NULL, 0, NULL}           /* sentinel */
};

PyTypeObject khmer_AsyncSequenceProcessorType = {
    PyObject_HEAD_INIT(NULL)
    0,                       /* ob_size */
    "AsyncSequenceProcessor",            /* tp_name */
    sizeof(khmer_AsyncSequenceProcessorObject), /* tp_basicsize */
    0,                       /* tp_itemsize */
    (destructor)khmer_asyncseqproc_dealloc, /* tp_dealloc */
    0,                       /* tp_print */
    0,                       /* tp_getattr */
    0,                       /* tp_setattr */
    0,                       /* tp_compare */
    0,                       /* tp_repr */
    0,                       /* tp_as_number */
    0,                       /* tp_as_sequence */
    0,                       /* tp_as_mapping */
    0,                       /* tp_hash */
    0,                       /* tp_call */
    0,                       /* tp_str */
    0,                       /* tp_getattro */
    0,                       /* tp_setattro */
    0,                       /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_ITER,   /* tp_flags */
    0,                       /* tp_doc */
    0,                       /* tp_traverse */
    0,                       /* tp_clear */
    0,                       /* tp_richcompare */
    0,                       /* tp_weaklistoffset */
    PyObject_SelfIter,                       /* tp_iter */
    (iternextfunc) asyncseqproc_processed_iternext,                       /* tp_iternext */
    khmer_asyncseqproc_methods, /* tp_methods */
    0,                       /* tp_members */
    0,                       /* tp_getset */
    0,                       /* tp_base */
    0,                       /* tp_dict */
    0,                       /* tp_descr_get */
    0,                       /* tp_descr_set */
    0,                       /* tp_dictoffset */
    (initproc)khmer_asyncseqproc_init,   /* tp_init */
    0,                       /* tp_alloc */
    khmer_asyncseqproc_new,
};


static void khmer_asyncseqproc_dealloc(PyObject* obj);

static PyObject * khmer_asyncseqproc_new(PyTypeObject *type, 
        PyObject *args, PyObject *kwds);

static int khmer_asyncseqproc_init(khmer_AsyncSequenceProcessorObject * self, 
        PyObject *args, PyObject *kwds);

static PyObject * asyncseqproc_start(PyObject * self, PyObject * args)
{
    khmer_AsyncSequenceProcessorObject * me = (khmer_AsyncSequenceProcessorObject *) self;
    AsyncSequenceProcessor * async_sp = me->async_sp;

    const char * filename;
    unsigned int n_threads = 1;
    PyObject * paired;

    if (!PyArg_ParseTuple(args, "s|OI", &filename, &paired, &n_threads)) {
        return NULL;
    }
    async_sp->start(filename, PyObject_IsTrue(paired), n_threads);

    Py_RETURN_NONE;
}

static PyObject * asyncseqproc_stop(PyObject * self, PyObject * args)
{
    khmer_AsyncSequenceProcessorObject * me = (khmer_AsyncSequenceProcessorObject *) self;
    AsyncSequenceProcessor * async_sp = me->async_sp;
    
    async_sp->stop();

    handle_exceptions(async_sp)

    Py_RETURN_NONE;
}

static PyObject * asyncseqproc_processed_iter(PyObject * self) {
    return PyObject_SelfIter(self);
}

static PyObject * asyncseqproc_processed_iternext(PyObject * self) {
    using namespace khmer::python;

    khmer_AsyncSequenceProcessorObject * me = (khmer_AsyncSequenceProcessorObject *) self;
    AsyncSequenceProcessor * async_sp = me->async_sp;
   
    ReadBatchPtr batch_ptr;
    Read * read_1_ptr;
    Read * read_2_ptr;

    while(!(async_sp->pop(batch_ptr))) {
        handle_exceptions(async_sp)
	if(async_sp->iter_stop()) {
            //async_sp->lock_stdout();
            //std::cout << "\nITER STOP: kept " << async_diginorm->n_kept() 
            //    << " of " << async_diginorm->n_processed()
            //    << " processed (" << async_diginorm->n_parsed()
            //    << " parsed)" << std::endl;
            //async_diginorm->unlock_stdout();
            //delete batch_ptr;
            PyErr_SetNone(PyExc_StopIteration);
            return NULL;
        }
    }

    if(async_sp->is_paired()) {
        
        read_1_ptr = new Read ( *batch_ptr->first() );
        read_2_ptr = new Read ( *batch_ptr->second() );

        delete batch_ptr;

        PyObject * read_1_OBJECT = Read_Type.tp_alloc( &Read_Type, 1 );
        ((Read_Object *)read_1_OBJECT)->read = read_1_ptr;
        PyObject * read_2_OBJECT = Read_Type.tp_alloc( &Read_Type, 1 );
        ((Read_Object *)read_2_OBJECT)->read = read_2_ptr;
        PyObject * tup = PyTuple_Pack( 2, read_1_OBJECT, read_2_OBJECT );
        Py_XDECREF(read_1_OBJECT);
        Py_XDECREF(read_2_OBJECT);
        return tup;
    }

    read_1_ptr = new Read ( * batch_ptr->first() );
    delete batch_ptr;

    PyObject * read_obj = khmer::python::Read_Type.tp_alloc(&khmer::python::Read_Type, 1);
    ((khmer::python::Read_Object *) read_obj)->read = read_1_ptr;
    return read_obj;
}

static PyObject * asyncseqproc_n_parsed(PyObject * self, PyObject * args)
{
    khmer_AsyncSequenceProcessorObject * me = (khmer_AsyncSequenceProcessorObject *) self;
    AsyncSequenceProcessor * async_sp = me->async_sp;

    unsigned int n = async_sp->n_parsed();

    return PyLong_FromUnsignedLongLong(n);
}

static PyObject * asyncseqproc_check_exception(PyObject * self, PyObject * args)
{
    khmer_AsyncSequenceProcessorObject * me = (khmer_AsyncSequenceProcessorObject *) self;
    AsyncSequenceProcessor * async_sp = me->async_sp;
    
    handle_exceptions(async_sp)
    Py_RETURN_NONE;
}

static PyObject * asyncseqproc_queue_load(PyObject * self, PyObject * args)
{
    khmer_AsyncSequenceProcessorObject * me = (khmer_AsyncSequenceProcessorObject *) self;
    AsyncSequenceProcessor * async_sp = me->async_sp;

    return PyLong_FromUnsignedLongLong(async_sp->parser_queue_load());
}

static PyObject * asyncseqproc_n_processed(PyObject * self, PyObject * args)
{
    khmer_AsyncSequenceProcessorObject * me = (khmer_AsyncSequenceProcessorObject *) self;
    AsyncSequenceProcessor * async_sp = me->async_sp;

    unsigned int n = async_sp->n_processed();

    return PyLong_FromUnsignedLongLong(n);
}



static void khmer_asyncseqproc_dealloc(PyObject* obj)
{
    khmer_AsyncSequenceProcessorObject * self = (khmer_AsyncSequenceProcessorObject *) obj;

    self->async_sp->stop();
    //delete self->async_diginorm;
    self->async_sp = NULL;

    obj->ob_type->tp_free((PyObject*)self);
}

static PyObject * khmer_asyncseqproc_new(PyTypeObject *type, PyObject *args,
                                      PyObject *kwds)
{
    khmer_AsyncSequenceProcessorObject *self;
    self = (khmer_AsyncSequenceProcessorObject*)type->tp_alloc(type, 0);

    if (self != NULL) {
        khmer_KCountingHashObject * counting_o;

        if (!PyArg_ParseTuple(args, "O!", &khmer_KCountingHashType, &counting_o)) {
            return NULL;
        }
        
        //self->async_sp = new AsyncSequenceProcessor(counting_o->counting);
    }

    return (PyObject *) self;
}

static int khmer_asyncseqproc_init(khmer_AsyncSequenceProcessorObject * self, PyObject *args,
                                PyObject *kwds)
{
    return 0;
}

////////////////////
// AsyncDiginorm
////////////////////

static void khmer_asyncdiginorm_dealloc(PyObject *);
static int khmer_asyncdiginorm_init(khmer_AsyncDiginormObject * self, 
        PyObject * args, PyObject * kwds);
static PyObject * khmer_asyncdiginorm_new(PyTypeObject * type, 
        PyObject * args, PyObject * kwds);
static PyObject * asyncdiginorm_start(PyObject * self, PyObject * args);
static PyObject * asyncdiginorm_n_kept(PyObject * self, PyObject * args);
static PyObject * asyncdiginorm_queue_load(PyObject * self, PyObject * args);

static PyMethodDef khmer_asyncdiginorm_methods[] = {
    { "start", asyncdiginorm_start, METH_VARARGS, "Start processing" },
    { "n_kept", asyncdiginorm_n_kept, METH_NOARGS, "Number of reads kept" },
    { "queue_load", asyncdiginorm_queue_load, METH_NOARGS, "Queue loads" },

    {NULL, NULL, 0, NULL}           /* sentinel */
};

PyTypeObject khmer_AsyncDiginormType = {
    PyObject_HEAD_INIT(NULL)
    0,                       /* ob_size */
    "AsyncDiginorm",            /* tp_name */
    sizeof(khmer_AsyncDiginormObject), /* tp_basicsize */
    0,                       /* tp_itemsize */
    (destructor)khmer_asyncdiginorm_dealloc, /* tp_dealloc */
    0,                       /* tp_print */
    0,  /* khmer_labelhash_getattr, tp_getattr */
    0,                       /* tp_setattr */
    0,                       /* tp_compare */
    0,                       /* tp_repr */
    0,                       /* tp_as_number */
    0,                       /* tp_as_sequence */
    0,                       /* tp_as_mapping */
    0,                       /* tp_hash */
    0,                       /* tp_call */
    0,                       /* tp_str */
    0,                       /* tp_getattro */
    0,                       /* tp_setattro */
    0,                       /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_ITER,   /* tp_flags */
    0,                       /* tp_doc */
    0,                       /* tp_traverse */
    0,                       /* tp_clear */
    0,                       /* tp_richcompare */
    0,                       /* tp_weaklistoffset */
    PyObject_SelfIter,                       /* tp_iter */
    0,                       /* tp_iternext */
    khmer_asyncdiginorm_methods, /* tp_methods */
    0,                       /* tp_members */
    0,                       /* tp_getset */
    &khmer_AsyncSequenceProcessorType,   /* tp_base */
    0,                       /* tp_dict */
    0,                       /* tp_descr_get */
    0,                       /* tp_descr_set */
    0,                       /* tp_dictoffset */
    (initproc)khmer_asyncdiginorm_init,   /* tp_init */
    0,                       /* tp_alloc */
    khmer_asyncdiginorm_new,
};

static PyObject * asyncdiginorm_start(PyObject * self, PyObject * args)
{
    khmer_AsyncDiginormObject * me = (khmer_AsyncDiginormObject *) self;
    AsyncDiginorm * async_diginorm = me->async_diginorm;

    const char * filename;
    unsigned int cutoff = 20;
    unsigned int n_threads = 1;
    PyObject * paired;

    if (!PyArg_ParseTuple(args, "s|IOI", &filename, &cutoff, &paired, &n_threads)) {
        return NULL;
    }
    async_diginorm->start(filename, cutoff, PyObject_IsTrue(paired), n_threads);

    Py_RETURN_NONE;
}

static PyObject * asyncdiginorm_n_kept(PyObject * self, PyObject * args)
{
    khmer_AsyncDiginormObject * me = (khmer_AsyncDiginormObject *) self;
    AsyncDiginorm * async_diginorm = me->async_diginorm;

    unsigned int n = async_diginorm->n_kept();

    return PyLong_FromUnsignedLongLong(n);
}

static PyObject * asyncdiginorm_queue_load(PyObject * self, PyObject * args)
{
    khmer_AsyncDiginormObject * me = (khmer_AsyncDiginormObject *) self;
    AsyncDiginorm * async_diginorm = me->async_diginorm;

    return PyTuple_Pack(2,
                        PyLong_FromUnsignedLongLong(async_diginorm->parser_queue_load()),
                        PyLong_FromUnsignedLongLong(async_diginorm->output_queue_load()));
}



static void khmer_asyncdiginorm_dealloc(PyObject* obj)
{
    khmer_AsyncDiginormObject * self = (khmer_AsyncDiginormObject *) obj;

    self->async_diginorm->stop();
    //delete self->async_diginorm;
    self->async_diginorm = NULL;

    obj->ob_type->tp_free((PyObject*)self);
}

static PyObject * khmer_asyncdiginorm_new(PyTypeObject *type, PyObject *args,
                                      PyObject *kwds)
{
    khmer_AsyncDiginormObject *self;
    self = (khmer_AsyncDiginormObject*)type->tp_alloc(type, 0);

    if (self != NULL) {
        khmer_KCountingHashObject * counting_o;

        if (!PyArg_ParseTuple(args, "O!", &khmer_KCountingHashType, &counting_o)) {
            return NULL;
        }
        
        self->async_diginorm = new AsyncDiginorm(counting_o->counting);
        self->async_sp.async_sp = (AsyncSequenceProcessor *)self->async_diginorm;
    }

    return (PyObject *) self;
}

static int khmer_asyncdiginorm_init(khmer_AsyncDiginormObject * self, PyObject *args,
                                PyObject *kwds)
{
    if (khmer_AsyncSequenceProcessorType.tp_init((PyObject *)self, args, kwds) < 0) {
        return -1;
    }
    return 0;
}

////////////////////
// AsyncSequenceProcessorTester
////////////////////

static void khmer_asyncsptester_dealloc(PyObject *);
static int khmer_asyncsptester_init(khmer_AsyncSequenceProcessorTesterObject * self, 
        PyObject * args, PyObject * kwds);
static PyObject * khmer_asyncsptester_new(PyTypeObject * type, 
        PyObject * args, PyObject * kwds);

static PyMethodDef khmer_asyncsptester_methods[] = {

    {NULL, NULL, 0, NULL}           /* sentinel */
};

PyTypeObject khmer_AsyncSequenceProcessorTesterType = {
    PyObject_HEAD_INIT(NULL)
    0,                       /* ob_size */
    "AsyncSequenceProcessorTester",            /* tp_name */
    sizeof(khmer_AsyncSequenceProcessorTesterObject), /* tp_basicsize */
    0,                       /* tp_itemsize */
    (destructor)khmer_asyncsptester_dealloc, /* tp_dealloc */
    0,                       /* tp_print */
    0,  /* khmer_labelhash_getattr, tp_getattr */
    0,                       /* tp_setattr */
    0,                       /* tp_compare */
    0,                       /* tp_repr */
    0,                       /* tp_as_number */
    0,                       /* tp_as_sequence */
    0,                       /* tp_as_mapping */
    0,                       /* tp_hash */
    0,                       /* tp_call */
    0,                       /* tp_str */
    0,                       /* tp_getattro */
    0,                       /* tp_setattro */
    0,                       /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_ITER,   /* tp_flags */
    0,                       /* tp_doc */
    0,                       /* tp_traverse */
    0,                       /* tp_clear */
    0,                       /* tp_richcompare */
    0,                       /* tp_weaklistoffset */
    PyObject_SelfIter,                       /* tp_iter */
    0,                       /* tp_iternext */
    khmer_asyncsptester_methods, /* tp_methods */
    0,                       /* tp_members */
    0,                       /* tp_getset */
    &khmer_AsyncSequenceProcessorType,   /* tp_base */
    0,                       /* tp_dict */
    0,                       /* tp_descr_get */
    0,                       /* tp_descr_set */
    0,                       /* tp_dictoffset */
    (initproc)khmer_asyncsptester_init,   /* tp_init */
    0,                       /* tp_alloc */
    khmer_asyncsptester_new,
};

static void khmer_asyncsptester_dealloc(PyObject* obj)
{
    khmer_AsyncSequenceProcessorTesterObject * self = (khmer_AsyncSequenceProcessorTesterObject *) obj;

    self->async_sptester->stop();
    //delete self->async_sptester;
    self->async_sptester = NULL;

    obj->ob_type->tp_free((PyObject*)self);
}

static PyObject * khmer_asyncsptester_new(PyTypeObject *type, PyObject *args,
                                      PyObject *kwds)
{
    khmer_AsyncSequenceProcessorTesterObject *self;
    self = (khmer_AsyncSequenceProcessorTesterObject*)type->tp_alloc(type, 0);

    if (self != NULL) {
        khmer_KCountingHashObject * counting_o;

        if (!PyArg_ParseTuple(args, "O!", &khmer_KCountingHashType, &counting_o)) {
            return NULL;
        }
        
        self->async_sptester = new AsyncSequenceProcessorTester(counting_o->counting);
        self->async_sp.async_sp = (AsyncSequenceProcessor *)self->async_sptester;
    }

    return (PyObject *) self;
}

static int khmer_asyncsptester_init(khmer_AsyncSequenceProcessorTesterObject * self, PyObject *args,
                                PyObject *kwds)
{
    if (khmer_AsyncSequenceProcessorType.tp_init((PyObject *)self, args, kwds) < 0) {
        return -1;
    }
    return 0;
}
