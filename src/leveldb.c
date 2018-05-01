/**
 * cpy-leveldb: Python bindings for leveldb using leveldb c api
 * (http://code.google.com/p/leveldb/)
 *
 * Copyright (C) 2011-2013  Fu Haiping (haipingf@gmail.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 **/

#include "leveldb.h"

static void Decode2UTF8(PyObject* input, PyObject** utf8, const char** ptr, size_t* size)
{
    if (PyUnicode_Check(input))
    {   
        *utf8 = PyUnicode_AsUTF8String(input);
        if (*utf8 == NULL)
        {   
            PyErr_SetString(PyExc_ValueError, "Bad string parameter.");
        }
        else
        {
            *ptr = PyString_AsString(*utf8);
            *size = PyString_Size(*utf8);
        }
    }
    else if (PyString_Check(input))
    {   
        *ptr = PyString_AsString(input);
        *size = PyString_Size(input);
    }
    else
    {
        PyErr_SetString(PyExc_ValueError, "Bad string parameter.");
    }
}

static void LevelDB_dealloc(LevelDB* self)
{
	Py_BEGIN_ALLOW_THREADS
	_XDECREF(self->_db);
	_XDECREF(self->_options);
	_XDECREF(self->_cache);
	_XDECREF(self->_env);
	_XDECREF(self->_roptions);
	Py_END_ALLOW_THREADS

	self->_db = NULL;
	self->_options = NULL;
	self->_cache = NULL;
	self->_env = NULL;
	self->_roptions = NULL;

	self->ob_type->tp_free((PyObject *)self);
}

static PyObject* LevelDB_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
	LevelDB* self = (LevelDB *)type->tp_alloc(type, 0);

	if (self != NULL) {
		self->_db = NULL;
		self->_options = NULL;
		self->_cache = NULL;
		self->_env = NULL;
		self->_roptions = NULL;
	}

	return (PyObject*)self;
}

