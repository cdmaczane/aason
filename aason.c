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

// TODO: Move into `#if AASON_HANDLE_LOCALE` scope once custom parsers are added
#if defined(_WIN32)
	#include <stdio.h>
#endif

#include "aason.h"

#if AASON_HANDLE_LOCALE
	#include <locale.h>

	#if defined(_WIN32)
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

#ifdef __cplusplus
extern "C" {
#endif

#include "src/archive.c"
#include "src/preparse.c"
#include "src/tokenise.c"
#include "src/validate.c"
#include "src/finalise.c"
#include "src/parse.c"
#include "src/read.c"
#include "src/write.c"
#include "src/to_string.c"
#include "src/from_string.c"

static aason_token* aason_get_next_token(aason_token_state* state)
{
	aason_token* token;
	for (;;)
	{
		aason_assert(state->token_index < state->tokens->count);

		token = &state->tokens->tokens[state->token_index++];
		if (token->type == aason_token_type_jump)
		{
			const uint16_t target_file_index = token->count;
			aason_assert(target_file_index < state->ctx->file_count);

			// Recursion is disallowed so we can store return position in each file struct
			aason_file* target_file = &state->ctx->files[target_file_index];
			target_file->return_file = state->file_index;
			target_file->return_index = state->token_index;

			// Update current position to point to target file
			state->file_index = target_file_index;
			state->token_index = target_file->token_index;
		}
		else if (token->type == aason_token_type_eof)
		{
			const uint16_t current_file_index = state->file_index;
			aason_assert(current_file_index < state->ctx->file_count);

			if (current_file_index == 0)
				break; // End of root file

			const aason_file* current_file = &state->ctx->files[current_file_index];
			aason_assert(current_file->return_file < state->ctx->file_count);
			aason_assert(current_file->return_index < state->tokens->count);

			// Restore previous position (note: token index was incrementing before jump)
			state->file_index = current_file->return_file;
			state->token_index = current_file->return_index;
		}
		else
		{
			break;
		}
	}

	return token;
}

void aason_destroy(aason_context* ctx)
{
	if (ctx)
	{
		aason_free_locale(ctx->locale);

		if (ctx->reading)
		{
			aason_free(&ctx->allocator, ctx->stack);
			for (uint32_t i = 0; i < ctx->file_count; ++i)
				aason_free(&ctx->allocator, ctx->files[i].buffer);
		}
		else
		{
			aason_write_flush(ctx);
			ctx->write_interface.close(ctx->write_interface.user_data, ctx->stream);
			aason_free(&ctx->allocator, ctx->buffer);
		}

		aason_free(&ctx->allocator, ctx);
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

#ifdef __cplusplus
}
#endif

#if !defined(__cplusplus) && __STDC_VERSION__ < 202311L
	#undef nullptr
#endif