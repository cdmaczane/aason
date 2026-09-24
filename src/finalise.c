typedef struct
{
	rgs_sdd*	sdd;
	char*		begin;
	sdd_token*	tokens;
	uint32_t	token_index;
	uint32_t	element_index;
	jmp_buf		jmp_ctx;
} sdd_finalise_ctx;

static void sdd_finalise_error(sdd_finalise_ctx* ctx, rgs_sdd_error error, const char* fmt, ...)
{
	char buffer[rgs_kib(4)];

	ctx->sdd->error = error;
	ctx->sdd->error_line = ctx->tokens[ctx->token_index].line;
	ctx->sdd->error_column = ctx->tokens[ctx->token_index].column;

	if (ctx->sdd->error_callback)
	{
		va_list args;
		va_start(args, fmt);
		const int64_t len = rgs_format_impl(buffer, sizeof(buffer), fmt, args);
		(void)len;
		va_end(args);
	
		ctx->sdd->error_callback(ctx->sdd->user_data, error, ctx->sdd->error_line, ctx->sdd->error_column, buffer);
	}

	longjmp(ctx->jmp_ctx, 1);
}

static sdd_token* sdd_finalise_get_next_token(sdd_finalise_ctx* ctx)
{
	rgs_assert(ctx->token_index < rgs_scratch_array_count(ctx->tokens));

	return &ctx->tokens[ctx->token_index++];
}

static rgs_sdd_element* sdd_finalise_allocate_elements(sdd_finalise_ctx* ctx, uint32_t count)
{
	rgs_assert(ctx->element_index + count <= ctx->sdd->element_count);

	rgs_sdd_element* alloc = &ctx->sdd->elements[ctx->element_index];
	ctx->element_index += count;

	return alloc;
}

static void sdd_finalise_parse_str(sdd_finalise_ctx* ctx, sdd_token* token, rgs_sdd_element* element)
{
	element->type = rgs_sdd_type_str;
	element->str_value.offset = (uint32_t)(token->begin - ctx->begin) + 1;
	element->str_value.len = (uint32_t)(token->end - token->begin) - 1;

	// Null terminate string
	token->end[-1] = 0;
}

static void sdd_finalise_parse_bin(sdd_finalise_ctx* ctx, sdd_token* token, rgs_sdd_element* element)
{
	element->type = rgs_sdd_type_int;

	if (rgs_from_string_i64(token->begin, &element->int_value, rgs_int_base_bin) < 0)
		sdd_finalise_error(ctx, rgs_sdd_error_out_of_range, "Binary integer too large");
}

static void sdd_finalise_parse_dec(sdd_finalise_ctx* ctx, sdd_token* token, rgs_sdd_element* element)
{
	element->type = rgs_sdd_type_int;

	if (rgs_from_string_i64(token->begin, &element->int_value, rgs_int_base_dec) < 0)
		sdd_finalise_error(ctx, rgs_sdd_error_out_of_range, "Decimal integer too large");
}

static void sdd_finalise_parse_hex(sdd_finalise_ctx* ctx, sdd_token* token, rgs_sdd_element* element)
{
	element->type = rgs_sdd_type_int;

	if (rgs_from_string_i64(token->begin, &element->int_value, rgs_int_base_hex) < 0)
		sdd_finalise_error(ctx, rgs_sdd_error_out_of_range, "Hexadecimal integer too large");
}

static void sdd_finalise_parse_hash(sdd_finalise_ctx* ctx, sdd_token* token, rgs_sdd_element* element)
{
	element->type = rgs_sdd_type_hash;
	rgs_from_string_u32(token->begin + 1, &element->hash_value, rgs_int_base_hex);
}

static void sdd_finalise_parse_true(sdd_finalise_ctx* ctx, sdd_token* token, rgs_sdd_element* element)
{
	element->type = rgs_sdd_type_bool;
	element->bool_value = true;
}

static void sdd_finalise_parse_false(sdd_finalise_ctx* ctx, sdd_token* token, rgs_sdd_element* element)
{
	element->type = rgs_sdd_type_bool;
	element->bool_value = false;
}

static void sdd_finalise_parse_float(sdd_finalise_ctx* ctx, sdd_token* token, rgs_sdd_element* element)
{
	element->type = rgs_sdd_type_float;

	if (rgs_from_string_float(token->begin, &element->float_value) < 0)
		sdd_finalise_error(ctx, rgs_sdd_error_out_of_range, "Floating point number out of range");
}

static void sdd_finalise_parse_hash_str(sdd_finalise_ctx* ctx, sdd_token* token, rgs_sdd_element* element)
{
	element->type = rgs_sdd_type_hash;
	element->hash_value = rgs_hash_mem_fnv32(token->begin + 2, token->end - token->begin - 3);
}

static void sdd_finalise_parse_enum(sdd_finalise_ctx* ctx, sdd_token* token, rgs_sdd_element* element)
{
	element->type = rgs_sdd_type_enum;
	element->enum_value.offset = (uint32_t)(token->begin - ctx->begin);
	element->enum_value.len = (uint32_t)(token->end - token->begin);

	// Null terminate string
	*token->end = 0;
}

