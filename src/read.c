static const char* aason_type_strings[] = {
	"",
	"string",
	"int",
	"bool",
	"hash",
	"enum",
	"float",
	"array",
	"object",
	"constructor"
};

static void aason_read_error(aason_context* ctx, aason_error_type error, uint32_t line, uint32_t column, const char* fmt, ...)
{
	char buffer[4096];

	ctx->error = error;
	ctx->error_line = line;
	ctx->error_column = column;

	if (ctx->error_interface.error)
	{
		va_list args;
		va_start(args, fmt);
		aason_format_string(ctx, buffer, sizeof(buffer), fmt, args);
		va_end(args);
	
		ctx->error_interface.error(
			ctx->error_interface.state,
			error,
			line,
			column,
			buffer
		);
	}
}

static const char* aason_read_get_key_buffer(aason_context* ctx, const aason_element* element)
{
	aason_assert(element->key_file_index < ctx->file_count);

	const aason_file* file = &ctx->files[element->key_file_index];
	const char* buffer = file->buffer + file->offset;

	return buffer;
}

static const char* aason_read_get_value_buffer(aason_context* ctx, const aason_element* element)
{
	aason_assert(element->value_file_index < ctx->file_count);

	const aason_file* file = &ctx->files[element->value_file_index];
	const char* buffer = file->buffer + file->offset;

	return buffer;
}

static const aason_element* aason_read_find_object_element(aason_context* ctx, const char* key, aason_flags flags, aason_type type)
{
	aason_assert(ctx);
	aason_assert(key);
	aason_assert(*key);

	const int64_t key_len = strlen(key);

	const uint32_t stack_depth = ctx->stack_depth;
	aason_assert(stack_depth < ctx->max_stack_depth);

	const uint32_t element_index = ctx->stack[stack_depth].element_index;
	aason_assert(element_index < ctx->element_count);
	const aason_element* object_element = &ctx->elements[element_index];

	if (stack_depth == 0)
	{
		const char* key_buffer = aason_read_get_key_buffer(ctx, object_element);
		if (key_len == object_element->key_len && memcmp(key, key_buffer + object_element->key_offset, key_len) == 0)
		{
			if (object_element->type == type)
			{
				return object_element;
			}
			else
			{
				aason_read_error(ctx, aason_error_wrong_type, object_element->line, object_element->column,
					"Root element '{s}' has type '{s}' instead of expected type '{s}'",
					key, aason_type_strings[object_element->type], aason_type_strings[type]
				);

				return nullptr;
			}
		}
	}
	else
	{
		const aason_element* children = ctx->elements + object_element->object_value.first_child;
		const uint32_t count = object_element->object_value.count;

		for (uint32_t i = 0; i < count; ++i)
		{
			const aason_element* element = &children[i];
			const char* key_buffer = aason_read_get_key_buffer(ctx, element);
	
			if (key_len == element->key_len && memcmp(key, key_buffer + element->key_offset, key_len) == 0)
			{
				if (element->type == type)
				{
					return element;
				}
				else
				{
					aason_read_error(ctx, aason_error_wrong_type, element->line, element->column,
						"Element '{s}' has type '{s}' instead of expected type '{s}'",
						key, aason_type_strings[element->type], aason_type_strings[type]
					);
	
					return nullptr;
				}
			}
		}
	}

	if (flags == aason_required)
	{
		// TODO: Improve error message when root element is not found
		aason_read_error(ctx, aason_error_key_not_found, object_element->line, object_element->column,
			"Unable to find required element '{s}'", key
		);
	}

	return nullptr;
}

static const aason_element* aason_read_get_next_array_element(aason_context* ctx, aason_type type)
{
	aason_assert(ctx);

	const uint32_t stack_depth = ctx->stack_depth;
	aason_assert(stack_depth > 0);
	aason_assert(stack_depth < ctx->max_stack_depth);

	const uint32_t element_index = ctx->stack[stack_depth].element_index;
	aason_assert(element_index < ctx->element_count);

	const aason_element* array_element = &ctx->elements[element_index];
	aason_assert(array_element->type == aason_type_array);

	const uint32_t array_index = ctx->stack[stack_depth].array_index;
	if (array_index < array_element->array_value.count)
	{
		const aason_element* children = ctx->elements + array_element->array_value.first_child;
		const aason_element* element = &children[array_index];

		if (element->type == type)
		{
			ctx->stack[stack_depth].array_index = array_index + 1;
			return element;
		}
		else
		{
			aason_read_error(ctx, aason_error_wrong_type, element->line, element->column,
				"Array element has type '{s}' instead of expected type '{s}'",
				aason_type_strings[element->type], aason_type_strings[type]
			);
		}
	}

	return nullptr;
}

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