static int LevelDB_init(LevelDB* self, PyObject* args, PyObject* kwds)
{
	// cleanup
	VERBOSE("%s","Entering LevelDB_init...\n");
	if (self->_db || self->_cache || self->_options
			|| self->_env || self->_roptions) {
		Py_BEGIN_ALLOW_THREADS
		_XDECREF(self->_db);
		_XDECREF(self->_options);
		_XDECREF(self->_cache);
		_XDECREF(self->_env);
		_XDECREF(self->_roptions);
		Py_END_ALLOW_THREADS

		self->_db = NULL;
		self->_options = NULL;
		self->_cache = NULL;
		self->_env = NULL;
		self->_roptions = NULL;
	}

	/* arguments */
	const char* db_dir = NULL;
	PyObject* create_if_missing = Py_True;
	PyObject* error_if_exists = Py_False;
	PyObject* paranoid_checks = Py_False;
	int block_cache_size = 8 * (2 << 20);
	int write_buffer_size = 4<<20;
	int block_size = 4096;
	int max_open_files = 1000;
	int block_restart_interval = 16;
	PyObject *compression = Py_False;
	const char* kwargs[] = {"filename", "create_if_missing",
        "error_if_exists", "paranoid_checks", "write_buffer_size",
        "block_size", "max_open_files", "block_restart_interval",
        "block_cache_size", "compression", 0};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, (char*)"s|O!O!O!iiiiiO!",
                (char**)kwargs, &db_dir,
		&PyBool_Type, &create_if_missing,
		&PyBool_Type, &error_if_exists,
		&PyBool_Type, &paranoid_checks,
		&write_buffer_size,
		&block_size,
		&max_open_files,
		&block_restart_interval,
		&block_cache_size,
		&PyBool_Type, &compression 
		)) {
		VERBOSE("%s", "Oooops, errors occured in parsing argument LevelDB_init.\n");
		return -1;
	}

	if (write_buffer_size <= 0 || block_size <= 0 || max_open_files <= 0 || block_restart_interval <= 0 || block_cache_size <= 0) {
		PyErr_Format(LevelDBError, "Arguments here must be greater than 0, please check.\n");
		return -1;
	}

	/* initializing LevelDBType fields */


	self->_options = leveldb_options_create(); 
	self->_cache = leveldb_cache_create_lru(block_cache_size); 
	self->_env = leveldb_create_default_env();

	self->_roptions = leveldb_readoptions_create();

	if (self->_options == NULL || self->_cache == NULL
			|| self->_env == NULL || self->_roptions == NULL) {
		_XDECREF(self->_options);
		_XDECREF(self->_cache);
		_XDECREF(self->_env);

		_XDECREF(self->_roptions);

		self->_options = NULL;
		self->_cache = NULL;
		self->_env = NULL;

		self->_roptions = NULL;
		return -1;
	}

	/* default : 'verify_checksums' option is off 
	 * and 'fill_cache' is on
	 *
	 * */
	leveldb_readoptions_set_verify_checksums(self->_roptions, 0);
	leveldb_readoptions_set_fill_cache(self->_roptions, 1);

	leveldb_options_set_create_if_missing(self->_options, (create_if_missing == Py_True) ? 1 : 0);
	leveldb_options_set_error_if_exists(self->_options, (error_if_exists == Py_True) ? 1 : 0);
	leveldb_options_set_paranoid_checks(self->_options, (paranoid_checks == Py_True) ? 1 : 0);
	leveldb_options_set_write_buffer_size(self->_options, write_buffer_size);
	leveldb_options_set_block_size(self->_options, block_size);
	leveldb_options_set_cache(self->_options, self->_cache);
	leveldb_options_set_max_open_files(self->_options, max_open_files);
	leveldb_options_set_block_restart_interval(self->_options, 8);
	leveldb_options_set_compression(self->_options, (compression == Py_True) ? 1 : 0);
	leveldb_options_set_env(self->_options, self->_env);
	leveldb_options_set_info_log(self->_options, NULL);
	char *err = NULL;

	Py_BEGIN_ALLOW_THREADS
	self->_db = leveldb_open(self->_options, db_dir, &err);

	if (err != NULL) {
		_XDECREF(self->_db);
		_XDECREF(self->_options);
		_XDECREF(self->_cache);
		_XDECREF(self->_env);

		_XDECREF(self->_roptions);

		self->_db = NULL;
		self->_options = NULL;
		self->_cache = NULL;
		self->_env = NULL;
		
		self->_roptions = NULL;
		PyErr_Format(LevelDBError, "error occurs in opening leveldb:\n\t%s\n", err);
		free(err);
	}
	Py_END_ALLOW_THREADS
	return 0;
}

