static bool aason_from_string_bin(const char* begin, const char* end, int64_t* value)
{
	errno = 0;
	char* actual_end;
	*value = strtoll(begin, &actual_end, 2);
	aason_assert(actual_end == end);

	return errno != ERANGE;
}

static bool aason_from_string_dec(const char* begin, const char* end, int64_t* value)
{
	errno = 0;
	char* actual_end;
	*value = strtoll(begin, &actual_end, 10);
	aason_assert(actual_end == end);

	return errno != ERANGE;
}

static bool aason_from_string_hex(const char* begin, const char* end, int64_t* value)
{
	errno = 0;
	char* actual_end;
	*value = strtoll(begin, &actual_end, 16);
	aason_assert(actual_end == end);

	return errno != ERANGE;
}

static bool aason_from_string_hash(const char* begin, const char* end, uint32_t* value)
{
	int64_t result;
	if (aason_from_string_hex(begin, end, &result))
	{
		if (result >= 0 && result <= UINT32_MAX)
		{
			*value = (uint32_t)result;
			return true;
		}
	}

	return false;
}

static bool aason_from_string_float(aason_locale locale, const char* begin, const char* end, float* value)
{
	errno = 0;
	char* actual_end;
	*value = aason_strtof(begin, &actual_end, locale);

	if (errno == ERANGE)
		return false;

	aason_assert(actual_end == end);

	return true;
}