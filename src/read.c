static const char* sdd_type_strings[] = {
	"",
	"string",
	"int",
	"bool",
	"hash",
	"enum",
	"float",
	"array",
	"object"
};

static void sdd_read_error(rgs_sdd* sdd, rgs_sdd_error error, uint32_t line, uint32_t column, const char* fmt, ...)
{
	char buffer[rgs_kib(4)];

	sdd->error = error;
	sdd->error_line = line;
	sdd->error_column = column;

	if (sdd->error_callback)
	{
		va_list args;
		va_start(args, fmt);
		const int64_t len = rgs_format_impl(buffer, sizeof(buffer), fmt, args);
		(void)len;
		va_end(args);
	
		sdd->error_callback(sdd->user_data, error, line, column, buffer);
	}
}

static const rgs_sdd_element* sdd_read_find_object_element(rgs_sdd* sdd, const char* key, rgs_sdd_flags flags, rgs_sdd_type type)
{
	rgs_assert(sdd);
	rgs_assert(key);
	rgs_assert(*key);

	const int64_t key_len = strlen(key);

	const uint32_t stack_depth = sdd->stack_depth;
	rgs_assert(stack_depth < sdd->max_stack_depth);

	const uint32_t element_index = sdd->stack[stack_depth].element_index;
	rgs_assert(element_index < sdd->element_count);
	const rgs_sdd_element* object_element = &sdd->elements[element_index];

	if (stack_depth == 0)
	{
		if (key_len == object_element->key_len && memcmp(key, sdd->buffer + object_element->key_offset, key_len) == 0)
		{
			if (object_element->type == type)
			{
				return object_element;
			}
			else
			{
				sdd_read_error(sdd, rgs_sdd_error_wrong_type, object_element->line, object_element->column,
					"Root element '{s}' has type '{s}' instead of expected type '{s}'",
					key, sdd_type_strings[object_element->type], sdd_type_strings[type]
				);

				return nullptr;
			}
		}
	}
	else
	{
		const rgs_sdd_element* children = sdd->elements + object_element->object_value.first_child;
		const uint32_t count = object_element->object_value.count;

		for (uint32_t i = 0; i < count; ++i)
		{
			const rgs_sdd_element* element = &children[i];
	
			if (key_len == element->key_len && memcmp(key, sdd->buffer + element->key_offset, key_len) == 0)
			{
				if (element->type == type)
				{
					return element;
				}
				else
				{
					sdd_read_error(sdd, rgs_sdd_error_wrong_type, element->line, element->column,
						"Element '{s}' has type '{s}' instead of expected type '{s}'",
						key, sdd_type_strings[element->type], sdd_type_strings[type]
					);
	
					return nullptr;
				}
			}
		}
	}

	if (flags == rgs_sdd_required)
	{
		// TODO: Improve error message when root element is not found
		sdd_read_error(sdd, rgs_sdd_error_key_not_found, object_element->line, object_element->column,
			"Unable to find required element '{s}'", key
		);
	}

	return nullptr;
}

static const rgs_sdd_element* sdd_read_get_next_array_element(rgs_sdd* sdd, rgs_sdd_type type)
{
	rgs_assert(sdd);

	const uint32_t stack_depth = sdd->stack_depth;
	rgs_assert(stack_depth > 0);
	rgs_assert(stack_depth < sdd->max_stack_depth);

	const uint32_t element_index = sdd->stack[stack_depth].element_index;
	rgs_assert(element_index < sdd->element_count);

	const rgs_sdd_element* array_element = &sdd->elements[element_index];
	rgs_assert(array_element->type == rgs_sdd_type_array);

	const uint32_t array_index = sdd->stack[stack_depth].array_index;
	if (array_index < array_element->array_value.count)
	{
		const rgs_sdd_element* children = sdd->elements + array_element->array_value.first_child;
		const rgs_sdd_element* element = &children[array_index];

		if (element->type == type)
		{
			sdd->stack[stack_depth].array_index = array_index + 1;
			return element;
		}
		else
		{
			sdd_read_error(sdd, rgs_sdd_error_wrong_type, element->line, element->column,
				"Array element has type '{s}' instead of expected type '{s}'",
				sdd_type_strings[element->type], sdd_type_strings[type]
			);
	
		}
	}

	return nullptr;
}