void aason_preparse_recursive(aason_allocator* scratch, aason_allocator* allocator, aason_read_interface* read_interface, aason_files* files, const char* path)
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
							aason_preparse_recursive(scratch, allocator, read_interface, files, path_begin);

							// Clear preprocessor statement
							memset(directive_begin, ' ', (current - directive_begin) + 2);

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
	}
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
	aason_assert(files.count);

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
			//uint8_t* alloc = (uint8_t*)desc->allocator(desc->allocator_data, nullptr, 0, packed_size);
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

	scratch.pop(scratch.self, frame);

	return ctx;
}

bool aason_read_array_enter(aason_context* ctx, const char* key, aason_flags flags, int64_t* size, int64_t max_size)
{
	const aason_element* element = aason_read_find_object_element(ctx, key, flags, aason_type_array);
	if (element)
	{
		if (element->array_value.count > max_size)
		{
			aason_read_error(ctx, aason_error_buffer_too_small, element->line, element->column,
				"Array '{s}' size is {u32} but max size is {i64}",
				key, element->array_value.count, max_size
			);

			return false;
		}

		const uint32_t stack_depth = ++ctx->stack_depth;
		aason_assert(stack_depth < ctx->max_stack_depth);

		ctx->stack[stack_depth].element_index = (uint32_t)(element - ctx->elements);
		ctx->stack[stack_depth].array_index = 0;

		if (size)
			*size = element->array_value.count;

		return true;
	}

	return false;
}

void aason_read_array_leave(aason_context* ctx)
{
	aason_assert(ctx);
	aason_assert(ctx->stack_depth > 0);
	aason_assert(ctx->stack_depth < ctx->max_stack_depth);

	const uint32_t element_index = ctx->stack[ctx->stack_depth].element_index;
	aason_assert(element_index < ctx->element_count);

	const aason_element* element = &ctx->elements[element_index];
	aason_assert(element->type == aason_type_array);

	--ctx->stack_depth;
}

bool aason_read_array_enter_object(aason_context* ctx)
{
	const aason_element* element = aason_read_get_next_array_element(ctx, aason_type_object);
	if (element)
	{
		const uint32_t stack_depth = ++ctx->stack_depth;
		aason_assert(stack_depth < ctx->max_stack_depth);

		ctx->stack[stack_depth].element_index = (uint32_t)(element - ctx->elements);
		++ctx->stack[stack_depth].array_index;

		return true;
	}

	return false;
}

void aason_read_array_leave_object(aason_context* ctx)
{
	aason_assert(ctx);
	aason_assert(ctx->stack_depth > 0);
	aason_assert(ctx->stack_depth < ctx->max_stack_depth);

	const uint32_t element_index = ctx->stack[ctx->stack_depth].element_index;
	aason_assert(element_index < ctx->element_count);

	const aason_element* element = &ctx->elements[element_index];
	aason_assert(element->type == aason_type_object);

	--ctx->stack_depth;
}

bool aason_read_array_str(aason_context* ctx, const char** value, int64_t* len)
{
	aason_assert(value);

	const aason_element* element = aason_read_get_next_array_element(ctx, aason_type_str);
	if (element)
	{
		const char* buffer = aason_read_get_value_buffer(ctx, element);

		*value = buffer + element->str_value.offset;

		if (len)
			*len = element->str_value.len;

		return true;
	}

	return false;
}

bool aason_read_array_fixed_str(aason_context* ctx, char* value, int64_t buffer_size, bool truncate)
{
	aason_assert(value);
	aason_assert(buffer_size > 0);

	const aason_element* element = aason_read_get_next_array_element(ctx, aason_type_str);
	if (element)
	{
		const char* buffer = aason_read_get_value_buffer(ctx, element);
		const char* str = buffer + element->str_value.offset;
		const int64_t len = element->str_value.len;

		if (len < buffer_size)
		{
			memcpy(value, str, len);
			value[len] = 0;
			return true;
		}
		else if (truncate)
		{
			memcpy(value, str, buffer_size - 1);
			value[buffer_size - 1] = 0;
			return true;
		}
		else
		{
			aason_read_error(ctx, aason_error_buffer_too_small, element->line, element->column,
				"String element of length '{u32}' is too large for fixed sized buffer size of '{i64}'",
				len, buffer_size
			);
		}
	}

	return false;
}

