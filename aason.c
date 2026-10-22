#include "aason.h"

#include <stdio.h>
#include <float.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdlib.h>
#include <inttypes.h>

#ifndef aason_assert
	#define aason_assert assert
#endif

#ifndef AASON_HANDLE_LOCALE
	#define AASON_HANDLE_LOCALE 0
#endif

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
	#define aason_new_locale() NULL
	#define aason_free_locale(locale) do {} while (0)
	#define aason_strtof(str, end, locale) strtof(str, end)
	#define aason_strtod(str, end, locale) strtod(str, end)
#endif
static_assert(sizeof(aason_locale) == sizeof(void*), "");

#ifdef __cplusplus
extern "C" {
#endif

#include "src/internal.h"
#include "src/archive.c"
#include "src/preparse.c"
#include "src/tokenise.c"
#include "src/validate.c"
#include "src/finalise.c"
#include "src/default.c"
#include "src/parse.c"
#include "src/read.c"
#include "src/write.c"
#include "src/to_string.c"
#include "src/from_string.c"

static const char* aason_error_type_strings[] = {
	"none",
	"recursion",
	"allocation",
	"empty_file",
	"invalid_key",
	"invalid_char",
	"key_expected",
	"out_of_range",
	"file_not_found",
	"multiple_roots",
	"colon_expected",
	"unexpected_eof",
	"trailing_comma",
	"unexpected_char",
	"unexpected_token",
	"expected_element",
	"expected_identifier",
	"unknown_constructor",
	"mismatched_array_depth",
	"mismatched_object_depth",
	"wrong_type",
	"unused_field",
	"end_of_array",
	"invalid_enum",
	"key_not_found",
	"invalid_range",
	"array_too_large",
	"buffer_too_small",
	"invalid_constructor"
};
static_assert(sizeof(aason_error_type_strings) / sizeof(const char*) == aason_error_count, "");

const char* aason_get_error_string(aason_error_type error)
{
	return aason_error_type_strings[error];
}

static aason_file* aason_get_file(aason_context* ctx, uint32_t index)
{
	aason_assert(index < ctx->file_count);
	return &ctx->files[index];
}

static const char* aason_get_file_path(aason_context* ctx, uint32_t index)
{
	aason_assert(index < ctx->file_count);
	return ctx->files[index].buffer;
}

static char* aason_get_file_data(aason_context* ctx, uint32_t index)
{
	aason_assert(index < ctx->file_count);
	return ctx->files[index].buffer + ctx->files[index].offset;
}

static uint32_t aason_get_file_size(aason_context* ctx, uint32_t index)
{
	aason_assert(index < ctx->file_count);
	return ctx->files[index].size;
}

static aason_token* aason_get_next_token(aason_token_state* state)
{
	aason_token* token;
	for (;;)
	{
		aason_assert(state->token_index < state->tokens->count);

		token = &state->tokens->tokens[state->token_index++];
		if (token->type == aason_token_type_jump)
		{
			const uint32_t target_file_index = token->jump_index;
			aason_assert(target_file_index < state->ctx->file_count);

			// Recursion is disallowed so we can store return position in each file struct
			aason_file* target_file = aason_get_file(state->ctx, target_file_index);
			target_file->return_file = state->file_index;
			target_file->return_index = state->token_index;

			// Update current position to point to target file
			state->file_index = target_file_index;
			state->token_index = target_file->token_index;
		}
		else if (token->type == aason_token_type_eof)
		{
			const uint32_t current_file_index = state->file_index;
			aason_assert(current_file_index < state->ctx->file_count);

			if (current_file_index == 0)
				break; // End of root file

			const aason_file* current_file = aason_get_file(state->ctx, current_file_index);
			aason_assert(current_file->return_file < state->ctx->file_count);
			aason_assert(current_file->return_index < state->tokens->count);

			// Restore previous position (note: token index was incrementing before jump)
			state->file_index = (uint16_t)current_file->return_file;
			state->token_index = current_file->return_index;
		}
		else
		{
			break;
		}
	}

	return token;
}

static void aason_report_error(aason_context* ctx, aason_error_type error, const char* file, uint32_t line, uint32_t column, const char* format, va_list args)
{
	ctx->error = error;
	ctx->error_file = file;
	ctx->error_line = line;
	ctx->error_column = column;

	if (ctx->error_interface.error)
	{
		char buffer[4096];
		vsnprintf(buffer, sizeof(buffer), format, args);
	
		ctx->error_interface.error(
			ctx->error_interface.user_data,
			error,
			file,
			line,
			column,
			buffer
		);
	}
}

void aason_error(aason_context* ctx, aason_error_type error, const char* format, ...)
{
	aason_assert(ctx);
	aason_assert(ctx->reading);
	aason_assert(ctx->error_element < ctx->element_count);

	const aason_element* element = &ctx->elements[ctx->error_element];

	va_list args;
	va_start(args, format);
	aason_report_error(
		ctx,
		error,
		aason_get_file_path(ctx, element->value_file_index),
		element->line,
		element->column,
		format,
		args
	);
	va_end(args);
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