// These are the three parsing passes that have been split into multiple modules
#include "rgs.sdd_tokenise.c"
#include "rgs.sdd_validate.c"
#include "rgs.sdd_finalise.c"

/*
	TODO:
	* SDD parsing has been refactored so that it returns a single allocation.
	* This makes it simpler for loading code to choose an allocator that may not need to free.
	* Therefore no terminate or destroy call is necessary.
	* The original code assumed an SDD object, but now we are hacking around it using scratch mem.
	* Consider a different approach that uses internal structs for tokenise, validate, and finalise.
*/
rgs_sdd* rgs_sdd_read(rgs_allocator allocator, char* src, int64_t size, uint32_t tab_size, rgs_sdd_error_callback callback, void* user_data)
{
	rgs_assert(src);
	rgs_assert(size >= 0);
	rgs_assert(tab_size <= 8);

	RGS_PROFILE_FUNCTION_BEGIN();
	rgs_frame frame = rgs_scratch_push();

	rgs_sdd* result = nullptr;

	if (tab_size == 0)
		tab_size = 4;

	rgs_sdd* sdd = rgs_scratch_alloc_type(rgs_sdd);

	sdd->buffer = src;
	sdd->error = rgs_sdd_error_none;
	sdd->reading = true;
	sdd->user_data = user_data;
	sdd->error_callback = callback;

	sdd_token* tokens = sdd_tokenise(sdd, src, size, tab_size);
	if (sdd->error == rgs_sdd_error_none)
	{
		if (sdd_validate(sdd, tokens))
		{
			sdd->elements = rgs_scratch_alloc_array(rgs_sdd_element, sdd->element_count);
			sdd->stack = rgs_scratch_alloc_array(rgs_sdd_stack_entry, sdd->max_stack_depth);
			sdd->stack[0] = (rgs_sdd_stack_entry){};

			if (sdd_finalise(sdd, src, tokens))
			{
				int64_t packed_size = 0;
				packed_size += rgs_packed_alloc_add_value(packed_size, rgs_sdd);
				packed_size += rgs_packed_alloc_add_array(packed_size, rgs_sdd_element, sdd->element_count);
				packed_size += rgs_packed_alloc_add_array(packed_size, rgs_sdd_stack_entry, sdd->max_stack_depth);

				void* alloc = rgs_alloc_with(allocator, packed_size, alignof(rgs_sdd));
				rgs_sdd* sdd_copy = rgs_packed_alloc_get_value(alloc, rgs_sdd);
				memcpy(sdd_copy, sdd, sizeof(rgs_sdd));

				sdd_copy->elements = rgs_packed_alloc_get_array(alloc, rgs_sdd_element, sdd->element_count);
				memcpy(sdd_copy->elements, sdd->elements, sizeof(rgs_sdd_element) * sdd->element_count);

				sdd_copy->stack = rgs_packed_alloc_get_array(alloc, rgs_sdd_stack_entry, sdd->max_stack_depth);
				memcpy(sdd_copy->stack, sdd->stack, sizeof(rgs_sdd_stack_entry) * sdd->max_stack_depth);

				sdd_copy->stack_depth = 0;

				result = sdd_copy;
			}
		}
	}

	rgs_scratch_pop(frame);
	RGS_PROFILE_FUNCTION_END();

	return result;
}

bool rgs_sdd_read_array_enter(rgs_sdd* sdd, const char* key, rgs_sdd_flags flags, int64_t* size, int64_t max_size)
{
	const rgs_sdd_element* element = sdd_read_find_object_element(sdd, key, flags, rgs_sdd_type_array);
	if (element)
	{
		if (element->array_value.count > max_size)
		{
			sdd_read_error(sdd, rgs_sdd_error_buffer_too_small, element->line, element->column,
				"Array '{s}' size is {u32} but max size is {i64}",
				key, element->array_value.count, max_size
			);

			return false;
		}

		const uint32_t stack_depth = ++sdd->stack_depth;
		rgs_assert(stack_depth < sdd->max_stack_depth);

		sdd->stack[stack_depth].element_index = (uint32_t)(element - sdd->elements);
		sdd->stack[stack_depth].array_index = 0;

		if (size)
			*size = element->array_value.count;

		return true;
	}

	return false;
}

