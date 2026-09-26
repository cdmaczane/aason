typedef struct
{
	aason_context*	ctx;
	char*			begin;
	aason_tokens*	tokens;
	uint32_t		token_index;
	uint32_t		element_index;
	jmp_buf			jmp_ctx;
} aason_finaliser;

static void aason_finalise_error(aason_finaliser* finaliser, aason_error error, const char* fmt, ...)
{
	char buffer[4096];

	finaliser->ctx->error = error;
	finaliser->ctx->error_line = finaliser->tokens->tokens[finaliser->token_index].line;
	finaliser->ctx->error_column = finaliser->tokens->tokens[finaliser->token_index].column;

	if (finaliser->ctx->read_desc->error_callback)
	{
		va_list args;
		va_start(args, fmt);
		aason_format_string(finaliser->ctx, buffer, sizeof(buffer), fmt, args);
		va_end(args);
	
		finaliser->ctx->read_desc->error_callback(
			finaliser->ctx->user_data,
			error,
			finaliser->ctx->error_line,
			finaliser->ctx->error_column,
			buffer
		);
	}

	longjmp(finaliser->jmp_ctx, 1);
}

static aason_token* aason_finalise_get_next_token(aason_finaliser* finaliser)
{
	aason_assert(finaliser->token_index < finaliser->tokens->count);

	return &finaliser->tokens->tokens[finaliser->token_index++];
}

static aason_element* aason_finalise_allocate_elements(aason_finaliser* finaliser, uint32_t count)
{
	aason_assert(finaliser->element_index + count <= finaliser->ctx->element_count);

	aason_element* alloc = &finaliser->ctx->elements[finaliser->element_index];
	finaliser->element_index += count;

	return alloc;
}

static void aason_finalise_parse_str(aason_finaliser* finaliser, aason_token* token, aason_element* element)
{
	element->type = aason_type_str;
	element->str_value.offset = (uint32_t)(token->begin - finaliser->begin) + 1;
	element->str_value.len = (uint32_t)(token->end - token->begin) - 1;

	// Null terminate string
	token->end[-1] = 0;
}

static bool aason_from_string_bin(const char* str, int64_t* value)
{
	//rgs_from_string_i64(token->begin, &element->int_value, rgs_int_base_bin) < 0
	return false;
}

static bool aason_from_string_dec(const char* str, int64_t* value)
{
	//rgs_from_string_i64(token->begin, &element->int_value, rgs_int_base_dec) < 0
	return false;
}

static bool aason_from_string_hex(const char* str, int64_t* value)
{
	//rgs_from_string_i64(token->begin, &element->int_value, rgs_int_base_hex) < 0
	return false;
}

static bool aason_from_string_hash(const char* str, uint32_t* value)
{
	//rgs_from_string_u32(token->begin + 1, &element->hash_value, rgs_int_base_hex);
	return false;
}

static void aason_finalise_parse_bin(aason_finaliser* finaliser, aason_token* token, aason_element* element)
{
	element->type = aason_type_int;

	if (!aason_from_string_bin(token->begin, &element->int_value))
		aason_finalise_error(finaliser, aason_error_out_of_range, "Binary integer too large");
}

static void aason_finalise_parse_dec(aason_finaliser* finaliser, aason_token* token, aason_element* element)
{
	element->type = aason_type_int;

	if (!aason_from_string_dec(token->begin, &element->int_value))
		aason_finalise_error(finaliser, aason_error_out_of_range, "Decimal integer too large");
}

static void aason_finalise_parse_hex(aason_finaliser* finaliser, aason_token* token, aason_element* element)
{
	element->type = aason_type_int;

	if (!aason_from_string_hex(token->begin, &element->int_value))
		aason_finalise_error(finaliser, aason_error_out_of_range, "Hexadecimal integer too large");
}

static void aason_finalise_parse_hash(aason_finaliser* finaliser, aason_token* token, aason_element* element)
{
	element->type = aason_type_hash;
	aason_from_string_hash(token->begin + 1, &element->hash_value);
}

static void aason_finalise_parse_true(aason_finaliser* finaliser, aason_token* token, aason_element* element)
{
	element->type = aason_type_bool;
	element->bool_value = true;
}

static void aason_finalise_parse_false(aason_finaliser* finaliser, aason_token* token, aason_element* element)
{
	element->type = aason_type_bool;
	element->bool_value = false;
}

static bool aason_from_string_float(const char* str, float* value)
{
	//rgs_from_string_float(token->begin, &element->float_value) < 0
	return false;
}

static void aason_finalise_parse_float(aason_finaliser* finaliser, aason_token* token, aason_element* element)
{
	element->type = aason_type_float;

	if (!aason_from_string_float(token->begin, &element->float_value))
		aason_finalise_error(finaliser, aason_error_out_of_range, "Floating point number out of range");
}

static uint32_t aason_fnv32(const char* str, size_t size)
{
	uint32_t hash = 2166136261;
	while (size--)
	{
		hash ^= *str++;
		hash *= 16777619;
	}

	return hash;
}

static void aason_finalise_parse_hash_str(aason_finaliser* finaliser, aason_token* token, aason_element* element)
{
	element->type = aason_type_hash;
	element->hash_value = aason_fnv32(token->begin + 2, token->end - token->begin - 3);
}

