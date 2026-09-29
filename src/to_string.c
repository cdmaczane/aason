static char* aason_to_string_int(char* out, int64_t value)
{
#if defined(_WIN32)
	const int result = _snprintf_s(out, aason_int_max_len, _TRUNCATE, "%" PRId64, value);
#else
	const int result = snprintf(out, aason_int_max_len, "%" PRId64, value);
#endif
	aason_assert(result >= 0);
	aason_assert(result < aason_int_max_len);

	return out + result;
}

static char* aason_to_string_bool(char* out, bool value)
{
	if (value)
	{
		*out++ = 't';
		*out++ = 'r';
		*out++ = 'u';
		*out++ = 'e';
	}
	else
	{
		*out++ = 'f';
		*out++ = 'a';
		*out++ = 'l';
		*out++ = 's';
		*out++ = 'e';
	}

	return out;
}

static char* aason_to_string_hash(char* out, uint32_t value)
{
#if defined(_WIN32)
	const int result = _snprintf_s(out, aason_hash_len, _TRUNCATE, "%" PRIu32, value);
#else
	const int result = snprintf(out, aason_hash_len, "%" PRIu32, value);
#endif
	aason_assert(result >= 0);
	aason_assert(result < aason_hash_len);

	return out + result;
}

static char* aason_to_string_float(aason_locale locale, char* out, float value)
{
#if AASON_HANDLE_LOCALE && defined(_WIN32)
	const int result = _snprintf_s_l(out, aason_float_max_len, _TRUNCATE, "%.9g", locale, value);
#elif AASON_HANDLE_LOCALE
	locale_t previous = uselocale(locale);
	const int result = snprintf(out, aason_float_max_len, "%.9g", value);
	uselocale(previous);
#else
	const int result = snprintf(out, aason_float_max_len, "%.9g", value);
#endif
	aason_assert(result >= 0);
	aason_assert(result < aason_float_max_len);

	return out + result;
}