static PyObject* LevelDB_Put(LevelDB* self, PyObject* args, PyObject* kwds)
{
	const char* kwargs[] = {"key", "value", "sync", 0};
	PyObject* sync = Py_False;
	char *err = NULL;
	leveldb_writeoptions_t *woptions = NULL;

	LEVELDB_DEFINE_KVBUF(key);	
	LEVELDB_DEFINE_KVBUF(value);	

    PyObject* key = NULL;
    PyObject* value = NULL;
    PyObject* key_uni = NULL;
    PyObject* val_uni = NULL;
	if (!PyArg_ParseTupleAndKeywords(args, kwds, (char*)"OO|O!", \
				(char**)kwargs, &key, &value, &PyBool_Type, &sync)) {
		return NULL;
	}

	woptions = leveldb_writeoptions_create();
	if (woptions == NULL) {
		PyErr_Format(LevelDBError, "Failed to create writeoptions.\n");
		return NULL;
	}

	Py_BEGIN_ALLOW_THREADS

    Decode2UTF8(key, &key_uni, &s_key, &i_key);
    Decode2UTF8(value, &val_uni, &s_value, &i_value);

	leveldb_writeoptions_set_sync(woptions, (sync == Py_True) ? 1 : 0);
	leveldb_put(self->_db, woptions, (const char *)s_key, (size_t)i_key, \
			(const char *)s_value, (size_t)i_value, &err);

    Py_XDECREF(key_uni);
    Py_XDECREF(val_uni);

	Py_END_ALLOW_THREADS

	if (err != NULL) {
		PyErr_Format(LevelDBError, "error occurs when put:\n\t%s\n", err);
		free(err);
		return NULL;
	}

	leveldb_writeoptions_destroy(woptions);
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject* LevelDB_Get(LevelDB* self, PyObject* args, PyObject* kwds)
{
	PyObject* verify_checksums = Py_False;
	PyObject* fill_cache = Py_True;

	const char* kwargs[] = {"key", "verify_checksums", "fill_cache", 0};
	char *value;
	size_t value_len;
	char *err = NULL;

	LEVELDB_DEFINE_KVBUF(key);

    PyObject* key = NULL;
    PyObject* key_uni = NULL;
	if (!PyArg_ParseTupleAndKeywords(args, kwds, (char*)"O|O!O!", \
				(char**)kwargs, &key, &PyBool_Type, &verify_checksums, \
				&PyBool_Type, &fill_cache))
    {
		return NULL;
    }

	Py_BEGIN_ALLOW_THREADS


	/* default : 'verify_checksums' option is off 
	 * and 'fill_cache' is on
	 *
	 * */

    Decode2UTF8(key, &key_uni, &s_key, &i_key);
	leveldb_readoptions_set_verify_checksums(self->_roptions, (verify_checksums == Py_True) ? 1 : 0);
	leveldb_readoptions_set_fill_cache(self->_roptions, (fill_cache == Py_True) ? 1 : 0);
	value = leveldb_get(self->_db, self->_roptions, (const char *)s_key, (size_t)i_key, &value_len, &err);
    Py_XDECREF(key_uni);

	/* reset readoptions _roptions to default */
	leveldb_readoptions_set_verify_checksums(self->_roptions, 0);
	leveldb_readoptions_set_fill_cache(self->_roptions, 1);

	Py_END_ALLOW_THREADS

	if (err != NULL) {
		PyErr_Format(LevelDBError, "error occurs get:\n\t%s\n", err);
		free(err);
		return NULL;
	}

    if (value_len == 0)
    {
        Py_INCREF(Py_None);
        return Py_None;
    }
	return PyString_FromStringAndSize(value, value_len);
}

static PyObject* LevelDB_Delete(LevelDB* self, PyObject* args, PyObject* kwds)
{
	PyObject* sync = Py_False;
	const char* kwargs[] = {"key", "sync", 0};
	leveldb_writeoptions_t * woptions = NULL;

	LEVELDB_DEFINE_KVBUF(key);	

    PyObject* key = NULL;
    PyObject* key_uni = NULL;
	char *err = NULL;
	if (!PyArg_ParseTupleAndKeywords(args, kwds, (char*)"O|O!", (char**)kwargs, \
				&key, &PyBool_Type, &sync))
		return NULL;
	
	woptions = leveldb_writeoptions_create();
	if (woptions == NULL) {
		PyErr_Format(LevelDBError, "Failed to create writeoptions.\n");
		return NULL;
	}
	leveldb_writeoptions_set_sync(woptions, (sync == Py_True) ? 1 : 0);

	Py_BEGIN_ALLOW_THREADS

    Decode2UTF8(key, &key_uni, &s_key, &i_key);
	leveldb_delete(self->_db, woptions, (const char *)s_key, (size_t)i_key, &err);
    Py_XDECREF(key_uni);

	Py_END_ALLOW_THREADS

	if (err != NULL) {
		PyErr_Format(LevelDBError, "error occurs when delete:\n\t%s\n", err);
		free(err);
		return NULL;
	}
	leveldb_writeoptions_destroy(woptions);

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * LevelDB_Write(LevelDB *self, PyObject *args, PyObject *kwds)
{
	const char *kwargs[] = {"writebatch", "sync", 0};
	char *err = NULL;
	WriteBatch *batch = NULL;
	PyObject *sync = Py_False;
	leveldb_writeoptions_t *woptions = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, (char *)"O!|O!", (char **)kwargs,\
				&WriteBatchType, &batch, &PyBool_Type, &sync))
		return NULL;

	woptions = leveldb_writeoptions_create();
	if (woptions == NULL) {
		PyErr_Format(LevelDBError, "Failed to create writeoptions.\n");
		return NULL;
	}
	leveldb_writeoptions_set_sync(woptions, (sync == Py_True) ? 1 : 0);

	leveldb_write(self->_db, woptions, batch->_writebatch, &err);
	if (err != NULL) {
		PyErr_Format(LevelDBError, "error occurs in leveldb write :\n\t%s\n", err);
		free(err);
		return NULL;
	}

	leveldb_writeoptions_destroy(woptions);

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * LevelDB_Close(LevelDB *self, PyObject *args)
{
	if (self->_db != NULL) {
		leveldb_close(self->_db);
		self->_db = NULL;
	}

	if (self->_options != NULL) {
		leveldb_options_destroy(self->_options);
		self->_options = NULL;
	}

	if (self->_cache != NULL) {
		leveldb_cache_destroy(self->_cache);
		self->_cache = NULL;
	}

	if (self->_env != NULL) {
		leveldb_env_destroy(self->_env);
		self->_env = NULL;
	}

	if (self->_roptions != NULL) {
		leveldb_readoptions_destroy(self->_roptions);
		self->_roptions = NULL;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

#if 0
/* dummy function */
static PyObject * LevelDB_Compare(LevelDB *self, PyObject *args)
{

	VERBOSE("Hi, I'm entering LevelDB_Compare.\n");
	int ret = 0;
	PyObject *ret_pyfunc = NULL;
	const char *a;
	const char *b;
	size_t alen, blen;
	if (!PyArg_ParseTuple(args, (char*)"t#t#", &a, &alen, &b, &blen))
		return NULL;
	ret = compare(NULL, a, alen, b, blen);
#if 0
	ret_pyfunc = PyObject_CallFunction(compare_callbak, "ss", "aaa", "bbb");

	if (ret_pyfunc == NULL) {
		PyErr_Format(LevelDBError, "error occurs in compare operation.\n");
		return 0;
	}

	ret = (int)PyInt_AsLong(ret_pyfunc);
	Py_XDECREF(ret_pyfunc);
#endif
	fprintf(stderr, "oops, LevelDB_Compare returned value: %d.\n",ret);
	Py_INCREF(Py_None);
	return Py_None;
}
#endif


static PyObject * LevelDB_Property(LevelDB *self, PyObject *args)
{

	const char *propname = NULL;
	char *propvalue;
	PyObject *result;

	if (!PyArg_ParseTuple(args, (char*)"s", &propname))
		return NULL;

	if (self->_db != NULL) {
		propvalue = leveldb_property_value(self->_db, propname);
	} else 
		return NULL;

	if (propvalue != NULL) {
		result = Py_BuildValue("s#", propvalue, strlen(propvalue));
		return result;
	} else {
		PyErr_Format(LevelDBError, "No such property available.\n");
		return NULL;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *LevelDB_RepairDB(LevelDB* self, PyObject* args, PyObject* kwds)
{
	const char* db_dir = NULL;
	PyObject* create_if_missing = Py_False;
	PyObject* error_if_exists = Py_False;
	char *err = NULL;
	const char* kwargs[] = {"filename", "create_if_missing", "error_if_exists", 0};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, (char*)"s|O!O!", (char**)kwargs,
		&db_dir,
		&PyBool_Type, &create_if_missing,
		&PyBool_Type, &error_if_exists))
		return NULL;

	leveldb_options_set_create_if_missing(self->_options, (create_if_missing == Py_True) ? 1 : 0);
	leveldb_options_set_error_if_exists(self->_options, (error_if_exists == Py_True) ? 1 : 0);

	Py_BEGIN_ALLOW_THREADS

	if (self->_db != NULL) {
		leveldb_close(self->_db);
		leveldb_repair_db(self->_options, db_dir, &err);
		if (err != NULL) {
			PyErr_Format(LevelDBError, "error occurs in repair db:\n\t%s\n", err);
			free(err);
			return NULL;
		}
	}
	self->_db = leveldb_open(self->_options, db_dir, &err);
	if (err != NULL) {
		_XDECREF(self->_db);
		_XDECREF(self->_options);
		_XDECREF(self->_cache);
		_XDECREF(self->_env);

		self->_db = NULL;
		self->_options = NULL;
		self->_cache = NULL;
		self->_env = NULL;
		PyErr_Format(LevelDBError, "error occurs in opening leveldb:\n\t%s\n", err);
		free(err);
		return NULL;
	}

	Py_END_ALLOW_THREADS

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject* LevelDB_RangeIter(LevelDB* self, PyObject* args, PyObject* kwds)
{
	LEVELDB_DEFINE_KVBUF(key_from);
	LEVELDB_DEFINE_KVBUF(key_to);
    PyObject* key_f = NULL;
    PyObject* key_t = NULL;
    PyObject* key_funi = NULL;
    PyObject* key_tuni = NULL;

	PyObject* verify_checksums = Py_False;
	PyObject* fill_cache = Py_True;
	PyObject* include_value = Py_True;
	const char* kwargs[] = {"key_from", "key_to", "verify_checksums", "fill_cache", "include_value", 0};

    VERBOSE("%s", "Entering LevelDB_RangeIter...");
	if (!PyArg_ParseTupleAndKeywords(args, kwds, (char*)"|OOO!O!O!",
        (char**)kwargs, &key_f, &key_t, &PyBool_Type, &verify_checksums, &PyBool_Type, &fill_cache, &PyBool_Type, &include_value))
		return 0;

    leveldb_readoptions_t *roptions;
    roptions = leveldb_readoptions_create();
    if (roptions == NULL) return NULL;

	leveldb_readoptions_set_verify_checksums(roptions, ((verify_checksums == Py_True) ? 1 : 0));
	leveldb_readoptions_set_fill_cache(roptions, ((fill_cache == Py_True) ? 1 : 0));

	int is_from = (key_f != NULL);
	int is_to = (key_t != NULL);

	// create iterator
	leveldb_iterator_t *iter = NULL;

	Py_BEGIN_ALLOW_THREADS
    iter = leveldb_create_iterator(self->_db, roptions);

	// if we have an iterator
	if (iter) {
		// position iterator
		if (!is_from)
            leveldb_iter_seek_to_first(iter);
		else
        {
            Decode2UTF8(key_f, &key_funi, &s_key_from, &i_key_from);
            leveldb_iter_seek(iter, s_key_from, i_key_from);
            Py_XDECREF(key_funi);
        }
	}

	Py_END_ALLOW_THREADS

	if (iter == 0)
		return PyErr_NoMemory();

	// if iterator is empty, return an empty iterator object
	if (!leveldb_iter_valid(iter)) {
		_XDECREF(iter);
		return RangeIterator_new(0, 0, 0, 0);
	}

	// otherwise, we're good
	char *s = NULL;

	if (is_to) {
        s = (char *) malloc(sizeof(char) * i_key_to);
		if (s == 0) {
			_XDECREF(iter);
			return PyErr_NoMemory();
		}

        Decode2UTF8(key_t, &key_tuni, &s_key_to, &i_key_to);
        strncpy(s, s_key_to, i_key_to);
        Py_XDECREF(key_tuni);
	}

	return RangeIterator_new(self, iter, s, (include_value == Py_True) ? 1 : 0);
}

static PyObject *LevelDB_CompactRange(LevelDB *self, PyObject *args, PyObject *kwds)
{
	const char *kwargs[] = {"range_start_key", "range_limit_key", 0};

	char *err = NULL;

	LEVELDB_DEFINE_KVBUF(start_key);
	LEVELDB_DEFINE_KVBUF(limit_key);
    PyObject* key_s = NULL;
    PyObject* key_l = NULL;
    PyObject* key_suni = NULL;
    PyObject* key_luni = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, (char*)"OO", \
				(char**)kwargs, &key_s, &key_l)) {
		return NULL;
	}
	Py_BEGIN_ALLOW_THREADS

    Decode2UTF8(key_s, &key_suni, &s_start_key, &i_start_key);
    Decode2UTF8(key_l, &key_luni, &s_limit_key, &i_limit_key);
	leveldb_compact_range(self->_db, (const char *)s_start_key, (size_t)i_start_key, \
			(const char *)s_limit_key, (size_t)i_limit_key);
    Py_XDECREF(key_suni);
    Py_XDECREF(key_luni);

	Py_END_ALLOW_THREADS

	if (err != NULL) {
		PyErr_Format(LevelDBError, "error occurs when compact range:\n\t%s\n", err);
		free(err);
		return NULL;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *LevelDB_ApproximateSizes(LevelDB *self, PyObject *args, PyObject *kwds)
{
	const char *kwargs[] = {"num_ranges", "range_start_key", "range_limit_key", 0};
	int num_ranges;
	PyObject *range_start_key_tuple = NULL;
	PyObject *range_limit_key_tuple = NULL;
	PyObject *sizes_tuple = NULL;

	if (PyArg_ParseTupleAndKeywords(args, kwds, (char *)"iO!O!", (char **)kwargs,\
				&num_ranges, 
				&PyTuple_Type, &range_start_key_tuple, 
				&PyTuple_Type, &range_limit_key_tuple)) {
		if (!PyTuple_Check(range_start_key_tuple) 
				|| !PyTuple_Check(range_limit_key_tuple)) {
			PyErr_SetString(PyExc_TypeError, "Parameters (range_start_key,range_limit_key) \
					in LevelDB_ApproximateSizes() must be tuples.\n");
			return NULL;
		}
	} else 
		return NULL;
	if (num_ranges != PyTuple_Size(range_start_key_tuple)
			&& num_ranges != PyTuple_Size(range_limit_key_tuple)) {
		PyErr_SetString(PyExc_ValueError, "Num_ranges didnot match range_start_key or range_limit_key sizes.\n");
		return NULL;
	}

	char **range_start_key = (char **)malloc(num_ranges * sizeof(char *));
	char **range_limit_key = (char **)malloc(num_ranges * sizeof(char *));

	size_t *start_len = (size_t *)malloc(num_ranges * sizeof(size_t));
	size_t *limit_len = (size_t *)malloc(num_ranges * sizeof(size_t));
	uint64_t *sizes = (uint64_t *)malloc(num_ranges * sizeof(uint64_t));

	/* extract objects from range_start_key_tuple and range_limit_key_tuple. */
	for (int i = 0; i < num_ranges; i++) {
		PyObject *obj_start_key = NULL;
		PyObject *obj_limit_key = NULL;
		char *char_obj = NULL;

		obj_start_key = PyTuple_GetItem(range_start_key_tuple, i);
		char_obj = PyString_AsString(obj_start_key);
		Py_XDECREF(obj_start_key);
		range_start_key[i] = (char *) malloc((strlen(char_obj) + 1) * sizeof(char));
		memcpy(range_start_key[i], char_obj, strlen(char_obj));
		start_len[i] = strlen(char_obj);
		//_XDECREF(char_obj);
		range_start_key[i][strlen(char_obj)] = '\0';

		obj_limit_key = PyTuple_GetItem(range_limit_key_tuple, i);
		char_obj = PyString_AsString(obj_limit_key);
		Py_XDECREF(obj_limit_key);
		range_limit_key[i] = (char *) malloc((strlen(char_obj) + 1) * sizeof(char));
		memcpy(range_limit_key[i], char_obj, strlen(char_obj));
		limit_len[i] = strlen(char_obj);
		//_XDECREF(char_obj);
		range_limit_key[i][strlen(char_obj)] = '\0';

	} /* end of objects extraction */
#if CPY_LEVELDB_DEBUG
	for (int i = 0; i < num_ranges; i++) {
		VERBOSE("num_ranges:%d\n", num_ranges);
		VERBOSE("range %d:range_start_keys:\n%s:%d\n", i, range_start_key[i], start_len[i]);
		VERBOSE("range %d:range_limit_keys:\n%s:%d\n", i, range_limit_key[i], limit_len[i]);
	}
#endif

	if (self->_db != NULL) {
		leveldb_approximate_sizes(self->_db, num_ranges, 
				(const char * const *)range_start_key, start_len,
				(const char * const *)range_limit_key, limit_len,
				sizes);
#ifdef CPY_LEVELDB_DEBUG
		for (int i = 0; i < num_ranges; i++) {
			VERBOSE("Range %d,sizes:%llu\n", i, sizes[i]);
		}
#endif
	}

	/* build return tuple. */
	sizes_tuple = PyTuple_New(num_ranges);

	if (sizes_tuple != NULL) {
		for (int i = 0; i < num_ranges; i++) {
			PyObject *tmp = NULL;
			tmp = Py_BuildValue("i", sizes[i]);
			Py_XINCREF(tmp);
			PyTuple_SetItem(sizes_tuple, i, tmp);
			Py_XDECREF(tmp);
		}
	}

	Py_INCREF(sizes_tuple);
	return sizes_tuple;
}



PyMethodDef LevelDB_methods[] = {
	{(char*)"Put",       (PyCFunction)LevelDB_Put,       METH_KEYWORDS, (char*)"add a key/value pair to database, with an optional synchronous disk write" },
	{(char*)"Get",       (PyCFunction)LevelDB_Get,       METH_KEYWORDS, (char*)"get a value from the database" },
	{(char*)"Delete",    (PyCFunction)LevelDB_Delete,    METH_KEYWORDS, (char*)"delete a value in the database" },
	{(char*)"Write",    (PyCFunction)LevelDB_Write,    METH_KEYWORDS, (char*)"apply a writebatch in database" },
	{(char*)"Property",    (PyCFunction)LevelDB_Property,    METH_KEYWORDS, (char*)"get a property value" },
	{(char*)"RepairDB",    (PyCFunction)LevelDB_RepairDB,    METH_KEYWORDS, (char*)"repair database" },
	{(char*)"RangeIter",    (PyCFunction)LevelDB_RangeIter,    METH_KEYWORDS, (char*)"range iterator" },
    {(char*)"CompactRange", (PyCFunction)LevelDB_CompactRange,  METH_KEYWORDS, (char*)"range compact" },
	{(char*)"GetApproximateSizes",    (PyCFunction)LevelDB_ApproximateSizes,    METH_KEYWORDS, (char*)"get approximate sizes." },
	{(char*)"Close",    (PyCFunction)LevelDB_Close,    METH_KEYWORDS, (char*)"close database" },
	//{(char*)"Compare",    (PyCFunction)LevelDB_Compare,    METH_KEYWORDS, (char*)"compare two objects" },
	{NULL}
};

PyTypeObject LevelDBType = {
	PyObject_HEAD_INIT(NULL)
	0,                             /*ob_size*/
	(char*)"leveldb.LevelDB",      /*tp_name*/
	sizeof(LevelDB),             /*tp_basicsize*/
	0,                             /*tp_itemsize*/
	(destructor)LevelDB_dealloc, /*tp_dealloc*/
	0,                             /*tp_print*/
	0,                             /*tp_getattr*/
	0,                             /*tp_setattr*/
	0,                             /*tp_compare*/
	0,                             /*tp_repr*/
	0,                             /*tp_as_number*/
	0,                             /*tp_as_sequence*/
	0,                             /*tp_as_mapping*/
	0,                             /*tp_hash */
	0,                             /*tp_call*/
	0,                             /*tp_str*/
	0,                             /*tp_getattro*/
	0,                             /*tp_setattro*/
	0,                             /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT,            /*tp_flags*/
	(char*)"LevelDB bindings",   /*tp_doc */
	0,                             /*tp_traverse */
	0,                             /*tp_clear */
	0,                             /*tp_richcompare */
	0,                             /*tp_weaklistoffset */
	0,                             /*tp_iter */
	0,                             /*tp_iternext */
	LevelDB_methods,             /*tp_methods */
	0,                             /*tp_members */
	0,                             /*tp_getset */
	0,                             /*tp_base */
	0,                             /*tp_dict */
	0,                             /*tp_descr_get */
	0,                             /*tp_descr_set */
	0,                             /*tp_dictoffset */
	(initproc)LevelDB_init,      /*tp_init */
	0,                             /*tp_alloc */
	LevelDB_new,                 /*tp_new */
};

/* 
 * vim:ts=4:sw=4
 **/
