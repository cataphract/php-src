/* 
   +----------------------------------------------------------------------+
   | PHP Version 5                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2012 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Gustavo Lopes <cataphract@php.net>                          |
   +----------------------------------------------------------------------+
*/

#include <php.h>
#include <math.h>
#include <assert.h>
#include <Zend/zend_llist.h>
#include <Zend/zend_ini.h> /* for XtOffsetOf */

enum ps_type {
	PART_SPEC_SINGLE_INDEX,
	PART_SPEC_INDEX_LIST,
	PART_SPEC_SPAN
};

union part_spec {
	enum ps_type	type;
	struct {
		enum ps_type	 type;
		zval			 *index;		/* must call zval_ptr_dtor */
	} single_index;
	struct {
		enum ps_type	 type;
		long			 num_indexes;
		zval			 *indexes[1];	/* must call zval_ptr_dtor */
	} index_list;
	struct {
		enum ps_type	 type;
		zval			 *start;		/* must call zval_ptr_dtor */
		zval			 *end;			/* must call zval_ptr_dtor */
		long			 step;
	} index_span;
};

struct dir_acc_cache {
	long leftmost;
	long rightmost;
	HashPosition positions[1];
};

enum part_type {
	PART_TYPE_SINGLE,
	PART_TYPE_MULTIPLE,
	PART_TYPE_NONE
};

static int _array_sanitize_index(zval **arg, /* should've been addref'ed */
								 ulong part_num,
								 zend_bool key_mode TSRMLS_DC)
{
	switch (Z_TYPE_PP(arg)) {
	case IS_LONG:
		break;
	case IS_DOUBLE:
		SEPARATE_ZVAL(arg);
double_case: /* already separated */
		if (Z_DVAL_PP(arg) < LONG_MIN || Z_DVAL_PP(arg) > LONG_MAX) {
			php_error_docref0(NULL TSRMLS_CC, E_WARNING, "In part "
				"specification with index '%lu', found number out of bounds: "
				"'%f'",	part_num, Z_DVAL_PP(arg));
			return FAILURE;
		}
		ZVAL_LONG(*arg, (long)Z_DVAL_PP(arg));
		break;
	case IS_STRING: {
		int result;
		long lval;
		double dval;
string_case:

		result = is_numeric_string(Z_STRVAL_PP(arg), Z_STRLEN_PP(arg),
				&lval, &dval, 0);
		if (result == 0) {
			if (key_mode) {
				break; /* do nothing */
			}
			php_error_docref0(NULL TSRMLS_CC, E_WARNING, "In part "
				"specification with index '%lu', expected only numeric values, "
				"but the string '%s' does not satisfy this requirement",
				part_num, Z_STRVAL_PP(arg));
			return FAILURE;
		} else if (result == IS_LONG) {
			SEPARATE_ZVAL(arg);
			STR_FREE(Z_STRVAL_PP(arg));
			ZVAL_LONG(*arg, lval);
		} else if (result == IS_DOUBLE) {
			SEPARATE_ZVAL(arg);
			STR_FREE(Z_STRVAL_PP(arg));
			ZVAL_DOUBLE(*arg, dval);
			goto double_case;
		}

		break;
	}
	case IS_OBJECT:
		convert_to_string_ex(arg);
		goto string_case;

	default:
		php_error_docref0(NULL TSRMLS_CC, E_WARNING, "In part specification "
				"with index '%lu', expected only numeric values, but "
				"an incompatible data type has been found", part_num);
		return FAILURE;
	}
	
	return SUCCESS;
}

static void _array_addref_or_nullify(zval **orig, zval **target)
{
	if (orig != NULL) {
		*target = *orig;
		zval_add_ref(target); /* or orig */
	} else {
		ALLOC_INIT_ZVAL(*target);
	}
}

static void _array_zval_ptr_dtor(zval **arr /* array of zval* */, int size)
{
	int i;
	for (i = 0; i < size; i++) {
		zval_ptr_dtor(&arr[i]);
	}
}

