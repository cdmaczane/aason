#include "../../aason.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "../../aason.c"

#define WORKING_DIR		""
#define COLOUR_RED		"\x1b[31m"
#define COLOUR_GREEN	"\x1b[32m"
#define COLOUR_RESET	"\x1b[0m"

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
	{WORKING_DIR"parse/comments/valid/nested_block_comments.aason", true},
	{WORKING_DIR"parse/comments/valid/adjacent_block_comments.aason", true},
	{WORKING_DIR"parse/comments/valid/block_comment_marker_inside_line_comment.aason", true},
	{WORKING_DIR"parse/comments/valid/block_comment_with_asterisks.aason", true},
	{WORKING_DIR"parse/comments/valid/deeply_nested_block_comments.aason", true},
	{WORKING_DIR"parse/comments/valid/line_comment_inside_block_comment.aason", true},

	{WORKING_DIR"parse/comments/invalid/block_comment_hides_closing_brace.aason", false},
	{WORKING_DIR"parse/comments/invalid/carriage_return_not_followed_by_lf.aason", false},
	{WORKING_DIR"parse/comments/invalid/comment_in_identifier.aason", false},
	{WORKING_DIR"parse/comments/invalid/comment_in_number.aason", false},
	{WORKING_DIR"parse/comments/invalid/line_comment_hides_closing_brace.aason", false},
	{WORKING_DIR"parse/comments/invalid/lone_carriage_return.aason", false},
	{WORKING_DIR"parse/comments/invalid/unterminated_block_comment.aason", false},
	{WORKING_DIR"parse/comments/invalid/unterminated_nested_block_comment.aason", false},
	{WORKING_DIR"parse/comments/invalid/unclosed_outer_block_comment.aason", false},

	{WORKING_DIR"parse/includes/valid/directive_text_in_string.aason", true},
	{WORKING_DIR"parse/includes/valid/include_array_value.aason", true},
	{WORKING_DIR"parse/includes/valid/include_chain.aason", true},
	{WORKING_DIR"parse/includes/valid/include_multiple.aason", true},
	{WORKING_DIR"parse/includes/valid/include_nested_array.aason", true},
	{WORKING_DIR"parse/includes/valid/include_object_value.aason", true},
	{WORKING_DIR"parse/includes/valid/include_root.aason", true},
	{WORKING_DIR"parse/includes/valid/include_same_file_twice.aason", true},
	{WORKING_DIR"parse/includes/valid/include_with_comments.aason", true},

	{WORKING_DIR"parse/includes/invalid/direct_recursion.aason", false},
	{WORKING_DIR"parse/includes/invalid/include_empty_file.aason", false},
	{WORKING_DIR"parse/includes/invalid/include_inside_identifier.aason", false},
	{WORKING_DIR"parse/includes/invalid/include_inside_number.aason", false},
	{WORKING_DIR"parse/includes/invalid/include_mismatched_container.aason", false},
	{WORKING_DIR"parse/includes/invalid/include_missing_path.aason", false},
	{WORKING_DIR"parse/includes/invalid/include_two_values.aason", false},
	{WORKING_DIR"parse/includes/invalid/include_unclosed_paren.aason", false},
	{WORKING_DIR"parse/includes/invalid/include_unknown_directive.aason", false},
	{WORKING_DIR"parse/includes/invalid/indirect_recursion.aason", false},
	{WORKING_DIR"parse/includes/invalid/missing_file.aason", false},

	{WORKING_DIR"parse/enums/valid/array_identifiers.aason", true},
	{WORKING_DIR"parse/enums/valid/identifier_with_digits.aason", true},
	{WORKING_DIR"parse/enums/valid/identifier_with_underscore.aason", true},
	{WORKING_DIR"parse/enums/valid/nested_identifiers.aason", true},
	{WORKING_DIR"parse/enums/valid/simple_identifier.aason", true},

	{WORKING_DIR"parse/enums/invalid/identifier_starts_with_digit.aason", false},
	{WORKING_DIR"parse/enums/invalid/identifier_with_dot.aason", false},
	{WORKING_DIR"parse/enums/invalid/identifier_with_hyphen.aason", false},
	{WORKING_DIR"parse/enums/invalid/identifier_with_space.aason", false},

	{WORKING_DIR"parse/constructors/valid/mixed_constructors.aason", true},
	{WORKING_DIR"parse/constructors/valid/nested_containers.aason", true},
	{WORKING_DIR"parse/constructors/valid/rgb_basic.aason", true},
	{WORKING_DIR"parse/constructors/valid/rgb_zero.aason", true},
	{WORKING_DIR"parse/constructors/valid/tag_empty.aason", true},
	{WORKING_DIR"parse/constructors/valid/tag_escaped.aason", true},
	{WORKING_DIR"parse/constructors/valid/tag_simple.aason", true},
	{WORKING_DIR"parse/constructors/valid/vec2_multiple.aason", true},
	{WORKING_DIR"parse/constructors/valid/vec2_negative.aason", true},
	{WORKING_DIR"parse/constructors/valid/vec2_zero.aason", true},

	{WORKING_DIR"parse/constructors/invalid/missing_argument.aason", false},
	{WORKING_DIR"parse/constructors/invalid/trailing_comma.aason", false},
	{WORKING_DIR"parse/constructors/invalid/unclosed_parenthesis.aason", false},
	{WORKING_DIR"parse/constructors/invalid/unexpected_closing_parenthesis.aason", false},
	{WORKING_DIR"parse/constructors/invalid/unknown_constructor.aason", false},
	{WORKING_DIR"parse/constructors/invalid/rgb_extra_argument.aason", false},
	{WORKING_DIR"parse/constructors/invalid/rgb_missing_argument.aason", false},
	{WORKING_DIR"parse/constructors/invalid/rgb_wrong_type.aason", false},
	{WORKING_DIR"parse/constructors/invalid/tag_extra_argument.aason", false},
	{WORKING_DIR"parse/constructors/invalid/tag_missing_argument.aason", false},
	{WORKING_DIR"parse/constructors/invalid/tag_wrong_type.aason", false},
	{WORKING_DIR"parse/constructors/invalid/vec2_extra_argument.aason", false},
	{WORKING_DIR"parse/constructors/invalid/vec2_missing_argument.aason", false},
	{WORKING_DIR"parse/constructors/invalid/vec2_wrong_type.aason", false},
};
static const size_t test_count = sizeof(test_cases) / sizeof(test_case);

