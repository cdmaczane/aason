static size_t aason_align_size(size_t size)
{
	const size_t alignment = sizeof(void*);
	return (size + (alignment - 1)) & ~(alignment - 1);
}

typedef struct
{
	aason_file*	files;
	uint32_t	count;
	uint32_t	capacity;
} aason_files;

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

			return nullptr;
		}
	}

	// Not found so load
	size_t file_size;
	void* stream = read_interface->open(read_interface->self, path, &file_size);
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
				read_interface->read(read_interface->self, stream, buffer + path_size, 0, file_size);

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

		read_interface->close(read_interface->self, stream);
	}
	else
	{
		// TODO: Report error
	}

	return nullptr;
}

uint32_t aason_preparse_recursive(aason_allocator* scratch, aason_allocator* allocator, aason_read_interface* read_interface, aason_files* files, const char* path)
{
	aason_file* file = aason_load_stream(scratch, allocator, read_interface, files, path);
	if (file)
	{
		file->inside = true;

		char* buffer = file->buffer + file->offset;
		char* current = buffer;
		while (*current)
		{
			if (*current == '#')
			{
				char* directive_begin = current;

				if (memcmp(current, "#include(\"", 9) == 0)
				{
					current += 10;
					char* path_begin = current;

					// Find end quote
					for (;;)
					{
						++current;
						if (*current == 0)
						{
							aason_assert(false);
						}
						else if (*current == '"')
						{
							if (current == path_begin)
							{
								aason_assert(false);
							}
							else if (current[1] != ')')
							{
								aason_assert(false);
							}

							// Null terminate path and recurse
							*current = 0;
							const uint32_t include_index = aason_preparse_recursive(scratch, allocator, read_interface, files, path_begin);

							// Clear preprocessor statement
							memset(directive_begin, ' ', (current - directive_begin) + 2);

							// Inject jump control code and file index
							directive_begin[0] = aason_tokenise_char_jump;
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

			++current;
		}

		file->inside = false;

		return (uint32_t)(file - files->files);
	}

	return UINT32_MAX;
}

aason_files aason_preparse(aason_allocator* scratch, aason_allocator* allocator, aason_read_interface* read_interface, const char* path)
{
	aason_files files = {};
	aason_preparse_recursive(scratch, allocator, read_interface, &files, path);

	return files;
}

aason_context* aason_read(const aason_read_desc* desc)
{
	aason_assert(desc);
	aason_assert(desc->path);
	aason_assert(desc->tab_size <= 8);
	aason_assert(desc->constructor_count <= UINT32_MAX);

	aason_context* ctx = nullptr;

	// Use 4-space tabs by default
	const uint32_t tab_size = desc->tab_size ? desc->tab_size : 4;

	// Store context on the stack until we know how much memory the parser requires
	aason_context temp_ctx = {
		.error = aason_error_none,
		.tab_size = tab_size,
		.reading = true,
		.constructor_count = (uint32_t)desc->constructor_count,
		.constructors = desc->constructors
	};

	if (desc->format)
		temp_ctx.format = desc->format;

	if (desc->error_interface)
		temp_ctx.error_interface = *desc->error_interface;

	temp_ctx.allocator = *desc->allocator;

	aason_allocator scratch = *desc->scratch_allocator;
	aason_read_interface read_interface = *desc->read_interface;

	void* frame = scratch.push(scratch.self);

	aason_files files = aason_preparse(&scratch, &temp_ctx.allocator, &read_interface, desc->path);
	if (files.count)
	{
		temp_ctx.file_count = files.count;
		temp_ctx.files = files.files;

		aason_tokens tokens = aason_tokenise(&temp_ctx, &scratch);
		if (temp_ctx.error == aason_error_none)
		{
			// Calculate the amount of memory required to copy constructors
			size_t constructor_count = desc->constructor_count;
			size_t constructor_arg_count = 0;
			size_t constructor_string_size = 0;
			for (size_t i = 0; i < constructor_count; ++i)
			{
				const size_t name_len = strlen(desc->constructors[i]->name);
				constructor_arg_count += desc->constructors[i]->count;
				constructor_string_size += name_len + 1; // Add null terminator
			}
			const size_t constructor_size = sizeof(aason_constructor_desc) * constructor_count;
			const size_t constructor_arg_size = sizeof(aason_arg) * constructor_arg_count;

			if (aason_validate(&temp_ctx, &tokens))
			{
				// Calculate the amount of memory required for parsing
				const size_t context_size	= aason_align_size(sizeof(aason_context));
				const size_t element_size	= aason_align_size(sizeof(aason_element) * temp_ctx.element_count);
				const size_t stack_size		= aason_align_size(sizeof(aason_stack_entry) * temp_ctx.max_stack_depth);
				const size_t file_size		= aason_align_size(sizeof(aason_file) * files.count);
				const size_t packed_size	= context_size + element_size + stack_size + file_size;

				// Allocate all parsing memory in one go
				uint8_t* alloc = (uint8_t*)aason_alloc(&temp_ctx.allocator, packed_size);
				if (alloc)
				{
					// Copy context from stack
					ctx = (aason_context*)alloc;
					memcpy(ctx, &temp_ctx, sizeof(aason_context));
					alloc += context_size;

					// Element array
					ctx->elements = (aason_element*)alloc;
					alloc += element_size;

					// Stack
					ctx->stack = (aason_stack_entry*)alloc;
					ctx->stack[0] = (aason_stack_entry){};
					alloc += stack_size;

					// File array
					ctx->file_count = files.count;
					ctx->files = (aason_file*)alloc;

					for (uint32_t i = 0; i < files.count; ++i)
						ctx->files[i] = files.files[i];

					if (aason_finalise(ctx, &tokens))
					{
						ctx->stack_depth = 0;
						ctx->locale = aason_new_locale();
					}
					else
					{
						// Free context in case of error
						aason_free(&ctx->allocator, alloc);
						ctx = nullptr;
					}
				}
			}

			// Free scratch arrays
			aason_free(&scratch, files.files);
			aason_free(&scratch, tokens.tokens);
		}
	}

	scratch.pop(scratch.self, frame);

	return ctx;
}