static void _array_destroy_single_part_spec(union part_spec *ps)
{
	if (ps->type == PART_SPEC_INDEX_LIST) {
		long j;
		for (j = 0; j < ps->index_list.num_indexes; j++) {
			zval_ptr_dtor(&ps->index_list.indexes[j]);
		}
	} else if (ps->type == PART_SPEC_SPAN) {
		zval_ptr_dtor(&ps->index_span.start);
		zval_ptr_dtor(&ps->index_span.end);
	} else { /* PART_SPEC_SINGLE_INDEX */
		zval_ptr_dtor(&ps->single_index.index);
	}
	efree(ps);
}

static int _array_build_ind_part_spec(zval *arg,
									  ulong part_num,
									  zend_bool key_mode,
									  union part_spec **part_spec TSRMLS_DC)
{
	int				ret = FAILURE;
	union part_spec *ps = NULL;
	
	if (Z_TYPE_P(arg) == IS_ARRAY) { /* array part spec */
		HashTable	*ht				= Z_ARRVAL_P(arg);
		zval		**start			= NULL,
					**end			= NULL;
		int			recogn_elems	= 0;

		if (zend_hash_find(ht, "start", sizeof("start"), (void**)&start)
				== SUCCESS) {
			recogn_elems++;
		}
		if (zend_hash_find(ht, "end", sizeof("end"), (void**)&end)
				== SUCCESS) {
			recogn_elems++;
		}

		if (recogn_elems != 0) { /* PART_SPEC_SPAN */
			zval	*start_end[2]	= {NULL, NULL}, 
					*step_zv,
					**orig_step;
			long	step			= 1;
			int		i;

			if (zend_hash_find(ht, "step", sizeof("step"), (void**)&orig_step)
					== SUCCESS) {
				recogn_elems++;
				step_zv = *orig_step;
				zval_add_ref(&step_zv);
				if (_array_sanitize_index(&step_zv, part_num, 0 TSRMLS_CC)
						== FAILURE) {
					zval_ptr_dtor(&step_zv);
					goto error;
				}
				step = Z_LVAL_P(step_zv);
				zval_ptr_dtor(&step_zv);

				if (step == 0) {
					php_error_docref0(NULL TSRMLS_CC, E_WARNING,
						"In part specification with index '%lu', a step of "
						"size 0 was specified", part_num);
					goto error;
				}
			}

			if (recogn_elems != zend_hash_num_elements(ht)) {
				php_error_docref0(NULL TSRMLS_CC, E_WARNING,
					"In part specification with index '%lu', found span "
					"specification with extraneous elements", part_num);
			}

			_array_addref_or_nullify(start, &start_end[0]);
			_array_addref_or_nullify(end, &start_end[1]);

			for (i = 0; i < 2; i++) {
				if (Z_TYPE_P(start_end[i]) != IS_NULL &&
						_array_sanitize_index(&start_end[i], part_num,
						key_mode TSRMLS_CC)	== FAILURE) {
					_array_zval_ptr_dtor(start_end, 2);
					goto error;
				}
			}

			ps = emalloc(sizeof ps->index_span);
			ps->type = PART_SPEC_SPAN;
			ps->index_span.start = start_end[0];
			ps->index_span.end = start_end[1];
			ps->index_span.step = step;
		} else { /* PART_SPEC_INDEX_LIST*/
			int				count = zend_hash_num_elements(ht);
			HashPosition	pos;
			ulong			j;

			if (count == 0) {
				php_error_docref0(NULL TSRMLS_CC, E_WARNING,
					"Part specification with index '%lu' is empty", part_num);
				goto error;
			}

			ps = safe_emalloc(count, sizeof(zval*), sizeof(ps->index_list));
			ps->type = PART_SPEC_INDEX_LIST;
			ps->index_list.num_indexes = 0;

			zend_hash_internal_pointer_reset_ex(ht, &pos);
			for (j = 0; j < (unsigned long)count; j++) {
				char	*str_index;
				uint	str_length;
				ulong	num_index;
				zval	**ht_elem,
						*elem;

				if (zend_hash_get_current_key_ex(ht, &str_index, &str_length,
						&num_index, 0, &pos) != HASH_KEY_IS_LONG) {
					php_error_docref0(NULL TSRMLS_CC, E_WARNING,
						"List of indexes in part specification with index "
						"'%lu' should be a numeric array, but found string "
						"index '%s'", part_num, str_index);
					goto error;
				}
				if (num_index != j) {
					php_error_docref0(NULL TSRMLS_CC, E_WARNING,
						"List of indexes in part specification with index "
						"'%lu' should be a numeric array, but either found "
						"non-sequential keys or the first key was "
						"not 0 (expected '%lu', got '%ld')", part_num, j,
						(long)num_index);
					goto error;
				}

				zend_hash_get_current_data_ex(ht, (void**)&ht_elem, &pos);
				elem = *ht_elem;
				zval_add_ref(&elem);
				if (_array_sanitize_index(&elem, part_num, key_mode TSRMLS_CC)
						== FAILURE) {
					zval_ptr_dtor(&elem);
					goto error;
				}

				ps->index_list.indexes[j] = elem;
				ps->index_list.num_indexes++;
				
				zend_hash_move_forward_ex(ht, &pos);
			}
		}
	} else { /* non array part, PART_SPEC_SINGLE_INDEX */
		zval_add_ref(&arg);

		if (_array_sanitize_index(&arg, part_num, key_mode TSRMLS_CC)
				== SUCCESS) {
			ps = emalloc(sizeof ps->single_index);
			ps->type = PART_SPEC_SINGLE_INDEX;
			ps->single_index.index = arg;
		} else {
			zval_ptr_dtor(&arg);
			goto error;
		}
	}
	
