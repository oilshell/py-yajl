/*
 * Copyright 2010, R. Tyler Ballance <tyler@monkeypox.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *
 *  3. Neither the name of R. Tyler Ballance nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <Python.h>

#include <yajl/yajl_parse.h>
#include <yajl/yajl_gen.h>
#include <yajl_alloc.h>
#include <yajl_buf.h>

#include "py_yajl.h"


static yajl_gen_status ProcessObject(_YajlEncoder *self, PyObject *object)
{
    yajl_gen handle = (yajl_gen)(self->_generator);
    yajl_gen_status status = yajl_gen_in_error_state;
    PyObject *iterator, *item;

    if (object == Py_None) {
        return yajl_gen_null(handle);
    }
    if (object == Py_True) {
        return yajl_gen_bool(handle, 1);
    }
    if (object == Py_False) {
        return yajl_gen_bool(handle, 0);
    }
    if (PyUnicode_Check(object)) {
        PyErr_SetObject(PyExc_TypeError, PyUnicode_FromString("Can't serialize unicode"));
        goto exit;
    }
    if (PyString_Check(object)) {
        const unsigned char *buffer = NULL;
        Py_ssize_t length;
        PyString_AsStringAndSize(object, (char **)&buffer, &length);
        return yajl_gen_string(handle, buffer, (unsigned int)(length));
    }
    if (PyInt_Check(object)) {
        long number = PyInt_AsLong(object);
        if ( (number == -1) && (PyErr_Occurred()) ) {
            return yajl_gen_in_error_state;
        }
        return yajl_gen_integer(handle, number);
    }
    if (PyLong_Check(object)) {
        long long number = PyLong_AsLongLong(object);
        char *buffer = NULL;

        if ( (number == -1) && (PyErr_Occurred()) ) {
            return yajl_gen_in_error_state;;
        }

        /*
         * Nifty trick for getting the buffer length of a long long, going
         * to convert this long long into a buffer to be handled by
         * yajl_gen_number()
         */
        unsigned int length = (unsigned int)(snprintf(NULL, 0, "%lld", number)) + 1;
        buffer = (char *)(malloc(length));
        snprintf(buffer, length, "%lld", number);
        return yajl_gen_number(handle, buffer, length - 1);
    }
    if (PyFloat_Check(object)) {
        return yajl_gen_double(handle, PyFloat_AsDouble(object));
    }
    if (PyList_Check(object)||PyGen_Check(object)||PyTuple_Check(object)) {
        /*
         * Recurse and handle the list
         */
        iterator = PyObject_GetIter(object);
        if (iterator == NULL)
            goto exit;
        status = yajl_gen_array_open(handle);
        if (status == yajl_max_depth_exceeded) {
            Py_XDECREF(iterator);
            goto exit;
        }
        while ((item = PyIter_Next(iterator))) {
            status = ProcessObject(self, item);
            Py_XDECREF(item);
        }
        Py_XDECREF(iterator);
        yajl_gen_status close_status = yajl_gen_array_close(handle);
        if (status == yajl_gen_in_error_state)
            return status;
        return close_status;
    }
    if (PyDict_Check(object)) {
        PyObject *key, *value;
        Py_ssize_t position = 0;

        status = yajl_gen_map_open(handle);
        if (status == yajl_max_depth_exceeded) goto exit;
        while (PyDict_Next(object, &position, &key, &value)) {
            PyObject *newKey = key;

            if ( (PyFloat_Check(key)) ||
                (PyLong_Check(key)) ) {

                /*
                 * Performing the conversion separately for Python 2
                 * and Python 3 to ensure we consistently generate
                 * unicode strings in both versions
                 */
                newKey = PyObject_Unicode(key);
            }

            status = ProcessObject(self, newKey);
            if (key != newKey) {
                Py_XDECREF(newKey);
            }
            if (status == yajl_gen_in_error_state) return status;
            if (status == yajl_max_depth_exceeded) goto exit;

            status = ProcessObject(self, value);
            if (status == yajl_gen_in_error_state) return status;
            if (status == yajl_max_depth_exceeded) goto exit;
        }
        return yajl_gen_map_close(handle);
    }
    else {
        PyErr_SetObject(PyExc_TypeError, PyUnicode_FromString("Not serializable to JSON"));
        goto exit;
    }

    exit:
        return yajl_gen_in_error_state;
}

yajl_alloc_funcs *y_allocs = NULL;
/* a structure used to pass context to our printer function */
struct StringAndUsedCount
{
    PyObject * str;
    size_t used;
};

static void py_yajl_printer(void * ctx,
                            const char * str,
                            unsigned int len)
{
    struct StringAndUsedCount * sauc = (struct StringAndUsedCount *) ctx;
    size_t newsize;

    if (!sauc || !sauc->str) return;

    /* resize our string if necc */
    newsize = Py_SIZE(sauc->str);
    while (sauc->used + len > newsize) newsize *= 2;
    if (newsize != Py_SIZE(sauc->str)) {
        _PyString_Resize(&(sauc->str), newsize);
        if (!sauc->str)
            return;
    }

    /* and append data if available */
    if (len && str) {
        memcpy((void *) (((PyStringObject *) sauc->str)->ob_sval + sauc->used), str, len);
        sauc->used += len;
    }
}

/* Efficiently allocate a python string of a fixed size containing uninitialized memory */
static PyObject * lowLevelStringAlloc(Py_ssize_t size)
{
    PyStringObject * op = (PyStringObject *)PyObject_MALLOC(sizeof(PyStringObject) + size);
    if (op) {
        PyObject_INIT_VAR(op, &PyString_Type, size);
        op->ob_shash = -1;
        op->ob_sstate = SSTATE_NOT_INTERNED;
    }
    return (PyObject *) op;
}

PyObject *_internal_encode(_YajlEncoder *self, PyObject *obj, char* spaces)
{
    yajl_gen generator = NULL;
    yajl_gen_status status;
    struct StringAndUsedCount sauc;

    /* initialize context for our printer function which
     * performs low level string appending, using the python
     * string implementation as a chunked growth buffer */
    sauc.used = 0;
    sauc.str = lowLevelStringAlloc(PY_YAJL_CHUNK_SZ);

    //yajl_gen_config config = {0, NULL};
    //generator = yajl_gen_alloc2(py_yajl_printer, &genconfig, NULL, (void *) &sauc);
    //generator = yajl_gen_alloc2(py_yajl_printer, NULL, (void *) &sauc);
    generator = yajl_gen_alloc(NULL);
    if (spaces) {
        yajl_gen_config(generator, yajl_gen_beautify, 1);
        yajl_gen_config(generator, yajl_gen_indent_string, spaces);
    }

    self->_generator = generator;

    status = ProcessObject(self, obj);

    // Oil patch: usage copied from json_reformat.c
    const unsigned char * buf;
    size_t len;
    yajl_gen_get_buf(generator, &buf, &len);
    PyObject* result = PyString_FromStringAndSize(buf, len);

    //fwrite(buf, 1, len, stdout);
    yajl_gen_clear(generator);

    yajl_gen_free(generator);
    self->_generator = NULL;

    return result;
}
