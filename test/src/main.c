#include "../../aason.h"
#include <stdio.h>
#include <stdlib.h>

#define WORKING_DIR "../aason/test/data/"

typedef struct
{
	const char* path;
	bool		expect_success;
} test_case;

static const test_case test_cases[] =
{
	{WORKING_DIR"parse/invalid/double_comma.aason", false},
	{WORKING_DIR"parse/invalid/empty_file.aason", false},
	{WORKING_DIR"parse/invalid/extra_closing_brace.aason", false},
	{WORKING_DIR"parse/invalid/extra_closing_bracket.aason", false},
	{WORKING_DIR"parse/invalid/mismatched_brackets.aason", false},
	{WORKING_DIR"parse/invalid/missing_colon.aason", false},
	{WORKING_DIR"parse/invalid/missing_key.aason", false},
	{WORKING_DIR"parse/invalid/missing_value.aason", false},
	{WORKING_DIR"parse/invalid/multiple_roots.aason", false},
	{WORKING_DIR"parse/invalid/unclosed_array.aason", false},
	{WORKING_DIR"parse/invalid/unclosed_object.aason", false},
	{WORKING_DIR"parse/invalid/unclosed_string.aason", false},
};
static const size_t test_count = sizeof(test_cases) / sizeof(test_case);

typedef struct
{
	aason_error_type error_type;
} error_state;

static void error_callback(void* user_data, aason_error_type error, const char* file, uint32_t line, uint32_t column, const char* msg)
{
	(void)file;
	(void)line;
	(void)column;
	(void)msg;

	error_state* state = user_data;
	state->error_type = error;
}

static aason_error_type test_parse(const char* path)
{
	error_state state = {
		.error_type = aason_error_none
	};

	const aason_error_interface error_interface = {
		.user_data = &state,
		.error = error_callback
	};

	const aason_read_desc desc = {
		.path = path,
		.error_interface = &error_interface
	};
	aason_context* ctx = aason_read(&desc);
	if (ctx)
		aason_destroy(ctx);

	return state.error_type;
}

static bool test_all(void)
{
	size_t passed = 0;
	for (size_t i = 0; i < test_count; ++i)
	{
		const test_case* test = &test_cases[i];

		const aason_error_type error_type = test_parse(test->path);
		const bool result = error_type == aason_error_none;

		if (error_type == aason_error_file_not_found)
		{
			printf("File not found: %s\n", test->path);
		}
		else if (result == test->expect_success)
		{
			++passed;
			printf("PASS: %s\n", test->path);
		}
		else
		{
			printf("FAIL: %s\n", test->path);
		}
	}

	printf("\n%zu/%zu tests passed\n", passed, test_count);

	return passed == test_count;
}

int main(void)
{
	if (test_all())
		return EXIT_SUCCESS;

	return EXIT_FAILURE;
}

#include "../../aason.c"