	ret = SUCCESS;

error:
	if (ret != SUCCESS && ps != NULL) {
		_array_destroy_single_part_spec(ps);
		ps = NULL;
	}

	*part_spec = ps;
	return ret;
}

static void _array_destroy_part_specs(union part_spec **part_specs,
									  int num_parts)
{
	int i;
	for (i = 0; i < num_parts; i++) {
		union part_spec *ps = part_specs[i];
		if (ps == NULL) {
			/* if the list was only partially constructed */
			break;
		}
		_array_destroy_single_part_spec(ps);
	}
	efree(part_specs);
}

static int _array_build_part_specs(HashTable *arg,
								   union part_spec ***part_specs,
								   int *num_parts,
								   zend_bool key_mode TSRMLS_DC)
{
	int				count = *num_parts = zend_hash_num_elements(arg);
	int				i;
	HashPosition	pos;
	
	if (count == 0) {
		php_error_docref0(NULL TSRMLS_CC, E_WARNING,
				"Empty list of part specifications given");
		return FAILURE;
	}
	*part_specs = safe_emalloc(count, sizeof(*part_specs), 0);
	for (i = 0; i < count; i++) {
		(*part_specs)[i] = NULL;
	}
	
	zend_hash_internal_pointer_reset_ex(arg, &pos);
	for (i = 0; i < count; i++) {
		ulong	 num_key;
		char	 *str_key = NULL;
		uint	 str_key_len;
		zval	 **elem = NULL;
		
		if (zend_hash_get_current_key_ex(arg, &str_key, &str_key_len,
				&num_key, 0, &pos) != HASH_KEY_IS_LONG) {
			php_error_docref0(NULL TSRMLS_CC, E_WARNING,
					"List of part specifications should be a numeric array, "
					"but found string index '%s'", str_key);
			goto error;
		}
		if (num_key != (unsigned)i) {
			php_error_docref0(NULL TSRMLS_CC, E_WARNING,
					"List of part specifications should be a numeric array, "
					"but found non-sequential keys or the first key was "
					"not 0 (expected '%d', got '%ld')", i, (long)num_key);
			goto error;
		}
		
		zend_hash_get_current_data_ex(arg, (void**)&elem, &pos);
		
		if (_array_build_ind_part_spec(*elem, i, key_mode,
				&(*part_specs)[i] TSRMLS_CC) == FAILURE) {
			goto error;
		}
		
		zend_hash_move_forward_ex(arg, &pos);
	}

	return SUCCESS;

error:
	_array_destroy_part_specs(*part_specs, count);

	return FAILURE;
}

