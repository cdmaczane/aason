static char* aason_write_new_line(aason_context* ctx, char* out)
{
	if (ctx->first)
		ctx->first = false;
	else
		*out++ = ',';

	if (ctx->first_line)
	{
		rgs_assert(ctx->stack_depth == 0);

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

static void aason_write_str(aason_context* ctx, const char* str, int64_t size)
{
	aason_write_flush(ctx);
	ctx->write_str(ctx->user_data, str, size);
}

static char* aason_write_alloc(aason_context* ctx, int64_t size)
{
	rgs_assert(size <= RGS_SDD_WRITE_BUFFER_SIZE);

	if (ctx->offset + size > RGS_SDD_WRITE_BUFFER_SIZE)
		aason_write_flush(ctx);

	char* out = ctx->buffer + ctx->offset;
	ctx->offset += size;

	return out;
}

static char* aason_write_add_array_element(aason_context* ctx, int64_t value_len, int64_t extra_len)
{
	rgs_assert(ctx);
	rgs_assert(value_len >= 0);
	rgs_assert(extra_len >= 0);

	const uint32_t stack_depth = ctx->stack_depth;
	const int64_t len = value_len + stack_depth + extra_len + !ctx->first + 1;

	return aason_write_new_line(ctx, aason_write_alloc(ctx, len));
}

static char* aason_write_add_object_element(aason_context* ctx, const char* key, int64_t value_len, int64_t extra_len)
{
	rgs_assert(ctx);
	rgs_assert(key);
	rgs_assert(*key);
	rgs_assert(value_len >= 0);
	rgs_assert(extra_len >= 0);

	const bool first = ctx->first;
	const bool first_line = ctx->first_line;
	const uint32_t stack_depth = ctx->stack_depth;
	const int64_t key_len = strlen(key);
	rgs_assert(key_len <= RGS_SDD_KEY_MAX_LEN);

	const int64_t len = key_len + value_len + stack_depth + extra_len + !first + !first_line + 2;
	char* out = aason_write_new_line(ctx, aason_write_alloc(ctx, len));
	memcpy(out, key, key_len);
	out += key_len;
	*out++ = ':';
	*out++ = ' ';

	return out;
}

aason_context* aason_write(aason_write_callback callback, void* user_data)
{
	rgs_assert(callback);

	aason_context* ctx = rgs_alloc(RGS_PAGE_SIZE, RGS_PAGE_SIZE);
	ctx->buffer = (char*)(ctx + 1);
	ctx->error = aason_error_none;
	ctx->reading = false;
	ctx->stack_depth = 0;
	ctx->user_data = user_data;
	ctx->first = true;
	ctx->first_line = true;
	ctx->offset = 0;
	ctx->write_str = callback;

	return ctx;
}

// TODO: Move
void aason_destroy(aason_context* ctx)
{
	if (!ctx->reading)
		aason_write_flush(ctx);

	rgs_free(ctx);
}

void aason_write_array_enter(aason_context* ctx, const char* key)
{
	char* out = aason_write_add_object_element(ctx, key, 0, 1);
	*out = '[';

	++ctx->stack_depth;
	ctx->first = true;
}

void aason_write_array_leave(aason_context* ctx)
{
	rgs_assert(ctx);
	rgs_assert(ctx->stack_depth > 0);

	const int64_t stack_depth = --ctx->stack_depth;

	if (ctx->first)
	{
		*aason_write_alloc(ctx, 1) = ']';
	}
	else
	{
		char* out = aason_write_alloc(ctx, stack_depth + 2);
		*out++ = '\n';
		for (uint32_t i = 0; i < ctx->stack_depth; ++i)
			*out++ = '\t';
		*out++ = ']';
	}

	ctx->first = false;
}

void aason_write_array_object_enter(aason_context* ctx)
{
	*aason_write_add_array_element(ctx, 0, 1) = '{';
	++ctx->stack_depth;
	ctx->first = true;
}

void aason_write_array_object_leave(aason_context* ctx)
{
	aason_write_object_leave(ctx);
}

void aason_write_array_str(aason_context* ctx, const char* value)
{
	rgs_assert(value);
	rgs_assert(*value);

	const int64_t len = strlen(value);
	*aason_write_add_array_element(ctx, 1, 0) = '"';
	aason_write_str(ctx, value, len);
	*aason_write_alloc(ctx, 1) = '"';
}

void aason_write_array_int(aason_context* ctx, int64_t value)
{
	char* out = aason_write_add_array_element(ctx, rgs_strlen_int(value, rgs_int_base_dec, 0), 0);
	rgs_to_string_unsafe_int(out, value, rgs_int_base_dec, 0);
}

void aason_write_array_bool(aason_context* ctx, bool value)
{
	char* out = aason_write_add_array_element(ctx, rgs_strlen_bool(value), 0);
	rgs_to_string_unsafe_bool(out, value);
}

void aason_write_array_hash(aason_context* ctx, uint32_t value)
{
	char* out = aason_write_add_array_element(ctx, 8, 1);
	rgs_to_string_unsafe_uint(out, value, rgs_int_base_hex, 8);
}

void aason_write_array_float(aason_context* ctx, float value)
{
	char* out = aason_write_add_array_element(ctx, rgs_strlen_float(value, RGS_FLOAT_DECIMAL_PLACES_MAX), 0);
	rgs_to_string_unsafe_float(out, value, RGS_FLOAT_DECIMAL_PLACES_MAX);
}

void aason_write_array_enum(aason_context* ctx, int32_t value, const char** strings, int32_t count)
{
	rgs_assert(strings);
	rgs_assert(value >= 0);
	rgs_assert(count > 0);
	rgs_assert(value < count);

	const int64_t len = strlen(strings[value]);
	rgs_assert(len <= RGS_SDD_ENUM_MAX_LEN);

	char* out = aason_write_add_array_element(ctx, len, 0);
	memcpy(out, strings[value], len);
}

void aason_write_object_enter(aason_context* ctx, const char* key)
{
	*aason_write_add_object_element(ctx, key, 0, 1) = '{';
	++ctx->stack_depth;
	ctx->first = true;
}

void aason_write_object_leave(aason_context* ctx)
{
	rgs_assert(ctx);
	rgs_assert(ctx->stack_depth > 0);

	const int64_t stack_depth = --ctx->stack_depth;

	if (ctx->first)
	{
		*aason_write_alloc(sdd, 1) = '}';
	}
	else
	{
		const int64_t len = stack_depth + 2;
		char* out = aason_write_alloc(ctx, len);

		*out++ = '\n';
		for (uint32_t i = 0; i < ctx->stack_depth; ++i)
			*out++ = '\t';
		*out++ = '}';
	}

	ctx->first = false;
}

void aason_write_object_str(aason_context* ctx, const char* key, const char* value)
{
	rgs_assert(value);
	rgs_assert(*value);

	const int64_t len = strlen(value);
	char* out = aason_write_add_object_element(ctx, key, len, 2); // ""
	*out++ = '"';
	memcpy(out, value, len);
	out += len;
	*out = '"';
}

void aason_write_object_int(aason_context* ctx, const char* key, int64_t value)
{
	char* out = aason_write_add_object_element(ctx, key, rgs_strlen_int(value, rgs_int_base_dec, 0), 0);
	rgs_to_string_unsafe_int(out, value, rgs_int_base_dec, 0);
}

void aason_write_object_bool(aason_context* ctx, const char* key, bool value)
{
	char* out = aason_write_add_object_element(ctx, key, rgs_strlen_bool(value), 0);
	rgs_to_string_unsafe_bool(out, value);
}

void aason_write_object_hash(aason_context* ctx, const char* key, uint32_t value)
{
	char* out = aason_write_add_object_element(ctx, key, 8, 1);
	*out++ = '#';
	rgs_to_string_unsafe_uint(out, value, rgs_int_base_hex, 8);
}

void aason_write_object_float(aason_context* ctx, const char* key, float value)
{
	char* out = aason_write_add_object_element(ctx, key, rgs_strlen_float(value, RGS_FLOAT_DECIMAL_PLACES_MAX), 0);
	rgs_to_string_unsafe_float(out, value, RGS_FLOAT_DECIMAL_PLACES_MAX);
}

void aason_write_object_enum(aason_context* ctx, const char* key, int32_t value, const char** strings, int32_t count)
{
	rgs_assert(strings);
	rgs_assert(value >= 0);
	rgs_assert(count > 0);
	rgs_assert(value < count);

	const int64_t len = strlen(strings[value]);
	char* out = aason_write_add_object_element(ctx, key, len, 0);
	memcpy(out, strings[value], len);
}