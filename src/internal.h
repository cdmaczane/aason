enum
{
	aason_hash_len		= 9,	// Includes null terminator
	aason_int_max_len	= 21,	// Includes null terminator
	aason_bool_max_len	= 5,
	aason_float_max_len	= 16	// Includes null terminator
};

// TODO: Add flags type that separates enum values using '|'
typedef enum
{
	aason_type_str = 1,
	aason_type_int,
	aason_type_bool,
	aason_type_hash,
	aason_type_enum,
	aason_type_float,
	aason_type_array,
	aason_type_object
} aason_type;

typedef struct
{
	aason_type			type;
	uint32_t			key_offset;
	uint32_t			key_len;
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
		uint32_t		hash_value;
		float			float_value;
	};
} aason_element;
static_assert(sizeof(aason_element) == 32);

typedef struct
{
	uint32_t element_index;
	uint32_t array_index;
} aason_stack_entry;

// TODO: Allow mechanism to get line and column of last element read for external error handling.
// TODO: Can probably compact as some data will no longer be required once error has occurred.
// TODO: Consider renaming elements within objects to "fields".
struct aason_context
{
	char*							buffer;
	uint16_t						error;
	bool							reading;
	uint32_t						stack_depth;
	void*							user_data;
	aason_locale					locale;

	union
	{
		// TODO: C++ doesn't support anonymous structs
		struct
		{
			const aason_read_desc*	read_desc;
			aason_stack_entry*		stack;
			aason_element*			elements;
			uint32_t				element_count;
			uint32_t				max_stack_depth;
			uint32_t				error_line;
			uint32_t				error_column;
			//aason_error_callback	error_callback;
		};

		struct
		{
			size_t					buffer_size;
			bool					first;
			bool					first_line;
			size_t					reserved;
			size_t					offset; // TODO: Rename to committed
			aason_write_callback	write_str;
		};
	};
};
//static_assert(sizeof(aason_context) == 64);

static char*	aason_to_string_int(char* out, int64_t value);
static char*	aason_to_string_bool(char* out, bool value);
static char*	aason_to_string_hash(char* out, uint32_t value);
static char*	aason_to_string_float(aason_locale locale, char* out, float value);

static bool		aason_from_string_bin(const char* begin, const char* end, int64_t* value);
static bool		aason_from_string_dec(const char* begin, const char* end, int64_t* value);
static bool		aason_from_string_hex(const char* begin, const char* end, int64_t* value);
static bool		aason_from_string_hash(const char* begin, const char* end, uint32_t* value);
static bool		aason_from_string_float(aason_locale locale, const char* begin, const char* end, float* value);

static void		aason_format_string(aason_context* ctx, char* out, size_t size, const char* format, va_list args);