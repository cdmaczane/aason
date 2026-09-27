#ifndef aason_assert
	#include <assert.h>
	#define aason_assert assert
#endif

#include <string.h>
#include <setjmp.h>
#include <stdarg.h>

#include "aason.h"
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

static void aason_format_string(aason_context* ctx, char* out, size_t size, const char* format, va_list args)
{
	if (ctx->read_desc->format_string)
		ctx->read_desc->format_string(out, size, format, args);
}

void aason_destroy(aason_context* ctx)
{
	if (ctx)
	{
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