typedef struct
{
	aason_error_type error_type;
} error_state;

static size_t test_index;
static size_t break_index = SIZE_MAX;

static void vec2_read_constructor(aason_context* ctx, void* value, const aason_arg* args, size_t count)
{
	(void)ctx;
	(void)value;

	assert(count == 2);
	assert(args[0].type == aason_arg_type_float);
	assert(args[1].type == aason_arg_type_float);
}

static const aason_arg_type vec2_args[] = {
	aason_arg_type_float,
	aason_arg_type_float
};

static const aason_constructor_desc vec2_constructor = {
	.name = "vec2",
	.args = vec2_args,
	.count = sizeof(vec2_args) / sizeof(aason_arg_type)
};

static void rgb_read_constructor(aason_context* ctx, void* value, const aason_arg* args, size_t count)
{
	(void)ctx;
	(void)value;

	assert(count == 3);
	assert(args[0].type == aason_arg_type_hex);
	assert(args[1].type == aason_arg_type_hex);
	assert(args[2].type == aason_arg_type_hex);
}

static const aason_arg_type rgb_args[] = {
	aason_arg_type_hex,
	aason_arg_type_hex,
	aason_arg_type_hex
};

static const aason_constructor_desc rgb_constructor = {
	.name = "rgb",
	.args = rgb_args,
	.count = sizeof(rgb_args) / sizeof(aason_arg_type)
};

static void tag_read_constructor(aason_context* ctx, void* value, const aason_arg* args, size_t count)
{
	(void)ctx;
	(void)value;

	assert(count == 1);
	assert(args[0].type == aason_arg_type_str);
}

static const aason_arg_type tag_args[] = {
	aason_arg_type_str
};

static const aason_constructor_desc tag_constructor = {
	.name = "tag",
	.args = tag_args,
	.count = sizeof(tag_args) / sizeof(aason_arg_type)
};

static const aason_constructor_desc* constructors[] = {
	&vec2_constructor,
	&rgb_constructor,
	&tag_constructor
};

static void error_callback(void* user_data, aason_error_type error, const char* file, uint32_t line, uint32_t column, const char* msg)
{
	(void)file;
	(void)line;
	(void)column;
	(void)msg;

	error_state* state = user_data;
	state->error_type = error;

	printf("    Error: %s (%u:%u) - %s.\n", file, line, column, msg);

	if (test_index == break_index)
	{
#if _WIN32
		__debugbreak();
#else
		__builtin_trap();
#endif
	}
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
		.error_interface = &error_interface,
		.constructors = constructors,
		.constructor_count = sizeof(constructors) / sizeof(aason_constructor_desc*)
	};
	aason_context* ctx = aason_read(&desc);
	if (ctx)
		aason_destroy(ctx);

	return state.error_type;
}

static bool test_all(void)
{
	size_t passed = 0;
	for (test_index = 0; test_index < test_count; ++test_index)
	{
		const test_case* test = &test_cases[test_index];

		printf("TEST [%zu]: %s\n", test_index, test->path);

		const aason_error_type error_type = test_parse(test->path);
		const bool result = error_type == aason_error_none;

		if (result == test->expect_success)
		{
			++passed;
			printf("    " COLOUR_GREEN "PASS: " COLOUR_RESET "% s\n", aason_get_error_string(error_type));
		}
		else
		{
			printf("    " COLOUR_RED "FAIL: " COLOUR_RESET "% s\n", aason_get_error_string(error_type));
		}
	}

	if (passed == test_count)
		printf("\n" COLOUR_GREEN "All tests passed successfully" COLOUR_RESET "\n");
	else
		printf("\n" COLOUR_RED "%zu test(s) failed" COLOUR_RESET "\n", test_count - passed);

	return passed == test_count;
}

int main(void)
{
	//break_index = 130;

	if (test_all())
		return EXIT_SUCCESS;

	return EXIT_FAILURE;
}