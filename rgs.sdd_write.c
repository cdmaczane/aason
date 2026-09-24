static char* sdd_write_new_line(rgs_sdd* sdd, char* out)
{
	if (sdd->first)
		sdd->first = false;
	else
		*out++ = ',';

	if (sdd->first_line)
	{
		rgs_assert(sdd->stack_depth == 0);

		sdd->first_line = false;
	}
	else
	{
		*out++ = '\n';
		for (uint32_t i = 0; i < sdd->stack_depth; ++i)
			*out++ = '\t';
	}

	return out;
}

static void sdd_write_flush(rgs_sdd* sdd)
{
	if (sdd->offset)
	{
		sdd->write_str(sdd->user_data, sdd->buffer, sdd->offset);
		sdd->offset = 0;
	}
}

static void sdd_write_str(rgs_sdd* sdd, const char* str, int64_t size)
{
	sdd_write_flush(sdd);
	sdd->write_str(sdd->user_data, str, size);
}

static char* sdd_write_alloc(rgs_sdd* sdd, int64_t size)
{
	rgs_assert(size <= RGS_SDD_WRITE_BUFFER_SIZE);

	if (sdd->offset + size > RGS_SDD_WRITE_BUFFER_SIZE)
		sdd_write_flush(sdd);

	char* out = sdd->buffer + sdd->offset;
	sdd->offset += size;

	return out;
}

static char* sdd_write_add_array_element(rgs_sdd* sdd, int64_t value_len, int64_t extra_len)
{
	rgs_assert(sdd);
	rgs_assert(value_len >= 0);
	rgs_assert(extra_len >= 0);

	const uint32_t stack_depth = sdd->stack_depth;
	const int64_t len = value_len + stack_depth + extra_len + !sdd->first + 1;

	return sdd_write_new_line(sdd, sdd_write_alloc(sdd, len));
}

static char* sdd_write_add_object_element(rgs_sdd* sdd, const char* key, int64_t value_len, int64_t extra_len)
{
	rgs_assert(sdd);
	rgs_assert(key);
	rgs_assert(*key);
	rgs_assert(value_len >= 0);
	rgs_assert(extra_len >= 0);

	const bool first = sdd->first;
	const bool first_line = sdd->first_line;
	const uint32_t stack_depth = sdd->stack_depth;
	const int64_t key_len = strlen(key);
	rgs_assert(key_len <= RGS_SDD_KEY_MAX_LEN);

	const int64_t len = key_len + value_len + stack_depth + extra_len + !first + !first_line + 2;
	char* out = sdd_write_new_line(sdd, sdd_write_alloc(sdd, len));
	memcpy(out, key, key_len);
	out += key_len;
	*out++ = ':';
	*out++ = ' ';

	return out;
}

rgs_sdd* rgs_sdd_write(rgs_sdd_write_callback callback, void* user_data)
{
	rgs_assert(callback);

	rgs_sdd* sdd = rgs_alloc(RGS_PAGE_SIZE, RGS_PAGE_SIZE);
	sdd->buffer = (char*)(sdd + 1);
	sdd->error = rgs_sdd_error_none;
	sdd->reading = false;
	sdd->stack_depth = 0;
	sdd->user_data = user_data;
	sdd->first = true;
	sdd->first_line = true;
	sdd->offset = 0;
	sdd->write_str = callback;

	return sdd;
}

// TODO: Move
void rgs_sdd_destroy(rgs_sdd* sdd)
{
	if (!sdd->reading)
		sdd_write_flush(sdd);

	rgs_free(sdd);
}

void rgs_sdd_write_array_enter(rgs_sdd* sdd, const char* key)
{
	char* out = sdd_write_add_object_element(sdd, key, 0, 1);
	*out = '[';

	++sdd->stack_depth;
	sdd->first = true;
}

void rgs_sdd_write_array_leave(rgs_sdd* sdd)
{
	rgs_assert(sdd);
	rgs_assert(sdd->stack_depth > 0);

	const int64_t stack_depth = --sdd->stack_depth;

	if (sdd->first)
	{
		*sdd_write_alloc(sdd, 1) = ']';
	}
	else
	{
		char* out = sdd_write_alloc(sdd, stack_depth + 2);
		*out++ = '\n';
		for (uint32_t i = 0; i < sdd->stack_depth; ++i)
			*out++ = '\t';
		*out++ = ']';
	}

	sdd->first = false;
}

void rgs_sdd_write_array_object_enter(rgs_sdd* sdd)
{
	*sdd_write_add_array_element(sdd, 0, 1) = '{';
	++sdd->stack_depth;
	sdd->first = true;
}

void rgs_sdd_write_array_object_leave(rgs_sdd* sdd)
{
	rgs_sdd_write_object_leave(sdd);
}

