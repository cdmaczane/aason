static const char* aason_token_type_strings[] = {
	"EOF",
	"string",
	"binary number",
	"decimal number",
	"hexadecimal number",
	"true",
	"false",
	",",
	":",
	"floating point number",
	"identifier",
	"constructor",
	"(",
	")",
	"[",
	"]",
	"{",
	"}",
	"JMP"
};
static_assert(sizeof(aason_token_type_strings) / sizeof(const char*) == aason_token_type_count, "");

typedef struct
{
	aason_token_state	token_state;
	uint32_t			depth;
	uint32_t			max_depth;
	uint32_t			element_count;
	jmp_buf*			jmp_ctx;
} aason_validator;

static void aason_validate_error(aason_validator* validator, aason_error_type error, const aason_token* token, const char* format, ...)
{
	aason_context* ctx = validator->token_state.ctx;

	va_list args;
	va_start(args, format);
	aason_report_error(
		ctx,
		error,
		aason_get_file_path(ctx, token->file_index),
		token->line,
		token->column,
		format,
		args
	);
	va_end(args);

	longjmp(*validator->jmp_ctx, 1);
}

static aason_token* aason_validate_expect_token(aason_validator* validator, aason_token_type type)
{
	aason_token* token = aason_get_next_token(&validator->token_state);
	if (token->type != type)
	{
		aason_validate_error(validator, aason_error_unexpected_token, token,
			"Token '%s' found instead of expected token '%s'",
			aason_token_type_strings[token->type], aason_token_type_strings[type]
		);
	}

	return token;
}

static aason_token* aason_validate_expect_either_token(aason_validator* validator, aason_token_type type1, aason_token_type type2)
{
	aason_token* token = aason_get_next_token(&validator->token_state);
	if (!(token->type == type1 || token->type == type2))
	{
		aason_validate_error(validator, aason_error_unexpected_token, token,
			"Token '%s' found instead of expected tokens '%s' or '%s'",
			aason_token_type_strings[token->type], aason_token_type_strings[type1], aason_token_type_strings[type2]
		);
	}

	return token;
}

static uint32_t aason_validate_find_constructor(aason_validator* validator, aason_token* token, const char* name)
{
	const aason_constructors* constructors = validator->token_state.ctx->constructors;
	if (constructors)
	{
		for (uint32_t i = 0; i < constructors->count; ++i)
		{
			if (strcmp(name, constructors->names[i]) == 0)
				return i;
		}
	}

	aason_validate_error(validator, aason_error_unknown_constructor, token,
		"Unknown constructor '%s'", name
	);

	return UINT32_MAX;
}

static void aason_validate_constructor(aason_validator* validator, aason_token* token)
{
	char* name = token->begin;
	const size_t len = token->end - token->begin;
	name[len] = 0; // Null terminate

	const uint32_t index = aason_validate_find_constructor(validator, token, name);
	token->count = 0;
	token->constructor_index = index;

	aason_validate_expect_token(validator, aason_token_type_open_paren);

	const aason_constructors* constructors = validator->token_state.ctx->constructors;
	const uint8_t arg_count = constructors->arg_counts[index];
	const uint32_t args_index = constructors->arg_indices[index];
	const uint8_t* args = constructors->args + args_index;

	for (uint8_t i = 0; i < arg_count; ++i)
	{
		const aason_arg_type type = args[i];
		switch (type)
		{
		case aason_arg_type_str:
			aason_validate_expect_token(validator, aason_token_type_str);
			break;
		case aason_arg_type_bin:
			aason_validate_expect_token(validator, aason_token_type_bin);
			break;
		case aason_arg_type_dec:
			aason_validate_expect_token(validator, aason_token_type_dec);
			break;
		case aason_arg_type_hex:
			aason_validate_expect_token(validator, aason_token_type_hex);
			break;
		case aason_arg_type_bool:
			aason_validate_expect_either_token(validator, aason_token_type_true, aason_token_type_false);
			break;
		case aason_arg_type_enum:
			aason_validate_expect_token(validator, aason_token_type_identifier);
			break;
		case aason_arg_type_float:
			aason_validate_expect_token(validator, aason_token_type_float);
			break;
		}

		if (i < constructors->count - 1)
			aason_validate_expect_token(validator, aason_token_type_comma); // Skip comma token

		++token->count;
		++validator->element_count;
	}

	aason_validate_expect_token(validator, aason_token_type_close_paren);
}

static void aason_validate_array(aason_validator* validator, aason_token* parent);

