static const char* aason_type_strings[] = {
	"",
	"string",
	"int",
	"bool",
	"enum",
	"float",
	"array",
	"object",
	"constructor"
};
static_assert(sizeof(aason_type_strings) / sizeof(const char*) == aason_type_count, "");

static void aason_read_error(aason_context* ctx, aason_error_type error, const aason_element* element , const char* fmt, ...)
{
	char buffer[4096];

	aason_assert(element->value_file_index < ctx->file_count);
	aason_file* file = &ctx->files[element->value_file_index];

	ctx->error = error;
	ctx->error_file = file->buffer;
	ctx->error_line = element->line;
	ctx->error_column = element->column;

	if (ctx->error_interface.error)
	{
		va_list args;
		va_start(args, fmt);
		vsnprintf(buffer, sizeof(buffer), fmt, args);
		va_end(args);
	
		ctx->error_interface.error(
			ctx->error_interface.user_data,
			error,
			file->buffer,
			element->line,
			element->column,
			buffer
		);
	}
}

static const char* aason_read_get_key_buffer(aason_context* ctx, const aason_element* element)
{
	aason_assert(element->key_file_index < ctx->file_count);

	const aason_file* file = &ctx->files[element->key_file_index];
	const char* buffer = file->buffer + file->offset;

	return buffer;
}

static const char* aason_read_get_value_buffer(aason_context* ctx, const aason_element* element)
{
	aason_assert(element->value_file_index < ctx->file_count);

	const aason_file* file = &ctx->files[element->value_file_index];
	const char* buffer = file->buffer + file->offset;

	return buffer;
}

static const aason_element* aason_read_find_object_element(aason_context* ctx, const char* key, aason_flags flags, aason_type type)
{
	aason_assert(ctx);
	aason_assert(key);
	aason_assert(*key);

	const size_t key_len = strlen(key);

	const uint32_t stack_depth = ctx->stack_depth;
	aason_assert(stack_depth < ctx->max_stack_depth);

	const uint32_t element_index = ctx->stack[stack_depth].element_index;
	aason_assert(element_index < ctx->element_count);
	aason_element* object_element = &ctx->elements[element_index];

	if (stack_depth == 0)
	{
		aason_assert(ctx->mode == aason_mode_aason && ctx->use_implicit_root);

		const char* key_buffer = aason_read_get_key_buffer(ctx, object_element);
		const char* key_string = key_buffer + object_element->key_offset;

		if (key_len == object_element->key_len && memcmp(key, key_string, key_len) == 0)
		{
			if (object_element->type == type)
			{
				object_element->touched = true;

				return object_element;
			}
			else
			{
				aason_read_error(ctx, aason_error_wrong_type, object_element,
					"Root element '%s' has type '%s' instead of expected type '%s'",
					key, aason_type_strings[object_element->type], aason_type_strings[type]
				);

				return NULL;
			}
		}
	}
	else
	{
		aason_element* children = ctx->elements + object_element->object_value.first_child;
		const uint32_t count = object_element->object_value.count;

		for (uint32_t i = 0; i < count; ++i)
		{
			aason_element* element = &children[i];
			const char* key_buffer = aason_read_get_key_buffer(ctx, element);
			const char* key_string = key_buffer + element->key_offset;
	
			if (key_len == element->key_len && memcmp(key, key_string, key_len) == 0)
			{
				if (element->type == type)
				{
					element->touched = true;

					return element;
				}
				else
				{
					aason_read_error(ctx, aason_error_wrong_type, element,
						"Element '%s' has type '%s' instead of expected type '%s'",
						key, aason_type_strings[element->type], aason_type_strings[type]
					);
	
					return NULL;
				}
			}
		}
	}

	if (flags == aason_required)
	{
		// TODO: Improve error message when root element is not found
		aason_read_error(ctx, aason_error_key_not_found, object_element,
			"Unable to find required element '%s'", key
		);
	}

	return NULL;
}

