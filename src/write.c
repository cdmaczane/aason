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
	if (ctx->offset)
	{
		ctx->write_str(ctx->user_data, ctx->buffer, ctx->offset);
		ctx->offset = 0;
	}
}

static void aason_write_str(aason_context* ctx, const char* str, size_t size)
{
	aason_write_flush(ctx);
	ctx->write_str(ctx->user_data, str, size);
}

static char* aason_write_reserve(aason_context* ctx, size_t size)
{
	aason_assert(ctx->reserved == 0);
	aason_assert(size <= ctx->buffer_size);

	if (ctx->offset + size > ctx->buffer_size)
		aason_write_flush(ctx);

	char* out = ctx->buffer + ctx->offset;
	ctx->reserved = ctx->offset + size;

	return out;
}

static void aason_write_commit(aason_context* ctx, char* end)
{
	aason_assert(end >= ctx->buffer);
	aason_assert(end <= ctx->buffer + ctx->reserved);

	ctx->offset = end - ctx->buffer;
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
	aason_assert(desc->buffer);
	aason_assert(desc->buffer_size >= 4096); // Somewhat arbitrary
	aason_assert(desc->callback);

	uint8_t* buffer = (uint8_t*)desc->buffer;

	aason_context* ctx = (aason_context*)buffer;
	buffer += sizeof(aason_context);

	ctx->buffer = (char*)buffer;
	ctx->error = aason_error_none;
	ctx->reading = false;
	ctx->stack_depth = 0;
	ctx->user_data = desc->user_data;
	ctx->locale = aason_new_locale();
	ctx->buffer_size = desc->buffer_size - sizeof(aason_context);
	ctx->first = true;
	ctx->first_line = true;
	ctx->offset = 0;
	ctx->write_str = desc->callback;

	return ctx;
}

void aason_write_array_enter(aason_context* ctx, const char* key)
{
	char* out = aason_write_add_object_element(ctx, key, 0, 1);
	*out++ = '[';
	aason_write_commit(ctx, out);

	++ctx->stack_depth;
	ctx->first = true;
}

void aason_write_array_leave(aason_context* ctx)
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

void aason_write_array_object_enter(aason_context* ctx)
{
	char* out = aason_write_add_array_element(ctx, 0, 1);
	*out++ = '{';
	aason_write_commit(ctx, out);

	++ctx->stack_depth;
	ctx->first = true;
}

void aason_write_array_object_leave(aason_context* ctx)
{
	aason_write_object_leave(ctx);
}

void aason_write_array_str(aason_context* ctx, const char* value)
{
	aason_assert(value);
	aason_assert(*value);

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

void aason_write_array_hash(aason_context* ctx, uint32_t value)
{
	char* out = aason_write_add_array_element(ctx, aason_hash_len, 0);
	out = aason_to_string_hash(out, value);
	aason_write_commit(ctx, out);
}

void aason_write_array_float(aason_context* ctx, float value)
{
	char* out = aason_write_add_array_element(ctx, aason_float_max_len, 0);
	out = aason_to_string_float(ctx->locale, out, value);
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

void aason_write_object_enter(aason_context* ctx, const char* key)
{
	char* out = aason_write_add_object_element(ctx, key, 0, 1);
	*out++ = '{';
	aason_write_commit(ctx, out);

	++ctx->stack_depth;
	ctx->first = true;
}

void aason_write_object_leave(aason_context* ctx)
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

void aason_write_object_str(aason_context* ctx, const char* key, const char* value)
{
	aason_assert(value);
	aason_assert(*value);

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

void aason_write_object_hash(aason_context* ctx, const char* key, uint32_t value)
{
	char* out = aason_write_add_object_element(ctx, key, aason_hash_len, 0);
	out = aason_to_string_hash(out, value);
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