static void aason_validate_object(aason_validator* validator, aason_token* parent)
{
	aason_token* token;
	aason_token* comma = NULL;

	parent->count = 0;

	if (++validator->depth > validator->max_depth)
		validator->max_depth = validator->depth;

	for (;;)
	{
		if (validator->token_state.ctx->mode == aason_mode_json)
			token = aason_validate_expect_either_token(validator, aason_token_type_str, aason_token_type_leave_object);
		else
			token = aason_validate_expect_either_token(validator, aason_token_type_identifier, aason_token_type_leave_object);

		if (token->type == aason_token_type_leave_object)
		{
			// If we get here then it means that the previous token was a comma
			if (parent->count != 0 && !validator->token_state.ctx->allow_trailing_commas)
			{
				aason_validate_error(validator, aason_error_trailing_comma, comma,
					"Trailing commas are not permitted with the current settings"
				);
			}

			--validator->depth;
			return;
		}

		aason_validate_expect_token(validator, aason_token_type_colon);

		token = aason_get_next_token(&validator->token_state);
		switch (token->type)
		{
		case aason_token_type_str:
		case aason_token_type_bin:
		case aason_token_type_dec:
		case aason_token_type_hex:
		case aason_token_type_true:
		case aason_token_type_false:
		case aason_token_type_float:
		case aason_token_type_identifier:
			++parent->count;
			break;
		case aason_token_type_constructor:
			aason_validate_constructor(validator, token);
			++parent->count;
			break;
		case aason_token_type_enter_array:
			aason_validate_array(validator, token);
			++parent->count;
			break;
		case aason_token_type_enter_object:
			aason_validate_object(validator, token);
			++parent->count;
			break;
		default:
			aason_validate_error(validator, aason_error_unexpected_token, token,
				"Unexpected token '%s'", aason_token_type_strings[token->type]
			);
		}

		++validator->element_count;

		comma = aason_validate_expect_either_token(validator, aason_token_type_comma, aason_token_type_leave_object);
		if (comma->type == aason_token_type_leave_object)
		{
			--validator->depth;
			return;
		}
	}
}

static void aason_validate_array(aason_validator* validator, aason_token* parent)
{
	aason_token* token;
	aason_token* comma = NULL;

	parent->count = 0;

	if (++validator->depth > validator->max_depth)
		validator->max_depth = validator->depth;

	for (;;)
	{
		token = aason_get_next_token(&validator->token_state);
		if (token->type == aason_token_type_leave_array)
		{
			if (parent->count != 0 && !validator->token_state.ctx->allow_trailing_commas)
			{
				aason_validate_error(validator, aason_error_trailing_comma, comma,
					"Trailing commas are not permitted with the current settings"
				);
			}

			--validator->depth;
			return;
		}

		switch (token->type)
		{
		case aason_token_type_str:
		case aason_token_type_bin:
		case aason_token_type_dec:
		case aason_token_type_hex:
		case aason_token_type_true:
		case aason_token_type_false:
		case aason_token_type_float:
		case aason_token_type_identifier:
			++parent->count;
			break;
		case aason_token_type_constructor:
			aason_validate_constructor(validator, token);
			++parent->count;
			break;
		case aason_token_type_enter_array:
			aason_validate_array(validator, token);
			++parent->count;
			break;
		case aason_token_type_enter_object:
			aason_validate_object(validator, token);
			++parent->count;
			break;
		default:
			aason_validate_error(validator, aason_error_unexpected_token, token,
				"Unexpected token '%s'", aason_token_type_strings[token->type]
			);
		}

		++validator->element_count;

		comma = aason_validate_expect_either_token(validator, aason_token_type_comma, aason_token_type_leave_array);
		if (comma->type == aason_token_type_leave_array)
		{
			--validator->depth;
			return;
		}
	}
}

static bool aason_run_validate_pass(aason_context* ctx, aason_tokens* tokens)
{
	jmp_buf jmp_ctx;

	aason_validator validator = {
		.token_state	= {
			.ctx		= ctx,
			.tokens		= tokens
		},
		.depth			= 1,
		.max_depth		= 1,
		.element_count	= 0,
		.jmp_ctx		= &jmp_ctx
	};

	if (!setjmp(*validator.jmp_ctx))
	{
		const bool skip_root_identifier = ctx->mode == aason_mode_json || !ctx->use_implicit_root;
		if (!skip_root_identifier)
		{
			aason_validate_expect_token(&validator, aason_token_type_identifier);
			aason_validate_expect_token(&validator, aason_token_type_colon);
		}
	
		aason_token* token = aason_get_next_token(&validator.token_state);
		switch (token->type)
		{
		case aason_token_type_str:
		case aason_token_type_bin:
		case aason_token_type_dec:
		case aason_token_type_hex:
		case aason_token_type_true:
		case aason_token_type_false:
		case aason_token_type_float:
		case aason_token_type_identifier:
		case aason_token_type_constructor:
			aason_validate_error(&validator, aason_error_unexpected_token, token,
				"Root must be a single object or array"
			);
		case aason_token_type_enter_array:
			aason_validate_array(&validator, token);
			break;
		case aason_token_type_enter_object:
			aason_validate_object(&validator, token);
			break;
		default:
			aason_validate_error(&validator, aason_error_unexpected_token, token,
				"Unexpected token '%s'", aason_token_type_strings[token->type]
			);
		}
	
		aason_validate_expect_token(&validator, aason_token_type_eof);
	
		ctx->element_count = validator.element_count + 1;
		ctx->max_stack_depth = validator.max_depth;
	
		aason_assert(validator.depth == 1);

		return true;
	}

	return false;
}