void rgs_sdd_read_array_leave(rgs_sdd* sdd)
{
	rgs_assert(sdd);
	rgs_assert(sdd->stack_depth > 0);
	rgs_assert(sdd->stack_depth < sdd->max_stack_depth);

	const uint32_t element_index = sdd->stack[sdd->stack_depth].element_index;
	rgs_assert(element_index < sdd->element_count);

	const rgs_sdd_element* element = &sdd->elements[element_index];
	rgs_assert(element->type == rgs_sdd_type_array);

	--sdd->stack_depth;
}

bool rgs_sdd_read_array_enter_object(rgs_sdd* sdd)
{
	const rgs_sdd_element* element = sdd_read_get_next_array_element(sdd, rgs_sdd_type_object);
	if (element)
	{
		const uint32_t stack_depth = ++sdd->stack_depth;
		rgs_assert(stack_depth < sdd->max_stack_depth);

		sdd->stack[stack_depth].element_index = (uint32_t)(element - sdd->elements);
		++sdd->stack[stack_depth].array_index;

		return true;
	}

	return false;
}

void rgs_sdd_read_array_leave_object(rgs_sdd* sdd)
{
	rgs_assert(sdd);
	rgs_assert(sdd->stack_depth > 0);
	rgs_assert(sdd->stack_depth < sdd->max_stack_depth);

	const uint32_t element_index = sdd->stack[sdd->stack_depth].element_index;
	rgs_assert(element_index < sdd->element_count);

	const rgs_sdd_element* element = &sdd->elements[element_index];
	rgs_assert(element->type == rgs_sdd_type_object);

	--sdd->stack_depth;
}

bool rgs_sdd_read_array_str(rgs_sdd* sdd, const char** value, int64_t* len)
{
	rgs_assert(value);

	const rgs_sdd_element* element = sdd_read_get_next_array_element(sdd, rgs_sdd_type_str);
	if (element)
	{
		*value = sdd->buffer + element->str_value.offset;

		if (len)
			*len = element->str_value.len;

		return true;
	}

	return false;
}

bool rgs_sdd_read_array_fixed_str(rgs_sdd* sdd, char* value, int64_t buffer_size, bool truncate)
{
	rgs_assert(value);
	rgs_assert(buffer_size > 0);

	const rgs_sdd_element* element = sdd_read_get_next_array_element(sdd, rgs_sdd_type_str);
	if (element)
	{
		const char* str = sdd->buffer + element->str_value.offset;
		const int64_t len = element->str_value.len;

		if (len < buffer_size)
		{
			// Handle length of 0
			memcpy(value, str, len);
			value[len] = 0;
			return true;
		}
		else if (truncate)
		{
			rgs_str_cpy(value, str, buffer_size);
			return true;
		}
		else
		{
			sdd_read_error(sdd, rgs_sdd_error_buffer_too_small, element->line, element->column,
				"String element of length '{u32}' is too large for fixed sized buffer size of '{i64}'",
				len, buffer_size
			);
		}
	}

	return false;
}

bool rgs_sdd_read_array_int(rgs_sdd* sdd, int64_t* value)
{
	rgs_assert(value);

	const rgs_sdd_element* element = sdd_read_get_next_array_element(sdd, rgs_sdd_type_int);
	if (element)
	{
		*value = element->int_value;
		return true;
	}

	return false;
}

bool rgs_sdd_read_array_int_ranged(rgs_sdd* sdd, int64_t* value, int64_t min, int64_t max)
{
	rgs_assert(value);

	const rgs_sdd_element* element = sdd_read_get_next_array_element(sdd, rgs_sdd_type_int);
	if (element)
	{
		if (element->int_value < min || element->int_value > max)
		{
			sdd_read_error(sdd, rgs_sdd_error_buffer_too_small, element->line, element->column,
				"Int out of range ({i64} to {i64})",
				min, max
			);

			return false;
		}

		*value = element->int_value;
		return true;
	}

	return false;
}