bool aason_read_array_int(aason_context* ctx, int64_t* value)
{
	aason_assert(value);

	const aason_element* element = aason_read_get_next_array_element(ctx, aason_type_int);
	if (element)
	{
		*value = element->int_value;
		return true;
	}

	return false;
}

bool aason_read_array_int_ranged(aason_context* ctx, int64_t* value, int64_t min, int64_t max)
{
	aason_assert(value);

	const aason_element* element = aason_read_get_next_array_element(ctx, aason_type_int);
	if (element)
	{
		if (element->int_value < min || element->int_value > max)
		{
			aason_read_error(ctx, aason_error_buffer_too_small, element->line, element->column,
				"Int out of range ({i64} to {i64})",
				min, max
			);

			return false;
		}

		*value = element->int_value;
		return true;
	}

	return false;
}

bool aason_read_array_bool(aason_context* ctx, bool* value)
{
	aason_assert(value);

	const aason_element* element = aason_read_get_next_array_element(ctx, aason_type_bool);
	if (element)
	{
		*value = element->bool_value;
		return true;
	}

	return false;
}

bool aason_read_array_float(aason_context* ctx, float* value)
{
	aason_assert(value);

	const aason_element* element = aason_read_get_next_array_element(ctx, aason_type_float);
	if (element)
	{
		*value = element->float_value;
		return true;
	}

	return false;
}

bool aason_read_array_enum(aason_context* ctx, int32_t* value, const char** strings, int32_t count)
{
	aason_assert(value);
	aason_assert(strings);
	aason_assert(count > 1);

	const aason_element* element = aason_read_get_next_array_element(ctx, aason_type_enum);
	if (element)
	{
		const char* buffer = aason_read_get_value_buffer(ctx, element);
		const uint32_t lhs_offset = element->enum_value.offset;
		const int64_t lhs_len = element->enum_value.len;
		const char* enum_value = &buffer[lhs_offset];

		for (int32_t i = 0; i < count; ++i)
		{
			const int64_t rhs_len = strlen(strings[i]);
			if (lhs_len == rhs_len && memcmp(enum_value, strings[i], lhs_len) == 0)
			{
				*value = i;
				return true;
			}
		}

		aason_read_error(ctx, aason_error_invalid_enum, element->line, element->column,
			"Invalid enum value '{s}' in array", enum_value
		);
	}

	return false;
}

bool aason_read_object_enter(aason_context* ctx, const char* key, aason_flags flags)
{
	aason_assert(ctx->stack_depth + 1 < ctx->max_stack_depth);

	const aason_element* element = aason_read_find_object_element(ctx, key, flags, aason_type_object);
	if (element)
	{
		const uint32_t stack_depth = ++ctx->stack_depth;
		aason_assert(stack_depth < ctx->max_stack_depth);

		ctx->stack[stack_depth].element_index = (uint32_t)(element - ctx->elements);
		return true;
	}

	return false;
}

void aason_read_object_leave(aason_context* ctx)
{
	aason_assert(ctx);
	aason_assert(ctx->stack_depth > 0);
	aason_assert(ctx->stack_depth < ctx->max_stack_depth);

	const uint32_t element_index = ctx->stack[ctx->stack_depth].element_index;
	aason_assert(element_index < ctx->element_count);

	const aason_element* element = &ctx->elements[element_index];
	aason_assert(element->type == aason_type_object);

	--ctx->stack_depth;
}

bool aason_read_object_str(aason_context* ctx, const char* key, aason_flags flags, const char** value, int64_t* len)
{
	aason_assert(value);

	const aason_element* element = aason_read_find_object_element(ctx, key, flags, aason_type_str);
	if (element)
	{
		const char* buffer = aason_read_get_value_buffer(ctx, element);

		*value = buffer + element->str_value.offset;

		if (len)
			*len = element->str_value.len;

		return true;
	}

	return false;
}

bool aason_read_object_fixed_str(aason_context* ctx, const char* key, aason_flags flags, char* value, int64_t buffer_size, bool truncate)
{
	aason_assert(value);
	aason_assert(buffer_size > 0);

	const aason_element* element = aason_read_find_object_element(ctx, key, flags, aason_type_str);
	if (element)
	{
		const char* buffer = aason_read_get_value_buffer(ctx, element);
		const char* str = buffer + element->str_value.offset;
		const int64_t len = element->str_value.len;

		if (len < buffer_size)
		{
			memcpy(value, str, len);
			value[len] = 0;
			return true;
		}
		else if (truncate)
		{
			memcpy(value, str, buffer_size - 1);
			value[buffer_size - 1] = 0;
			return true;
		}
		else
		{
			aason_read_error(ctx, aason_error_buffer_too_small, element->line, element->column,
				"String element of length '{u32}' is too large for fixed sized buffer size of '{i64}'",
				len, buffer_size
			);
		}
	}

	return false;
}

