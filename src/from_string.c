static bool aason_from_string_bin(const char* begin, const char* end, int64_t* value)
{
	errno = 0;
	char* actual_end;
	*value = strtoll(begin + 2, &actual_end, 2);

	return actual_end == end && actual_end != begin + 2 && errno != ERANGE;
}

static bool aason_from_string_dec(const char* begin, const char* end, int64_t* value)
{
	errno = 0;
	char* actual_end;
	*value = strtoll(begin, &actual_end, 10);

	return actual_end == end && actual_end != begin && errno != ERANGE;
}

static bool aason_from_string_hex(const char* begin, const char* end, int64_t* value)
{
	errno = 0;
	char* actual_end;
	*value = strtoll(begin + 2, &actual_end, 16);

	return actual_end == end && actual_end != begin + 2 && errno != ERANGE;
}

static bool aason_from_string_float(aason_locale locale, const char* begin, const char* end, float* value)
{
	(void)locale;

	errno = 0;
	char* actual_end;
	*value = aason_strtof(begin, &actual_end, locale);

	return actual_end == end && errno != ERANGE;
}