static void sdd_finalise_parse_array(sdd_finalise_ctx* ctx, sdd_token* token, rgs_sdd_element* element);
static void sdd_finalise_parse_object(sdd_finalise_ctx* ctx, sdd_token* token, rgs_sdd_element* element);

static void sdd_finalise_parse_element(sdd_finalise_ctx* ctx, sdd_token* parent, sdd_token* token, rgs_sdd_element* element, bool inside_array)
{
	switch (token->type)
	{
	case sdd_token_type_str:
		sdd_finalise_parse_str(ctx, token, element);
		break;
	case sdd_token_type_bin:
		sdd_finalise_parse_bin(ctx, token, element);
		break;
	case sdd_token_type_dec:
		sdd_finalise_parse_dec(ctx, token, element);
		break;
	case sdd_token_type_hex:
		sdd_finalise_parse_hex(ctx, token, element);
		break;
	case sdd_token_type_hash:
		sdd_finalise_parse_hash(ctx, token, element);
		break;
	case sdd_token_type_true:
		sdd_finalise_parse_true(ctx, token, element);
		break;
	case sdd_token_type_false:
		sdd_finalise_parse_false(ctx, token, element);
		break;
	case sdd_token_type_float:
		sdd_finalise_parse_float(ctx, token, element);
		break;
	case sdd_token_type_hash_str:
		sdd_finalise_parse_hash_str(ctx, token, element);
		break;
	case sdd_token_type_identifier:
		sdd_finalise_parse_enum(ctx, token, element);
		break;
	case sdd_token_type_enter_array:
		sdd_finalise_parse_array(ctx, parent, element);
		break;
	case sdd_token_type_enter_object:
		sdd_finalise_parse_object(ctx, inside_array ? token : parent, element);
		break;
	default:
		break;
	}
}

static void sdd_finalise_parse_array(sdd_finalise_ctx* ctx, sdd_token* self, rgs_sdd_element* element)
{
	element->type = rgs_sdd_type_array;

	const uint32_t array_count = self->count;
	element->array_value.count = array_count;
	element->array_value.first_child = ctx->element_index;

	rgs_sdd_element* children = sdd_finalise_allocate_elements(ctx, array_count);

	for (uint32_t i = 0; i < array_count; ++i)
	{
		sdd_token* token = sdd_finalise_get_next_token(ctx);

		children[i].key_offset = 0;
		children[i].key_len = 0;
		children[i].line = token->line;
		children[i].column = token->column;

		sdd_finalise_parse_element(ctx, self, token, &children[i], true);

		if (i < array_count - 1)
			sdd_finalise_get_next_token(ctx); // Skip comma token
	}

	sdd_finalise_get_next_token(ctx); // Skip leave array token
}

static void sdd_finalise_parse_object(sdd_finalise_ctx* ctx, sdd_token* self, rgs_sdd_element* element)
{
	element->type = rgs_sdd_type_object;

	const uint32_t field_count = self->count;
	element->object_value.count = field_count;
	element->object_value.first_child = ctx->element_index;

	rgs_sdd_element* children = sdd_finalise_allocate_elements(ctx, field_count);

	for (uint32_t i = 0; i < field_count; ++i)
	{
		self = sdd_finalise_get_next_token(ctx);
		sdd_finalise_get_next_token(ctx); // Skip colon token
		*self->end = 0; // Null terminate key string

		children[i].key_offset = (uint32_t)(self->begin - ctx->begin);
		children[i].key_len = (uint32_t)(self->end - self->begin);
		children[i].line = self->line;
		children[i].column = self->column;

		sdd_token* token = sdd_finalise_get_next_token(ctx);
		sdd_finalise_parse_element(ctx, self, token, &children[i], false);

		if (i < field_count - 1)
			sdd_finalise_get_next_token(ctx); // Skip comma token
	}

	sdd_finalise_get_next_token(ctx); // Skip leave object token
}

static bool sdd_finalise(rgs_sdd* sdd, char* buffer, sdd_token* tokens)
{
	sdd_finalise_ctx ctx = {
		.sdd	= sdd,
		.begin	= buffer,
		.tokens	= tokens
	};

	if (!setjmp(ctx.jmp_ctx))
	{
		sdd_token* self = sdd_finalise_get_next_token(&ctx);
		sdd_finalise_get_next_token(&ctx); // Skip colon token
		*self->end = 0; // Null terminate key string
	
		// Allocate root element
		rgs_sdd_element* element = sdd_finalise_allocate_elements(&ctx, 1);
		element->key_offset = (uint32_t)(self->begin - buffer);
		element->key_len = (uint32_t)(self->end - self->begin);
		element->line = self->line;
		element->column = self->column;

		sdd_token* token = sdd_finalise_get_next_token(&ctx);
		sdd_finalise_parse_element(&ctx, self, token, element, false);
	
		rgs_assert(ctx.element_index == sdd->element_count);

		return true;
	}

	return false;
}