bool aason_read_object_int(aason_context* ctx, const char* key, aason_flags flags, int64_t* value)
{
	aason_assert(value);

	const aason_element* element = aason_read_find_object_element(ctx, key, flags, aason_type_int);
	if (element)
	{
		*value = element->int_value;
		return true;
	}

	return false;
}

bool aason_read_object_int_ranged(aason_context* ctx, const char* key, aason_flags flags, int64_t* value, int64_t min, int64_t max)
{
	aason_assert(value);

	const aason_element* element = aason_read_find_object_element(ctx, key, flags, aason_type_int);
	if (element)
	{
		if (element->int_value < min || element->int_value > max)
		{
			aason_read_error(ctx, aason_error_buffer_too_small, element->line, element->column,
				"Int element '{s}' out of range ({i64} to {i64})",
				key, min, max
			);

			return false;
		}

		*value = element->int_value;
		return true;
	}

	return false;
}

bool aason_read_object_bool(aason_context* ctx, const char* key, aason_flags flags, bool* value)
{
	aason_assert(value);

	const aason_element* element = aason_read_find_object_element(ctx, key, flags, aason_type_bool);
	if (element)
	{
		*value = element->bool_value;
		return true;
	}

	return false;
}

bool aason_read_object_float(aason_context* ctx, const char* key, aason_flags flags, float* value)
{
	aason_assert(value);

	const aason_element* element = aason_read_find_object_element(ctx, key, flags, aason_type_float);
	if (element)
	{
		*value = element->float_value;
		return true;
	}

	return false;
}

bool aason_read_object_enum(aason_context* ctx, const char* key, aason_flags flags, int32_t* value, const char** strings, int32_t count)
{
	aason_assert(value);
	aason_assert(strings);
	aason_assert(count >= 1);

	const aason_element* element = aason_read_find_object_element(ctx, key, flags, aason_type_enum);
	if (element)
	{
		const char* buffer = aason_read_get_value_buffer(ctx, element);
		const uint32_t lhs_offset = element->enum_value.offset;
		const int64_t lhs_len = element->enum_value.len;
		const char* enum_value = &buffer[lhs_offset];

		for (int32_t i = 0; i < count; ++i)
		{
			const int64_t rhs_len = strlen(strings[i]);
			if (lhs_len == rhs_len && memcmp(enum_value, strings[i], lhs_len) == 0)
			{
				*value = i;
				return true;
			}
		}

		aason_read_error(ctx, aason_error_invalid_enum, element->line, element->column,
			"Invalid enum value '{s}' found in element '{s}'", enum_value, key
		);
	}

	return false;
}

bool aason_read_object_constructor(aason_context* ctx, const char* key, aason_flags flags, void* value, const char* type)
{
	aason_assert(value);
	aason_assert(type);

	const aason_element* element = aason_read_find_object_element(ctx, key, flags, aason_type_constructor);
	if (element)
	{
		aason_assert(element->constructor_index < ctx->constructor_count);

		const aason_constructor_desc* constructor = ctx->constructors[element->constructor_index];
		aason_assert(strcmp(type, constructor->name) == 0);

		// TODO: Get rid of allocation
		aason_arg* args = alloca(sizeof(aason_arg) * constructor->count);

		const aason_element* children = ctx->elements + element->constructor_value.first_child;
		for (uint32_t i = 0; i < constructor->count; ++i)
		{
			const char* buffer = aason_read_get_value_buffer(ctx, &children[i]);

			args[i].type = constructor->args[i];

			switch (constructor->args[i])
			{
			case aason_arg_type_str:
				args[i].str_value = buffer + children[i].str_value.offset;
				break;
			case aason_arg_type_bin:
			case aason_arg_type_dec:
			case aason_arg_type_hex:
				args[i].int_value = children[i].int_value;
				break;
			case aason_arg_type_bool:
				args[i].bool_value = children[i].bool_value;
				break;
			case aason_arg_type_enum:
				args[i].enum_value = buffer + children[i].enum_value.offset;
				break;
			case aason_arg_type_float:
				args[i].float_value = children[i].float_value;
				break;
			}
		}

		constructor->read(ctx, value, args, constructor->count);

		return true;
	}

	return false;
}