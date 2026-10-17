typedef struct
{
	aason_context*			ctx;
	aason_files*			files;
	char*					current;
	char*					end;
	uint32_t				file_index;
	uint32_t				line;
	uint32_t				column;
	aason_allocator*		scratch;
	aason_read_interface*	read_interface;
	jmp_buf					jmp_ctx;
} aason_preparser;

aason_noreturn static void aason_preparse_error(aason_preparser* preparser, aason_error_type error, const char* fmt, ...)
{
	aason_assert(false);

	longjmp(preparser->jmp_ctx, 1);
}

static aason_file* aason_allocate_file(aason_allocator* scratch, aason_files* files)
{
	if (files->count == files->capacity)
	{
		if (files->count)
			files->capacity <<= 1;
		else
			files->capacity = 64;

		files->files = aason_realloc(scratch, files->files, sizeof(aason_file) * files->capacity);
	}

	return &files->files[files->count++];
}

// TODO: Use file indices instead of pointers because the file list may grow and pointers become invalid
static aason_file* aason_load_stream(
	aason_allocator* scratch,
	aason_allocator* allocator,
	aason_read_interface* read_interface,
	aason_files* files,
	const char* path
)
{
	// Check if file has already been loaded
	for (uint32_t i = 0; i < files->count; ++i)
	{
		if (strcmp(path, files->files[i].buffer) == 0)
		{
			// TODO: Turn into error
			aason_assert(!files->files[i].inside);

			return NULL;
		}
	}

	// Not found so load
	size_t file_size;
	void* stream = read_interface->open(read_interface->user_data, path, &file_size);
	if (stream)
	{
		if (file_size)
		{
			const size_t path_size = strlen(path) + 1;
			const size_t total_size = path_size + file_size + 1;

			char* buffer = (char*)aason_alloc(allocator, total_size);
			if (buffer)
			{
				aason_file* file = aason_allocate_file(scratch, files);
				file->buffer = buffer;
				file->size = (uint32_t)file_size;
				file->offset = (uint32_t)path_size;

				memcpy(buffer, path, path_size);
				read_interface->read(read_interface->user_data, stream, buffer + path_size, 0, file_size);
				read_interface->close(read_interface->user_data, stream);

				// Null terminate file data to make parsing simpler
				buffer[path_size + file_size] = 0;

				return file;
			}
			else
			{
				// TODO: Report error
			}
		}
		else
		{
			// TODO: Report error
		}

		read_interface->close(read_interface->user_data, stream);
	}
	else
	{
		// TODO: Report error
	}

	return NULL;
}

static char aason_preparse_get_char_ascii(aason_preparser* preparser)
{
	const char c = *preparser->current++;

	// UTF-8 characters are only supported within strings and comments
	if (c & 0x80)
	{
		aason_preparse_error(preparser, aason_error_invalid_char, "Invalid character");
	}
	else if (c < 32)
	{
		if (c == '\t')
		{
			const uint32_t tab_size = preparser->ctx->tab_size;
			preparser->column += tab_size - ((preparser->column - 1) % tab_size);
		}
		else if (c == '\r')
		{
			// Ignore
			if (*preparser->current != '\n')
				aason_preparse_error(preparser, aason_error_invalid_char, "Invalid character");
		}
		else if (c == '\n')
		{
			++preparser->line;
			preparser->column = 1;
		}
		else if (c == 0)
		{
			if (preparser->current - 1 != preparser->end)
				aason_preparse_error(preparser, aason_error_invalid_char, "Unexpected null character");
		}
		else
		{
			aason_preparse_error(preparser, aason_error_invalid_char, "Invalid character");
		}
	}
	else if (c == 127)
	{
		// Delete character
		aason_preparse_error(preparser, aason_error_invalid_char, "Invalid character");
	}
	else
	{
		++preparser->column;
	}

	return c;
}

