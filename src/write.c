static char* aason_write_new_line(aason_context* ctx, char* out)
{
	if (ctx->first)
		ctx->first = false;
	else
		*out++ = ',';

	if (ctx->first_line)
	{
		aason_assert(ctx->stack_depth == 0);

		ctx->first_line = false;
	}
	else
	{
		*out++ = '\n';
		for (uint32_t i = 0; i < ctx->stack_depth; ++i)
			*out++ = '\t';
	}

	return out;
}

static void aason_write_flush(aason_context* ctx)
{
	if (ctx->committed)
	{
		ctx->write_interface.write(ctx->write_interface.user_data, ctx->stream, ctx->buffer, ctx->committed);
		ctx->committed = 0;
	}
}

static void aason_write_str(aason_context* ctx, const char* str, size_t length)
{
	aason_write_flush(ctx);
	ctx->write_interface.write(ctx->write_interface.user_data, ctx->stream, str, length);
}

static char* aason_write_reserve(aason_context* ctx, size_t size)
{
	aason_assert(ctx->reserved == 0);
	aason_assert(size <= ctx->buffer_size);

	if (ctx->committed + size > ctx->buffer_size)
		aason_write_flush(ctx);

	char* out = ctx->buffer + ctx->committed;
	ctx->reserved = ctx->committed + size;

	return out;
}

static void aason_write_commit(aason_context* ctx, char* end)
{
	aason_assert(end >= ctx->buffer);
	aason_assert(end <= ctx->buffer + ctx->reserved);

	ctx->committed = end - ctx->buffer;
	ctx->reserved = 0;
}

static char* aason_write_add_array_element(aason_context* ctx, size_t value_len, size_t extra_len)
{
	aason_assert(ctx);
	aason_assert(value_len >= 0);
	aason_assert(extra_len >= 0);

	const uint32_t stack_depth = ctx->stack_depth;
	const size_t len = value_len + stack_depth + extra_len + !ctx->first + 1;

	return aason_write_new_line(ctx, aason_write_reserve(ctx, len));
}

static char* aason_write_add_object_element(aason_context* ctx, const char* key, size_t value_len, size_t extra_len)
{
	aason_assert(ctx);
	aason_assert(key);
	aason_assert(*key);
	aason_assert(value_len >= 0);
	aason_assert(extra_len >= 0);

	const bool first = ctx->first;
	const bool first_line = ctx->first_line;
	const uint32_t stack_depth = ctx->stack_depth;
	const size_t key_len = strlen(key);

	const size_t len = key_len + value_len + stack_depth + extra_len + !first + !first_line + 2;
	char* out = aason_write_new_line(ctx, aason_write_reserve(ctx, len));

	memcpy(out, key, key_len);
	out += key_len;
	*out++ = ':';
	*out++ = ' ';

	return out;
}

aason_context* aason_write(const aason_write_desc* desc)
{
	aason_assert(desc);

	// Use 4-space tabs by default
	const uint32_t tab_size = desc->tab_size ? desc->tab_size : 4;

	const size_t buffer_size = desc->buffer_size ? desc->buffer_size : 4096;

	aason_allocator allocator = aason_default_allocator;
	if (desc->allocator)
		allocator = *desc->allocator;

	aason_write_interface write_interface = aason_default_write_interface;
	if (desc->write_interface)
		write_interface = *desc->write_interface;

	void* stream = write_interface.open(write_interface.user_data, desc->path);
	if (stream)
	{
		char* buffer = aason_alloc(&allocator, buffer_size);
		if (buffer)
		{
			// Calculate the amount of memory required to copy constructors
			const aason_constructor_sizes constructor_sizes = aason_get_constructor_sizes(desc->constructors, desc->constructor_count);
			const size_t constructor_size = aason_get_constructors_allocation_size(constructor_sizes, desc->constructor_count);

			// The context and constructor data are packed into a single allocation
			const size_t context_size	= aason_align_size(sizeof(aason_context));
			const size_t packed_size	= context_size + constructor_size;

			uint8_t* alloc = aason_alloc(&allocator, packed_size);
			if (alloc)
			{
				aason_context* ctx = aason_suballoc(&alloc, context_size);
				*ctx = (aason_context){
					.error				= aason_error_none,
					.tab_size			= tab_size,
					.reading			= false,
					.locale				= aason_new_locale(),
					.allocator			= allocator,
					//.constructors		= desc->constructors,
					.buffer				= buffer,
					.buffer_size		= buffer_size,
					.first				= true,
					.first_line			= true,
					.reserved			= 0,
					.committed			= 0,
					.stream				= stream,
					.write_interface	= write_interface
				};

				if (desc->constructor_count)
					ctx->constructors = aason_copy_constructors(alloc, constructor_sizes, desc->constructors, desc->constructor_count, true);

				return ctx;
			}
			else
			{
				// TODO: Error
				aason_assert(false);
			}
		}

		write_interface.close(write_interface.user_data, stream);
	}
	else
	{
		// TODO: Report error
		aason_assert(false);
	}

	return NULL;
}