static void _array_do_unit(zval *elem,
						   zval *w,
						   zend_llist *new_read_targets,
						   zend_llist *new_write_targets,
						   enum part_type next_type)
{
	switch (next_type) {
	case PART_TYPE_NONE:
		SEPARATE_ARG_IF_REF(elem); /* simply increments refcount if not ref */
		zend_hash_next_index_insert(Z_ARRVAL_P(w), &elem, sizeof(elem), NULL);
		break;
	case PART_TYPE_MULTIPLE: {
		zval *new_sub_arr;

		MAKE_STD_ZVAL(new_sub_arr);
		array_init(new_sub_arr);

		zend_hash_next_index_insert(Z_ARRVAL_P(w), &new_sub_arr,
			sizeof(new_sub_arr), NULL);

		zend_llist_add_element(new_read_targets, &elem);
		zend_llist_add_element(new_write_targets, &new_sub_arr);
		break;
	}
	case PART_TYPE_SINGLE: {
		zend_llist_add_element(new_read_targets, &elem);
		zend_llist_add_element(new_write_targets, &w);
		break;
	}
	default:
		assert(0);
	}
}

static int _array_part_common(HashTable *ht, int *count TSRMLS_DC)
{
	*count = zend_hash_num_elements(ht);

	if (*count == 0) {
		php_error_docref0(NULL TSRMLS_CC, E_WARNING,
			"Tried to get part of empty array");
		return FAILURE;
	}

	return SUCCESS;
}

static int _array_validate_offset(int count,
								  long *offset TSRMLS_DC) /* in/out */
{
	if (*offset >= 0) {
		if (*offset >= count) {
			php_error_docref0(NULL TSRMLS_CC, E_WARNING,
				"Tried to get offset '%ld' from array with only %d elements",
				*offset, count);
			return FAILURE;
		}
	} else {
		if (-(*offset + 1) >= count) {
			php_error_docref0(NULL TSRMLS_CC, E_WARNING,
				"The offset '%ld' is too large in absolute value when "
				"accessing an array with only %d elements", *offset, count);
			return FAILURE;
		}
		*offset = count + *offset;
	}

	return SUCCESS;
}

static int _array_get_part_offset(HashTable *ht,
								  long offset,
								  struct dir_acc_cache *cache, /* NULL OK */
								  HashPosition *pos TSRMLS_DC)
{
	int				count;
	long			cur_offset;
	int				move_forward;
	int				result;

	if (_array_part_common(ht, &count TSRMLS_CC) == FAILURE) {
		return FAILURE;
	}

	if (_array_validate_offset(count, &offset TSRMLS_CC) == FAILURE) {
		return FAILURE;
	}

	if (cache == NULL) {
		if (offset < count / 2) {
			cur_offset = 0;
			move_forward = 1;
			zend_hash_internal_pointer_reset_ex(ht, pos);
		} else {
			cur_offset = count - 1;
			move_forward = 0;
			zend_hash_internal_pointer_end_ex(ht, pos);
		}
	} else {
		if (offset <= cache->leftmost || offset >= cache->rightmost) {
			*pos = cache->positions[offset];
			/*php_printf("requested %ld, cache hit\n", offset);*/
			return SUCCESS;
		}
		
		if (offset - cache->leftmost < cache->rightmost - offset) {
			cur_offset = cache->leftmost;
			move_forward = 1;
			cache->leftmost = offset;
		} else {
			cur_offset = cache->rightmost;
			move_forward = 0;
			cache->rightmost = offset;
		}
		*pos = cache->positions[cur_offset];
		/*php_printf("requested %ld, starting with %ld, move forward: %d\n",
				offset, cur_offset, move_forward);*/
	}

	if (move_forward) {
		while (cur_offset < offset) {
			result = zend_hash_move_forward_ex(ht, pos);
			assert(result == SUCCESS);
			cur_offset++;
			
			if (cache != NULL) {
				cache->positions[cur_offset] = *pos;
			}
		}
	} else {
		while (cur_offset > offset) {
			result = zend_hash_move_backwards_ex(ht, pos);
			assert(result == SUCCESS);
			cur_offset--;
			
			if (cache != NULL) {
				cache->positions[cur_offset] = *pos;
			}
		}
	}

	return SUCCESS;
}

