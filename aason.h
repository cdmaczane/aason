#define AASON_KEY_MAX_LEN		32
#define AASON_ENUM_MAX_LEN		32
#define AASON_WRITE_BUFFER_SIZE	(RGS_PAGE_SIZE - sizeof(aason_context))

typedef enum
{
	aason_optional,
	aason_required
} aason_flags;

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

typedef enum
{
	aason_error_none,
	// Parse errors
	aason_error_invalid_key,
	aason_error_invalid_char,
	aason_error_key_expected,
	aason_error_out_of_range,
	aason_error_multiple_roots,
	aason_error_colon_expected,
	aason_error_unexpected_eof,
	aason_error_unexpected_char,
	aason_error_unexpected_token,
	aason_error_expected_element,
	aason_error_expected_identifier,
	aason_error_mismatched_array_depth,
	aason_error_mismatched_object_depth,
	// Read errors
	aason_error_wrong_type,
	aason_error_end_of_array,
	aason_error_invalid_enum,
	aason_error_key_not_found,
	aason_error_buffer_too_small
} aason_error;

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

typedef void (*aason_error_callback)(void* user_data, aason_error error, uint32_t line, uint32_t column, const char* str);
typedef void (*aason_write_callback)(void* user_data, const char* str, int64_t size);

// TODO: Allow mechanism to get line and column of last element read for external error handling.
// TODO: Save error message for when not using error callbacks.
// TODO: Can probably compact as some data will no longer be required once error has occurred.
// TODO: Consider renaming elements within objects to "fields".
typedef struct
{
	char*							buffer;
	uint16_t						error;
	bool							reading;
	uint32_t						stack_depth;
	void*							user_data;

	union
	{
		struct
		{
			aason_stack_entry*		stack;
			aason_element*			elements;
			uint32_t				element_count;
			uint32_t				max_stack_depth;
			uint32_t				error_line;
			uint32_t				error_column;
			aason_error_callback	error_callback;
		};

		struct
		{
			bool					first;
			bool					first_line;
			int64_t					offset;
			aason_write_callback	write_str;
		};
	};
} aason_context;
static_assert(sizeof(aason_context) == 64);

aason_context* aason_read(rgs_allocator allocator, char* src, int64_t size, uint32_t tab_size, aason_error_callback callback, void* user_data);
aason_context* aason_write(aason_write_callback callback, void* user_data);
void aason_destroy(aason_context* ctx);
bool aason_reading(const aason_context* ctx);
bool aason_writing(const aason_context* ctx);

bool aason_read_array_enter(aason_context* ctx, const char* key, aason_flags flags, int64_t* size, int64_t max_size);
void aason_read_array_leave(aason_context* ctx);
bool aason_read_array_enter_object(aason_context* ctx);
void aason_read_array_leave_object(aason_context* ctx);
bool aason_read_array_str(aason_context* ctx, const char** value, int64_t* len);
bool aason_read_array_fixed_str(aason_context* ctx, char* value, int64_t buffer_size, bool truncate);
bool aason_read_array_int(aason_context* ctx, int64_t* value);
bool aason_read_array_int_ranged(aason_context* ctx, int64_t* value, int64_t min, int64_t max);
bool aason_read_array_bool(aason_context* ctx, bool* value);
bool aason_read_array_hash(aason_context* ctx, uint32_t* value);
bool aason_read_array_float(aason_context* ctx, float* value);
bool aason_read_array_enum(aason_context* ctx, int32_t* value, const char** strings, int32_t count);
bool aason_read_object_enter(aason_context* ctx, const char* key, aason_flags flags);
void aason_read_object_leave(aason_context* ctx);
bool aason_read_object_str(aason_context* ctx, const char* key, aason_flags flags, const char** value, int64_t* len);
bool aason_read_object_fixed_str(aason_context* ctx, const char* key, aason_flags flags, char* value, int64_t buffer_size, bool truncate);
bool aason_read_object_int(aason_context* ctx, const char* key, aason_flags flags, int64_t* value);
bool aason_read_object_int_ranged(aason_context* ctx, const char* key, aason_flags flags, int64_t* value, int64_t min, int64_t max);
bool aason_read_object_bool(aason_context* ctx, const char* key, aason_flags flags, bool* value);
bool aason_read_object_hash(aason_context* ctx, const char* key, aason_flags flags, uint32_t* value);
bool aason_read_object_float(aason_context* ctx, const char* key, aason_flags flags, float* value);
bool aason_read_object_enum(aason_context* ctx, const char* key, aason_flags flags, int32_t* value, const char** strings, int32_t count);

void aason_write_array_enter(aason_context* ctx, const char* key);
void aason_write_array_leave(aason_context* ctx);
void aason_write_array_object_enter(aason_context* ctx);
void aason_write_array_object_leave(aason_context* ctx);
void aason_write_array_str(aason_context* ctx, const char* value);
void aason_write_array_int(aason_context* ctx, int64_t value);
void aason_write_array_bool(aason_context* ctx, bool value);
void aason_write_array_hash(aason_context* ctx, uint32_t value);
void aason_write_array_float(aason_context* ctx, float value);
void aason_write_array_enum(aason_context* ctx, int32_t value, const char** strings, int32_t count);
void aason_write_object_enter(aason_context* ctx, const char* key);
void aason_write_object_leave(aason_context* ctx);
void aason_write_object_str(aason_context* ctx, const char* key, const char* value);
void aason_write_object_int(aason_context* ctx, const char* key, int64_t value);
void aason_write_object_bool(aason_context* ctx, const char* key, bool value);
void aason_write_object_hash(aason_context* ctx, const char* key, uint32_t value);
void aason_write_object_float(aason_context* ctx, const char* key, float value);
void aason_write_object_enum(aason_context* ctx, const char* key, int32_t value, const char** strings, int32_t count);

bool aason_archive_array_enter(aason_context* ctx, const char* key, int64_t* size);
void aason_archive_array_leave(aason_context* ctx);
bool aason_archive_array_object_enter(aason_context* ctx);
void aason_archive_array_object_leave(aason_context* ctx);
void aason_archive_array_str(aason_context* ctx, char** value);
void aason_archive_array_fixed_str(aason_context* ctx, char* value, int64_t buffer_size, bool truncate);
void aason_archive_array_int(aason_context* ctx, int64_t* value);
void aason_archive_array_bool(aason_context* ctx, bool* value);
void aason_archive_array_hash(aason_context* ctx, uint32_t* value);
void aason_archive_array_float(aason_context* ctx, float* value);
void aason_archive_array_enum(aason_context* ctx, int32_t* value, const char** strings, int32_t count);
bool aason_archive_object_enter(aason_context* ctx, const char* key);
void aason_archive_object_leave(aason_context* ctx);
bool aason_archive_object_str(aason_context* ctx, const char* key, char** value, const char* default_value);
bool aason_archive_object_fixed_str(aason_context* ctx, const char* key, char* value, const char* default_value, int64_t buffer_size, bool truncate);
bool aason_archive_object_int(aason_context* ctx, const char* key, int64_t* value, int64_t default_value);
bool aason_archive_object_bool(aason_context* ctx, const char* key, bool* value, bool default_value);
bool aason_archive_object_hash(aason_context* ctx, const char* key, uint32_t* value, uint32_t default_value);
bool aason_archive_object_float(aason_context* ctx, const char* key, float* value, float default_value);
bool aason_archive_object_enum(aason_context* ctx, const char* key, int32_t* value, int32_t default_value, const char** strings, int32_t count);