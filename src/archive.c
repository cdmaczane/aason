bool rgs_sdd_archive_array_enter(rgs_sdd* sdd, const char* key, int64_t* size)
{
	if (sdd->reading)
		return rgs_sdd_read_array_enter(sdd, key, rgs_sdd_optional, size, INT64_MAX);

	rgs_sdd_write_array_enter(sdd, key);
	return true;
}

void rgs_sdd_archive_array_leave(rgs_sdd* sdd)
{
	if (sdd->reading)
		rgs_sdd_read_array_leave(sdd);
	else
		rgs_sdd_write_array_leave(sdd);
}

bool rgs_sdd_archive_array_object_enter(rgs_sdd* sdd)
{
	if (sdd->reading)
		return rgs_sdd_read_array_enter_object(sdd);

	rgs_sdd_write_array_object_enter(sdd);
	return true;
}

void rgs_sdd_archive_array_object_leave(rgs_sdd* sdd)
{
	if (sdd->reading)
		rgs_sdd_read_array_leave_object(sdd);
	else
		rgs_sdd_archive_array_object_leave(sdd);
}

void rgs_sdd_archive_array_str(rgs_sdd* sdd, char** value)
{
	if (sdd->reading)
		rgs_sdd_read_array_str(sdd, (const char**)value, nullptr);
	else
		rgs_sdd_write_array_str(sdd, *value);
}

void rgs_sdd_archive_array_fixed_str(rgs_sdd* sdd, char* value, int64_t buffer_size, bool truncate)
{
	if (sdd->reading)
		rgs_sdd_read_array_fixed_str(sdd, value, buffer_size, truncate);
	else
		rgs_sdd_write_array_str(sdd, value);
}

void rgs_sdd_archive_array_int(rgs_sdd* sdd, int64_t* value)
{
	if (sdd->reading)
		rgs_sdd_read_array_int(sdd, value);
	else
		rgs_sdd_write_array_int(sdd, *value);
}

void rgs_sdd_archive_array_bool(rgs_sdd* sdd, bool* value)
{
	if (sdd->reading)
		rgs_sdd_read_array_bool(sdd, value);
	else
		rgs_sdd_write_array_bool(sdd, *value);
}

void rgs_sdd_archive_array_hash(rgs_sdd* sdd, uint32_t* value)
{
	if (sdd->reading)
		rgs_sdd_read_array_hash(sdd, value);
	else
		rgs_sdd_write_array_hash(sdd, *value);
}

void rgs_sdd_archive_array_float(rgs_sdd* sdd, float* value)
{
	if (sdd->reading)
		rgs_sdd_read_array_float(sdd, value);
	else
		rgs_sdd_write_array_float(sdd, *value);
}

void rgs_sdd_archive_array_enum(rgs_sdd* sdd, int32_t* value, const char** strings, int32_t count)
{
	if (sdd->reading)
		rgs_sdd_read_array_enum(sdd, value, strings, count);
	else
		rgs_sdd_write_array_enum(sdd, *value, strings, count);
}

bool rgs_sdd_archive_object_enter(rgs_sdd* sdd, const char* key)
{
	if (sdd->reading)
		return rgs_sdd_read_object_enter(sdd, key, rgs_sdd_optional);

	rgs_sdd_write_object_enter(sdd, key);
	return true;
}

void rgs_sdd_archive_object_leave(rgs_sdd* sdd)
{
	if (sdd->reading)
		rgs_sdd_read_object_leave(sdd);
	else
		rgs_sdd_write_object_leave(sdd);
}

bool rgs_sdd_archive_object_str(rgs_sdd* sdd, const char* key, char** value, const char* default_value)
{
	rgs_assert(default_value);

	if (sdd->reading)
	{
		if (!rgs_sdd_read_object_str(sdd, key, rgs_sdd_optional, (const char**)value, nullptr))
		{
			*value = (char*)default_value;
			return false;
		}
	}
	else
	{
		rgs_sdd_write_object_str(sdd, key, *value);
	}

	return true;
}

bool rgs_sdd_archive_object_fixed_str(rgs_sdd* sdd, const char* key, char* value, const char* default_value, int64_t buffer_size, bool truncate)
{
	rgs_assert(default_value);
	rgs_assert(strlen(default_value) < buffer_size);

	if (sdd->reading)
	{
		if (!rgs_sdd_read_object_fixed_str(sdd, key, rgs_sdd_optional, value, buffer_size, truncate))
		{
			strcpy(value, default_value);
			return false;
		}
	}
	else
	{
		rgs_sdd_write_object_str(sdd, key, value);
	}

	return true;
}

bool rgs_sdd_archive_object_int(rgs_sdd* sdd, const char* key, int64_t* value, int64_t default_value)
{
	if (sdd->reading)
	{
		if (!rgs_sdd_read_object_int(sdd, key, rgs_sdd_optional, value))
		{
			*value = default_value;
			return false;
		}
	}
	else
	{
		rgs_sdd_write_object_int(sdd, key, *value);
	}

	return true;
}

bool rgs_sdd_archive_object_bool(rgs_sdd* sdd, const char* key, bool* value, bool default_value)
{
	if (sdd->reading)
	{
		if (!rgs_sdd_read_object_bool(sdd, key, rgs_sdd_optional, value))
		{
			*value = default_value;
			return false;
		}
	}
	else
	{
		rgs_sdd_write_object_bool(sdd, key, *value);
	}

	return true;
}

bool rgs_sdd_archive_object_hash(rgs_sdd* sdd, const char* key, uint32_t* value, uint32_t default_value)
{
	if (sdd->reading)
	{
		if (!rgs_sdd_read_object_hash(sdd, key, rgs_sdd_optional, value))
		{
			*value = default_value;
			return false;
		}
	}
	else
	{
		rgs_sdd_write_object_hash(sdd, key, *value);
	}

	return true;
}

bool rgs_sdd_archive_object_float(rgs_sdd* sdd, const char* key, float* value, float default_value)
{
	if (sdd->reading)
	{
		if (!rgs_sdd_read_object_float(sdd, key, rgs_sdd_optional, value))
		{
			*value = default_value;
			return false;
		}
	}
	else
	{
		rgs_sdd_write_object_float(sdd, key, *value);
	}

	return true;
}

bool rgs_sdd_archive_object_enum(rgs_sdd* sdd, const char* key, int32_t* value, int32_t default_value, const char** strings, int32_t count)
{
	if (sdd->reading)
	{
		if (!rgs_sdd_read_object_enum(sdd, key, rgs_sdd_optional, value, strings, count))
		{
			*value = default_value;
			return false;
		}
	}
	else
	{
		rgs_sdd_write_object_enum(sdd, key, *value, strings, count);
	}

	return true;
}