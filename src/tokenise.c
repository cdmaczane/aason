typedef struct
{
	aason_context*	ctx;
	char			c;
	char*			begin;
	char*			end;
	char*			current;
	char*			next;
	const char*		file;
	uint32_t		line;
	uint32_t		column;
	uint32_t		next_line;
	uint32_t		next_column;
	uint16_t		file_index;
	jmp_buf*		jmp_ctx;
} aason_tokeniser;

static bool aason_tokenise_is_valid_bin_char(char c)
{
	if (c == '0' || c == '1')
		return true;

	return false;
}

static bool aason_tokenise_is_valid_dec_char(char c)
{
	if ((c >= '0' && c <= '9') || c == '-')
		return true;

	return false;
}

static bool aason_tokenise_is_valid_hex_char(char c)
{
	if (c >= '0' && c <= '9')
		return true;
	else if (c >= 'A' && c <= 'F')
		return true;
	else if (c >= 'a' && c <= 'f')
		return true;

	return false;
}

static bool aason_tokenise_is_valid_identifier_first_char(char c)
{
	if (c >= 'a' && c <= 'z')
		return true;
	else if (c >= 'A' && c <= 'Z')
		return true;
	else if (c == '_')
		return true;

	return false;
}

static bool aason_tokenise_is_valid_identifier_char(char c)
{
	if (aason_tokenise_is_valid_identifier_first_char(c))
		return true;
	else if (c >= '0' && c <= '9')
		return true;

	return false;
}

static void aason_tokenise_error(aason_tokeniser* tokeniser, aason_error_type error, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	aason_report_error(
		tokeniser->ctx,
		error,
		tokeniser->file,
		tokeniser->line,
		tokeniser->column,
		format,
		args
	);
	va_end(args);

	longjmp(*tokeniser->jmp_ctx, 1);
}

// TODO: Move
static uint32_t aason_utf8_byte_count(char c)
{
	if ((c & 0b11110000) == 0b11110000)
		return 4;
	else if ((c & 0b11100000) == 0b11100000)
		return 3;
	else if ((c & 0b11000000) == 0b11000000)
		return 2;
	else
		return 1;
}

static char aason_tokenise_get_char(aason_tokeniser* tokeniser)
{
	for (;;)
	{
		// These updates are delayed to make delegation of parsing to other functions easier
		tokeniser->current = tokeniser->next;
		tokeniser->line = tokeniser->next_line;
		tokeniser->column = tokeniser->next_column;

		const char c = *tokeniser->next;

		if (c == aason_char_op_skip_line_short)
		{
			tokeniser->next += (uint8_t)tokeniser->next[1];
			tokeniser->c = ' ';

			return tokeniser->c;
		}
		else if (c == aason_char_op_skip_line_long)
		{
			uint32_t byte_count;
			memcpy(&byte_count, tokeniser->next + 1, 4);

			tokeniser->next += byte_count;
			tokeniser->c = ' ';

			return tokeniser->c;
		}
		else if (c == aason_char_op_skip_columns_short)
		{
			tokeniser->next_column += (uint8_t)tokeniser->next[2];
			tokeniser->next += (uint8_t)tokeniser->next[1];
			tokeniser->c = ' ';

			return tokeniser->c;
		}
		else if (c == aason_char_op_skip_columns_long)
		{
			uint32_t byte_count;
			memcpy(&byte_count, tokeniser->next + 1, 4);

			uint32_t column_count;
			memcpy(&column_count, tokeniser->next + 5, 4);

			tokeniser->next_column += column_count;
			tokeniser->next += byte_count;
			tokeniser->c = ' ';

			return tokeniser->c;
		}
		else if (c == 0)
		{
			tokeniser->c = aason_char_op_eof;
			return tokeniser->c;
		}

		tokeniser->next += aason_utf8_byte_count(c);

		if (c == '\t')
		{
			const uint32_t tab_size = tokeniser->ctx->tab_size;
			tokeniser->next_column += tab_size - ((tokeniser->next_column - 1) % tab_size);
		}
		else if (c == '\n')
		{
			++tokeniser->next_line;
			tokeniser->next_column = 1;
		}
		else if (c == '\r')
		{
			// Ignore
		}
		else if (c == aason_char_op_jump)
		{
			// ???
		}
		else
		{
			++tokeniser->next_column;
		}

		tokeniser->c = c;
		return c;
	}
}

