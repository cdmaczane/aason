static char* aason_to_string_dec(char* out, int64_t value, int min_digits)
{
	aason_assert(min_digits >= 0);
	aason_assert(min_digits <= 19);

	int digits[19];
	int digit_count = 0;

	const bool negative = value < 0;
	uint64_t magnitude = negative ? UINT64_C(0) - (uint64_t)value : (uint64_t)value;

	do
	{
		digits[digit_count++] = magnitude % 10;
		magnitude /= 10;
	} while (magnitude != 0);

	if (negative)
		*out++ = '-';

	for (int i = min_digits; i > digit_count; --i)
		*out++ = '0';

	for (int i = digit_count - 1; i >= 0; --i)
		*out++ = '0' + (char)digits[i];

	return out;
}

static char* aason_to_string_hex(char* out, int64_t value, int min_digits)
{
	aason_assert(min_digits >= 0);
	aason_assert(min_digits <= 16);

	int digits[16];
	int digit_count = 0;

	uint64_t magnitude = (uint64_t)value;

	do
	{
		digits[digit_count++] = magnitude % 16;
		magnitude /= 16;
	} while (magnitude != 0);

	*out++ = '0';
	*out++ = 'x';

	for (int i = min_digits; i > digit_count; --i)
		*out++ = '0';

	for (int i = digit_count - 1; i >= 0; --i)
	{
		const int digit = digits[i];
		if (digit < 10)
			*out++ = '0' + (char)digit;
		else
			*out++ = 'A' + (char)(digit - 10);
	}

	return out;
}

static char* aason_to_string_bin(char* out, int64_t value, int min_digits)
{
	aason_assert(min_digits >= 0);
	aason_assert(min_digits <= 64);

	const uint64_t magnitude = (uint64_t)value;
	uint64_t temp = magnitude;
	int bit_count = 0;

	// TODO: We could use intrinsics when supported
	do
	{
		++bit_count;
		temp >>= 1;
	} while (temp);

	*out++ = '0';
	*out++ = 'b';

	for (int i = min_digits; i > bit_count; --i)
		*out++ = '0';

	while (bit_count-- > 0)
	{
		if (magnitude & (UINT64_C(1) << bit_count))
			*out++ = '1';
		else
			*out++ = '0';
	}

	return out;
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

static char* aason_to_string_float(aason_locale locale, char* out, float value)
{
#if AASON_HANDLE_LOCALE && defined(_WIN32)
	const int result = _snprintf_s_l(out, aason_float_max_len, _TRUNCATE, "%.9g", locale, value);
#elif AASON_HANDLE_LOCALE
	locale_t previous = uselocale(locale);
	const int result = snprintf(out, aason_float_max_len, "%.9g", value);
	uselocale(previous);
#else
	(void)locale;
	const int result = snprintf(out, aason_float_max_len, "%.9g", value);
#endif
	aason_assert(result >= 0);
	aason_assert(result < aason_float_max_len);

	const char* original = out;
	out += result;

	if (!strchr(original, '.'))
	{
		*out++ = '.';
		*out++ = '0';
	}

	return out;
}