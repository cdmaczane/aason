static void* aason_default_alloc(void* user_data, size_t size, size_t align, const char* function, const char* file, int line)
{
	(void)user_data;
	(void)function;
	(void)file;
	(void)line;

#if defined(_WIN32)
	return _aligned_malloc(size, align);
#else
	return aligned_alloc(align, size);
#endif
}

static void* aason_default_realloc(void* user_data, void* ptr, size_t old_size, size_t new_size, size_t align, const char* function, const char* file, int line)
{
	(void)user_data;
	(void)function;
	(void)file;
	(void)line;

#if defined(_WIN32)
	(void)old_size;

	return _aligned_realloc(ptr, new_size, align);
#else
	// There is no standard function for realloc with alignment
	void* new_ptr = aligned_alloc(align, new_size);
	if (new_ptr)
	{
		if (ptr)
		{
			const size_t copy_size = old_size < new_size ? old_size : new_size;
			memcpy(new_ptr, ptr, copy_size);
			free(ptr);
		}
	}

	return new_ptr;
#endif
}

static void aason_default_free(void* user_data, void* ptr, const char* function, const char* file, int line)
{
	(void)user_data;
	(void)function;
	(void)file;
	(void)line;

#if defined(_WIN32)
	_aligned_free(ptr);
#else
	free(ptr);
#endif
}

static void* aason_default_push(void* user_data)
{
	(void)user_data;

	return NULL;
}

static void aason_default_pop(void* user_data, void* frame)
{
	(void)user_data;
	(void)frame;
}

static void* aason_default_open(void* user_data, const char* path, size_t* size)
{
	(void)user_data;

	FILE* file = fopen(path, "rb");
	if (file)
	{
		fseek(file, 0, SEEK_END);
		long length = ftell(file);
		*size = (size_t)length;
	}

	return file;
}

static void aason_default_read(void* user_data, void* stream, void* buffer, size_t offset, size_t size)
{
	(void)user_data;

	FILE* file = (FILE*)stream;
	fseek(file, (long)offset, SEEK_SET);
	fread(buffer, 1, size, file);
}

static void aason_default_close(void* user_data, void* stream)
{
	(void)user_data;

	FILE* file = (FILE*)stream;
	fclose(file);
}

static const aason_allocator aason_default_allocator = {
	.alloc		= aason_default_alloc,
	.realloc	= aason_default_realloc,
	.free		= aason_default_free,
	.push		= aason_default_push,
	.pop		= aason_default_pop
};

static const aason_read_interface aason_default_read_interface = {
	.open	= aason_default_open,
	.read	= aason_default_read,
	.close	= aason_default_close
};