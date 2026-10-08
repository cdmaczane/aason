typedef struct
{
	aason_context*	ctx;
	aason_tokens*	tokens;
	uint16_t		file_index;
	uint32_t		line;
	uint32_t		column;
	uint32_t		current_token;
	uint32_t		depth;
	uint32_t		max_depth;
	uint32_t		element_count;
	jmp_buf			jmp_ctx;
} aason_validator;

static void aason_validate_error(aason_validator* validator, aason_error_type error, const char* fmt, ...)
{
	char buffer[4096];

	validator->ctx->error = error;
	//validator->ctx->error_file = validator->file;
	validator->ctx->error_line = validator->line;
	validator->ctx->error_column = validator->column;

	if (validator->ctx->error_interface.error)
	{
		va_list args;
		va_start(args, fmt);
		aason_format_string(validator->ctx, buffer, sizeof(buffer), fmt, args);
		va_end(args);
	
		validator->ctx->error_interface.error(
			validator->ctx->error_interface.state,
			error,
			validator->line,
			validator->column,
			buffer
		);
	}

	longjmp(validator->jmp_ctx, 1);
}

static aason_token_type aason_validate_get_next_token(aason_validator* validator, aason_token** out_token)
{
	// TODO: Check if this could occur from bad data and, if so, convert to an error
	aason_assert(validator->current_token < validator->tokens->count);

	aason_token* token = &validator->tokens->tokens[validator->current_token++];
	*out_token = token;

	validator->file_index = token->file_index;
	validator->line = token->line;
	validator->column = token->column;

	return token->type;
}

static void aason_validate_expect_token(aason_validator* validator, aason_token_type type, aason_token** out_token)
{
	const aason_token_type token_type = aason_validate_get_next_token(validator, out_token);

	if (token_type != type)
	{
		aason_validate_error(validator, aason_error_unexpected_token,
			"Token '{s}' found instead of expected token '{s}'",
			aason_token_type_strings[token_type], aason_token_type_strings[type]
		);
	}
}

static aason_token_type aason_validate_expect_either_token(aason_validator* validator, aason_token_type type1, aason_token_type type2, aason_token** out_token)
{
	const aason_token_type token_type = aason_validate_get_next_token(validator, out_token);

	if (!(token_type == type1 || token_type == type2))
	{
		aason_validate_error(validator, aason_error_unexpected_token,
			"Token '{s}' found instead of expected tokens '{s}' or '{s}'",
			aason_token_type_strings[token_type], aason_token_type_strings[type1], aason_token_type_strings[type2]
		);
	}

	return token_type;
}

static void aason_validate_constructor(aason_validator* validator, aason_token* parent, aason_token* self)
{
	aason_token* token;

	char* name = self->begin;
	const size_t len = self->end - self->begin;
	name[len] = 0; // Null terminate

	uint32_t index = 0;
	const aason_constructor_desc* constructor = nullptr;
	for (index = 0; index < validator->ctx->constructor_count; ++index)
	{
		if (strcmp(name, validator->ctx->constructors[index]->name) == 0)
		{
			constructor = validator->ctx->constructors[index];
			break;
		}
	}

	// TODO: Raise error
	aason_assert(constructor);

	parent->constructor_index = index;

	aason_validate_expect_token(validator, aason_token_type_open_paren, &token);

	for (uint32_t i = 0; i < constructor->count; ++i)
	{
		switch (constructor->args[i])
		{
		case aason_arg_type_str:
			aason_validate_expect_token(validator, aason_token_type_str, &token);
			break;
		case aason_arg_type_bin:
			aason_validate_expect_token(validator, aason_token_type_bin, &token);
			break;
		case aason_arg_type_dec:
			aason_validate_expect_token(validator, aason_token_type_dec, &token);
			break;
		case aason_arg_type_hex:
			aason_validate_expect_token(validator, aason_token_type_hex, &token);
			break;
		case aason_arg_type_bool:
			aason_validate_expect_either_token(validator, aason_token_type_true, aason_token_type_false, &token);
			break;
		case aason_arg_type_enum:
			aason_validate_expect_token(validator, aason_token_type_identifier, &token);
			break;
		case aason_arg_type_float:
			aason_validate_expect_token(validator, aason_token_type_float, &token);
			break;
		}

		if (i < constructor->count - 1)
			aason_validate_expect_token(validator, aason_token_type_comma, &token); // Skip comma token

		++parent->count;
		++validator->element_count;
	}

	aason_validate_expect_token(validator, aason_token_type_close_paren, &token);
}