static const aason_element* aason_read_get_next_array_element(aason_context* ctx, aason_type type)
{
	aason_assert(ctx);

	const uint32_t stack_depth = ctx->stack_depth;
	aason_assert(stack_depth > 0);
	aason_assert(stack_depth < ctx->max_stack_depth);

	const uint32_t element_index = ctx->stack[stack_depth].element_index;
	aason_assert(element_index < ctx->element_count);

	const aason_element* array_element = &ctx->elements[element_index];
	aason_assert(array_element->type == aason_type_array);

	const uint32_t array_index = ctx->stack[stack_depth].child_index;
	if (array_index < array_element->array_value.count)
	{
		const aason_element* children = ctx->elements + array_element->array_value.first_child;
		const aason_element* element = &children[array_index];

		// Increment irrespective of whether an error was found
		ctx->stack[stack_depth].child_index = array_index + 1;

		if (element->type == type)
		{
			return element;
		}
		else
		{
			aason_read_error(ctx, aason_error_wrong_type, element,
				"Array element has type '%s' instead of expected type '%s'",
				aason_type_strings[element->type], aason_type_strings[type]
			);
		}
	}

	return NULL;
}

static bool aason_read_enter_object_common(aason_context* ctx, const aason_element* element)
{
	if (element)
	{
		const uint32_t stack_depth = ++ctx->stack_depth;
		aason_assert(stack_depth < ctx->max_stack_depth);

		ctx->stack[stack_depth].element_index = (uint32_t)(element - ctx->elements);

		return true;
	}

	return false;
}

static void aason_read_leave_common(aason_context* ctx, aason_type type)
{
	aason_assert(ctx);
	aason_assert(type == aason_type_array || type == aason_type_object);
	aason_assert(ctx->stack_depth > 0);
	aason_assert(ctx->stack_depth < ctx->max_stack_depth);

	const uint32_t element_index = ctx->stack[ctx->stack_depth].element_index;
	aason_assert(element_index < ctx->element_count);

	const aason_element* element = &ctx->elements[element_index];
	aason_assert(element->type == type);

	--ctx->stack_depth;
}

static bool aason_read_str_common(aason_context* ctx, const aason_element* element, const char** value, size_t* len)
{
	aason_assert(value);

	if (element)
	{
		const char* buffer = aason_read_get_value_buffer(ctx, element);

		*value = buffer + element->str_value.offset;

		if (len)
			*len = element->str_value.len;

		return true;
	}

	return false;
}

static bool aason_read_fixed_str_common(aason_context* ctx, const aason_element* element, char* value, size_t buffer_size, bool truncate)
{
	aason_assert(value);
	aason_assert(buffer_size > 0);

	if (element)
	{
		const char* buffer = aason_read_get_value_buffer(ctx, element);
		const char* str = buffer + element->str_value.offset;
		const size_t len = element->str_value.len;

		if (len < buffer_size)
		{
			memcpy(value, str, len);
			value[len] = 0;
			return true;
		}
		else if (truncate)
		{
			memcpy(value, str, buffer_size - 1);
			value[buffer_size - 1] = 0;
			return true;
		}
		else
		{
			aason_read_error(ctx, aason_error_buffer_too_small, element,
				"String of length %zu exceeds fixed-sized buffer capacity of %zu",
				len, buffer_size
			);
		}
	}

	return false;
}

static bool aason_read_int_common(aason_context* ctx, const aason_element* element, int64_t* value)
{
	aason_assert(value);

	if (element)
	{
		*value = element->int_value;
		return true;
	}

	return false;
}

static bool aason_read_int_ranged_common(aason_context* ctx, const aason_element* element, int64_t* value, int64_t min, int64_t max)
{
	aason_assert(value);
	aason_assert(min <= max);

	if (element)
	{
		if (element->int_value < min || element->int_value > max)
		{
			aason_read_error(ctx, aason_error_invalid_range, element,
				"Integer out of range (%" PRId64 " to %" PRId64 ")",
				min, max
			);

			return false;
		}

		*value = element->int_value;
		return true;
	}

	return false;
}

