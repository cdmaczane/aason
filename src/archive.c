bool aason_archive_array_enter_object(aason_context* ctx)
{
	if (ctx->reading)
		return aason_read_array_enter_object(ctx);

	aason_write_array_enter_object(ctx);
	return true;
}

void aason_archive_array_str(aason_context* ctx, char** value)
{
	if (ctx->reading)
		aason_read_array_str(ctx, (const char**)value, NULL);
	else
		aason_write_array_str(ctx, *value);
}

void aason_archive_array_fixed_str(aason_context* ctx, char* value, size_t buffer_size, bool truncate)
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

bool aason_archive_object_enter_array(aason_context* ctx, const char* key, size_t* size)
{
	if (ctx->reading)
		return aason_read_object_enter_array(ctx, key, aason_optional, size, UINT32_MAX);

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

// TODO: This function probably needs to copy the string on reading
bool aason_archive_object_str(aason_context* ctx, const char* key, char** value, const char* default_value)
{
	aason_assert(default_value);

	if (ctx->reading)
	{
		if (!aason_read_object_str(ctx, key, aason_optional, (const char**)value, NULL))
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

static void aason_copy_string(char* restrict dst, const char* restrict src, size_t dst_size)
{
	aason_assert(dst);
	aason_assert(src);
	aason_assert(dst != src);
	aason_assert(dst_size > 0);

	for (;;)
	{
		if (!*src || !(--dst_size))
			break;
		*dst++ = *src++;
	}
	*dst = 0;
}

bool aason_archive_object_fixed_str(aason_context* ctx, const char* key, char* value, const char* default_value, size_t buffer_size, bool truncate)
{
	aason_assert(default_value);
	aason_assert(value);
	aason_assert(buffer_size);
	aason_assert(truncate || strlen(default_value) < buffer_size);

	if (ctx->reading)
	{
		if (!aason_read_object_fixed_str(ctx, key, aason_optional, value, buffer_size, truncate))
		{
			aason_copy_string(value, default_value, buffer_size);
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