static void aason_finalise_parse_enum(aason_finaliser* finaliser, aason_token* token, aason_element* element)
{
	element->type = aason_type_enum;
	element->enum_value.offset = (uint32_t)(token->begin - finaliser->begin);
	element->enum_value.len = (uint32_t)(token->end - token->begin);

	// Null terminate string
	*token->end = 0;
}

static void aason_finalise_parse_array(aason_finaliser* finaliser, aason_token* token, aason_element* element);
static void aason_finalise_parse_object(aason_finaliser* finaliser, aason_token* token, aason_element* element);

static void aason_finalise_parse_element(aason_finaliser* finaliser, aason_token* parent, aason_token* token, aason_element* element, bool inside_array)
{
	switch (token->type)
	{
	case aason_token_type_str:
		aason_finalise_parse_str(finaliser, token, element);
		break;
	case aason_token_type_bin:
		aason_finalise_parse_bin(finaliser, token, element);
		break;
	case aason_token_type_dec:
		aason_finalise_parse_dec(finaliser, token, element);
		break;
	case aason_token_type_hex:
		aason_finalise_parse_hex(finaliser, token, element);
		break;
	case aason_token_type_hash:
		aason_finalise_parse_hash(finaliser, token, element);
		break;
	case aason_token_type_true:
		aason_finalise_parse_true(finaliser, token, element);
		break;
	case aason_token_type_false:
		aason_finalise_parse_false(finaliser, token, element);
		break;
	case aason_token_type_float:
		aason_finalise_parse_float(finaliser, token, element);
		break;
	case aason_token_type_hash_str:
		aason_finalise_parse_hash_str(finaliser, token, element);
		break;
	case aason_token_type_identifier:
		aason_finalise_parse_enum(finaliser, token, element);
		break;
	case aason_token_type_enter_array:
		aason_finalise_parse_array(finaliser, parent, element);
		break;
	case aason_token_type_enter_object:
		aason_finalise_parse_object(finaliser, inside_array ? token : parent, element);
		break;
	default:
		break;
	}
}

static void aason_finalise_parse_array(aason_finaliser* finaliser, aason_token* self, aason_element* element)
{
	element->type = aason_type_array;

	const uint32_t array_count = self->count;
	element->array_value.count = array_count;
	element->array_value.first_child = finaliser->element_index;

	aason_element* children = aason_finalise_allocate_elements(finaliser, array_count);

	for (uint32_t i = 0; i < array_count; ++i)
	{
		aason_token* token = aason_finalise_get_next_token(finaliser);

		children[i].key_offset = 0;
		children[i].key_len = 0;
		children[i].line = token->line;
		children[i].column = token->column;

		aason_finalise_parse_element(finaliser, self, token, &children[i], true);

		if (i < array_count - 1)
			aason_finalise_get_next_token(finaliser); // Skip comma token
	}

	aason_finalise_get_next_token(finaliser); // Skip leave array token
}

static void aason_finalise_parse_object(aason_finaliser* finaliser, aason_token* self, aason_element* element)
{
	element->type = aason_type_object;

	const uint32_t field_count = self->count;
	element->object_value.count = field_count;
	element->object_value.first_child = finaliser->element_index;

	aason_element* children = aason_finalise_allocate_elements(finaliser, field_count);

	for (uint32_t i = 0; i < field_count; ++i)
	{
		self = aason_finalise_get_next_token(finaliser);
		aason_finalise_get_next_token(finaliser); // Skip colon token
		*self->end = 0; // Null terminate key string

		children[i].key_offset = (uint32_t)(self->begin - finaliser->begin);
		children[i].key_len = (uint32_t)(self->end - self->begin);
		children[i].line = self->line;
		children[i].column = self->column;

		aason_token* token = aason_finalise_get_next_token(finaliser);
		aason_finalise_parse_element(finaliser, self, token, &children[i], false);

		if (i < field_count - 1)
			aason_finalise_get_next_token(finaliser); // Skip comma token
	}

	aason_finalise_get_next_token(finaliser); // Skip leave object token
}

static bool aason_finalise(aason_context* ctx, char* buffer, aason_tokens* tokens)
{
	aason_finaliser finaliser = {
		.ctx	= ctx,
		.begin	= buffer,
		.tokens	= tokens
	};

	if (!setjmp(finaliser.jmp_ctx))
	{
		aason_token* self = aason_finalise_get_next_token(&finaliser);
		aason_finalise_get_next_token(&finaliser); // Skip colon token
		*self->end = 0; // Null terminate key string
	
		// Allocate root element
		aason_element* element = aason_finalise_allocate_elements(&finaliser, 1);
		element->key_offset = (uint32_t)(self->begin - buffer);
		element->key_len = (uint32_t)(self->end - self->begin);
		element->line = self->line;
		element->column = self->column;

		aason_token* token = aason_finalise_get_next_token(&finaliser);
		aason_finalise_parse_element(&finaliser, self, token, element, false);
	
		aason_assert(finaliser.element_index == ctx->element_count);

		return true;
	}

	return false;
}