void aason_write_enter_root_array(aason_context* ctx)
{
	aason_assert(ctx->stack_depth == 0);

	char* out = aason_write_new_line(ctx, aason_write_reserve(ctx, 1));
	*out++ = '[';
	aason_write_commit(ctx, out);

	++ctx->stack_depth;
	ctx->first = true;
}

void aason_write_enter_root_object(aason_context* ctx)
{
	aason_assert(ctx->stack_depth == 0);

	char* out = aason_write_new_line(ctx, aason_write_reserve(ctx, 1));
	*out++ = '{';
	aason_write_commit(ctx, out);

	++ctx->stack_depth;
	ctx->first = true;
}

void aason_write_leave_array(aason_context* ctx)
{
	aason_assert(ctx);
	aason_assert(ctx->stack_depth > 0);

	const uint32_t stack_depth = --ctx->stack_depth;

	if (ctx->first)
	{
		char* out = aason_write_reserve(ctx, 1);
		*out++ = ']';
		aason_write_commit(ctx, out);
	}
	else
	{
		char* out = aason_write_reserve(ctx, stack_depth + 2);
		*out++ = '\n';
		for (uint32_t i = 0; i < ctx->stack_depth; ++i)
			*out++ = '\t';
		*out++ = ']';
		aason_write_commit(ctx, out);
	}

	ctx->first = false;
}

void aason_write_leave_object(aason_context* ctx)
{
	aason_assert(ctx);
	aason_assert(ctx->stack_depth > 0);

	const uint32_t stack_depth = --ctx->stack_depth;

	if (ctx->first)
	{
		char* out = aason_write_reserve(ctx, 1);
		*out++ = '}';
		aason_write_commit(ctx, out);
	}
	else
	{
		const size_t len = stack_depth + 2;

		char* out = aason_write_reserve(ctx, len);
		*out++ = '\n';
		for (uint32_t i = 0; i < ctx->stack_depth; ++i)
			*out++ = '\t';
		*out++ = '}';
		aason_write_commit(ctx, out);
	}

	ctx->first = false;
}

void aason_write_array_enter_object(aason_context* ctx)
{
	char* out = aason_write_add_array_element(ctx, 0, 1);
	*out++ = '{';
	aason_write_commit(ctx, out);

	++ctx->stack_depth;
	ctx->first = true;
}

void aason_write_array_object_leave(aason_context* ctx)
{
	aason_write_leave_object(ctx);
}

void aason_write_array_str(aason_context* ctx, const char* value)
{
	aason_assert(value);
	//aason_assert(*value);

	char* out = aason_write_add_array_element(ctx, 1, 0);
	*out++ = '"';
	aason_write_commit(ctx, out);

	// Flush and write to callback directly in case string exceeds buffer size
	const size_t len = strlen(value);
	aason_write_str(ctx, value, len);

	out = aason_write_reserve(ctx, 1);
	*out++ = '"';
	aason_write_commit(ctx, out);
}