static bool aason_read_bool_common(aason_context* ctx, const aason_element* element, bool* value)
{
	aason_assert(value);

	if (element)
	{
		*value = element->bool_value;
		return true;
	}

	return false;
}

static bool aason_read_float_common(aason_context* ctx, const aason_element* element, float* value)
{
	aason_assert(value);

	if (element)
	{
		*value = element->float_value;
		return true;
	}

	return false;
}

static bool aason_read_enum_common(aason_context* ctx, const aason_element* element, int32_t* value, const char** strings, int32_t count)
{
	aason_assert(value);
	aason_assert(strings);
	aason_assert(count >= 1);

	if (element)
	{
		const char* buffer = aason_read_get_value_buffer(ctx, element);
		const uint32_t lhs_offset = element->enum_value.offset;
		const int64_t lhs_len = element->enum_value.len;
		const char* enum_value = &buffer[lhs_offset];

		for (int32_t i = 0; i < count; ++i)
		{
			const int64_t rhs_len = strlen(strings[i]);
			if (lhs_len == rhs_len && memcmp(enum_value, strings[i], lhs_len) == 0)
			{
				*value = i;
				return true;
			}
		}

		aason_read_error(ctx, aason_error_invalid_enum, element,
			"Invalid enum value '%s'", enum_value
		);
	}

	return false;
}

static bool aason_read_constructor_common(aason_context* ctx, const aason_element* element, void* value, const char* type)
{
	aason_assert(value);
	aason_assert(type);

	if (element)
	{
		const uint32_t constructor_index = element->constructor_index;
		aason_assert(constructor_index < ctx->constructors->count);

		const aason_constructors* constructors = ctx->constructors;
		const uint8_t arg_count = constructors->arg_counts[constructor_index];
		const uint32_t args_index = constructors->arg_indices[constructor_index];
		const uint8_t* args = constructors->args + args_index;
		aason_assert(strcmp(type, constructors->names[constructor_index]) == 0);

		const aason_element* children = ctx->elements + element->constructor_value.first_child;
		for (uint8_t i = 0; i < arg_count; ++i)
		{
			const char* buffer = aason_read_get_value_buffer(ctx, &children[i]);

			aason_arg* arg = &constructors->temp_args[i];
			arg->type = args[i];

			switch (arg->type)
			{
			case aason_arg_type_str:
				arg->str_value = buffer + children[i].str_value.offset;
				break;
			case aason_arg_type_bin:
			case aason_arg_type_dec:
			case aason_arg_type_hex:
				arg->int_value = children[i].int_value;
				break;
			case aason_arg_type_bool:
				arg->bool_value = children[i].bool_value;
				break;
			case aason_arg_type_enum:
				arg->enum_value = buffer + children[i].enum_value.offset;
				break;
			case aason_arg_type_float:
				arg->float_value = children[i].float_value;
				break;
			}
		}

		constructors->read_callbacks[constructor_index](ctx, value, constructors->temp_args, arg_count);

		return true;
	}

	return false;
}

bool aason_read_enter_root_array(aason_context* ctx, size_t* size, size_t max_size)
{
	aason_assert(ctx);
	aason_assert(ctx->mode == aason_mode_json || !ctx->use_implicit_root);
	aason_assert(ctx->stack_depth == 0);

	aason_element* root_element = &ctx->elements[0];
	//root_element->touched = true;

	if (root_element->type != aason_type_array)
	{
		aason_read_error(ctx, aason_error_wrong_type, root_element,
			"Root element has type '%s' instead of expected type 'array'",
			aason_type_strings[root_element->type]
		);

		return false;
	}

	if (root_element->array_value.count > max_size)
	{
		aason_read_error(ctx, aason_error_array_too_large, root_element,
			"Root array size of %" PRIu32 " exceeds maximum of %zu",
			root_element->array_value.count, max_size
		);

		return false;
	}

	const uint32_t stack_depth = ++ctx->stack_depth;
	aason_assert(stack_depth < ctx->max_stack_depth);

	ctx->stack[stack_depth].element_index = (uint32_t)(root_element - ctx->elements);
	ctx->stack[stack_depth].child_index = 0;

	if (size)
		*size = root_element->array_value.count;

	return true;
}

