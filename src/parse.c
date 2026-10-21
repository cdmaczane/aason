typedef struct
{
	size_t arg_count;
	size_t string_size;
	size_t max_arg_count;
} aason_constructor_sizes;

static size_t aason_align_size(size_t size)
{
	const size_t alignment = sizeof(void*);
	return (size + (alignment - 1)) & ~(alignment - 1);
}

static void* aason_suballoc(uint8_t** alloc, size_t size)
{
	void* result = *alloc;
	*alloc += aason_align_size(size);

	return result;
}

//static uint32_t aason_hash_string(const char* str)
//{
//	uint32_t hash = 2166136261;
//	while (*str)
//	{
//		hash ^= *str++;
//		hash *= 16777619;
//	}

//	return hash;
//}

static char* aason_copy_string_and_hash(char* aason_restrict dst, const char* aason_restrict src, uint32_t* aason_restrict hash_out)
{
	aason_assert(aason_tokenise_is_valid_identifier_first_char(*src));

	uint32_t hash = 2166136261;

	char c;
	while ((c = *src++) != 0)
	{
		aason_assert(aason_tokenise_is_valid_identifier_char(c));

		*dst++ = c;
		hash ^= (uint8_t)c;
		hash *= 16777619;
	}

	*dst++ = 0;
	*hash_out = hash;

	return dst;
}

static aason_constructor_sizes aason_get_constructor_sizes(const aason_constructor_desc* const* descs, size_t count)
{
	aason_constructor_sizes sizes = {};

	for (size_t i = 0; i < count; ++i)
	{
		const size_t name_len = strlen(descs[i]->name);
		const size_t arg_count = descs[i]->count;
		aason_assert(arg_count <= UINT8_MAX);

		sizes.arg_count += arg_count;
		sizes.string_size += name_len + 1; // Add null terminator

		if (arg_count > sizes.max_arg_count)
			sizes.max_arg_count = arg_count;
	}

	aason_assert(sizes.arg_count <= UINT32_MAX);

	return sizes;
}

static size_t aason_get_constructors_allocation_size(aason_constructor_sizes sizes, size_t count)
{
	if (count == 0)
		return 0;

	const size_t struct_size		= aason_align_size(sizeof(aason_constructors));
	const size_t names_size			= aason_align_size(sizeof(const char*) * count);
	const size_t hashes_size		= aason_align_size(sizeof(uint32_t) * count);
	const size_t arg_counts_size	= aason_align_size(sizeof(uint8_t) * count);
	const size_t arg_indices_size	= aason_align_size(sizeof(uint32_t) * count);
	const size_t callbacks_size		= aason_align_size(sizeof(void*) * count);
	const size_t args_size			= aason_align_size(sizeof(uint8_t) * sizes.arg_count);
	const size_t temp_args_size		= aason_align_size(sizeof(aason_arg) * sizes.max_arg_count);

	const size_t total_size =
		struct_size +
		names_size +
		hashes_size +
		arg_counts_size +
		arg_indices_size +
		callbacks_size +
		args_size +
		temp_args_size +
		sizes.string_size
	;

	return total_size;
}

static aason_constructors* aason_copy_constructors(uint8_t* alloc, aason_constructor_sizes sizes, const aason_constructor_desc* const* descs, size_t count, bool write)
{
	if (count == 0)
		return NULL;

	aason_constructors* constructors = aason_suballoc(&alloc, sizeof(aason_constructors));
	constructors->count = (uint16_t)count;

	constructors->names = aason_suballoc(&alloc, sizeof(const char*) * count);;
	constructors->hashes = aason_suballoc(&alloc, sizeof(uint32_t) * count);
	constructors->arg_counts = aason_suballoc(&alloc, sizeof(uint8_t) * count);
	constructors->arg_indices = aason_suballoc(&alloc, sizeof(uint32_t) * count);

	if (write)
	{
		constructors->read_callbacks = NULL;
		constructors->write_callbacks = aason_suballoc(&alloc, sizeof(aason_write_constructor_callback*) * count);
	}
	else
	{
		constructors->read_callbacks = aason_suballoc(&alloc, sizeof(aason_read_constructor_callback*) * count);
		constructors->write_callbacks = NULL;
	}

	constructors->args = aason_suballoc(&alloc, sizeof(uint8_t) * sizes.arg_count);
	constructors->temp_args = aason_suballoc(&alloc, sizeof(aason_arg) * sizes.max_arg_count);

	char* name_buffer = aason_suballoc(&alloc, sizes.string_size);

	uint32_t arg_index = 0;
	for (size_t i = 0; i < count; ++i)
	{
		const aason_constructor_desc* constructor = descs[i];

		// Copy name, calculate hash, and validate constructor name in a single loop
		constructors->names[i] = name_buffer;
		name_buffer = aason_copy_string_and_hash(name_buffer, constructor->name, &constructors->hashes[i]);

		const uint8_t arg_count = (uint8_t)constructor->count;
		constructors->arg_counts[i] = arg_count;
		constructors->arg_indices[i] = arg_index;

		if (write)
			constructors->write_callbacks[i] = constructor->write;
		else
			constructors->read_callbacks[i] = constructor->read;

		for (uint8_t j = 0; j < arg_count; ++j)
			constructors->args[arg_index++] = constructor->args[j];
	}

	aason_assert(arg_index == sizes.arg_count);

	return constructors;
}

