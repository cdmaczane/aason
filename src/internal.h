#define aason_alloc(allocator, size)		(allocator)->alloc((allocator)->self, (size), sizeof(void*), __func__, __FILE__, __LINE__)
#define aason_free(allocator, ptr)			(allocator)->free((allocator)->self, (ptr), __func__, __FILE__, __LINE__)
#define aason_realloc(allocator, ptr, size)	(allocator)->realloc((allocator)->self, (ptr), (size), sizeof(void*), __func__, __FILE__, __LINE__)

enum
{
	aason_int_max_len	= 21,	// Includes null terminator
	aason_bool_max_len	= 5,
	aason_float_max_len	= 16	// Includes null terminator
};

enum
{
	aason_char_op_eof,
	aason_char_op_jump,
	aason_char_op_skip_line_short,
	aason_char_op_skip_line_long,
	aason_char_op_skip_columns_short,
	aason_char_op_skip_columns_long
};

// TODO: Add flags type that separates enum values using '|'
typedef enum
{
	aason_type_str = 1,
	aason_type_int,
	aason_type_bool,
	aason_type_enum,
	aason_type_float,
	aason_type_array,
	aason_type_object,
	aason_type_constructor
} aason_type;

typedef enum
{
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

// TODO: I think if we add a union we can remove some 16-bit limitations
typedef struct
{
	uint16_t			type;
	uint16_t			file_index;
	uint16_t			constructor_index;
	uint16_t			count;
	char*				begin;
	char*				end;
	uint32_t			line;
	uint32_t			column;
} aason_token;
static_assert(sizeof(aason_token) == 32);

typedef struct
{
	aason_token*	tokens;
	uint32_t		count;
	uint32_t		capacity;
} aason_tokens;

typedef struct
{
	char*		buffer;			// Buffer to path string followed by the file data and a null terminator
	uint32_t	size;			// Does not include the null terminator which simplifies parsing
	uint32_t	offset;			// Buffer starts with path string, so this is the offset to the actual data
	uint32_t	token_index;	// Index of first token from this file in global token array
	uint32_t	return_file;
	uint32_t	return_index;
	//uint32_t	token_count;
	bool		inside;			// Used to check for recursion
} aason_file;

typedef struct
{
	aason_file*	files;
	uint32_t	count;
	uint32_t	capacity;
} aason_files;

typedef struct
{
	aason_context*	ctx;
	aason_tokens*	tokens;
	uint16_t		file_index;
	uint32_t		token_index;
} aason_token_state;

/*
	TODO:
	* Store value offset and remove line and column.
	* Instead calculate line and column on demand when an error occurs.
	* Move constructor index into union, making the index and count both uint16_t.
	* Now that we have value offset, we can replace offset with a hash in enum.
	* With these changes the key and value file indices could probably be uint32_t.
	* Create a parallel array of uint32_t key hashes to improve speed of field searches.
	* Maintain field search cursor, assuming that subsequent fields are likely to match data and parse order.
*/
typedef struct
{
	aason_type			type;
	uint16_t			key_file_index;
	uint16_t			value_file_index;
	uint16_t			constructor_index;
	uint16_t			key_len;
	uint32_t			key_offset;
	uint32_t			line;
	uint32_t			column;

	union
	{
		struct
		{
			uint32_t	count;
			uint32_t	first_child;
		} array_value;

		struct
		{
			uint32_t	count;
			uint32_t	first_child;
		} object_value;

		struct
		{
			uint32_t	count;
			uint32_t	first_child;
		} constructor_value;

		struct
		{
			uint32_t	offset;
			uint32_t	len;
		} str_value;

		struct
		{
			uint32_t	offset;
			uint32_t	len;
		} enum_value;

		int64_t			int_value;
		bool			bool_value;
		float			float_value;
	};
} aason_element;
static_assert(sizeof(aason_element) == 32);

typedef struct
{
	uint32_t element_index;
	uint32_t child_index;
} aason_stack_entry;

typedef void (*aason_read_constructor_callback)(aason_context* ctx, void* value, const aason_arg* args, size_t count);
typedef void (*aason_write_constructor_callback)(aason_context* ctx, void* value);

typedef struct
{
	uint32_t							count;

	// The following are arrays of size count
	const char**						names;
	uint32_t*							hashes;
	uint8_t*							arg_counts;
	uint32_t*							arg_indices;
	aason_read_constructor_callback*	read_callbacks;
	aason_write_constructor_callback*	write_callbacks;

	uint8_t*							args;
	aason_arg*							temp_args;
} aason_constructors;

// TODO: Allow mechanism to get line and column of last element read for external error handling.
// TODO: Can probably compact as some data will no longer be required once error has occurred.
// TODO: Consider renaming elements within objects to "fields".
struct aason_context
{
	//const char*						path;
	aason_error_type				error;
	uint32_t						tab_size;
	uint8_t							mode;
	bool							reading;
	bool							allow_trailing_commas;
	uint32_t						stack_depth;
	aason_locale					locale;
	aason_format_callback			format;
	aason_error_interface			error_interface;
	aason_allocator					scratch;
	aason_allocator					allocator;
	aason_constructors*				constructors;

	union
	{
		// TODO: C++ doesn't support anonymous structs
		struct
		{
			aason_stack_entry*		stack;
			aason_element*			elements;
			uint32_t				element_count;
			uint32_t				max_stack_depth;
			const char*				error_file;
			uint32_t				error_line;
			uint32_t				error_column;
			uint32_t				file_count;
			aason_file*				files;
		};

		struct
		{
			char*					buffer;
			size_t					buffer_size;
			bool					first;
			bool					first_line;
			size_t					reserved;
			size_t					committed;
			void*					stream;
			aason_write_interface	write_interface; // TODO: We only need the write function and state pointer
		};
	};
};
//static_assert(sizeof(aason_context) == 64);

static uint32_t		aason_utf8_byte_count(char c);

static char*		aason_to_string_int(char* out, int64_t value);
static char*		aason_to_string_bool(char* out, bool value);
static char*		aason_to_string_float(aason_locale locale, char* out, float value);

static bool			aason_from_string_bin(const char* begin, const char* end, int64_t* value);
static bool			aason_from_string_dec(const char* begin, const char* end, int64_t* value);
static bool			aason_from_string_hex(const char* begin, const char* end, int64_t* value);
static bool			aason_from_string_float(aason_locale locale, const char* begin, const char* end, float* value);

static void			aason_format_string(aason_context* ctx, char* out, size_t size, const char* format, va_list args);

static aason_token*	aason_get_next_token(aason_token_state* state);