bool aason_archive_array_enter(aason_context* ctx, const char* key, int64_t* size)
{
	if (ctx->reading)
		return aason_read_array_enter(ctx, key, aason_optional, size, INT64_MAX);

	aason_write_array_enter(ctx, key);
	return true;
}

void aason_archive_array_leave(aason_context* ctx)
{
	if (ctx->reading)
		aason_read_array_leave(ctx);
	else
		aason_write_array_leave(ctx);
}

bool aason_archive_array_object_enter(aason_context* ctx)
{
	if (ctx->reading)
		return aason_read_array_enter_object(ctx);

	aason_write_array_object_enter(ctx);
	return true;
}

void aason_archive_array_object_leave(aason_context* ctx)
{
	if (ctx->reading)
		aason_read_array_leave_object(ctx);
	else
		aason_archive_array_object_leave(ctx);
}

void aason_archive_array_str(aason_context* ctx, char** value)
{
	if (ctx->reading)
		aason_read_array_str(ctx, (const char**)value, nullptr);
	else
		aason_write_array_str(ctx, *value);
}

void aason_archive_array_fixed_str(aason_context* ctx, char* value, int64_t buffer_size, bool truncate)
{
	if (ctx->reading)
		aason_read_array_fixed_str(ctx, value, buffer_size, truncate);
	else
		aason_write_array_str(ctx, value);
}

void aason_archive_array_int(aason_context* ctx, int64_t* value)
{
	if (ctx->reading)
		aason_read_array_int(ctx, value);
	else
		aason_write_array_int(ctx, *value);
}

void aason_archive_array_bool(aason_context* ctx, bool* value)
{
	if (ctx->reading)
		aason_read_array_bool(ctx, value);
	else
		aason_write_array_bool(ctx, *value);
}

void aason_archive_array_float(aason_context* ctx, float* value)
{
	if (ctx->reading)
		aason_read_array_float(ctx, value);
	else
		aason_write_array_float(ctx, *value);
}

void aason_archive_array_enum(aason_context* ctx, int32_t* value, const char** strings, int32_t count)
{
	if (ctx->reading)
		aason_read_array_enum(ctx, value, strings, count);
	else
		aason_write_array_enum(ctx, *value, strings, count);
}

bool aason_archive_object_enter(aason_context* ctx, const char* key)
{
	if (ctx->reading)
		return aason_read_object_enter(ctx, key, aason_optional);

	aason_write_object_enter(ctx, key);
	return true;
}

void aason_archive_object_leave(aason_context* ctx)
{
	if (ctx->reading)
		aason_read_object_leave(ctx);
	else
		aason_write_object_leave(ctx);
}

bool aason_archive_object_str(aason_context* ctx, const char* key, char** value, const char* default_value)
{
	aason_assert(default_value);

	if (ctx->reading)
	{
		if (!aason_read_object_str(ctx, key, aason_optional, (const char**)value, nullptr))
		{
			*value = (char*)default_value;
			return false;
		}
	}
	else
	{
		aason_write_object_str(ctx, key, *value);
	}

	return true;
}

// TODO: Handle length errors as runtime errors, not using asserts
bool aason_archive_object_fixed_str(aason_context* ctx, const char* key, char* value, const char* default_value, int64_t buffer_size, bool truncate)
{
	aason_assert(default_value);
	aason_assert(strlen(default_value) < buffer_size);

	if (ctx->reading)
	{
		if (!aason_read_object_fixed_str(ctx, key, aason_optional, value, buffer_size, truncate))
		{
			strcpy(value, default_value);
			return false;
		}
	}
	else
	{
		aason_write_object_str(ctx, key, value);
	}

	return true;
}

bool aason_archive_object_int(aason_context* ctx, const char* key, int64_t* value, int64_t default_value)
{
	if (ctx->reading)
	{
		if (!aason_read_object_int(ctx, key, aason_optional, value))
		{
			*value = default_value;
			return false;
		}
	}
	else
	{
		aason_write_object_int(ctx, key, *value);
	}

	return true;
}

bool aason_archive_object_bool(aason_context* ctx, const char* key, bool* value, bool default_value)
{
	if (ctx->reading)
	{
		if (!aason_read_object_bool(ctx, key, aason_optional, value))
		{
			*value = default_value;
			return false;
		}
	}
	else
	{
		aason_write_object_bool(ctx, key, *value);
	}

	return true;
}

bool aason_archive_object_float(aason_context* ctx, const char* key, float* value, float default_value)
{
	if (ctx->reading)
	{
		if (!aason_read_object_float(ctx, key, aason_optional, value))
		{
			*value = default_value;
			return false;
		}
	}
	else
	{
		aason_write_object_float(ctx, key, *value);
	}

	return true;
}

bool aason_archive_object_enum(aason_context* ctx, const char* key, int32_t* value, int32_t default_value, const char** strings, int32_t count)
{
	if (ctx->reading)
	{
		if (!aason_read_object_enum(ctx, key, aason_optional, value, strings, count))
		{
			*value = default_value;
			return false;
		}
	}
	else
	{
		aason_write_object_enum(ctx, key, *value, strings, count);
	}

	return true;
}