bool rgs_sdd_read_array_bool(rgs_sdd* sdd, bool* value)
{
	rgs_assert(value);

	const rgs_sdd_element* element = sdd_read_get_next_array_element(sdd, rgs_sdd_type_bool);
	if (element)
	{
		*value = element->bool_value;
		return true;
	}

	return false;
}

bool rgs_sdd_read_array_hash(rgs_sdd* sdd, uint32_t* value)
{
	rgs_assert(value);

	const rgs_sdd_element* element = sdd_read_get_next_array_element(sdd, rgs_sdd_type_hash);
	if (element)
	{
		*value = element->hash_value;
		return true;
	}

	return false;
}

bool rgs_sdd_read_array_float(rgs_sdd* sdd, float* value)
{
	rgs_assert(value);

	const rgs_sdd_element* element = sdd_read_get_next_array_element(sdd, rgs_sdd_type_float);
	if (element)
	{
		*value = element->float_value;
		return true;
	}

	return false;
}

bool rgs_sdd_read_array_enum(rgs_sdd* sdd, int32_t* value, const char** strings, int32_t count)
{
	rgs_assert(value);
	rgs_assert(strings);
	rgs_assert(count > 1);

	const rgs_sdd_element* element = sdd_read_get_next_array_element(sdd, rgs_sdd_type_enum);
	if (element)
	{
		const uint32_t lhs_offset = element->enum_value.offset;
		const int64_t lhs_len = element->enum_value.len;
		const char* enum_value = &sdd->buffer[lhs_offset];

		for (int32_t i = 0; i < count; ++i)
		{
			const int64_t rhs_len = strlen(strings[i]);
			if (lhs_len == rhs_len && memcmp(enum_value, strings[i], lhs_len) == 0)
			{
				*value = i;
				return true;
			}
		}

		sdd_read_error(sdd, rgs_sdd_error_invalid_enum, element->line, element->column,
			"Invalid enum value '{s}' in array", enum_value
		);
	}

	return false;
}

bool rgs_sdd_read_object_enter(rgs_sdd* sdd, const char* key, rgs_sdd_flags flags)
{
	rgs_assert(sdd->stack_depth + 1 < sdd->max_stack_depth);

	const rgs_sdd_element* element = sdd_read_find_object_element(sdd, key, flags, rgs_sdd_type_object);
	if (element)
	{
		const uint32_t stack_depth = ++sdd->stack_depth;
		rgs_assert(stack_depth < sdd->max_stack_depth);

		sdd->stack[stack_depth].element_index = (uint32_t)(element - sdd->elements);
		return true;
	}

	return false;
}

void rgs_sdd_read_object_leave(rgs_sdd* sdd)
{
	rgs_assert(sdd);
	rgs_assert(sdd->stack_depth > 0);
	rgs_assert(sdd->stack_depth < sdd->max_stack_depth);

	const uint32_t element_index = sdd->stack[sdd->stack_depth].element_index;
	rgs_assert(element_index < sdd->element_count);

	const rgs_sdd_element* element = &sdd->elements[element_index];
	rgs_assert(element->type == rgs_sdd_type_object);

	--sdd->stack_depth;
}

bool rgs_sdd_read_object_str(rgs_sdd* sdd, const char* key, rgs_sdd_flags flags, const char** value, int64_t* len)
{
	rgs_assert(value);

	const rgs_sdd_element* element = sdd_read_find_object_element(sdd, key, flags, rgs_sdd_type_str);
	if (element)
	{
		*value = sdd->buffer + element->str_value.offset;

		if (len)
			*len = element->str_value.len;

		return true;
	}

	return false;
}

