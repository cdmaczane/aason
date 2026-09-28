static char* aason_to_string_float(aason_locale locale, char* out, float value)
{
#if AASON_HANDLE_LOCALE && defined(_WIN32)
	const int result = _snprintf_s_l(out, aason_float_max_len, aason_float_max_len - 1, "%.9g", locale, value);
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