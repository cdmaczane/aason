typedef struct
{
	rgs_sdd*	sdd;
	sdd_token*	tokens;
	uint32_t	line;
	uint32_t	column;
	uint32_t	current_token;
	uint32_t	depth;
	uint32_t	max_depth;
	uint32_t	element_count;
	jmp_buf		jmp_ctx;
} sdd_validate_ctx;

static void sdd_validate_error(sdd_validate_ctx* ctx, rgs_sdd_error error, const char* fmt, ...)
{
	char buffer[rgs_kib(4)];

	ctx->sdd->error = error;
	ctx->sdd->error_line = ctx->line;
	ctx->sdd->error_column = ctx->column;

	if (ctx->sdd->error_callback)
	{
		va_list args;
		va_start(args, fmt);
		const int64_t len = rgs_format_impl(buffer, sizeof(buffer), fmt, args);
		(void)len;
		va_end(args);
	
		ctx->sdd->error_callback(ctx->sdd->user_data, error, ctx->line, ctx->column, buffer);
	}

	longjmp(ctx->jmp_ctx, 1);
}

static sdd_token_type sdd_validate_get_next_token(sdd_validate_ctx* ctx, sdd_token** out_token)
{
	// TODO: Check if this could occur from bad data and, if so, convert to an error
	rgs_assert(ctx->current_token < rgs_scratch_array_count(ctx->tokens));

	sdd_token* token = &ctx->tokens[ctx->current_token++];
	*out_token = token;

	ctx->line = token->line;
	ctx->column = token->column;

	return token->type;
}

static void sdd_validate_expect_token(sdd_validate_ctx* ctx, sdd_token_type type, sdd_token** out_token)
{
	const sdd_token_type token_type = sdd_validate_get_next_token(ctx, out_token);

	if (token_type != type)
	{
		sdd_validate_error(ctx, rgs_sdd_error_unexpected_token,
			"Token '{s}' found instead of expected token '{s}'",
			sdd_token_type_strings[token_type], sdd_token_type_strings[type]
		);
	}
}

static sdd_token_type sdd_validate_expect_either_token(sdd_validate_ctx* ctx, sdd_token_type type1, sdd_token_type type2, sdd_token** out_token)
{
	const sdd_token_type token_type = sdd_validate_get_next_token(ctx, out_token);

	if (!(token_type == type1 || token_type == type2))
	{
		sdd_validate_error(ctx, rgs_sdd_error_unexpected_token,
			"Token '{s}' found instead of expected tokens '{s}' or '{s}'",
			sdd_token_type_strings[token_type], sdd_token_type_strings[type1], sdd_token_type_strings[type2]
		);
	}

	return token_type;
}

static void sdd_validate_array(sdd_validate_ctx* ctx, sdd_token* parent);

static void sdd_validate_object(sdd_validate_ctx* ctx, sdd_token* parent)
{
	sdd_token* self;
	sdd_token* token;

	if (++ctx->depth > ctx->max_depth)
		ctx->max_depth = ctx->depth;

	for (;;)
	{
		sdd_validate_expect_token(ctx, sdd_token_type_identifier, &self);
		sdd_validate_expect_token(ctx, sdd_token_type_colon, &token);

		switch (sdd_validate_get_next_token(ctx, &token))
		{
		case sdd_token_type_str:
		case sdd_token_type_bin:
		case sdd_token_type_dec:
		case sdd_token_type_hex:
		case sdd_token_type_hash:
		case sdd_token_type_true:
		case sdd_token_type_false:
		case sdd_token_type_float:
		case sdd_token_type_hash_str:
		case sdd_token_type_identifier:
			++parent->count;
			break;
		case sdd_token_type_enter_array:
			sdd_validate_array(ctx, self);
			++parent->count;
			break;
		case sdd_token_type_enter_object:
			sdd_validate_object(ctx, self);
			++parent->count;
			break;
		default:
			sdd_validate_error(ctx, rgs_sdd_error_unexpected_token,
				"Unexpected token '{s}'", sdd_token_type_strings[token->type]
			);
		}

		++ctx->element_count;

		if (sdd_validate_expect_either_token(ctx, sdd_token_type_comma, sdd_token_type_leave_object, &token) == sdd_token_type_leave_object)
		{
			--ctx->depth;
			return;
		}
	}
}

static void sdd_validate_array(sdd_validate_ctx* ctx, sdd_token* parent)
{
	sdd_token* token;

	if (++ctx->depth > ctx->max_depth)
		ctx->max_depth = ctx->depth;

	for (;;)
	{
		switch (sdd_validate_get_next_token(ctx, &token))
		{
		case sdd_token_type_str:
		case sdd_token_type_bin:
		case sdd_token_type_dec:
		case sdd_token_type_hex:
		case sdd_token_type_hash:
		case sdd_token_type_true:
		case sdd_token_type_false:
		case sdd_token_type_float:
		case sdd_token_type_hash_str:
		case sdd_token_type_identifier:
			++parent->count;
			break;
		case sdd_token_type_enter_object:
			sdd_validate_object(ctx, token);
			++parent->count;
			break;
		case sdd_token_type_leave_array:
			// Empty array
			--ctx->depth;
			return;
		default:
			sdd_validate_error(ctx, rgs_sdd_error_unexpected_token,
				"Unexpected token '{s}'", sdd_token_type_strings[token->type]
			);
		}

		++ctx->element_count;

		if (sdd_validate_expect_either_token(ctx, sdd_token_type_comma, sdd_token_type_leave_array, &token) == sdd_token_type_leave_array)
		{
			--ctx->depth;
			return;
		}
	}
}

static bool sdd_validate(rgs_sdd* sdd, sdd_token* tokens)
{
	sdd_validate_ctx ctx = {
		.sdd			= sdd,
		.tokens			= tokens,
		.line			= 1,
		.column			= 1,
		.current_token	= 0,
		.depth			= 1,
		.max_depth		= 1,
		.element_count	= 0
	};

	if (!setjmp(ctx.jmp_ctx))
	{
		sdd_token* self;
		sdd_token* token;
		sdd_validate_expect_token(&ctx, sdd_token_type_identifier, &self);
		sdd_validate_expect_token(&ctx, sdd_token_type_colon, &token);
	
		switch (sdd_validate_get_next_token(&ctx, &token))
		{
		case sdd_token_type_str:
		case sdd_token_type_bin:
		case sdd_token_type_dec:
		case sdd_token_type_hex:
		case sdd_token_type_hash:
		case sdd_token_type_true:
		case sdd_token_type_false:
		case sdd_token_type_float:
		case sdd_token_type_hash_str:
		case sdd_token_type_identifier:
			break;
		case sdd_token_type_enter_array:
			sdd_validate_array(&ctx, self);
			break;
		case sdd_token_type_enter_object:
			sdd_validate_object(&ctx, self);
			break;
		default:
			sdd_validate_error(&ctx, rgs_sdd_error_unexpected_token,
				"Unexpected token '{s}'", sdd_token_type_strings[token->type]
			);
		}
	
		sdd_validate_expect_token(&ctx, sdd_token_type_eof, &token);
	
		sdd->element_count = ctx.element_count + 1;
		sdd->max_stack_depth = ctx.max_depth;
	
		rgs_assert(ctx.depth == 1);

		return true;
	}

	return false;
}