static char aason_tokenise_skip_whitespace(aason_tokeniser* tokeniser)
{
	char c = tokeniser->c;

	while (c == ' ' || c == '\t' || c == '\n' || c == '\r')
		c = aason_tokenise_get_char(tokeniser);

	return c;
}

/*
	TODO:
	* Preparse has already validated the string and knows how long it is.
	* It could add a new char op type that designates the end of the string.
	* This would allow for a tiny loop checking for a single value.
*/
static aason_token_type aason_tokenise_parse_str(aason_tokeniser* tokeniser)
{
	bool inside_escape = false;
	for (;;)
	{
		const char c = aason_tokenise_get_char(tokeniser);

		if (c == '\\')
		{
			// We don't care about escape sequences except '\"'
			inside_escape = !inside_escape;
		}
		else if (c == '"' && !inside_escape)
		{
			break;
		}
		else if (c == aason_char_op_eof)
		{
			aason_tokenise_error(tokeniser, aason_error_invalid_char, "Unexpected end-of-file");
		}
		else
		{
			inside_escape = false;
		}
	}

	// Swallow final " character
	aason_tokenise_get_char(tokeniser);

	return aason_token_type_str;
}

static aason_token_type aason_tokenise_parse_bin(aason_tokeniser* tokeniser)
{
	for (;;)
	{
		if (!aason_tokenise_is_valid_bin_char(aason_tokenise_get_char(tokeniser)))
			break;
	}

	return aason_token_type_bin;
}

static aason_token_type aason_tokenise_parse_hex(aason_tokeniser* tokeniser)
{
	for (;;)
	{
		if (!aason_tokenise_is_valid_hex_char(aason_tokenise_get_char(tokeniser)))
			break;
	}

	return aason_token_type_hex;
}

static aason_token_type aason_tokenise_parse_number(aason_tokeniser* tokeniser)
{
	char c = tokeniser->c;

	if (c == '0')
	{
		c = aason_tokenise_get_char(tokeniser);

		if (c == 'x')
			return aason_tokenise_parse_hex(tokeniser);
		else if (c == 'b')
			return aason_tokenise_parse_bin(tokeniser);
	}

	aason_token_type type = aason_token_type_dec;
	for (;;)
	{
		if (c == '.')
		{
			if (type == aason_token_type_float)
				aason_tokenise_error(tokeniser, aason_error_invalid_char, "Floating point numbers may only contain a single '.' character");

			type = aason_token_type_float;
		}
		else if (!aason_tokenise_is_valid_dec_char(c))
		{
			break;
		}

		c = aason_tokenise_get_char(tokeniser);
	}

	return type;
}

static aason_token_type aason_tokenise_parse_identifier(aason_tokeniser* tokeniser)
{
	const char* begin = tokeniser->current;
	for (;;)
	{
		const char c = aason_tokenise_get_char(tokeniser);
		if (!aason_tokenise_is_valid_identifier_char(c))
		{
			const int64_t len = tokeniser->current - begin;
			if (c == '(')
			{
				return aason_token_type_constructor;
			}
			else if (len == 4)
			{
				if (begin[0] == 't' && begin[1] == 'r' && begin[2] == 'u' && begin[3] == 'e')
					return aason_token_type_true;
			}
			else if (len == 5)
			{
				if (begin[0] == 'f' && begin[1] == 'a' && begin[2] == 'l' && begin[3] == 's' && begin[4] == 'e')
					return aason_token_type_false;
			}

			return aason_token_type_identifier;
		}
	}

	//rgs_unreachable();
}