bool rgs_sdd_read_object_fixed_str(rgs_sdd* sdd, const char* key, rgs_sdd_flags flags, char* value, int64_t buffer_size, bool truncate)
{
	rgs_assert(value);
	rgs_assert(buffer_size > 0);

	const rgs_sdd_element* element = sdd_read_find_object_element(sdd, key, flags, rgs_sdd_type_str);
	if (element)
	{
		const char* str = sdd->buffer + element->str_value.offset;
		const int64_t len = element->str_value.len;

		if (len < buffer_size)
		{
			// Handle length of 0
			memcpy(value, str, len);
			value[len] = 0;
			return true;
		}
		else if (truncate)
		{
			rgs_str_cpy(value, str, buffer_size);
			return true;
		}
		else
		{
			sdd_read_error(sdd, rgs_sdd_error_buffer_too_small, element->line, element->column,
				"String element of length '{u32}' is too large for fixed sized buffer size of '{i64}'",
				len, buffer_size
			);
		}
	}

	return false;
}

bool rgs_sdd_read_object_int(rgs_sdd* sdd, const char* key, rgs_sdd_flags flags, int64_t* value)
{
	rgs_assert(value);

	const rgs_sdd_element* element = sdd_read_find_object_element(sdd, key, flags, rgs_sdd_type_int);
	if (element)
	{
		*value = element->int_value;
		return true;
	}

	return false;
}

bool rgs_sdd_read_object_int_ranged(rgs_sdd* sdd, const char* key, rgs_sdd_flags flags, int64_t* value, int64_t min, int64_t max)
{
	rgs_assert(value);

	const rgs_sdd_element* element = sdd_read_find_object_element(sdd, key, flags, rgs_sdd_type_int);
	if (element)
	{
		if (element->int_value < min || element->int_value > max)
		{
			sdd_read_error(sdd, rgs_sdd_error_buffer_too_small, element->line, element->column,
				"Int element '{s}' out of range ({i64} to {i64})",
				key, min, max
			);

			return false;
		}

		*value = element->int_value;
		return true;
	}

	return false;
}

bool rgs_sdd_read_object_bool(rgs_sdd* sdd, const char* key, rgs_sdd_flags flags, bool* value)
{
	rgs_assert(value);

	const rgs_sdd_element* element = sdd_read_find_object_element(sdd, key, flags, rgs_sdd_type_bool);
	if (element)
	{
		*value = element->bool_value;
		return true;
	}

	return false;
}

bool rgs_sdd_read_object_hash(rgs_sdd* sdd, const char* key, rgs_sdd_flags flags, uint32_t* value)
{
	rgs_assert(value);

	const rgs_sdd_element* element = sdd_read_find_object_element(sdd, key, flags, rgs_sdd_type_hash);
	if (element)
	{
		*value = element->hash_value;
		return true;
	}

	return false;
}

bool rgs_sdd_read_object_float(rgs_sdd* sdd, const char* key, rgs_sdd_flags flags, float* value)
{
	rgs_assert(value);

	const rgs_sdd_element* element = sdd_read_find_object_element(sdd, key, flags, rgs_sdd_type_float);
	if (element)
	{
		*value = element->float_value;
		return true;
	}

	return false;
}

bool rgs_sdd_read_object_enum(rgs_sdd* sdd, const char* key, rgs_sdd_flags flags, int32_t* value, const char** strings, int32_t count)
{
	rgs_assert(value);
	rgs_assert(strings);
	rgs_assert(count >= 1);

	const rgs_sdd_element* element = sdd_read_find_object_element(sdd, key, flags, rgs_sdd_type_enum);
	if (element)
	{
		const uint32_t lhs_offset = element->enum_value.offset;
		const int64_t lhs_len = element->enum_value.len;
		const char* enum_value = &sdd->buffer[lhs_offset];

		for (int32_t i = 0; i < count; ++i)
		{
			const int64_t rhs_len = strlen(strings[i]);
			if (lhs_len == rhs_len && memcmp(enum_value, strings[i], lhs_len) == 0)
			{
				*value = i;
				return true;
			}
		}

		sdd_read_error(sdd, rgs_sdd_error_invalid_enum, element->line, element->column,
			"Invalid enum value '{s}' found in element '{s}'", enum_value, key
		);
	}

	return false;
}