typedef struct
{
	aason_token_state	token_state;
	uint32_t			depth;
	uint32_t			max_depth;
	uint32_t			element_count;
	jmp_buf				jmp_ctx;
} aason_validator;

static void aason_validate_error(aason_validator* validator, aason_error_type error, const aason_token* token, const char* fmt, ...)
{
	char buffer[4096];

	aason_assert(token->file_index < validator->token_state.ctx->file_count);
	aason_file* file = &validator->token_state.ctx->files[token->file_index];

	validator->token_state.ctx->error = error;
	validator->token_state.ctx->error_file = file->buffer;
	validator->token_state.ctx->error_line = token->line;
	validator->token_state.ctx->error_column = token->column;

	if (validator->token_state.ctx->error_interface.error)
	{
		va_list args;
		va_start(args, fmt);
		aason_format_string(validator->token_state.ctx, buffer, sizeof(buffer), fmt, args);
		va_end(args);
	
		validator->token_state.ctx->error_interface.error(
			validator->token_state.ctx->error_interface.user_data,
			error,
			file->buffer,
			token->line,
			token->column,
			buffer
		);
	}

	longjmp(validator->jmp_ctx, 1);
}

static aason_token* aason_validate_expect_token(aason_validator* validator, aason_token_type type)
{
	aason_token* token = aason_get_next_token(&validator->token_state);
	if (token->type != type)
	{
		aason_validate_error(validator, aason_error_unexpected_token, token,
			"Token '{s}' found instead of expected token '{s}'",
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
			"Token '{s}' found instead of expected tokens '{s}' or '{s}'",
			aason_token_type_strings[token->type], aason_token_type_strings[type1], aason_token_type_strings[type2]
		);
	}

	return token;
}

static void aason_validate_constructor(aason_validator* validator, aason_token* token)
{
	char* name = token->begin;
	const size_t len = token->end - token->begin;
	name[len] = 0; // Null terminate

	uint32_t index = 0;
	const aason_constructor_desc* constructor = nullptr;
	for (index = 0; index < validator->token_state.ctx->constructor_count; ++index)
	{
		if (strcmp(name, validator->token_state.ctx->constructors[index]->name) == 0)
		{
			constructor = validator->token_state.ctx->constructors[index];
			break;
		}
	}

	if (!constructor)
	{
		aason_validate_error(validator, aason_error_unknown_constructor, token,
			"Unknown constructor '%s'", name
		);
	}

	token->constructor_index = index;

	aason_validate_expect_token(validator, aason_token_type_open_paren);

	for (uint32_t i = 0; i < constructor->count; ++i)
	{
		switch (constructor->args[i])
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

		if (i < constructor->count - 1)
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
	aason_token* comma;

	if (++validator->depth > validator->max_depth)
		validator->max_depth = validator->depth;

	for (;;)
	{
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
				"Unexpected token '{s}'", aason_token_type_strings[token->type]
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
	aason_token* comma;

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
				"Unexpected token '{s}'", aason_token_type_strings[token->type]
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

static bool aason_validate(aason_context* ctx, aason_tokens* tokens)
{
	aason_validator validator = {
		.token_state	= {
			.ctx		= ctx,
			.tokens		= tokens
		},
		.depth			= 1,
		.max_depth		= 1,
		.element_count	= 0
	};

	if (!setjmp(validator.jmp_ctx))
	{
		aason_validate_expect_token(&validator, aason_token_type_identifier);
		aason_validate_expect_token(&validator, aason_token_type_colon);
	
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
				"Unexpected token '{s}'", aason_token_type_strings[token->type]
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