// TODO: Profile with large amounts of non-ASCII data and optimise
static char aason_preparse_get_char_utf8(aason_preparser* preparser)
{
	const uint8_t c = *preparser->current;

	// Regular ASCII
	if (c < 128)
		return aason_preparse_get_char_ascii(preparser);

	uint32_t count;
	uint32_t minimum;
	uint32_t codepoint;

	// Calculate number of bytes for code point
	if (c >= 0xC2 && c <= 0xDF)
	{
		count = 2;
		minimum = 0x80;
		codepoint = c & 0x1F;
	}
	else if (c >= 0xE0 && c <= 0xEF)
	{
		count = 3;
		minimum = 0x800;
		codepoint = c & 0x0F;
	}
	else if (c >= 0xF0 && c <= 0xF4)
	{
		count = 4;
		minimum = 0x10000;
		codepoint = c & 0x07;
	}
	else
	{
		aason_preparse_error(preparser, aason_error_invalid_char, "Invalid UTF-8 character");
	}

	// Ensure there is actually enough space left in the buffer for the complete code point bytes
	const uint32_t remaining = (uint32_t)(preparser->end - preparser->current);
	if (remaining < count)
		aason_preparse_error(preparser, aason_error_invalid_char, "Incomplete UTF-8 character");

	// Construct code point and validate
	for (uint32_t i = 1; i < count; ++i)
	{
		const uint8_t next = preparser->current[i];

		// Subsequent bytes must have the binary prefix 0b10
		if ((next & 0xC0) != 0x80)
			aason_preparse_error(preparser, aason_error_invalid_char, "Invalid UTF-8 continuation byte");

		// Each byte provides 6 bits of the code point
		codepoint = (codepoint << 6) | (next & 0x3F);
	}

	if (
		codepoint < minimum ||							// Reject overlong encodings
		codepoint > 0x10FFFF ||							// Reject code points outside of Unicode range
		(codepoint >= 0xD800 && codepoint <= 0xDFFF)	// Reject UTF-16 surrogate code points
	)
	{
		aason_preparse_error(preparser, aason_error_invalid_char, "Invalid UTF-8 code point");
	}

	preparser->current += count;
	++preparser->column;

	return c;
}

static void aason_preparse_string(aason_preparser* preparser)
{
	for (;;)
	{
		const char c = aason_preparse_get_char_utf8(preparser);
		if (c == '"')
		{
			break;
		}
		else if (c == 0)
		{
			aason_preparse_error(preparser, aason_error_invalid_char, "Unexpected end-of-file in string");
		}
		else if (c == '\r' || c == '\n')
		{
			aason_preparse_error(preparser, aason_error_invalid_char, "Unexpected newline in string");
		}
		else if (c == '\\')
		{
			const char escaped = aason_preparse_get_char_ascii(preparser);
			switch (escaped)
			{
			case '"':
			case '\\':
			case 'n':
			case 't':
				break;
			default:
				aason_preparse_error(preparser, aason_error_invalid_char, "Invalid escape sequence");
			}
		}
	}
}

static void aason_preparse_write_skip_line(char* begin, char* end)
{
	const uint32_t byte_count = (uint32_t)(end - begin);

	if (byte_count == 0)
	{
	}
	else if (byte_count == 1)
	{
		begin[0] = ' ';
	}
	else if (byte_count <= UINT8_MAX)
	{
		begin[0] = aason_char_op_skip_line_short;
		begin[1] = (uint8_t)byte_count;
	}
	else
	{
		begin[0] = aason_char_op_skip_line_long;
		memcpy(begin + 1, &byte_count, 4);
	}
}

static void aason_preparse_write_skip_columns(char* begin, char* end, uint32_t column_count)
{
	const uint32_t byte_count = (uint32_t)(end - begin);

	if (byte_count == 0)
	{
	}
	else if (byte_count == 1)
	{
		begin[0] = ' ';
	}
	else if (byte_count == 2)
	{
		begin[0] = ' ';
		begin[1] = ' ';
	}
	else if (byte_count <= UINT8_MAX && column_count <= UINT8_MAX)
	{
		begin[0] = aason_char_op_skip_columns_short;
		begin[1] = (uint8_t)byte_count;
		begin[2] = (uint8_t)column_count;
	}
	else
	{
		begin[0] = aason_char_op_skip_columns_long;
		memcpy(begin + 1, &byte_count, 4);
		memcpy(begin + 5, &column_count, 4);
	}
}