static int _array_get_part_key(HashTable *ht,
							   zval *key,
							   zval ***elem,
							   HashPosition *out TSRMLS_DC)
{
	int				count;
	int				result = SUCCESS;

	if (_array_part_common(ht, &count TSRMLS_CC) == FAILURE) {
		return FAILURE;
	}

	if (Z_TYPE_P(key) == IS_LONG) {
		result = zend_hash_index_find(ht, (ulong)Z_LVAL_P(key), (void**)elem);
		if (result == FAILURE) {
			php_error_docref0(NULL TSRMLS_CC, E_WARNING,
					"Tried to get element with key '%ld' from array that does "
					"not have it", Z_LVAL_P(key));
		}
	} else if (Z_TYPE_P(key) == IS_STRING) {
		/* we don't need to use the symtable functions because we've
		 * already converted strings to numbers before */
		result = zend_hash_find(ht, Z_STRVAL_P(key), Z_STRLEN_P(key) + 1,
				(void**)elem);
		if (result == FAILURE) {
			php_error_docref0(NULL TSRMLS_CC, E_WARNING,
					"Tried to get element with key '%s' from array that does "
					"not have it", Z_STRVAL_P(key));
		}
	} else {
		assert(0);
	}

	if (result == SUCCESS) {
		/* Let's beat the hell out of the abstraction: */
		*out = (HashPosition)((char*)*elem -
				XtOffsetOf(struct bucket, pDataPtr));
	}

	return result;
}

#define DO_PART_FUNC(name) \
	static int _array_do_ ## name(HashTable *ht, \
								  zval *w, \
								  union part_spec *ps, \
								  zend_llist *new_read_targets, \
								  zend_llist *new_write_targets, \
								  enum part_type next_type TSRMLS_DC)

DO_PART_FUNC(single_index_offset)
{
	zval			**val;
	HashPosition	pos;
	int				result;

	assert(Z_TYPE_P(ps->single_index.index) == IS_LONG);

	result = _array_get_part_offset(ht, Z_LVAL_P(ps->single_index.index), NULL,
			&pos TSRMLS_CC);
	if (result == SUCCESS) {
		result = zend_hash_get_current_data_ex(ht, (void**)&val, &pos);
		assert(result == SUCCESS);
		_array_do_unit(*val, w, new_read_targets, new_write_targets, next_type);
	}
	return result;
}

DO_PART_FUNC(single_index_key)
{
	zval			**val;
	HashPosition	pos;
	int				result;

	result = _array_get_part_key(ht, ps->single_index.index, &val,
			&pos TSRMLS_CC);
	if (result == SUCCESS) {
		_array_do_unit(*val, w, new_read_targets, new_write_targets, next_type);
	}
	return result;
}

static struct dir_acc_cache *_array_build_dir_acc_cache(long ind_num,
														HashTable *ht)
{
	struct dir_acc_cache	*cache = NULL;
	int						r_count;

	if (ind_num > 1 && (r_count = zend_hash_num_elements(ht)) > 4) {
		int i;

		cache = safe_emalloc(r_count, sizeof(*cache->positions),
			sizeof(*cache));
		for (i = 0; i < r_count; i++) {
			cache->positions[i] = 0;
		}

		cache->leftmost = 0;
		cache->rightmost = r_count - 1;
		zend_hash_internal_pointer_reset_ex(ht,
			&cache->positions[cache->leftmost]);
		zend_hash_internal_pointer_end_ex(ht,
			&cache->positions[cache->rightmost]);
	}

	return cache;
}

DO_PART_FUNC(multiple_indexes_offset)
{
	long					i,
							ind_num = ps->index_list.num_indexes;
	struct dir_acc_cache	*cache;
	int						result = SUCCESS;

	cache = _array_build_dir_acc_cache(ind_num, ht);

	for (i = 0; i < ind_num; i++) {
		HashPosition pos;

		assert(Z_TYPE_P(ps->index_list.indexes[i]) == IS_LONG);
		result = _array_get_part_offset(ht, Z_LVAL_P(ps->index_list.indexes[i]),
			cache, &pos TSRMLS_CC);
		if (result == SUCCESS) {
			zval **val;
			result = zend_hash_get_current_data_ex(ht, (void**)&val, &pos);
			assert(result == SUCCESS);
			_array_do_unit(*val, w, new_read_targets, new_write_targets,
				next_type);
		} else {
			break;
		}
	}

	if (cache) {
		efree(cache);
	}

	return result;
}