void aason_write_array_int(aason_context* ctx, int64_t value)
{
	char* out = aason_write_add_array_element(ctx, aason_int_max_len, 0);
	out = aason_to_string_int(out, value);
	aason_write_commit(ctx, out);
}

void aason_write_array_bool(aason_context* ctx, bool value)
{
	char* out = aason_write_add_array_element(ctx, aason_bool_max_len, 0);
	out = aason_to_string_bool(out, value);
	aason_write_commit(ctx, out);
}

void aason_write_array_enum(aason_context* ctx, int32_t value, const char** strings, int32_t count)
{
	aason_assert(strings);
	aason_assert(value >= 0);
	aason_assert(count > 0);
	aason_assert(value < count);

	const size_t len = strlen(strings[value]);

	char* out = aason_write_add_array_element(ctx, len, 0);
	memcpy(out, strings[value], len);
	aason_write_commit(ctx, out + len);
}

void aason_write_array_float(aason_context* ctx, float value)
{
	char* out = aason_write_add_array_element(ctx, aason_float_max_len, 0);
	out = aason_to_string_float(ctx->locale, out, value);
	aason_write_commit(ctx, out);
}

void aason_write_object_enter_array(aason_context* ctx, const char* key)
{
	char* out = aason_write_add_object_element(ctx, key, 0, 1);
	*out++ = '[';
	aason_write_commit(ctx, out);

	++ctx->stack_depth;
	ctx->first = true;
}

void aason_write_object_enter_object(aason_context* ctx, const char* key)
{
	char* out = aason_write_add_object_element(ctx, key, 0, 1);
	*out++ = '{';
	aason_write_commit(ctx, out);

	++ctx->stack_depth;
	ctx->first = true;
}

void aason_write_object_str(aason_context* ctx, const char* key, const char* value)
{
	aason_assert(value);
	//aason_assert(*value);

	char* out = aason_write_add_object_element(ctx, key, 1, 0);
	*out++ = '"';
	aason_write_commit(ctx, out);

	// Flush and write to callback directly in case string exceeds buffer size
	const size_t len = strlen(value);
	aason_write_str(ctx, value, len);

	out = aason_write_reserve(ctx, 1);
	*out++ = '"';
	aason_write_commit(ctx, out);
}

void aason_write_object_int(aason_context* ctx, const char* key, int64_t value)
{
	char* out = aason_write_add_object_element(ctx, key, aason_int_max_len, 0);
	out = aason_to_string_int(out, value);
	aason_write_commit(ctx, out);
}

void aason_write_object_bool(aason_context* ctx, const char* key, bool value)
{
	char* out = aason_write_add_object_element(ctx, key, aason_bool_max_len, 0);
	out = aason_to_string_bool(out, value);
	aason_write_commit(ctx, out);
}

void aason_write_object_float(aason_context* ctx, const char* key, float value)
{
	char* out = aason_write_add_object_element(ctx, key, aason_float_max_len, 0);
	out = aason_to_string_float(ctx->locale, out, value);
	aason_write_commit(ctx, out);
}

void aason_write_object_enum(aason_context* ctx, const char* key, int32_t value, const char** strings, int32_t count)
{
	aason_assert(strings);
	aason_assert(value >= 0);
	aason_assert(count > 0);
	aason_assert(value < count);

	const size_t len = strlen(strings[value]);

	char* out = aason_write_add_object_element(ctx, key, len, 0);
	memcpy(out, strings[value], len);
	aason_write_commit(ctx, out + len);
}

static uint16_t aason_write_find_constructor(const aason_constructors* constructors, const char* type)
{
	aason_assert(type);

	if (constructors)
	{
		for (uint16_t i = 0; i < constructors->count; ++i)
		{
			if (strcmp(type, constructors->names[i]) == 0)
				return i;
		}
	}

	aason_assert(false);

	return UINT16_MAX;
}

