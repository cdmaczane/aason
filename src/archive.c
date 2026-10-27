bool aason_archive_enter_root_array(aason_context* ctx, size_t* size)
{
	aason_assert(size);

	if (ctx->reading)
		return aason_read_enter_root_array(ctx, size, SIZE_MAX);

	aason_write_enter_root_array(ctx);
	return true;
}

bool aason_archive_enter_root_object(aason_context* ctx)
{
	if (ctx->reading)
		return aason_read_enter_root_object(ctx);

	aason_write_enter_root_object(ctx);
	return true;
}

void aason_archive_leave_array(aason_context* ctx)
{
	if (ctx->reading)
		aason_read_leave_array(ctx);
	else
		aason_write_leave_array(ctx);
}

void aason_archive_leave_object(aason_context* ctx)
{
	if (ctx->reading)
		aason_read_leave_object(ctx);
	else
		aason_write_leave_object(ctx);
}

bool aason_archive_array_enter_array(aason_context* ctx, size_t* size)
{
	if (ctx->reading)
		return aason_read_array_enter_array(ctx, size, SIZE_MAX);

	aason_write_array_enter_array(ctx);
	return true;
}

bool aason_archive_array_enter_object(aason_context* ctx)
{
	if (ctx->reading)
		return aason_read_array_enter_object(ctx);

	aason_write_array_enter_object(ctx);
	return true;
}

void aason_archive_array_fixed_str(aason_context* ctx, char* value, size_t buffer_size, bool truncate)
{
	if (ctx->reading)
		aason_read_array_fixed_str(ctx, value, buffer_size, truncate);
	else
		aason_write_array_str(ctx, value);
}

void aason_archive_array_duplicated_str(aason_context* ctx, char** value)
{
	aason_assert(value);

	if (ctx->reading)
		aason_read_array_duplicated_str(ctx, value);
	else
		aason_write_array_str(ctx, *value);
}

void aason_archive_array_int(aason_context* ctx, int64_t* value)
{
	aason_assert(value);

	if (ctx->reading)
		aason_read_array_int(ctx, value);
	else
		aason_write_array_int(ctx, *value);
}

void aason_archive_array_dec(aason_context* ctx, int64_t* value, int min_digits)
{
	aason_assert(value);

	if (ctx->reading)
		aason_read_array_int(ctx, value);
	else
		aason_write_array_dec(ctx, *value, min_digits);
}

void aason_archive_array_hex(aason_context* ctx, int64_t* value, int min_digits)
{
	aason_assert(value);

	if (ctx->reading)
		aason_read_array_int(ctx, value);
	else
		aason_write_array_hex(ctx, *value, min_digits);
}

void aason_archive_array_bin(aason_context* ctx, int64_t* value, int min_digits)
{
	aason_assert(value);

	if (ctx->reading)
		aason_read_array_int(ctx, value);
	else
		aason_write_array_bin(ctx, *value, min_digits);
}

void aason_archive_array_bool(aason_context* ctx, bool* value)
{
	aason_assert(value);

	if (ctx->reading)
		aason_read_array_bool(ctx, value);
	else
		aason_write_array_bool(ctx, *value);
}

void aason_archive_array_float(aason_context* ctx, float* value)
{
	aason_assert(value);

	if (ctx->reading)
		aason_read_array_float(ctx, value);
	else
		aason_write_array_float(ctx, *value);
}

void aason_archive_array_enum(aason_context* ctx, int32_t* value, const char** strings, int32_t count)
{
	aason_assert(value);

	if (ctx->reading)
		aason_read_array_enum(ctx, value, strings, count);
	else
		aason_write_array_enum(ctx, *value, strings, count);
}

void aason_archive_array_constructor(aason_context* ctx, void* value, const char* type)
{
	aason_assert(value);

	if (ctx->reading)
		aason_read_array_constructor(ctx, value, type);
	else
		aason_write_array_constructor(ctx, value, type);
}

bool aason_archive_object_enter_array(aason_context* ctx, const char* key, size_t* size)
{
	aason_assert(size);

	if (ctx->reading)
		return aason_read_object_enter_array(ctx, key, aason_optional, size, SIZE_MAX);

	aason_write_object_enter_array(ctx, key);
	return true;
}

bool aason_archive_object_enter_object(aason_context* ctx, const char* key)
{
	if (ctx->reading)
		return aason_read_object_enter_object(ctx, key, aason_optional);

	aason_write_object_enter_object(ctx, key);
	return true;
}

bool aason_archive_object_fixed_str(aason_context* ctx, const char* key, char* value, size_t buffer_size, bool truncate)
{
	aason_assert(value);
	aason_assert(buffer_size);

	if (ctx->reading)
		return aason_read_object_fixed_str(ctx, key, aason_optional, value, buffer_size, truncate);

	aason_write_object_str(ctx, key, value);
	return true;
}

bool aason_archive_object_duplicated_str(aason_context* ctx, const char* key, char** value)
{
	aason_assert(value);

	if (ctx->reading)
		return aason_read_object_duplicated_str(ctx, key, aason_optional, value);

	aason_write_object_str(ctx, key, *value);
	return true;
}

bool aason_archive_object_int(aason_context* ctx, const char* key, int64_t* value)
{
	aason_assert(value);

	if (ctx->reading)
		return aason_read_object_int(ctx, key, aason_optional, value);

	aason_write_object_int(ctx, key, *value);
	return true;
}

bool aason_archive_object_dec(aason_context* ctx, const char* key, int64_t* value, int min_digits)
{
	aason_assert(value);

	if (ctx->reading)
		return aason_read_object_int(ctx, key, aason_optional, value);

	aason_write_object_dec(ctx, key, *value, min_digits);
	return false;
}

bool aason_archive_object_hex(aason_context* ctx, const char* key, int64_t* value, int min_digits)
{
	aason_assert(value);

	if (ctx->reading)
		return aason_read_object_int(ctx, key, aason_optional, value);

	aason_write_object_hex(ctx, key, *value, min_digits);
	return false;
}

bool aason_archive_object_bin(aason_context* ctx, const char* key, int64_t* value, int min_digits)
{
	aason_assert(value);

	if (ctx->reading)
		return aason_read_object_int(ctx, key, aason_optional, value);

	aason_write_object_bin(ctx, key, *value, min_digits);
	return false;
}

bool aason_archive_object_bool(aason_context* ctx, const char* key, bool* value)
{
	aason_assert(value);

	if (ctx->reading)
		return aason_read_object_bool(ctx, key, aason_optional, value);

	aason_write_object_bool(ctx, key, *value);
	return true;
}

bool aason_archive_object_enum(aason_context* ctx, const char* key, int32_t* value, const char** strings, int32_t count)
{
	aason_assert(value);

	if (ctx->reading)
		return aason_read_object_enum(ctx, key, aason_optional, value, strings, count);

	aason_write_object_enum(ctx, key, *value, strings, count);
	return true;
}

bool aason_archive_object_float(aason_context* ctx, const char* key, float* value)
{
	aason_assert(value);

	if (ctx->reading)
		return aason_read_object_float(ctx, key, aason_optional, value);

	aason_write_object_float(ctx, key, *value);
	return true;
}