DO_PART_FUNC(multiple_indexes_key)
{
	long	i;
	int		result = SUCCESS;

	for (i = 0; i < ps->index_list.num_indexes; i++) {
		HashPosition	pos;
		zval			**val;

		result = _array_get_part_key(ht, ps->index_list.indexes[i], &val,
				&pos TSRMLS_CC);
		if (result == SUCCESS) {
			_array_do_unit(*val, w, new_read_targets, new_write_targets,
				next_type);
		} else {
			break;
		}
	}

	return result;
}

DO_PART_FUNC(span_offset)
{
	int						result,
							count_r;
	long					start,
							end,
							step = ps->index_span.step,
							i;
	HashPosition			pos;

	if (Z_TYPE_P(ps->index_span.start) == IS_NULL) {
		start = step > 0 ? 0 : -1;
	} else {
		assert(Z_TYPE_P(ps->index_span.start) == IS_LONG);
		start = Z_LVAL_P(ps->index_span.start);
	}
	if (Z_TYPE_P(ps->index_span.end) == IS_NULL) {
		end = step > 0 ? -1 : 0;
	} else {
		assert(Z_TYPE_P(ps->index_span.end) == IS_LONG);
		end = Z_LVAL_P(ps->index_span.end);
	}

	/* As an exception, if we request all elements allow empty arrays */
	if ((count_r = zend_hash_num_elements(ht)) == 0) {
		if ((step == 1 && start == 0 && end == -1) ||
				(step == -1 && start == -1 && end == 0)) {
			return SUCCESS;
		} else {
			php_error_docref0(NULL TSRMLS_CC, E_WARNING,
				"Tried to get part of empty array with span specification that "
				"does not include all elements");
			return FAILURE;
		}
	}

	if (_array_validate_offset(count_r, &start TSRMLS_CC) == FAILURE
			|| _array_validate_offset(count_r, &end TSRMLS_CC) == FAILURE) {
		return FAILURE;
	}
	
	if ((step > 0 && end < start) || (step < 0 && start < end)) {
		return SUCCESS; /* nothing to do */
	}
	
	/* locate start */
	result = _array_get_part_offset(ht, start, NULL, &pos TSRMLS_CC);
	assert(result == SUCCESS);
	
	i = start;
	
	if (step > 0) {
		do {
			zval	**val;
			long	left = step;
			
			result = zend_hash_get_current_data_ex(ht, (void**)&val, &pos);
			assert(result == SUCCESS);
			_array_do_unit(*val, w, new_read_targets, new_write_targets,
				next_type);
			
			while (left--) {
				i++;
				zend_hash_move_forward_ex(ht, &pos);
			}
		} while (i <= end);
	} else {
		do {
			zval	**val;
			long	left = step;
			
			result = zend_hash_get_current_data_ex(ht, (void**)&val, &pos);
			assert(result == SUCCESS);
			_array_do_unit(*val, w, new_read_targets, new_write_targets,
				next_type);
			
			while (left++) {
				i--;
				zend_hash_move_backwards_ex(ht, &pos);
			}
		} while (i >= end);
	}
	
	return SUCCESS;
}

/* "end" in the sense of extremity */
static int _array_position_end(HashTable *ht,
							   zval *key,
							   int def_on_end,
							   HashPosition *pos TSRMLS_DC)
{
	int result = SUCCESS;
	if (Z_TYPE_P(key) == IS_NULL) {
		if (!def_on_end) {
			zend_hash_internal_pointer_reset_ex(ht, pos);
		} else {
			zend_hash_internal_pointer_end_ex(ht, pos);
		}
	} else {
		zval **val;
		result = _array_get_part_key(ht, key, &val, pos TSRMLS_CC);
	}
	
	return result;
}

