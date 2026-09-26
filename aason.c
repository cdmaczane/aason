#ifndef aason_assert
	#include <assert.h>
	#define aason_assert assert
#endif

#include <setjmp.h>

void aason_destroy(aason_context* ctx)
{
	if (!ctx->reading)
		aason_write_flush(ctx);

	rgs_free(ctx);
}

bool aason_reading(const aason_context* ctx)
{
	aason_assert(sdd);

	return ctx->reading;
}

bool aason_writing(const aason_context* ctx)
{
	aason_assert(sdd);

	return !ctx->reading;
}

#include "src/archive.c"
#include "src/finalise.c"
#include "src/read.c"
#include "src/tokenise.c"
#include "src/validate.c"
#include "src/write.c"