bool aason_read_enter_root_object(aason_context* ctx)
{
	aason_assert(ctx);
	aason_assert(ctx->mode == aason_mode_json || !ctx->use_implicit_root);
	aason_assert(ctx->stack_depth == 0);

	aason_element* root_element = &ctx->elements[0];
	root_element->touched = true;

	if (root_element->type != aason_type_object)
	{
		aason_read_error(ctx, aason_error_wrong_type, root_element,
			"Root element has type '%s' instead of expected type 'object'",
			aason_type_strings[root_element->type]
		);

		return false;
	}

	return aason_read_enter_object_common(ctx, root_element);
}

void aason_read_leave_array(aason_context* ctx)
{
	aason_read_leave_common(ctx, aason_type_array);
}

void aason_read_leave_object(aason_context* ctx)
{
	if (ctx->check_unused_fields)
	{
		const uint32_t stack_depth = ctx->stack_depth;
		aason_assert(stack_depth < ctx->max_stack_depth);

		const uint32_t element_index = ctx->stack[stack_depth].element_index;
		aason_assert(element_index < ctx->element_count);
		const aason_element* object_element = &ctx->elements[element_index];

		if (stack_depth == 0)
		{
			// It would be invalid to leave if not entered
			aason_assert(object_element->touched);
		}
		else
		{
			aason_element* children = ctx->elements + object_element->object_value.first_child;
			const uint32_t count = object_element->object_value.count;

			for (uint32_t i = 0; i < count; ++i)
			{
				aason_element* element = &children[i];
				if (!element->touched)
				{
					const char* child_key_string = aason_read_get_key_buffer(ctx, element) + element->key_offset;
					aason_read_error(ctx, aason_error_unused_field, object_element,
						"Unused field '%s'",
						child_key_string
					);
				}
			}
		}
	}

	aason_read_leave_common(ctx, aason_type_object);
}

bool aason_read_array_enter_object(aason_context* ctx)
{
	const aason_element* element = aason_read_get_next_array_element(ctx, aason_type_object);
	return aason_read_enter_object_common(ctx, element);
}

bool aason_read_array_str(aason_context* ctx, const char** value, size_t* len)
{
	const aason_element* element = aason_read_get_next_array_element(ctx, aason_type_str);
	return aason_read_str_common(ctx, element, value, len);
}

bool aason_read_array_fixed_str(aason_context* ctx, char* value, size_t buffer_size, bool truncate)
{
	const aason_element* element = aason_read_get_next_array_element(ctx, aason_type_str);
	return aason_read_fixed_str_common(ctx, element, value, buffer_size, truncate);
}

bool aason_read_array_int(aason_context* ctx, int64_t* value)
{
	const aason_element* element = aason_read_get_next_array_element(ctx, aason_type_int);
	return aason_read_int_common(ctx, element, value);
}

bool aason_read_array_int_ranged(aason_context* ctx, int64_t* value, int64_t min, int64_t max)
{
	const aason_element* element = aason_read_get_next_array_element(ctx, aason_type_int);
	return aason_read_int_ranged_common(ctx, element, value, min, max);
}

bool aason_read_array_bool(aason_context* ctx, bool* value)
{
	const aason_element* element = aason_read_get_next_array_element(ctx, aason_type_bool);
	return aason_read_bool_common(ctx, element, value);
}

bool aason_read_array_float(aason_context* ctx, float* value)
{
	const aason_element* element = aason_read_get_next_array_element(ctx, aason_type_float);
	return aason_read_float_common(ctx, element, value);
}

