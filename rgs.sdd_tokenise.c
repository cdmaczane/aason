typedef enum
{
	sdd_token_type_error,
	sdd_token_type_eof,
	sdd_token_type_str,
	sdd_token_type_bin,
	sdd_token_type_dec,
	sdd_token_type_hex,
	sdd_token_type_hash,
	sdd_token_type_true,
	sdd_token_type_false,
	sdd_token_type_comma,
	sdd_token_type_colon,
	sdd_token_type_float,
	sdd_token_type_hash_str,
	sdd_token_type_identifier,
	sdd_token_type_enter_array,
	sdd_token_type_leave_array,
	sdd_token_type_enter_object,
	sdd_token_type_leave_object
} sdd_token_type;

static const char* sdd_token_type_strings[] = {
	"",
	"EOF",
	"string",
	"binary number",
	"decimal number",
	"hexadecimal number",
	"#",
	"true",
	"false",
	",",
	":",
	"floating point number",
	"hashed string",
	"identifier",
	"[",
	"]",
	"{",
	"}"
};

enum
{
	sdd_tokenise_char_error,
	sdd_tokenise_char_eof
};

typedef struct
{
	sdd_token_type	type;
	uint32_t		count;
	char*			begin;
	char*			end;
	uint32_t		line;
	uint32_t		column;
} sdd_token;
static_assert(sizeof(sdd_token) == 32);

typedef struct
{
	rgs_sdd*	sdd;
	uint32_t	tab_size;
	char		c;
	char*		begin;
	char*		end;
	char*		current;
	char*		next;
	uint32_t	line;
	uint32_t	column;
	uint32_t	next_line;
	uint32_t	next_column;
} sdd_tokenise_ctx;

static bool sdd_tokenise_is_valid_bin_char(char c)
{
	if (c == '0' || c == '1')
		return true;

	return false;
}

static bool sdd_tokenise_is_valid_dec_char(char c)
{
	if ((c >= '0' && c <= '9') || c == '-')
		return true;

	return false;
}

static bool sdd_tokenise_is_valid_hex_char(char c)
{
	if (c >= '0' && c <= '9')
		return true;
	else if (c >= 'A' && c <= 'F')
		return true;
	else if (c >= 'a' && c <= 'f')
		return true;

	return false;
}

static bool sdd_tokenise_is_valid_identifier_first_char(char c)
{
	if (c >= 'a' && c <= 'z')
		return true;
	else if (c >= 'A' && c <= 'Z')
		return true;
	else if (c == '_')
		return true;

	return false;
}

static bool sdd_tokenise_is_valid_identifier_char(char c)
{
	if (sdd_tokenise_is_valid_identifier_first_char(c))
		return true;
	else if (c >= '0' && c <= '9')
		return true;

	return false;
}

static void sdd_tokenise_error(sdd_tokenise_ctx* ctx, rgs_sdd_error error, const char* fmt, ...)
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
}

static char sdd_tokenise_get_char(sdd_tokenise_ctx* ctx)
{
	char c;

	if (ctx->next == ctx->end)
	{
		// Check for EOF before we start dereferencing invalid memory
		c = sdd_tokenise_char_eof;
	}
	else if (ctx->next > ctx->end)
	{
		// This means invalid UTF-8 sequence
		sdd_tokenise_error(ctx, rgs_sdd_error_invalid_char, "Invalid UTF-8 character");
		c = sdd_tokenise_char_error;
	}
	else
	{
		// These updates are delayed to make delegation of parsing to other functions easier
		ctx->current =	ctx->next;
		ctx->line = ctx->next_line;
		ctx->column = ctx->next_column;
	
		// This should handle simple UTF-8 but we need a way to determine non-printable characters
		c = *ctx->current;
		const int64_t byte_count = rgs_utf8_byte_count(c);
		ctx->next += byte_count;
	
		if (c < 32)
		{
			if (c == '\t')
			{
				// TODO: Test this
				const uint32_t tab_size = ctx->tab_size - ((ctx->next_column - 1) % ctx->tab_size);
				ctx->next_column += tab_size;
			}
			else if (c == '\r')
			{
				// Ignore
			}
			else if (c == '\n')
			{
				++ctx->next_line;
				ctx->next_column = 1;
			}
			else
			{
				// Invalid control character
				sdd_tokenise_error(ctx, rgs_sdd_error_invalid_char, "Invalid control character '{i32}'", c);
				c = sdd_tokenise_char_error;
			}
		}
		else if (c == 127)
		{
			// Delete character
			sdd_tokenise_error(ctx, rgs_sdd_error_invalid_char, "Invalid delete character");
			c = sdd_tokenise_char_error;
		}
		else
		{
			++ctx->next_column;
		}
	}

	ctx->c = c;

	return c;
}

