static const char* aason_type_strings[] = {
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

static void aason_read_error(aason_context* ctx, aason_error error, uint32_t line, uint32_t column, const char* fmt, ...)
{
	char buffer[4096];

	ctx->error = error;
	ctx->error_line = line;
	ctx->error_column = column;

	if (ctx->read_desc->error_callback)
	{
		va_list args;
		va_start(args, fmt);
		aason_format_string(ctx, buffer, sizeof(buffer), fmt, args);
		va_end(args);
	
		ctx->read_desc->error_callback(
			ctx->user_data,
			error,
			line,
			column,
			buffer
		);
	}
}

static const aason_element* aason_read_find_object_element(aason_context* ctx, const char* key, aason_flags flags, aason_type type)
{
	aason_assert(ctx);
	aason_assert(key);
	aason_assert(*key);

	const int64_t key_len = strlen(key);

	const uint32_t stack_depth = ctx->stack_depth;
	aason_assert(stack_depth < ctx->max_stack_depth);

	const uint32_t element_index = ctx->stack[stack_depth].element_index;
	aason_assert(element_index < ctx->element_count);
	const aason_element* object_element = &ctx->elements[element_index];

	if (stack_depth == 0)
	{
		if (key_len == object_element->key_len && memcmp(key, ctx->buffer + object_element->key_offset, key_len) == 0)
		{
			if (object_element->type == type)
			{
				return object_element;
			}
			else
			{
				aason_read_error(ctx, aason_error_wrong_type, object_element->line, object_element->column,
					"Root element '{s}' has type '{s}' instead of expected type '{s}'",
					key, aason_type_strings[object_element->type], aason_type_strings[type]
				);

				return nullptr;
			}
		}
	}
	else
	{
		const aason_element* children = ctx->elements + object_element->object_value.first_child;
		const uint32_t count = object_element->object_value.count;

		for (uint32_t i = 0; i < count; ++i)
		{
			const aason_element* element = &children[i];
	
			if (key_len == element->key_len && memcmp(key, ctx->buffer + element->key_offset, key_len) == 0)
			{
				if (element->type == type)
				{
					return element;
				}
				else
				{
					aason_read_error(ctx, aason_error_wrong_type, element->line, element->column,
						"Element '{s}' has type '{s}' instead of expected type '{s}'",
						key, aason_type_strings[element->type], aason_type_strings[type]
					);
	
					return nullptr;
				}
			}
		}
	}

	if (flags == aason_required)
	{
		// TODO: Improve error message when root element is not found
		aason_read_error(ctx, aason_error_key_not_found, object_element->line, object_element->column,
			"Unable to find required element '{s}'", key
		);
	}

	return nullptr;
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

	const uint32_t array_index = ctx->stack[stack_depth].array_index;
	if (array_index < array_element->array_value.count)
	{
		const aason_element* children = ctx->elements + array_element->array_value.first_child;
		const aason_element* element = &children[array_index];

		if (element->type == type)
		{
			ctx->stack[stack_depth].array_index = array_index + 1;
			return element;
		}
		else
		{
			aason_read_error(ctx, aason_error_wrong_type, element->line, element->column,
				"Array element has type '{s}' instead of expected type '{s}'",
				aason_type_strings[element->type], aason_type_strings[type]
			);
		}
	}

	return nullptr;
}

static size_t aason_align_size(size_t size)
{
	const size_t alignment = sizeof(void*);
	return (size + (alignment - 1)) & ~(alignment - 1);
}

aason_context* aason_read(const aason_read_desc* desc)
{
	aason_assert(desc);
	aason_assert(desc->source);
	aason_assert(desc->tab_size <= 8);

	aason_context* ctx = nullptr;

	// Store context on the stack until we know how much memory the parser requires
	aason_context temp_ctx = {
		.buffer = desc->source,
		.error = aason_error_none,
		.reading = true,
		.read_desc = desc
	};

	// Use 4-space tabs by default
	const uint32_t tab_size = desc->tab_size ? desc->tab_size : 4;

	aason_tokens tokens = aason_tokenise(&temp_ctx, desc->source, desc->length, tab_size);
	if (temp_ctx.error == aason_error_none)
	{
		if (aason_validate(&temp_ctx, &tokens))
		{
			// Calculate the amount of memory required for parsing
			const size_t context_size	= aason_align_size(sizeof(aason_context));
			const size_t element_size	= aason_align_size(sizeof(aason_element) * temp_ctx.element_count);
			const size_t stack_size		= aason_align_size(sizeof(aason_stack_entry) * temp_ctx.max_stack_depth);
			const size_t packed_size	= context_size + element_size + stack_size;

			// Allocate all parsing memory in one go
			uint8_t* alloc = (uint8_t*)desc->allocator(desc->allocator_data, nullptr, 0, packed_size);
			if (alloc)
			{
				// Copy context from stack
				ctx = (aason_context*)alloc;
				memcpy(ctx, &temp_ctx, sizeof(aason_context));
				alloc += context_size;

				// Element array
				ctx->elements = (aason_element*)alloc;
				alloc += element_size;

				// Stack
				ctx->stack = (aason_stack_entry*)alloc;
				ctx->stack[0] = (aason_stack_entry){};

				if (aason_finalise(ctx, desc->source, &tokens))
				{
					ctx->stack_depth = 0;
					ctx->locale = aason_new_locale();
				}
				else
				{
					// Free context in case of error
					desc->allocator(desc->allocator_data, alloc, 0, 0);
					ctx = nullptr;
				}
			}
		}

		// Free scratch token array
		desc->scratch(desc->scratch_data, tokens.tokens, 0, 0);
	}

	return ctx;
}

bool aason_read_array_enter(aason_context* ctx, const char* key, aason_flags flags, int64_t* size, int64_t max_size)
{
	const aason_element* element = aason_read_find_object_element(ctx, key, flags, aason_type_array);
	if (element)
	{
		if (element->array_value.count > max_size)
		{
			aason_read_error(ctx, aason_error_buffer_too_small, element->line, element->column,
				"Array '{s}' size is {u32} but max size is {i64}",
				key, element->array_value.count, max_size
			);

			return false;
		}

		const uint32_t stack_depth = ++ctx->stack_depth;
		aason_assert(stack_depth < ctx->max_stack_depth);

		ctx->stack[stack_depth].element_index = (uint32_t)(element - ctx->elements);
		ctx->stack[stack_depth].array_index = 0;

		if (size)
			*size = element->array_value.count;

		return true;
	}

	return false;
}

void aason_read_array_leave(aason_context* ctx)
{
	aason_assert(ctx);
	aason_assert(ctx->stack_depth > 0);
	aason_assert(ctx->stack_depth < ctx->max_stack_depth);

	const uint32_t element_index = ctx->stack[ctx->stack_depth].element_index;
	aason_assert(element_index < ctx->element_count);

	const aason_element* element = &ctx->elements[element_index];
	aason_assert(element->type == aason_type_array);

	--ctx->stack_depth;
}

bool aason_read_array_enter_object(aason_context* ctx)
{
	const aason_element* element = aason_read_get_next_array_element(ctx, aason_type_object);
	if (element)
	{
		const uint32_t stack_depth = ++ctx->stack_depth;
		aason_assert(stack_depth < ctx->max_stack_depth);

		ctx->stack[stack_depth].element_index = (uint32_t)(element - ctx->elements);
		++ctx->stack[stack_depth].array_index;

		return true;
	}

	return false;
}

void aason_read_array_leave_object(aason_context* ctx)
{
	aason_assert(ctx);
	aason_assert(ctx->stack_depth > 0);
	aason_assert(ctx->stack_depth < ctx->max_stack_depth);

	const uint32_t element_index = ctx->stack[ctx->stack_depth].element_index;
	aason_assert(element_index < ctx->element_count);

	const aason_element* element = &ctx->elements[element_index];
	aason_assert(element->type == aason_type_object);

	--ctx->stack_depth;
}

bool aason_read_array_str(aason_context* ctx, const char** value, int64_t* len)
{
	aason_assert(value);

	const aason_element* element = aason_read_get_next_array_element(ctx, aason_type_str);
	if (element)
	{
		*value = ctx->buffer + element->str_value.offset;

		if (len)
			*len = element->str_value.len;

		return true;
	}

	return false;
}

bool aason_read_array_fixed_str(aason_context* ctx, char* value, int64_t buffer_size, bool truncate)
{
	aason_assert(value);
	aason_assert(buffer_size > 0);

	const aason_element* element = aason_read_get_next_array_element(ctx, aason_type_str);
	if (element)
	{
		const char* str = ctx->buffer + element->str_value.offset;
		const int64_t len = element->str_value.len;

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
			aason_read_error(ctx, aason_error_buffer_too_small, element->line, element->column,
				"String element of length '{u32}' is too large for fixed sized buffer size of '{i64}'",
				len, buffer_size
			);
		}
	}

	return false;
}

