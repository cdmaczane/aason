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
	{WORKING_DIR"parse/basic/valid/array_numbers.aason", true},
	{WORKING_DIR"parse/basic/valid/array_strings.aason", true},
	{WORKING_DIR"parse/basic/valid/comments.aason", true},
	{WORKING_DIR"parse/basic/valid/empty_array.aason", true},
	{WORKING_DIR"parse/basic/valid/empty_object.aason", true},
	{WORKING_DIR"parse/basic/valid/mixed_containers.aason", true},
	{WORKING_DIR"parse/basic/valid/nested_arrays.aason", true},
	{WORKING_DIR"parse/basic/valid/nested_objects.aason", true},
	{WORKING_DIR"parse/basic/valid/object_primitives.aason", true},
	{WORKING_DIR"parse/basic/valid/whitespace.aason", true},

	{WORKING_DIR"parse/basic/invalid/double_comma.aason", false},
	{WORKING_DIR"parse/basic/invalid/empty_file.aason", false},
	{WORKING_DIR"parse/basic/invalid/extra_closing_brace.aason", false},
	{WORKING_DIR"parse/basic/invalid/extra_closing_bracket.aason", false},
	{WORKING_DIR"parse/basic/invalid/mismatched_brackets.aason", false},
	{WORKING_DIR"parse/basic/invalid/missing_colon.aason", false},
	{WORKING_DIR"parse/basic/invalid/missing_key.aason", false},
	{WORKING_DIR"parse/basic/invalid/missing_value.aason", false},
	{WORKING_DIR"parse/basic/invalid/multiple_roots.aason", false},
	{WORKING_DIR"parse/basic/invalid/unclosed_array.aason", false},
	{WORKING_DIR"parse/basic/invalid/unclosed_object.aason", false},
	{WORKING_DIR"parse/basic/invalid/unclosed_string.aason", false},

	{WORKING_DIR"parse/numbers/valid/array_numbers.aason", true},
	{WORKING_DIR"parse/numbers/valid/decimal_negative.aason", true},
	{WORKING_DIR"parse/numbers/valid/decimal_positive.aason", true},
	{WORKING_DIR"parse/numbers/valid/decimal_zero.aason", true},
	{WORKING_DIR"parse/numbers/valid/float_fraction.aason", true},
	{WORKING_DIR"parse/numbers/valid/float_negative.aason", true},
	{WORKING_DIR"parse/numbers/valid/binary_leading_zeroes.aason", true},
	{WORKING_DIR"parse/numbers/valid/binary_max_int64.aason", true},
	{WORKING_DIR"parse/numbers/valid/binary_valid.aason", true},
	{WORKING_DIR"parse/numbers/valid/binary_zero.aason", true},
	{WORKING_DIR"parse/numbers/valid/hex_leading_zeroes.aason", true},
	{WORKING_DIR"parse/numbers/valid/hex_lowercase.aason", true},
	{WORKING_DIR"parse/numbers/valid/hex_max_int64.aason", true},
	{WORKING_DIR"parse/numbers/valid/hex_mixed_case.aason", true},
	{WORKING_DIR"parse/numbers/valid/hex_uppercase.aason", true},
	{WORKING_DIR"parse/numbers/valid/hex_zero.aason", true},
	{WORKING_DIR"parse/numbers/valid/decimal_int64_max.aason", true},
	{WORKING_DIR"parse/numbers/valid/decimal_int64_max_minus_one.aason", true},
	{WORKING_DIR"parse/numbers/valid/decimal_int64_min.aason", true},
	{WORKING_DIR"parse/numbers/valid/decimal_int64_min_plus_one.aason", true},
	{WORKING_DIR"parse/numbers/valid/decimal_large_leading_zeros.aason", true},
	{WORKING_DIR"parse/numbers/valid/decimal_many_leading_zeros.aason", true},
	{WORKING_DIR"parse/numbers/valid/decimal_negative_zero.aason", true},
	{WORKING_DIR"parse/numbers/valid/float_many_fraction_digits.aason", true},
	{WORKING_DIR"parse/numbers/valid/float_negative_fraction.aason", true},
	{WORKING_DIR"parse/numbers/valid/float_small_fraction.aason", true},

	{WORKING_DIR"parse/numbers/invalid/double_decimal_point.aason", false},
	{WORKING_DIR"parse/numbers/invalid/leading_minus_only.aason", false},
	{WORKING_DIR"parse/numbers/invalid/leading_plus_only.aason", false},
	{WORKING_DIR"parse/numbers/invalid/missing_array_comma.aason", false},
	{WORKING_DIR"parse/numbers/invalid/number_with_letters.aason", false},
	{WORKING_DIR"parse/numbers/invalid/two_signs.aason", false},
	{WORKING_DIR"parse/numbers/invalid/binary_invalid_digit.aason", false},
	{WORKING_DIR"parse/numbers/invalid/binary_missing_digits.aason", false},
	{WORKING_DIR"parse/numbers/invalid/binary_negative.aason", false},
	{WORKING_DIR"parse/numbers/invalid/binary_overflow.aason", false},
	{WORKING_DIR"parse/numbers/invalid/hex_invalid_digit.aason", false},
	{WORKING_DIR"parse/numbers/invalid/hex_missing_digits.aason", false},
	{WORKING_DIR"parse/numbers/invalid/hex_negative.aason", false},
	{WORKING_DIR"parse/numbers/invalid/hex_overflow.aason", false},
	{WORKING_DIR"parse/numbers/invalid/decimal_huge_negative.aason", false},
	{WORKING_DIR"parse/numbers/invalid/decimal_huge_positive.aason", false},
	{WORKING_DIR"parse/numbers/invalid/decimal_negative_overflow.aason", false},
	{WORKING_DIR"parse/numbers/invalid/decimal_overflow_with_leading_zeros.aason", false},
	{WORKING_DIR"parse/numbers/invalid/decimal_positive_overflow.aason", false},
	{WORKING_DIR"parse/numbers/invalid/exponent_fraction.aason", false},
	{WORKING_DIR"parse/numbers/invalid/exponent_negative.aason", false},
	{WORKING_DIR"parse/numbers/invalid/exponent_positive.aason", false},
	{WORKING_DIR"parse/numbers/invalid/float_missing_fraction_digits.aason", false},
	{WORKING_DIR"parse/numbers/invalid/float_two_decimal_points.aason", false},

	{WORKING_DIR"parse/strings/valid/empty_string.aason", true},
	{WORKING_DIR"parse/strings/valid/escaped_backslash.aason", true},
	{WORKING_DIR"parse/strings/valid/escaped_carriage_return.aason", false}, // TODO
	{WORKING_DIR"parse/strings/valid/escaped_newline.aason", true},
	{WORKING_DIR"parse/strings/valid/escaped_quote.aason", true},
	{WORKING_DIR"parse/strings/valid/escaped_tab.aason", true},
	{WORKING_DIR"parse/strings/valid/multiple_strings.aason", true},
	{WORKING_DIR"parse/strings/valid/punctuation.aason", true},
	{WORKING_DIR"parse/strings/valid/simple_ascii.aason", true},
	{WORKING_DIR"parse/strings/valid/spaces_in_string.aason", true},
	{WORKING_DIR"parse/strings/valid/unicode_four_byte.aason", true},
	{WORKING_DIR"parse/strings/valid/unicode_three_byte.aason", true},
	{WORKING_DIR"parse/strings/valid/unicode_two_byte.aason", true},

	{WORKING_DIR"parse/strings/invalid/invalid_utf8_continuation.aason", false},
	{WORKING_DIR"parse/strings/invalid/overlong_utf8.aason", false},
	{WORKING_DIR"parse/strings/invalid/raw_newline_in_string.aason", false},
	{WORKING_DIR"parse/strings/invalid/trailing_backslash.aason", false},
	{WORKING_DIR"parse/strings/invalid/truncated_utf8.aason", false},
	{WORKING_DIR"parse/strings/invalid/unclosed_string.aason", false},
	{WORKING_DIR"parse/strings/invalid/unicode_escape.aason", false},
	{WORKING_DIR"parse/strings/invalid/unknown_escape.aason", false},
	{WORKING_DIR"parse/strings/invalid/utf8_above_unicode_max.aason", false},
	{WORKING_DIR"parse/strings/invalid/utf8_surrogate.aason", false},

	{WORKING_DIR"parse/comments/valid/blank_lines_and_tabs.aason", true},
	{WORKING_DIR"parse/comments/valid/block_comment_after_root.aason", true},
	{WORKING_DIR"parse/comments/valid/block_comment_before_root.aason", true},
	{WORKING_DIR"parse/comments/valid/block_comment_between_fields.aason", true},
	{WORKING_DIR"parse/comments/valid/block_comment_between_tokens.aason", true},
	{WORKING_DIR"parse/comments/valid/block_comment_multiline.aason", true},
	{WORKING_DIR"parse/comments/valid/comment_markers_in_string.aason", true},
	{WORKING_DIR"parse/comments/valid/crlf_line_endings.aason", true},
	{WORKING_DIR"parse/comments/valid/empty_block_comment.aason", true},
	{WORKING_DIR"parse/comments/valid/line_comment_after_root.aason", true},
	{WORKING_DIR"parse/comments/valid/line_comment_after_value.aason", true},
	{WORKING_DIR"parse/comments/valid/line_comment_at_eof.aason", true},
	{WORKING_DIR"parse/comments/valid/line_comment_before_root.aason", true},

	{WORKING_DIR"parse/comments/invalid/block_comment_hides_closing_brace.aason", false},
	{WORKING_DIR"parse/comments/invalid/carriage_return_not_followed_by_lf.aason", false},
	{WORKING_DIR"parse/comments/invalid/comment_in_identifier.aason", false},
	{WORKING_DIR"parse/comments/invalid/comment_in_number.aason", false},
	{WORKING_DIR"parse/comments/invalid/line_comment_hides_closing_brace.aason", false},
	{WORKING_DIR"parse/comments/invalid/lone_carriage_return.aason", false},
	{WORKING_DIR"parse/comments/invalid/unterminated_block_comment.aason", false},
	{WORKING_DIR"parse/comments/invalid/unterminated_nested_block_comment.aason", false},
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
			printf("File not found [%zu]: %s\n", i ,test->path);
		}
		else if (error_type == aason_error_allocation)
		{
			printf("Allocation failure [%zu]: %s\n", i, test->path);
		}
		else if (result == test->expect_success)
		{
			++passed;
			printf("PASS: %s\n", test->path);
		}
		else
		{
			printf("FAIL [%zu]:%s\n", i, test->path);
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