bool aason_read_array_enum(aason_context* ctx, int32_t* value, const char** strings, int32_t count)
{
	const aason_element* element = aason_read_get_next_array_element(ctx, aason_type_enum);
	return aason_read_enum_common(ctx, element, value, strings, count);
}

bool aason_read_array_constructor(aason_context* ctx, void* value, const char* type)
{
	const aason_element* element = aason_read_get_next_array_element(ctx, aason_type_constructor);
	return aason_read_constructor_common(ctx, element, value, type);
}

bool aason_read_object_enter_array(aason_context* ctx, const char* key, aason_flags flags, size_t* size, size_t max_size)
{
	const aason_element* element = aason_read_find_object_element(ctx, key, flags, aason_type_array);
	if (element)
	{
		if (element->array_value.count > max_size)
		{
			aason_read_error(ctx, aason_error_array_too_large, element,
				"Array '%s' size of %" PRIu32 " exceeds maximum of %zu",
				key, element->array_value.count, max_size
			);

			return false;
		}

		const uint32_t stack_depth = ++ctx->stack_depth;
		aason_assert(stack_depth < ctx->max_stack_depth);

		ctx->stack[stack_depth].element_index = (uint32_t)(element - ctx->elements);
		ctx->stack[stack_depth].child_index = 0;

		if (size)
			*size = element->array_value.count;

		return true;
	}

	return false;
}

bool aason_read_object_enter_object(aason_context* ctx, const char* key, aason_flags flags)
{
	const aason_element* element = aason_read_find_object_element(ctx, key, flags, aason_type_object);
	return aason_read_enter_object_common(ctx, element);
}

bool aason_read_object_str(aason_context* ctx, const char* key, aason_flags flags, const char** value, size_t* len)
{
	const aason_element* element = aason_read_find_object_element(ctx, key, flags, aason_type_str);
	return aason_read_str_common(ctx, element, value, len);
}

bool aason_read_object_fixed_str(aason_context* ctx, const char* key, aason_flags flags, char* value, size_t buffer_size, bool truncate)
{
	const aason_element* element = aason_read_find_object_element(ctx, key, flags, aason_type_str);
	return aason_read_fixed_str_common(ctx, element, value, buffer_size, truncate);
}

bool aason_read_object_int(aason_context* ctx, const char* key, aason_flags flags, int64_t* value)
{
	const aason_element* element = aason_read_find_object_element(ctx, key, flags, aason_type_int);
	return aason_read_int_common(ctx, element, value);
}

bool aason_read_object_int_ranged(aason_context* ctx, const char* key, aason_flags flags, int64_t* value, int64_t min, int64_t max)
{
	const aason_element* element = aason_read_find_object_element(ctx, key, flags, aason_type_int);
	return aason_read_int_ranged_common(ctx, element, value, min, max);
}

bool aason_read_object_bool(aason_context* ctx, const char* key, aason_flags flags, bool* value)
{
	const aason_element* element = aason_read_find_object_element(ctx, key, flags, aason_type_bool);
	return aason_read_bool_common(ctx, element, value);
}

bool aason_read_object_float(aason_context* ctx, const char* key, aason_flags flags, float* value)
{
	const aason_element* element = aason_read_find_object_element(ctx, key, flags, aason_type_float);
	return aason_read_float_common(ctx, element, value);
}

bool aason_read_object_enum(aason_context* ctx, const char* key, aason_flags flags, int32_t* value, const char** strings, int32_t count)
{
	const aason_element* element = aason_read_find_object_element(ctx, key, flags, aason_type_enum);
	return aason_read_enum_common(ctx, element, value, strings, count);
}

bool aason_read_object_constructor(aason_context* ctx, const char* key, aason_flags flags, void* value, const char* type)
{
	const aason_element* element = aason_read_find_object_element(ctx, key, flags, aason_type_constructor);
	return aason_read_constructor_common(ctx, element, value, type);
}