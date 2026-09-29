#ifndef aason_assert
	#include <assert.h>
	#define aason_assert assert
#endif

#ifndef AASON_HANDLE_LOCALE
	#define AASON_HANDLE_LOCALE 0
#endif

#include <float.h>
#include <errno.h>
#include <string.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdlib.h>
#include <inttypes.h>

#include "aason.h"

#if AASON_HANDLE_LOCALE
	#include <locale.h>

	#if defined(_WIN32)
		#include <stdio.h>

		typedef _locale_t aason_locale;
		#define aason_new_locale() _create_locale(LC_NUMERIC, "C")
		#define aason_free_locale(locale) _free_locale(locale)
		#define aason_strtof(str, end, locale) _strtof_l(str, end, locale)
		#define aason_strtod(str, end, locale) _strtod_l(str, end, locale)
	#else // POSIX
		typedef locale_t aason_locale;
		#define aason_new_locale() newlocale(LC_NUMERIC_MASK, "C", (locale_t)0)
		#define aason_free_locale(locale) freelocale(locale)
		#define aason_strtof(str, end, locale) strtof_l(str, end, locale)
		#define aason_strtod(str, end, locale) strtod_l(str, end, locale)
	#endif
#else
	typedef void* aason_locale;
	#define aason_new_locale() nullptr
	#define aason_free_locale(locale) do {} while (0)
	#define aason_strtof(str, end, locale) strtof(str, end)
	#define aason_strtod(str, end, locale) strtod(str, end)
#endif
static_assert(sizeof(aason_locale) == sizeof(void*));

#include "src/internal.h"

#if !defined(__cplusplus) || __STDC_VERSION__ < 202311L
	#define nullptr ((void*)0)
#endif

#include "src/archive.c"
#include "src/tokenise.c"
#include "src/validate.c"
#include "src/finalise.c"
#include "src/read.c"
#include "src/write.c"
#include "src/to_string.c"
#include "src/from_string.c"

static void aason_format_string(aason_context* ctx, char* out, size_t size, const char* format, va_list args)
{
	if (ctx->read_desc->format_string)
		ctx->read_desc->format_string(out, size, format, args);
}

void aason_destroy(aason_context* ctx)
{
	if (ctx)
	{
		aason_free_locale(ctx->locale);

		if (ctx->reading)
			ctx->read_desc->allocator(ctx->read_desc->allocator_data, ctx, 0, 0);
		else
			aason_write_flush(ctx);
	}
}

bool aason_reading(const aason_context* ctx)
{
	aason_assert(ctx);

	return ctx->reading;
}

bool aason_writing(const aason_context* ctx)
{
	aason_assert(ctx);

	return !ctx->reading;
}

#if !defined(__cplusplus) && __STDC_VERSION__ < 202311L
	#undef nullptr
#endif