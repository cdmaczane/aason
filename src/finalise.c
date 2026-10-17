typedef struct
{
	aason_token_state	token_state;
	uint32_t			element_index;
	jmp_buf				jmp_ctx;
} aason_finaliser;

// TODO: Share code using a common error function shared between modules
static void aason_finalise_error(aason_finaliser* finaliser, aason_error_type error, const aason_token* token, const char* fmt, ...)
{
	char buffer[4096];

	aason_assert(token->file_index < finaliser->token_state.ctx->file_count);
	aason_file* file = &finaliser->token_state.ctx->files[token->file_index];

	finaliser->token_state.ctx->error = error;

	// TODO: Get this data from token instead of finaliser
	finaliser->token_state.ctx->error_file = file->buffer;
	finaliser->token_state.ctx->error_line = token->line;
	finaliser->token_state.ctx->error_column = token->column;

	if (finaliser->token_state.ctx->error_interface.error)
	{
		va_list args;
		va_start(args, fmt);
		vsnprintf(buffer, sizeof(buffer), fmt, args);
		va_end(args);
	
		finaliser->token_state.ctx->error_interface.error(
			finaliser->token_state.ctx->error_interface.user_data,
			error,
			file->buffer,
			token->line,
			token->column,
			buffer
		);
	}

	longjmp(finaliser->jmp_ctx, 1);
}

static aason_element* aason_finalise_allocate_elements(aason_finaliser* finaliser, uint32_t count)
{
	aason_assert(finaliser->element_index + count <= finaliser->token_state.ctx->element_count);

	aason_element* alloc = &finaliser->token_state.ctx->elements[finaliser->element_index];
	finaliser->element_index += count;

	return alloc;
}

static void aason_finalise_parse_str(aason_finaliser* finaliser, aason_token* token, aason_element* element)
{
	aason_file* file = &finaliser->token_state.ctx->files[token->file_index];
	char* buffer = file->buffer + file->offset;

	element->type = aason_type_str;
	element->str_value.offset = (uint32_t)(token->begin - buffer) + 1;
	element->str_value.len = (uint32_t)(token->end - token->begin) - 1;

	// Null terminate string
	token->end[-1] = 0;
}

static void aason_finalise_parse_bin(aason_finaliser* finaliser, aason_token* token, aason_element* element)
{
	element->type = aason_type_int;

	if (!aason_from_string_bin(token->begin, token->end, &element->int_value))
		aason_finalise_error(finaliser, aason_error_out_of_range, token, "Binary integer too large");
}

static void aason_finalise_parse_dec(aason_finaliser* finaliser, aason_token* token, aason_element* element)
{
	element->type = aason_type_int;

	if (!aason_from_string_dec(token->begin, token->end, &element->int_value))
		aason_finalise_error(finaliser, aason_error_out_of_range, token, "Decimal integer too large");
}

static void aason_finalise_parse_hex(aason_finaliser* finaliser, aason_token* token, aason_element* element)
{
	element->type = aason_type_int;

	if (!aason_from_string_hex(token->begin, token->end, &element->int_value))
		aason_finalise_error(finaliser, aason_error_out_of_range, token, "Hexadecimal integer too large");
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

static void aason_finalise_parse_float(aason_finaliser* finaliser, aason_token* token, aason_element* element)
{
	element->type = aason_type_float;

	if (!aason_from_string_float(finaliser->token_state.ctx->locale, token->begin, token->end, &element->float_value))
		aason_finalise_error(finaliser, aason_error_out_of_range, token, "Floating point number out of range");
}

static void aason_finalise_parse_enum(aason_finaliser* finaliser, aason_token* token, aason_element* element)
{
	aason_file* file = &finaliser->token_state.ctx->files[token->file_index];
	char* buffer = file->buffer + file->offset;

	element->type = aason_type_enum;
	element->enum_value.offset = (uint32_t)(token->begin - buffer);
	element->enum_value.len = (uint32_t)(token->end - token->begin);

	// Null terminate string
	*token->end = 0;
}

// Forward declared as these functions call each other
// TODO: Try using an iterative approach
static void aason_finalise_parse_value(aason_finaliser* finaliser, aason_token* token, aason_element* element);
static void aason_finalise_parse_array(aason_finaliser* finaliser, aason_token* token, aason_element* element);
static void aason_finalise_parse_object(aason_finaliser* finaliser, aason_token* token, aason_element* element);
static void aason_finalise_parse_constructor(aason_finaliser* finaliser, aason_token* token, aason_element* element);

static void aason_finalise_parse_value(aason_finaliser* finaliser, aason_token* token, aason_element* element)
{
	element->line = token->line;
	element->column = token->column;
	element->value_file_index = token->file_index;

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
	case aason_token_type_true:
		aason_finalise_parse_true(finaliser, token, element);
		break;
	case aason_token_type_false:
		aason_finalise_parse_false(finaliser, token, element);
		break;
	case aason_token_type_float:
		aason_finalise_parse_float(finaliser, token, element);
		break;
	case aason_token_type_identifier:
		aason_finalise_parse_enum(finaliser, token, element);
		break;
	case aason_token_type_enter_array:
		aason_finalise_parse_array(finaliser, token, element);
		break;
	case aason_token_type_enter_object:
		aason_finalise_parse_object(finaliser, token, element);
		break;
	case aason_token_type_constructor:
		aason_finalise_parse_constructor(finaliser, token, element);
		break;
	default:
		aason_assert(false);
		break;
	}
}