static void aason_preparse_comment(aason_preparser* preparser)
{
	// The opening '/' character has already been consumed
	char* begin = preparser->current - 1;
	uint32_t column = preparser->column - 1;

	const char type = aason_preparse_get_char_ascii(preparser);	
	if (type == '/')
	{
		// Single-line comment
		for (;;)
		{
			const char c = aason_preparse_get_char_utf8(preparser);
			if (c == '\n' || c == 0)
			{
				aason_preparse_write_skip_line(begin, preparser->current - 1);
				break;
			}
		}
	}
	else if (type == '*')
	{
		// Multiline comment
		uint32_t depth = 1;
		char prev = 0;

		for (;;)
		{
			char* current = preparser->current;
			const char c = aason_preparse_get_char_utf8(preparser);

			if (c == 0)
			{
				aason_preparse_error(preparser, aason_error_invalid_char, "Unexpected end-of-file in comment");
			}
			else if (c == '\n')
			{
				aason_preparse_write_skip_line(begin, current);
				begin = preparser->current;
				column = preparser->column;
				prev = 0;
			}
			else if (prev == '/' && c == '*')
			{
				++depth;
				prev = 0;
			}
			else if (prev == '*' && c == '/')
			{
				if (--depth == 0)
					break;
				prev = 0;
			}
			else
			{
				prev = c;
			}
		}

		aason_preparse_write_skip_columns(begin, preparser->current, preparser->column - column);
	}
	else
	{
		aason_preparse_error(preparser, aason_error_invalid_char, "Expected '/' or '*' for comment");
	}
}

static uint32_t aason_preparse_recursive(aason_context* ctx, aason_allocator* scratch, aason_allocator* allocator, aason_read_interface* read_interface, aason_files* files, const char* path);

static void aason_preparse_directive(aason_preparser* preparser)
{
	// HACK: '#' character has already been consumed
	preparser->current[-1] = ' ';

	char* directive_begin = preparser->current;

	if (strncmp(directive_begin, "include(\"", 9) == 0)
	{
		preparser->current += 9;
		char* path_begin = preparser->current;

		// Find end quote
		for (;;)
		{
			++preparser->current;
			if (*preparser->current == 0)
			{
				aason_assert(false);
			}
			else if (*preparser->current == '"')
			{
				if (preparser->current == path_begin)
				{
					aason_assert(false);
				}
				else if (preparser->current[1] != ')')
				{
					aason_assert(false);
				}

				// Null terminate path and recurse
				*preparser->current = 0;
				const uint32_t include_index = aason_preparse_recursive(
					preparser->ctx,
					preparser->scratch,
					&preparser->ctx->allocator,
					preparser->read_interface,
					preparser->files,
					path_begin
				);
				if (include_index == UINT32_MAX)
					return;

				// Clear preprocessor statement
				memset(directive_begin, ' ', (preparser->current - directive_begin) + 2);

				// Inject jump control code and file index
				directive_begin[0] = aason_char_op_jump;
				memcpy(directive_begin + 1, &include_index, 4);

				break;
			}
		}
	}
	else
	{
		aason_assert(false);
	}
}

static uint32_t aason_preparse_recursive(aason_context* ctx, aason_allocator* scratch, aason_allocator* allocator, aason_read_interface* read_interface, aason_files* files, const char* path)
{
	aason_file* file = aason_load_stream(scratch, allocator, read_interface, files, path);
	if (file)
	{
		file->inside = true;

		aason_preparser preparser = {
			.ctx = ctx,
			.files = files,
			.current = file->buffer + file->offset,
			.end = file->buffer + file->offset + file->size,
			.file_index = (uint32_t)(file - files->files),
			.line = 1,
			.column = 1,
			.scratch = scratch,
			.read_interface = read_interface
		};

		// TODO: Jump buffer needs to exist outside of recursion
		if (setjmp(preparser.jmp_ctx))
			return UINT32_MAX;

		for (;;)
		{
			const char c = aason_preparse_get_char_ascii(&preparser);
			if (c == 0)
				break;
			else if (c == '"')
				aason_preparse_string(&preparser);
			else if (c == '#')
				aason_preparse_directive(&preparser);
			else if (c == '/')
				aason_preparse_comment(&preparser);
		}

		// File pointer may have been invalidated
		files->files[preparser.file_index].inside = false;

		return preparser.file_index;
	}

	return UINT32_MAX;
}

aason_files aason_run_preparse_pass(aason_context* ctx, aason_allocator* scratch, aason_allocator* allocator, aason_read_interface* read_interface, const char* path)
{
	aason_files files = {};
	aason_preparse_recursive(ctx, scratch, allocator, read_interface, &files, path);

	return files;
}