static void aason_validate_array(aason_validator* validator, aason_token* parent);

static void aason_validate_object(aason_validator* validator, aason_token* parent)
{
	aason_token* self;
	aason_token* token;

	if (++validator->depth > validator->max_depth)
		validator->max_depth = validator->depth;

	for (;;)
	{
		aason_validate_expect_token(validator, aason_token_type_identifier, &self);
		aason_validate_expect_token(validator, aason_token_type_colon, &token);

		switch (aason_validate_get_next_token(validator, &token))
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
			aason_validate_constructor(validator, self, token);
			++parent->count;
			break;
		case aason_token_type_enter_array:
			aason_validate_array(validator, self);
			++parent->count;
			break;
		case aason_token_type_enter_object:
			aason_validate_object(validator, self);
			++parent->count;
			break;
		default:
			aason_validate_error(validator, aason_error_unexpected_token,
				"Unexpected token '{s}'", aason_token_type_strings[token->type]
			);
		}

		++validator->element_count;

		if (aason_validate_expect_either_token(validator, aason_token_type_comma, aason_token_type_leave_object, &token) == aason_token_type_leave_object)
		{
			--validator->depth;
			return;
		}
	}
}

static void aason_validate_array(aason_validator* validator, aason_token* parent)
{
	aason_token* token;

	if (++validator->depth > validator->max_depth)
		validator->max_depth = validator->depth;

	for (;;)
	{
		switch (aason_validate_get_next_token(validator, &token))
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
			aason_validate_constructor(validator, token, token);
			++parent->count;
			break;
		case aason_token_type_enter_object:
			aason_validate_object(validator, token);
			++parent->count;
			break;
		case aason_token_type_leave_array:
			// Empty array
			--validator->depth;
			return;
		default:
			aason_validate_error(validator, aason_error_unexpected_token,
				"Unexpected token '{s}'", aason_token_type_strings[token->type]
			);
		}

		++validator->element_count;

		if (aason_validate_expect_either_token(validator, aason_token_type_comma, aason_token_type_leave_array, &token) == aason_token_type_leave_array)
		{
			--validator->depth;
			return;
		}
	}
}

static bool aason_validate(aason_context* ctx, aason_tokens* tokens)
{
	aason_validator validator = {
		.ctx			= ctx,
		.tokens			= tokens,
		.line			= 1,
		.column			= 1,
		.current_token	= 0,
		.depth			= 1,
		.max_depth		= 1,
		.element_count	= 0
	};

	if (!setjmp(validator.jmp_ctx))
	{
		aason_token* self;
		aason_token* token;
		aason_validate_expect_token(&validator, aason_token_type_identifier, &self);
		aason_validate_expect_token(&validator, aason_token_type_colon, &token);
	
		switch (aason_validate_get_next_token(&validator, &token))
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
			break;
		case aason_token_type_enter_array:
			aason_validate_array(&validator, self);
			break;
		case aason_token_type_enter_object:
			aason_validate_object(&validator, self);
			break;
		default:
			aason_validate_error(&validator, aason_error_unexpected_token,
				"Unexpected token '{s}'", aason_token_type_strings[token->type]
			);
		}
	
		aason_validate_expect_token(&validator, aason_token_type_eof, &token);
	
		ctx->element_count = validator.element_count + 1;
		ctx->max_stack_depth = validator.max_depth;
	
		aason_assert(validator.depth == 1);

		return true;
	}

	return false;
}