static char sdd_tokenise_skip_comments(sdd_tokenise_ctx* ctx)
{
	char c = sdd_tokenise_get_char(ctx);
	if (c == '/')
	{
		// Skip past the end of the line for C++ comments
		do
		{
			c = sdd_tokenise_get_char(ctx);
		} while (c != '\n'); // TODO: We need to check for sdd_tokenise_char_error

		// Consume final '\n'
		c = sdd_tokenise_get_char(ctx);
	}
	else if (c == '*')
	{
		char prev = 0;
		int64_t depth = 1;

		for (;;)
		{
			c = sdd_tokenise_get_char(ctx);
			if (prev == '*' && c == '/')
			{
				if (--depth == 0)
				{
					// Consume final /
					c = sdd_tokenise_get_char(ctx);
					break;
				}

				prev = 0; // Prevent misinterpreting string "*/*" as "*/" and "/*"
			}
			else if (c == '/' && prev == '*')
			{
				++depth;
				prev = 0; // Prevent misinterpreting string "/*/" as "/*" and "*/"
			}
			else if (c == sdd_tokenise_char_eof)
			{
				sdd_tokenise_error(ctx, rgs_sdd_error_invalid_char, "Unexpected end-of-file");
				c = sdd_tokenise_char_error;
				break;
			}
			else if (c == sdd_tokenise_char_error)
			{
				break;
			}

			prev = c;
		}
	}
	else
	{
		sdd_tokenise_error(ctx, rgs_sdd_error_invalid_char, "Expected '/' or '*' for comment");
		c = sdd_tokenise_char_error;
	}

	return c;
}

static char sdd_tokenise_skip_whitespace(sdd_tokenise_ctx* ctx)
{
	char c = ctx->c;

	for (;;)
	{
		switch (c)
		{
		case ' ':
		case '\r':
		case '\t':
		case '\n':
			c = sdd_tokenise_get_char(ctx);
			break;
		case '/':
			c = sdd_tokenise_skip_comments(ctx);
			break;
		default:
			return c;
		}
	}

	rgs_unreachable();
}

static sdd_token_type sdd_tokenise_parse_str(sdd_tokenise_ctx* ctx)
{
	bool inside_escape = false;
	for (;;)
	{
		const char c = sdd_tokenise_get_char(ctx);

		if (c == '\\')
		{
			// We don't care about escape sequences except '\"'
			inside_escape = !inside_escape;
		}
		else if (c == '"' && !inside_escape)
		{
			break;
		}
		else if (c == sdd_tokenise_char_eof)
		{
			sdd_tokenise_error(ctx, rgs_sdd_error_invalid_char, "Unexpected end-of-file");
			return sdd_token_type_error;
		}
		else if (c == sdd_tokenise_char_error)
		{
			return sdd_token_type_error;
		}
		else
		{
			inside_escape = false;
		}
	}

	// Swallow final " character
	sdd_tokenise_get_char(ctx);

	return sdd_token_type_str;
}

static sdd_token_type sdd_tokenise_parse_hash(sdd_tokenise_ctx* ctx)
{
	char c = sdd_tokenise_get_char(ctx);

	if (c == '"')
	{
		const sdd_token_type type = sdd_tokenise_parse_str(ctx);
		if (type == sdd_token_type_str)
			return sdd_token_type_hash_str;
		else
			return type;
	}

	for (int i = 0; i < 8; ++i)
	{
		if (!sdd_tokenise_is_valid_hex_char(c))
		{
			sdd_tokenise_error(ctx, rgs_sdd_error_invalid_char, "Character '{c}' is not a valid hexadecimal character");
			return sdd_token_type_error;
		}

		c = sdd_tokenise_get_char(ctx);
	}

	return sdd_token_type_hash;
}

static sdd_token_type sdd_tokenise_parse_bin(sdd_tokenise_ctx* ctx)
{
	for (;;)
	{
		if (!sdd_tokenise_is_valid_bin_char(sdd_tokenise_get_char(ctx)))
			break;
	}

	return sdd_token_type_bin;
}

static sdd_token_type sdd_tokenise_parse_hex(sdd_tokenise_ctx* ctx)
{
	for (;;)
	{
		if (!sdd_tokenise_is_valid_hex_char(sdd_tokenise_get_char(ctx)))
			break;
	}

	return sdd_token_type_hex;
}

