typedef enum
{
	aason_token_type_error,
	aason_token_type_eof,
	aason_token_type_str,
	aason_token_type_bin,
	aason_token_type_dec,
	aason_token_type_hex,
	aason_token_type_true,
	aason_token_type_false,
	aason_token_type_comma,
	aason_token_type_colon,
	aason_token_type_float,
	aason_token_type_identifier,
	aason_token_type_constructor,
	aason_token_type_open_paren,
	aason_token_type_close_paren,
	aason_token_type_enter_array,
	aason_token_type_leave_array,
	aason_token_type_enter_object,
	aason_token_type_leave_object,
	aason_token_type_jump,
	aason_token_type_count
} aason_token_type;

static const char* aason_token_type_strings[] = {
	"",
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
static_assert(sizeof(aason_token_type_strings) / sizeof(const char*) == aason_token_type_count);

enum
{
	aason_tokenise_char_error,
	aason_tokenise_char_eof,
	aason_tokenise_char_jump
};

// TODO: I think if we add a union we can remove some 16-bit limitations
typedef struct
{
	uint16_t			type;
	uint16_t			file_index;
	uint16_t			constructor_index;
	uint16_t			count;
	char*				begin;
	char*				end;
	//const char*			file; // TODO
	uint32_t			line;
	uint32_t			column;
} aason_token;
static_assert(sizeof(aason_token) == 32);

typedef struct
{
	aason_context*	ctx;
	//uint32_t		tab_size;
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

static void aason_tokenise_error(aason_tokeniser* tokeniser, aason_error_type error, const char* fmt, ...)
{
	char buffer[4096];

	tokeniser->ctx->error = error;
	tokeniser->ctx->error_file = tokeniser->file;
	tokeniser->ctx->error_line = tokeniser->line;
	tokeniser->ctx->error_column = tokeniser->column;

	if (tokeniser->ctx->error_interface.error)
	{
		va_list args;
		va_start(args, fmt);
		aason_format_string(tokeniser->ctx, buffer, sizeof(buffer), fmt, args);
		va_end(args);
	
		tokeniser->ctx->error_interface.error(
			tokeniser->ctx->error_interface.user_data,
			error,
			tokeniser->file,
			tokeniser->line,
			tokeniser->column,
			buffer
		);
	}
}

static int64_t aason_utf8_byte_count(char c)
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
	char c;

	if (tokeniser->next == tokeniser->end)
	{
		// Check for EOF before we start dereferencing invalid memory
		c = aason_tokenise_char_eof;
	}
	else if (tokeniser->next > tokeniser->end)
	{
		// This means invalid UTF-8 sequence
		aason_tokenise_error(tokeniser, aason_error_invalid_char, "Invalid UTF-8 character");
		c = aason_tokenise_char_error;
	}
	else
	{
		// These updates are delayed to make delegation of parsing to other functions easier
		tokeniser->current = tokeniser->next;
		tokeniser->line = tokeniser->next_line;
		tokeniser->column = tokeniser->next_column;
	
		// This should handle simple UTF-8 but we need a way to determine non-printable characters
		c = *tokeniser->current;
		const int64_t byte_count = aason_utf8_byte_count(c);
		tokeniser->next += byte_count;
	
		if (c < 32)
		{
			if (c == '\t')
			{
				const uint32_t tab_size = tokeniser->ctx->tab_size - ((tokeniser->next_column - 1) % tokeniser->ctx->tab_size);
				tokeniser->next_column += tab_size;
			}
			else if (c == '\r')
			{
				// Ignore
			}
			else if (c == '\n')
			{
				++tokeniser->next_line;
				tokeniser->next_column = 1;
			}
			else if (c == aason_tokenise_char_jump)
			{
				c = aason_tokenise_char_jump;
			}
			else
			{
				// Invalid control character
				aason_tokenise_error(tokeniser, aason_error_invalid_char, "Invalid control character '{i32}'", c);
				c = aason_tokenise_char_error;
			}
		}
		else if (c == 127)
		{
			// Delete character
			aason_tokenise_error(tokeniser, aason_error_invalid_char, "Invalid delete character");
			c = aason_tokenise_char_error;
		}
		else
		{
			++tokeniser->next_column;
		}
	}

	tokeniser->c = c;

	return c;
}

static char aason_tokenise_skip_comments(aason_tokeniser* tokeniser)
{
	char c = aason_tokenise_get_char(tokeniser);
	if (c == '/')
	{
		// Skip past the end of the line for C++ comments
		do
		{
			c = aason_tokenise_get_char(tokeniser);
		} while (c != '\n'); // TODO: We need to check for aason_tokenise_char_error

		// Consume final '\n'
		c = aason_tokenise_get_char(tokeniser);
	}
	else if (c == '*')
	{
		char prev = 0;
		int64_t depth = 1;

		for (;;)
		{
			c = aason_tokenise_get_char(tokeniser);
			if (prev == '*' && c == '/')
			{
				if (--depth == 0)
				{
					// Consume final /
					c = aason_tokenise_get_char(tokeniser);
					break;
				}

				prev = 0; // Prevent misinterpreting string "*/*" as "*/" and "/*"
			}
			else if (c == '/' && prev == '*')
			{
				++depth;
				prev = 0; // Prevent misinterpreting string "/*/" as "/*" and "*/"
			}
			else if (c == aason_tokenise_char_eof)
			{
				aason_tokenise_error(tokeniser, aason_error_invalid_char, "Unexpected end-of-file");
				c = aason_tokenise_char_error;
				break;
			}
			else if (c == aason_tokenise_char_error)
			{
				break;
			}

			prev = c;
		}
	}
	else
	{
		aason_tokenise_error(tokeniser, aason_error_invalid_char, "Expected '/' or '*' for comment");
		c = aason_tokenise_char_error;
	}

	return c;
}

static char aason_tokenise_skip_whitespace(aason_tokeniser* tokeniser)
{
	char c = tokeniser->c;

	for (;;)
	{
		switch (c)
		{
		case ' ':
		case '\r':
		case '\t':
		case '\n':
			c = aason_tokenise_get_char(tokeniser);
			break;
		case '/':
			c = aason_tokenise_skip_comments(tokeniser);
			break;
		default:
			return c;
		}
	}

	//rgs_unreachable();
}

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
		else if (c == aason_tokenise_char_eof)
		{
			aason_tokenise_error(tokeniser, aason_error_invalid_char, "Unexpected end-of-file");
			return aason_token_type_error;
		}
		else if (c == aason_tokenise_char_error)
		{
			return aason_token_type_error;
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
			{
				aason_tokenise_error(tokeniser, aason_error_invalid_char, "Floating point numbers may only contain a single '.' character");
				return aason_token_type_error;
			}

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

typedef struct
{
	aason_token*	tokens;
	uint32_t		count;
	uint32_t		capacity;
} aason_tokens;

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

static void aason_tokenise_file(aason_context* ctx, aason_allocator* scratch, aason_tokens* tokens, uint32_t file_index)
{
	aason_assert(file_index < ctx->file_count);

	aason_file* file = &ctx->files[file_index];
	char* buffer = file->buffer + file->offset;

	aason_tokeniser tokeniser = {
		.ctx			= ctx,
		.begin			= buffer,
		.end			= buffer + file->size,
		.current		= buffer,
		.next			= buffer,
		.line			= 1,
		.column			= 1,
		.next_line		= 1,
		.next_column	= 1
	};

	aason_tokenise_get_char(&tokeniser);

	for (;;)
	{
		char c = aason_tokenise_skip_whitespace(&tokeniser);
		if (c == '/')
			c = aason_tokenise_skip_comments(&tokeniser);

		aason_token* token = aason_allocate_token(scratch, tokens);
		token->file_index = file_index;
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
		else if (c == aason_tokenise_char_jump)
		{
			token->type = aason_token_type_jump;
			uint32_t file_index;
			memcpy(&file_index, tokeniser.current + 1, 4);
			token->count = file_index; // TODO: Don't re-use this variable
			tokeniser.next += 5;
			tokeniser.current = tokeniser.next;
			aason_tokenise_get_char(&tokeniser);
		}
		else if (c == aason_tokenise_char_eof)
		{
			token->type = aason_token_type_eof;
			break;
		}
		else if (c == aason_tokenise_char_error)
		{
			break;
		}
		else
		{
			aason_tokenise_error(&tokeniser, aason_error_invalid_char, "Invalid token");
			break;
		}

		if (token->type == aason_token_type_error)
			break;

		token->end = tokeniser.current;
	}
}

static aason_tokens aason_tokenise(aason_context* ctx, aason_allocator* scratch)
{
	aason_tokens tokens = {};

	for (uint32_t i = 0; i < ctx->file_count; ++i)
	{
		ctx->files[i].token_index = tokens.count;

		aason_tokenise_file(ctx, scratch, &tokens, i);
		if (ctx->error != aason_error_none)
			break;

		//ctx->files[i].token_count = tokens.count - ctx->files[i].token_index;
	}

	return tokens;
}