static void aason_finalise_parse_array(aason_finaliser* finaliser, aason_token* token, aason_element* element)
{
	element->type = aason_type_array;

	const uint32_t array_count = token->count;
	element->array_value.count = array_count;
	element->array_value.first_child = finaliser->element_index;

	aason_element* children = aason_finalise_allocate_elements(finaliser, array_count);

	for (uint32_t i = 0; i < array_count; ++i)
	{
		token = aason_get_next_token(&finaliser->token_state);

		children[i].key_file_index = token->file_index;
		children[i].key_len = 0;
		children[i].key_offset = 0;

		aason_finalise_parse_value(finaliser, token, &children[i]);

		token = aason_get_next_token(&finaliser->token_state); // Skip comma token
	}

	// In the case of no trailing comma, this will be a leave array token
	if (token->type != aason_token_type_leave_array)
		aason_get_next_token(&finaliser->token_state); // Skip leave object token
}

static void aason_finalise_init_key(aason_finaliser* finaliser, aason_element* element)
{
	aason_token* token = aason_get_next_token(&finaliser->token_state);
	aason_get_next_token(&finaliser->token_state); // Skip colon token

	char* begin = token->begin;
	char* end = token->end;
	if (finaliser->token_state.ctx->mode == aason_mode_json)
	{
		// Trim quotes
		++begin;
		--end;
	}

	aason_assert(token->file_index < finaliser->token_state.ctx->file_count);
	aason_file* file = &finaliser->token_state.ctx->files[token->file_index];
	char* file_buffer = file->buffer + file->offset;
	*end = 0; // Null terminate key string

	element->key_file_index = token->file_index;
	element->key_len = (uint32_t)(end - begin);
	element->key_offset = (uint32_t)(begin - file_buffer);
}

static void aason_finalise_parse_object(aason_finaliser* finaliser, aason_token* token, aason_element* element)
{
	element->type = aason_type_object;

	const uint32_t field_count = token->count;
	element->object_value.count = field_count;
	element->object_value.first_child = finaliser->element_index;

	aason_element* children = aason_finalise_allocate_elements(finaliser, field_count);

	for (uint32_t i = 0; i < field_count; ++i)
	{
		aason_finalise_init_key(finaliser, &children[i]);
		token = aason_get_next_token(&finaliser->token_state);
		aason_finalise_parse_value(finaliser, token, &children[i]);

		token = aason_get_next_token(&finaliser->token_state); // Skip comma token
	}

	// In the case of no trailing comma, this will be a leave object token
	if (token->type != aason_token_type_leave_object)
		aason_get_next_token(&finaliser->token_state); // Skip leave object token
}

static void aason_finalise_parse_constructor(aason_finaliser* finaliser, aason_token* token, aason_element* element)
{
	aason_assert(token->constructor_index < finaliser->token_state.ctx->constructors->count);

	element->type = aason_type_constructor;
	element->constructor_index = token->constructor_index;

	const uint32_t arg_count = token->count;
	element->constructor_value.count = arg_count;
	element->constructor_value.first_child = finaliser->element_index;

	aason_get_next_token(&finaliser->token_state); // Skip open paren

	aason_element* children = aason_finalise_allocate_elements(finaliser, arg_count);

	for (uint32_t i = 0; i < arg_count; ++i)
	{
		token = aason_get_next_token(&finaliser->token_state);
		aason_finalise_parse_value(finaliser, token, &children[i]);

		if (i < arg_count - 1)
			aason_get_next_token(&finaliser->token_state); // Skip comma token
	}

	aason_get_next_token(&finaliser->token_state); // Skip close paren
}

static bool aason_run_finalise_pass(aason_context* ctx, aason_tokens* tokens)
{
	aason_finaliser finaliser = {
		.token_state = {
			.ctx	= ctx,
			.tokens	= tokens
		}
	};

	if (!setjmp(finaliser.jmp_ctx))
	{
		aason_element* element = aason_finalise_allocate_elements(&finaliser, 1);

		if (ctx->mode != aason_mode_json)
			aason_finalise_init_key(&finaliser, element);

		aason_token* token = aason_get_next_token(&finaliser.token_state);
		aason_finalise_parse_value(&finaliser, token, element);

		aason_assert(finaliser.element_index == ctx->element_count);

		return true;
	}

	return false;
}