void rgs_sdd_write_array_str(rgs_sdd* sdd, const char* value)
{
	rgs_assert(value);
	rgs_assert(*value);

	const int64_t len = strlen(value);
	*sdd_write_add_array_element(sdd, 1, 0) = '"';
	sdd_write_str(sdd, value, len);
	*sdd_write_alloc(sdd, 1) = '"';
}

void rgs_sdd_write_array_int(rgs_sdd* sdd, int64_t value)
{
	char* out = sdd_write_add_array_element(sdd, rgs_strlen_int(value, rgs_int_base_dec, 0), 0);
	rgs_to_string_unsafe_int(out, value, rgs_int_base_dec, 0);
}

void rgs_sdd_write_array_bool(rgs_sdd* sdd, bool value)
{
	char* out = sdd_write_add_array_element(sdd, rgs_strlen_bool(value), 0);
	rgs_to_string_unsafe_bool(out, value);
}

void rgs_sdd_write_array_hash(rgs_sdd* sdd, uint32_t value)
{
	char* out = sdd_write_add_array_element(sdd, 8, 1);
	rgs_to_string_unsafe_uint(out, value, rgs_int_base_hex, 8);
}

void rgs_sdd_write_array_float(rgs_sdd* sdd, float value)
{
	char* out = sdd_write_add_array_element(sdd, rgs_strlen_float(value, RGS_FLOAT_DECIMAL_PLACES_MAX), 0);
	rgs_to_string_unsafe_float(out, value, RGS_FLOAT_DECIMAL_PLACES_MAX);
}

void rgs_sdd_write_array_enum(rgs_sdd* sdd, int32_t value, const char** strings, int32_t count)
{
	rgs_assert(strings);
	rgs_assert(value >= 0);
	rgs_assert(count > 0);
	rgs_assert(value < count);

	const int64_t len = strlen(strings[value]);
	rgs_assert(len <= RGS_SDD_ENUM_MAX_LEN);

	char* out = sdd_write_add_array_element(sdd, len, 0);
	memcpy(out, strings[value], len);
}

void rgs_sdd_write_object_enter(rgs_sdd* sdd, const char* key)
{
	*sdd_write_add_object_element(sdd, key, 0, 1) = '{';
	++sdd->stack_depth;
	sdd->first = true;
}

void rgs_sdd_write_object_leave(rgs_sdd* sdd)
{
	rgs_assert(sdd);
	rgs_assert(sdd->stack_depth > 0);

	const int64_t stack_depth = --sdd->stack_depth;

	if (sdd->first)
	{
		*sdd_write_alloc(sdd, 1) = '}';
	}
	else
	{
		const int64_t len = stack_depth + 2;
		char* out = sdd_write_alloc(sdd, len);

		*out++ = '\n';
		for (uint32_t i = 0; i < sdd->stack_depth; ++i)
			*out++ = '\t';
		*out++ = '}';
	}

	sdd->first = false;
}

void rgs_sdd_write_object_str(rgs_sdd* sdd, const char* key, const char* value)
{
	rgs_assert(value);
	rgs_assert(*value);

	const int64_t len = strlen(value);
	char* out = sdd_write_add_object_element(sdd, key, len, 2); // ""
	*out++ = '"';
	memcpy(out, value, len);
	out += len;
	*out = '"';
}

void rgs_sdd_write_object_int(rgs_sdd* sdd, const char* key, int64_t value)
{
	char* out = sdd_write_add_object_element(sdd, key, rgs_strlen_int(value, rgs_int_base_dec, 0), 0);
	rgs_to_string_unsafe_int(out, value, rgs_int_base_dec, 0);
}

void rgs_sdd_write_object_bool(rgs_sdd* sdd, const char* key, bool value)
{
	char* out = sdd_write_add_object_element(sdd, key, rgs_strlen_bool(value), 0);
	rgs_to_string_unsafe_bool(out, value);
}

void rgs_sdd_write_object_hash(rgs_sdd* sdd, const char* key, uint32_t value)
{
	char* out = sdd_write_add_object_element(sdd, key, 8, 1);
	*out++ = '#';
	rgs_to_string_unsafe_uint(out, value, rgs_int_base_hex, 8);
}

void rgs_sdd_write_object_float(rgs_sdd* sdd, const char* key, float value)
{
	char* out = sdd_write_add_object_element(sdd, key, rgs_strlen_float(value, RGS_FLOAT_DECIMAL_PLACES_MAX), 0);
	rgs_to_string_unsafe_float(out, value, RGS_FLOAT_DECIMAL_PLACES_MAX);
}

void rgs_sdd_write_object_enum(rgs_sdd* sdd, const char* key, int32_t value, const char** strings, int32_t count)
{
	rgs_assert(strings);
	rgs_assert(value >= 0);
	rgs_assert(count > 0);
	rgs_assert(value < count);

	const int64_t len = strlen(strings[value]);
	char* out = sdd_write_add_object_element(sdd, key, len, 0);
	memcpy(out, strings[value], len);
}