aason_context* aason_preparse(const aason_read_desc* desc)
{
	aason_assert(desc);
	aason_assert(desc->path);
	aason_assert(desc->tab_size <= 8);
	aason_assert(desc->constructor_count <= UINT16_MAX);
	aason_assert(desc->mode != aason_mode_json || !desc->use_implicit_root);

	aason_context* ctx = NULL;

	// Use 4-space tabs by default
	const uint32_t tab_size = desc->tab_size ? desc->tab_size : 4;

	// Store context on the stack until we know how much memory the parser requires
	aason_context temp_ctx = {
		.error = aason_error_none,
		.tab_size = tab_size,
		.mode = desc->mode,
		.reading = true,
		.use_implicit_root = desc->use_implicit_root,
		.check_unused_fields = desc->check_unused_fields,
		.allow_trailing_commas = desc->allow_trailing_commas
	};

	if (desc->error_interface)
		temp_ctx.error_interface = *desc->error_interface;

	temp_ctx.allocator = aason_default_allocator;
	if (desc->allocator)
		temp_ctx.allocator = *desc->allocator;

	temp_ctx.scratch = temp_ctx.allocator;
	if (desc->scratch_allocator)
		temp_ctx.scratch = *desc->scratch_allocator;

	aason_read_interface read_interface = aason_default_read_interface;
	if (desc->read_interface)
		*desc->read_interface;

	void* frame = temp_ctx.scratch.push(temp_ctx.scratch.user_data);

	aason_files files = aason_run_preparse_pass(&temp_ctx, &temp_ctx.scratch, &temp_ctx.allocator, &read_interface, desc->path);
	if (files.count)
	{
		temp_ctx.file_count = files.count;
		temp_ctx.files = files.files;

		// Calculate the amount of memory required to copy constructors
		const aason_constructor_sizes constructor_sizes = aason_get_constructor_sizes(desc->constructors, desc->constructor_count);
		const size_t constructor_size = aason_get_constructors_allocation_size(constructor_sizes, desc->constructor_count);

		// Calculate the amount of memory required for parsing
		const size_t context_size	= aason_align_size(sizeof(aason_context));
		const size_t file_size		= aason_align_size(sizeof(aason_file) * files.count);
		const size_t packed_size	= context_size + constructor_size + file_size;

		// Allocate all parsing memory in one go
		uint8_t* alloc = aason_alloc(&temp_ctx.allocator, packed_size);
		if (alloc)
		{
			// Copy context from stack
			ctx = aason_suballoc(&alloc, context_size);;
			memcpy(ctx, &temp_ctx, sizeof(aason_context));

			if (desc->constructor_count)
			{
				ctx->constructors = aason_copy_constructors(alloc, constructor_sizes, desc->constructors, desc->constructor_count, false);
				alloc += constructor_size;
			}

			// File array
			ctx->file_count = files.count;
			ctx->files = (aason_file*)alloc;

			for (uint32_t i = 0; i < files.count; ++i)
				ctx->files[i] = files.files[i];
		}

		aason_free(&temp_ctx.scratch, files.files);
	}

	temp_ctx.scratch.pop(temp_ctx.scratch.user_data, frame);

	return ctx;
}

bool aason_parse(aason_context* ctx)
{
	bool result = false;

	void* frame = ctx->scratch.push(ctx->scratch.user_data);

	aason_tokens tokens = aason_run_tokenise_pass(ctx, &ctx->scratch);
	if (ctx->error == aason_error_none)
	{
		if (aason_run_validate_pass(ctx, &tokens))
		{
			const size_t stack_size		= aason_align_size(sizeof(aason_stack_entry) * ctx->max_stack_depth);
			const size_t element_size	= aason_align_size(sizeof(aason_element) * ctx->element_count);
			const size_t packed_size	= stack_size + element_size;

			// Allocate all parsing memory in one go
			uint8_t* alloc = aason_alloc(&ctx->allocator, packed_size);
			if (alloc)
			{
				ctx->stack = (aason_stack_entry*)alloc;
				ctx->stack[0] = (aason_stack_entry){};
				alloc += stack_size;

				ctx->elements = (aason_element*)alloc;
				alloc += element_size;

				if (aason_run_finalise_pass(ctx, &tokens))
				{
					ctx->stack_depth = 0;
					ctx->locale = aason_new_locale();

					result = true;
				}
			}
		}
	}

	aason_free(&ctx->scratch, tokens.tokens);
	ctx->scratch.pop(ctx->scratch.user_data, frame);

	return result;
}

const char* aason_get_path(const aason_context* ctx, size_t index)
{
	aason_assert(ctx);
	aason_assert(index < ctx->file_count);

	return ctx->files[index].buffer;
}

size_t aason_get_path_count(const aason_context* ctx)
{
	aason_assert(ctx);

	return ctx->file_count;
}

aason_context* aason_read(const aason_read_desc* desc)
{
	aason_context* ctx = aason_preparse(desc);
	if (ctx)
	{
		if (aason_parse(ctx))
			return ctx;
		else
			aason_destroy(ctx);
	}

	return NULL;
}