DO_PART_FUNC(span_key)
{
	int						result,
							count_r;
	long					step = ps->index_span.step;
	HashPosition			pos_start,
							pos_end;
	zval					***elems;
	long					saved_elems;
	
	/* As an exception, if we request all elements allow empty arrays */
	if ((count_r = zend_hash_num_elements(ht)) == 0) {
		if (Z_TYPE_P(ps->index_span.start) == IS_NULL &&
				Z_TYPE_P(ps->index_span.end) == IS_NULL) {
			return SUCCESS;
		} else {
			php_error_docref0(NULL TSRMLS_CC, E_WARNING,
				"Tried to get part of empty array with span specification that "
				"does not include all elements");
			return FAILURE;
		}
	}


	if (_array_position_end(ht, ps->index_span.start, step < 0,
			&pos_start TSRMLS_CC) == FAILURE) {
		return FAILURE;
	}
	if (_array_position_end(ht, ps->index_span.end, step > 0,
			&pos_end TSRMLS_CC)	== FAILURE) {
		return FAILURE;
	}
	
	elems = safe_emalloc(count_r, sizeof(*elems), 0);
	saved_elems = 0;
	
	{
		HashPosition	pos_curr = pos_start;
		int				(*move_func)(HashTable *, HashPosition*);
		int				rel_pos;
		/* rel_pos: -1 continue, 0 last iteration, 1 stop iterating */
		
		move_func = step > 0
				? zend_hash_move_forward_ex
				: zend_hash_move_backwards_ex;
		if (step > 0) {
			step = -step;
		}
		
		rel_pos = (pos_curr != pos_end) ? -1 : 0;
		result = SUCCESS;
		do {
			long left = step;
			
			result = zend_hash_get_current_data_ex(ht,
					(void**)&elems[saved_elems], &pos_curr);
			assert(result == SUCCESS);
			saved_elems++;
			
			if (rel_pos == 0) {
				break;
			}
			
			while (result == SUCCESS && rel_pos == -1 && left++) {
				result = move_func(ht, &pos_curr);
				if (pos_curr == pos_end) {
					if (left == 0) {
						rel_pos = 0; /* we want another outer iteration */
					} else {
						rel_pos = 1;
					}
				}
			}
		} while (pos_curr != NULL && rel_pos != 1);

		/* signal to the if() below we got past one end */
		if (pos_curr == NULL) {
			result = FAILURE;
		}
	}
	
	if (result == SUCCESS) {
		long i;
		for (i = 0; i < saved_elems; i++) {
			_array_do_unit(*elems[i], w, new_read_targets, new_write_targets,
				next_type);
		}
	} /* else start comes after end (or before, if step was < 0) */
	
	efree(elems);
	
	return SUCCESS;	
}

static int _array_do_part(union part_spec *ps,
						  int part_num,
						  enum part_type next_type,
						  zend_llist **read_targets,	/* read/write */
						  zend_llist **write_targets,	/* read/write */
						  zend_bool key_mode TSRMLS_DC)
{
	zend_llist			*new_read_targets	= emalloc(sizeof *new_read_targets);
	zend_llist			*new_write_targets	= emalloc(sizeof *new_write_targets);
	int					count				= zend_llist_count(*read_targets);
	zend_llist_position	rpos,
						wpos;
	zval				**r,
						**w;
	int					(*part_func)(HashTable*, zval*, union part_spec*,
								zend_llist*, zend_llist*, enum part_type
								TSRMLS_DC);
	int					result = SUCCESS;

	zend_llist_init(new_read_targets, sizeof(zval*), NULL, 0);
	zend_llist_init(new_write_targets, sizeof(zval*), NULL, 0);

	assert (count == zend_llist_count(*write_targets));

	if (ps->type == PART_SPEC_SINGLE_INDEX) {
		part_func = !key_mode
			? _array_do_single_index_offset
			: _array_do_single_index_key;
	} else if (ps->type == PART_SPEC_INDEX_LIST) {
		part_func = !key_mode
			? _array_do_multiple_indexes_offset
			: _array_do_multiple_indexes_key;
	} else { /* PART_SPEC_INDEX_SPAN */
		part_func = !key_mode
			? _array_do_span_offset
			: _array_do_span_key;
	}

	r = zend_llist_get_first_ex(*read_targets, &rpos);
	w = zend_llist_get_first_ex(*write_targets, &wpos);
	
	while (r != NULL) {
		assert(w != NULL);
		
		if (Z_TYPE_PP(r) != IS_ARRAY) {
			php_error_docref0(NULL TSRMLS_CC, E_WARNING,
				"The depth of the part specification is too large; tried to "
				"get part of non-array with part of index '%d'", part_num);
			result = FAILURE;
			break;
		}
		
		if (part_func(Z_ARRVAL_PP(r), *w, ps, new_read_targets,
				new_write_targets, next_type TSRMLS_CC) == FAILURE) {
			result = FAILURE;
			break;
		}

		r = zend_llist_get_next_ex(*read_targets, &rpos);
		w = zend_llist_get_next_ex(*write_targets, &wpos);
	}

	zend_llist_destroy(*read_targets);
	efree(*read_targets);
	zend_llist_destroy(*write_targets);
	efree(*write_targets);

	*read_targets	= new_read_targets;
	*write_targets	= new_write_targets;

	return result;
}