bool aason_read_array_int(aason_context* ctx, int64_t* value)
{
	aason_assert(value);

	const aason_element* element = aason_read_get_next_array_element(ctx, aason_type_int);
	if (element)
	{
		*value = element->int_value;
		return true;
	}

	return false;
}

bool aason_read_array_int_ranged(aason_context* ctx, int64_t* value, int64_t min, int64_t max)
{
	aason_assert(value);

	const aason_element* element = aason_read_get_next_array_element(ctx, aason_type_int);
	if (element)
	{
		if (element->int_value < min || element->int_value > max)
		{
			aason_read_error(ctx, aason_error_buffer_too_small, element->line, element->column,
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

bool aason_read_array_bool(aason_context* ctx, bool* value)
{
	aason_assert(value);

	const aason_element* element = aason_read_get_next_array_element(ctx, aason_type_bool);
	if (element)
	{
		*value = element->bool_value;
		return true;
	}

	return false;
}

bool aason_read_array_hash(aason_context* ctx, uint32_t* value)
{
	aason_assert(value);

	const aason_element* element = aason_read_get_next_array_element(ctx, aason_type_hash);
	if (element)
	{
		*value = element->hash_value;
		return true;
	}

	return false;
}

bool aason_read_array_float(aason_context* ctx, float* value)
{
	aason_assert(value);

	const aason_element* element = aason_read_get_next_array_element(ctx, aason_type_float);
	if (element)
	{
		*value = element->float_value;
		return true;
	}

	return false;
}

bool aason_read_array_enum(aason_context* ctx, int32_t* value, const char** strings, int32_t count)
{
	aason_assert(value);
	aason_assert(strings);
	aason_assert(count > 1);

	const aason_element* element = aason_read_get_next_array_element(ctx, aason_type_enum);
	if (element)
	{
		const uint32_t lhs_offset = element->enum_value.offset;
		const int64_t lhs_len = element->enum_value.len;
		const char* enum_value = &ctx->buffer[lhs_offset];

		for (int32_t i = 0; i < count; ++i)
		{
			const int64_t rhs_len = strlen(strings[i]);
			if (lhs_len == rhs_len && memcmp(enum_value, strings[i], lhs_len) == 0)
			{
				*value = i;
				return true;
			}
		}

		aason_read_error(ctx, aason_error_invalid_enum, element->line, element->column,
			"Invalid enum value '{s}' in array", enum_value
		);
	}

	return false;
}

bool aason_read_object_enter(aason_context* ctx, const char* key, aason_flags flags)
{
	aason_assert(ctx->stack_depth + 1 < ctx->max_stack_depth);

	const aason_element* element = aason_read_find_object_element(ctx, key, flags, aason_type_object);
	if (element)
	{
		const uint32_t stack_depth = ++ctx->stack_depth;
		aason_assert(stack_depth < ctx->max_stack_depth);

		ctx->stack[stack_depth].element_index = (uint32_t)(element - ctx->elements);
		return true;
	}

	return false;
}

void aason_read_object_leave(aason_context* ctx)
{
	aason_assert(ctx);
	aason_assert(ctx->stack_depth > 0);
	aason_assert(ctx->stack_depth < ctx->max_stack_depth);

	const uint32_t element_index = ctx->stack[ctx->stack_depth].element_index;
	aason_assert(element_index < ctx->element_count);

	const aason_element* element = &ctx->elements[element_index];
	aason_assert(element->type == aason_type_object);

	--ctx->stack_depth;
}

bool aason_read_object_str(aason_context* ctx, const char* key, aason_flags flags, const char** value, int64_t* len)
{
	aason_assert(value);

	const aason_element* element = aason_read_find_object_element(ctx, key, flags, aason_type_str);
	if (element)
	{
		*value = ctx->buffer + element->str_value.offset;

		if (len)
			*len = element->str_value.len;

		return true;
	}

	return false;
}

bool aason_read_object_fixed_str(aason_context* ctx, const char* key, aason_flags flags, char* value, int64_t buffer_size, bool truncate)
{
	aason_assert(value);
	aason_assert(buffer_size > 0);

	const aason_element* element = aason_read_find_object_element(ctx, key, flags, aason_type_str);
	if (element)
	{
		const char* str = ctx->buffer + element->str_value.offset;
		const int64_t len = element->str_value.len;

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
			aason_read_error(ctx, aason_error_buffer_too_small, element->line, element->column,
				"String element of length '{u32}' is too large for fixed sized buffer size of '{i64}'",
				len, buffer_size
			);
		}
	}

	return false;
}

bool aason_read_object_int(aason_context* ctx, const char* key, aason_flags flags, int64_t* value)
{
	aason_assert(value);

	const aason_element* element = aason_read_find_object_element(ctx, key, flags, aason_type_int);
	if (element)
	{
		*value = element->int_value;
		return true;
	}

	return false;
}

bool aason_read_object_int_ranged(aason_context* ctx, const char* key, aason_flags flags, int64_t* value, int64_t min, int64_t max)
{
	aason_assert(value);

	const aason_element* element = aason_read_find_object_element(ctx, key, flags, aason_type_int);
	if (element)
	{
		if (element->int_value < min || element->int_value > max)
		{
			aason_read_error(ctx, aason_error_buffer_too_small, element->line, element->column,
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

bool aason_read_object_bool(aason_context* ctx, const char* key, aason_flags flags, bool* value)
{
	aason_assert(value);

	const aason_element* element = aason_read_find_object_element(ctx, key, flags, aason_type_bool);
	if (element)
	{
		*value = element->bool_value;
		return true;
	}

	return false;
}

bool aason_read_object_hash(aason_context* ctx, const char* key, aason_flags flags, uint32_t* value)
{
	aason_assert(value);

	const aason_element* element = aason_read_find_object_element(ctx, key, flags, aason_type_hash);
	if (element)
	{
		*value = element->hash_value;
		return true;
	}

	return false;
}

bool aason_read_object_float(aason_context* ctx, const char* key, aason_flags flags, float* value)
{
	aason_assert(value);

	const aason_element* element = aason_read_find_object_element(ctx, key, flags, aason_type_float);
	if (element)
	{
		*value = element->float_value;
		return true;
	}

	return false;
}

bool aason_read_object_enum(aason_context* ctx, const char* key, aason_flags flags, int32_t* value, const char** strings, int32_t count)
{
	aason_assert(value);
	aason_assert(strings);
	aason_assert(count >= 1);

	const aason_element* element = aason_read_find_object_element(ctx, key, flags, aason_type_enum);
	if (element)
	{
		const uint32_t lhs_offset = element->enum_value.offset;
		const int64_t lhs_len = element->enum_value.len;
		const char* enum_value = &ctx->buffer[lhs_offset];

		for (int32_t i = 0; i < count; ++i)
		{
			const int64_t rhs_len = strlen(strings[i]);
			if (lhs_len == rhs_len && memcmp(enum_value, strings[i], lhs_len) == 0)
			{
				*value = i;
				return true;
			}
		}

		aason_read_error(ctx, aason_error_invalid_enum, element->line, element->column,
			"Invalid enum value '{s}' found in element '{s}'", enum_value, key
		);
	}

	return false;
}