static aason_token* aason_allocate_token(aason_allocator* scratch, aason_tokens* tokens)
{
	if (tokens->count == tokens->capacity)
	{
		if (tokens->count)
			tokens->capacity <<= 1;
		else
			tokens->capacity = 64;

		tokens->tokens = aason_realloc(scratch, tokens->tokens, sizeof(aason_token) * tokens->capacity);
	}

	return &tokens->tokens[tokens->count++];
}

static bool aason_tokenise_file(aason_context* ctx, aason_allocator* scratch, aason_tokens* tokens, uint32_t file_index)
{
	char* file_data = aason_get_file_data(ctx, file_index);
	const size_t file_size = aason_get_file_size(ctx, file_index);

	jmp_buf jmp_ctx;

	aason_tokeniser tokeniser = {
		.ctx			= ctx,
		.begin			= file_data,
		.end			= file_data + file_size,
		.current		= file_data,
		.next			= file_data,
		.line			= 1,
		.column			= 1,
		.next_line		= 1,
		.next_column	= 1,
		.jmp_ctx		= &jmp_ctx
	};

	if (setjmp(jmp_ctx))
		return false;

	aason_tokenise_get_char(&tokeniser);
	for (;;)
	{
		const char c = aason_tokenise_skip_whitespace(&tokeniser);

		aason_token* token = aason_allocate_token(scratch, tokens);
		token->file_index = (uint16_t)file_index;
		token->count = 0;
		token->begin = tokeniser.current;
		token->line = tokeniser.line;
		token->column = tokeniser.column;

		if (c == ':')
		{
			token->type = aason_token_type_colon;
			aason_tokenise_get_char(&tokeniser);
		}
		else if (c == ',')
		{
			token->type = aason_token_type_comma;
			aason_tokenise_get_char(&tokeniser);
		}
		else if (c == '[')
		{
			token->type = aason_token_type_enter_array;
			aason_tokenise_get_char(&tokeniser);
		}
		else if (c == ']')
		{
			token->type = aason_token_type_leave_array;
			aason_tokenise_get_char(&tokeniser);
		}
		else if (c == '{')
		{
			token->type = aason_token_type_enter_object;
			aason_tokenise_get_char(&tokeniser);
		}
		else if (c == '}')
		{
			token->type = aason_token_type_leave_object;
			aason_tokenise_get_char(&tokeniser);
		}
		else if (c == '(')
		{
			token->type = aason_token_type_open_paren;
			aason_tokenise_get_char(&tokeniser);
		}
		else if (c == ')')
		{
			token->type = aason_token_type_close_paren;
			aason_tokenise_get_char(&tokeniser);
		}
		else if (c == '"')
		{
			token->type = aason_tokenise_parse_str(&tokeniser);
		}
		else if (aason_tokenise_is_valid_dec_char(c))
		{
			token->type = aason_tokenise_parse_number(&tokeniser);
		}
		else if (aason_tokenise_is_valid_identifier_first_char(c))
		{
			token->type = aason_tokenise_parse_identifier(&tokeniser);
		}
		else if (c == aason_char_op_jump)
		{
			token->type = aason_token_type_jump;

			uint32_t jump_index;
			memcpy(&jump_index, tokeniser.current + 1, 4);
			token->count = (uint16_t)jump_index; // TODO: Don't re-use this variable

			tokeniser.next += 4;
			tokeniser.next_column += 4;

			aason_tokenise_get_char(&tokeniser);
		}
		else if (c == aason_char_op_eof)
		{
			token->type = aason_token_type_eof;
			break;
		}
		else
		{
			aason_tokenise_error(&tokeniser, aason_error_invalid_char, "Invalid token");
		}

		token->end = tokeniser.current;
	}

	return true;
}

static aason_tokens aason_run_tokenise_pass(aason_context* ctx, aason_allocator* scratch)
{
	aason_tokens tokens = {};

	for (uint32_t i = 0; i < ctx->file_count; ++i)
	{
		ctx->files[i].token_index = tokens.count;

		if (!aason_tokenise_file(ctx, scratch, &tokens, i))
			break;
	}

	return tokens;
}