PHP_FUNCTION(array_part)
{
	zend_bool		key_mode = 0;
	zval			*array,
					*ps_arg;
	union part_spec	**part_specs;
	int				num_parts,
					part_num;
	zend_llist		*read_targets,
					*write_targets;
	zval			*result_container;
	int				failure = 0;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "aa|b",
			&array, &ps_arg, &key_mode) == FAILURE) {
		RETURN_FALSE;
	}
	
	if (_array_build_part_specs(Z_ARRVAL_P(ps_arg), &part_specs,
			&num_parts, key_mode TSRMLS_CC) == FAILURE) {
		RETURN_FALSE;
	}

	read_targets = emalloc(sizeof *read_targets);
	zend_llist_init(read_targets, sizeof(zval*), NULL, 0);
	
	zend_llist_add_element(read_targets, &array);
	
	write_targets = emalloc(sizeof *write_targets);
	zend_llist_init(write_targets, sizeof(zval*), NULL, 0);

	MAKE_STD_ZVAL(result_container);
	array_init_size(result_container, 1);

	if (part_specs[0]->type != PART_SPEC_SINGLE_INDEX) {
		/* result value will be an array */
		array_init(return_value);
		
		/* create new array inside result container */
		zend_hash_next_index_insert(Z_ARRVAL_P(result_container),
				(void*)&return_value, sizeof(return_value), NULL);
		/* add ref so it's not destroyed when we destroy result_container */
		zval_add_ref(&return_value);
		
		zend_llist_add_element(write_targets, &return_value);
	} else {
		zend_llist_add_element(write_targets, &result_container);
	}
	
	for (part_num = 0; part_num < num_parts; part_num++) {
		enum part_type	next_type;
		int				result;
		
		if (part_num + 1 == num_parts) {
			next_type = PART_TYPE_NONE;
		} else if (part_specs[part_num + 1]->type == PART_SPEC_SINGLE_INDEX) {
			next_type = PART_TYPE_SINGLE;
		} else {
			next_type = PART_TYPE_MULTIPLE;
		}
		
		result = _array_do_part(part_specs[part_num], part_num, next_type,
				&read_targets, &write_targets, key_mode TSRMLS_CC);
		/*php_var_dump(&result_container, 2 TSRMLS_CC);
		php_printf("r size: %d, w size: %d\n",
				zend_llist_count(read_targets), zend_llist_count(write_targets));*/
		if (result == FAILURE) {
			failure = 1;
			break;
		}
	}
	
	zend_llist_destroy(read_targets);
	efree(read_targets);
	zend_llist_destroy(write_targets);
	efree(write_targets);
	
	if (!failure) {
		if (part_specs[0]->type == PART_SPEC_SINGLE_INDEX) {
			zval **val;
			zend_hash_index_find(Z_ARRVAL_P(result_container), 0, (void**)&val);
			assert(val != NULL);
			RETVAL_ZVAL(*val, 1, 0);
		}
	} else {
		zval_dtor(return_value);
		RETVAL_FALSE;
	}
	
	_array_destroy_part_specs(part_specs, num_parts);
	
	zval_ptr_dtor(&result_container);
}