static sdd_token_type sdd_tokenise_parse_number(sdd_tokenise_ctx* ctx)
{
	char c = ctx->c;

	if (c == '0')
	{
		c = sdd_tokenise_get_char(ctx);

		if (c == 'x')
			return sdd_tokenise_parse_hex(ctx);
		else if (c == 'b')
			return sdd_tokenise_parse_bin(ctx);
	}

	sdd_token_type type = sdd_token_type_dec;
	for (;;)
	{
		if (c == '.')
		{
			if (type == sdd_token_type_float)
			{
				sdd_tokenise_error(ctx, rgs_sdd_error_invalid_char, "Floating point numbers may only contain a single '.' character");
				return sdd_token_type_error;
			}

			type = sdd_token_type_float;
		}
		else if (!sdd_tokenise_is_valid_dec_char(c))
		{
			break;
		}

		c = sdd_tokenise_get_char(ctx);
	}

	return type;
}

static sdd_token_type sdd_tokenise_parse_identifier(sdd_tokenise_ctx* ctx)
{
	const char* begin = ctx->current;
	for (;;)
	{
		const char c = sdd_tokenise_get_char(ctx);
		if (!sdd_tokenise_is_valid_identifier_char(c))
		{
			const int64_t len = ctx->current - begin;
			if (len == 4)
			{
				if (begin[0] == 't' && begin[1] == 'r' && begin[2] == 'u' && begin[3] == 'e')
					return sdd_token_type_true;
			}
			else if (len == 5)
			{
				if (begin[0] == 'f' && begin[1] == 'a' && begin[2] == 'l' && begin[3] == 's' && begin[4] == 'e')
					return sdd_token_type_false;
			}

			return sdd_token_type_identifier;
		}
	}

	rgs_unreachable();
}

static sdd_token* sdd_tokenise(rgs_sdd* sdd, char* buffer, int64_t size, uint32_t tab_size)
{
	sdd_tokenise_ctx ctx = {
		.sdd			= sdd,
		.tab_size		= tab_size,
		.begin			= buffer,
		.end			= buffer + size,
		.current		= buffer,
		.next			= buffer,
		.line			= 1,
		.column			= 1,
		.next_line		= 1,
		.next_column	= 1
	};

	sdd_token* tokens = nullptr;
	sdd_tokenise_get_char(&ctx);

	for (;;)
	{
		char c = sdd_tokenise_skip_whitespace(&ctx);
		if (c == '/')
			c = sdd_tokenise_skip_comments(&ctx);

		sdd_token* token = rgs_scratch_array_emplace(tokens);
		token->count = 0;
		token->begin = ctx.current;
		token->line = ctx.line;
		token->column = ctx.column;

		if (c == ':')
		{
			token->type = sdd_token_type_colon;
			sdd_tokenise_get_char(&ctx);
		}
		else if (c == ',')
		{
			token->type = sdd_token_type_comma;
			sdd_tokenise_get_char(&ctx);
		}
		else if (c == '[')
		{
			token->type = sdd_token_type_enter_array;
			sdd_tokenise_get_char(&ctx);
		}
		else if (c == ']')
		{
			token->type = sdd_token_type_leave_array;
			sdd_tokenise_get_char(&ctx);
		}
		else if (c == '{')
		{
			token->type = sdd_token_type_enter_object;
			sdd_tokenise_get_char(&ctx);
		}
		else if (c == '}')
		{
			token->type = sdd_token_type_leave_object;
			sdd_tokenise_get_char(&ctx);
		}
		else if (c == '"')
		{
			token->type = sdd_tokenise_parse_str(&ctx);
		}
		else if (c == '#')
		{
			token->type = sdd_tokenise_parse_hash(&ctx);
		}
		else if (sdd_tokenise_is_valid_dec_char(c))
		{
			token->type = sdd_tokenise_parse_number(&ctx);
		}
		else if (sdd_tokenise_is_valid_identifier_first_char(c))
		{
			token->type = sdd_tokenise_parse_identifier(&ctx);
		}
		else if (c == sdd_tokenise_char_eof)
		{
			token->type = sdd_token_type_eof;
			break;
		}
		else if (c == sdd_tokenise_char_error)
		{
			break;
		}
		else
		{
			sdd_tokenise_error(&ctx, rgs_sdd_error_invalid_char, "Invalid token");
			break;
		}

		if (token->type == sdd_token_type_error)
			break;

		token->end = ctx.current;
	}

	return tokens;
}