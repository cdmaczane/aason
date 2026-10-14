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
				read_interface->close(read_interface->self, stream);

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

				if (strncmp(current, "#include(\"", 10) == 0)
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