void aason_write_object_constructor(aason_context* ctx, const char* key, const void* value, const char* type)
{
	aason_assert(value);
	aason_assert(type);

	const aason_constructors* constructors = ctx->constructors;
	const uint16_t constructor_index = aason_write_find_constructor(constructors, type);

	const char* name = constructors->names[constructor_index];

	// TODO: Merge compare and length query into a single loop
	const size_t len = strlen(name);

	char* out = aason_write_add_object_element(ctx, key, len + 1, 0);
	memcpy(out, name, len);
	out += len;
	*out++ = '(';
	aason_write_commit(ctx, out);

	ctx->constructor_index = constructor_index;
	ctx->arg_index = 0;

	constructors->write_callbacks[constructor_index](ctx, value);

	out = aason_write_reserve(ctx, 1);
	*out++ = ')';
	aason_write_commit(ctx, out);
}

static char* aason_write_add_arg(aason_context* ctx, size_t len, aason_arg_type type)
{
	aason_assert(ctx);
	aason_assert(len >= 0);

	const aason_constructors* constructors = ctx->constructors;
	const uint32_t constructor_index = ctx->constructor_index;
	aason_assert(constructor_index < constructors->count);

	const uint32_t arg_count = constructors->arg_counts[constructor_index];
	aason_assert(arg_count <= UINT8_MAX);

	const uint32_t current_index = ctx->arg_index++;
	aason_assert(current_index < arg_count);

	const uint32_t arg_index = constructors->arg_indices[constructor_index] + current_index;
	aason_arg_type arg_type = constructors->args[arg_index];
	aason_assert(type == arg_type);

	char* out;
	if (current_index)
	{
		out = aason_write_reserve(ctx, len + 2);
		*out++ = ',';
		*out++ = ' ';
	}
	else
	{
		out = aason_write_reserve(ctx, len);
	}

	return out;
}

void aason_write_arg_str(aason_context* ctx, const char* value)
{
	aason_assert(value);

	char* out = aason_write_add_arg(ctx, 1, aason_arg_type_str);
	*out++ = '"';
	aason_write_commit(ctx, out);

	// Flush and write to callback directly in case string exceeds buffer size
	const size_t len = strlen(value);
	aason_write_str(ctx, value, len);

	out = aason_write_reserve(ctx, 1);
	*out++ = '"';
	aason_write_commit(ctx, out);
}

void aason_write_arg_int(aason_context* ctx, int64_t value, aason_base base)
{
	aason_arg_type arg_type;
	switch (base)
	{
	case aason_base_bin:
		arg_type = aason_arg_type_bin;
		break;
	case aason_base_hex:
		arg_type = aason_arg_type_hex;
		break;
	default:
		arg_type = aason_arg_type_dec;
		break;
	}

	char* out = aason_write_add_arg(ctx, aason_int_max_len, arg_type);
	out = aason_to_string_int(out, value);
	aason_write_commit(ctx, out);
}

void aason_write_arg_bool(aason_context* ctx, bool value)
{
	char* out = aason_write_add_arg(ctx, aason_bool_max_len, aason_arg_type_bool);
	out = aason_to_string_bool(out, value);
	aason_write_commit(ctx, out);
}

void aason_write_arg_enum(aason_context* ctx, int32_t value, const char** strings, int32_t count)
{
	aason_assert(value >= 0);
	aason_assert(count > 0);
	aason_assert(value < count);

	const size_t len = strlen(strings[value]);

	char* out = aason_write_add_arg(ctx, aason_bool_max_len, aason_arg_type_enum);
	memcpy(out, strings[value], len);
	aason_write_commit(ctx, out + len);
}

void aason_write_arg_float(aason_context* ctx, float value)
{
	char* out = aason_write_add_arg(ctx, aason_float_max_len, aason_arg_type_float);
	out = aason_to_string_float(ctx->locale, out, value);
	aason_write_commit(ctx, out);
}