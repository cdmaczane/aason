#define RGS_SDD_KEY_MAX_LEN			32
#define RGS_SDD_ENUM_MAX_LEN		32
#define RGS_SDD_WRITE_BUFFER_SIZE	(RGS_PAGE_SIZE - sizeof(rgs_sdd))

typedef enum
{
	rgs_sdd_optional,
	rgs_sdd_required
} rgs_sdd_flags;

// TODO: Add flags type that separates enum values using '|'
typedef enum
{
	rgs_sdd_type_str = 1,
	rgs_sdd_type_int,
	rgs_sdd_type_bool,
	rgs_sdd_type_hash,
	rgs_sdd_type_enum,
	rgs_sdd_type_float,
	rgs_sdd_type_array,
	rgs_sdd_type_object
} rgs_sdd_type;

typedef enum
{
	rgs_sdd_error_none,
	// Parse errors
	rgs_sdd_error_invalid_key,
	rgs_sdd_error_invalid_char,
	rgs_sdd_error_key_expected,
	rgs_sdd_error_out_of_range,
	rgs_sdd_error_multiple_roots,
	rgs_sdd_error_colon_expected,
	rgs_sdd_error_unexpected_eof,
	rgs_sdd_error_unexpected_char,
	rgs_sdd_error_unexpected_token,
	rgs_sdd_error_expected_element,
	rgs_sdd_error_expected_identifier,
	rgs_sdd_error_mismatched_array_depth,
	rgs_sdd_error_mismatched_object_depth,
	// Read errors
	rgs_sdd_error_wrong_type,
	rgs_sdd_error_end_of_array,
	rgs_sdd_error_invalid_enum,
	rgs_sdd_error_key_not_found,
	rgs_sdd_error_buffer_too_small
} rgs_sdd_error;

typedef struct
{
	rgs_sdd_type		type;
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
} rgs_sdd_element;
static_assert(sizeof(rgs_sdd_element) == 32);

typedef struct
{
	uint32_t element_index;
	uint32_t array_index;
} rgs_sdd_stack_entry;

typedef void (*rgs_sdd_error_callback)(void* user_data, rgs_sdd_error error, uint32_t line, uint32_t column, const char* str);
typedef void (*rgs_sdd_write_callback)(void* user_data, const char* str, int64_t size);

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
			rgs_sdd_stack_entry*	stack;
			rgs_sdd_element*		elements;
			uint32_t				element_count;
			uint32_t				max_stack_depth;
			uint32_t				error_line;
			uint32_t				error_column;
			rgs_sdd_error_callback	error_callback;
		};

		struct
		{
			bool					first;
			bool					first_line;
			int64_t					offset;
			rgs_sdd_write_callback	write_str;
		};
	};
} rgs_sdd;
static_assert(sizeof(rgs_sdd) == 64);

rgs_sdd* rgs_sdd_read(rgs_allocator allocator, char* src, int64_t size, uint32_t tab_size, rgs_sdd_error_callback callback, void* user_data);
rgs_sdd* rgs_sdd_write(rgs_sdd_write_callback callback, void* user_data);
void rgs_sdd_destroy(rgs_sdd* sdd);
bool rgs_sdd_reading(const rgs_sdd* sdd);
bool rgs_sdd_writing(const rgs_sdd* sdd);

bool rgs_sdd_read_array_enter(rgs_sdd* sdd, const char* key, rgs_sdd_flags flags, int64_t* size, int64_t max_size);
void rgs_sdd_read_array_leave(rgs_sdd* sdd);
bool rgs_sdd_read_array_enter_object(rgs_sdd* sdd);
void rgs_sdd_read_array_leave_object(rgs_sdd* sdd);
bool rgs_sdd_read_array_str(rgs_sdd* sdd, const char** value, int64_t* len);
bool rgs_sdd_read_array_fixed_str(rgs_sdd* sdd, char* value, int64_t buffer_size, bool truncate);
bool rgs_sdd_read_array_int(rgs_sdd* sdd, int64_t* value);
bool rgs_sdd_read_array_int_ranged(rgs_sdd* sdd, int64_t* value, int64_t min, int64_t max);
bool rgs_sdd_read_array_bool(rgs_sdd* sdd, bool* value);
bool rgs_sdd_read_array_hash(rgs_sdd* sdd, uint32_t* value);
bool rgs_sdd_read_array_float(rgs_sdd* sdd, float* value);
bool rgs_sdd_read_array_enum(rgs_sdd* sdd, int32_t* value, const char** strings, int32_t count);
bool rgs_sdd_read_object_enter(rgs_sdd* sdd, const char* key, rgs_sdd_flags flags);
void rgs_sdd_read_object_leave(rgs_sdd* sdd);
bool rgs_sdd_read_object_str(rgs_sdd* sdd, const char* key, rgs_sdd_flags flags, const char** value, int64_t* len);
bool rgs_sdd_read_object_fixed_str(rgs_sdd* sdd, const char* key, rgs_sdd_flags flags, char* value, int64_t buffer_size, bool truncate);
bool rgs_sdd_read_object_int(rgs_sdd* sdd, const char* key, rgs_sdd_flags flags, int64_t* value);
bool rgs_sdd_read_object_int_ranged(rgs_sdd* sdd, const char* key, rgs_sdd_flags flags, int64_t* value, int64_t min, int64_t max);
bool rgs_sdd_read_object_bool(rgs_sdd* sdd, const char* key, rgs_sdd_flags flags, bool* value);
bool rgs_sdd_read_object_hash(rgs_sdd* sdd, const char* key, rgs_sdd_flags flags, uint32_t* value);
bool rgs_sdd_read_object_float(rgs_sdd* sdd, const char* key, rgs_sdd_flags flags, float* value);
bool rgs_sdd_read_object_enum(rgs_sdd* sdd, const char* key, rgs_sdd_flags flags, int32_t* value, const char** strings, int32_t count);

void rgs_sdd_write_array_enter(rgs_sdd* sdd, const char* key);
void rgs_sdd_write_array_leave(rgs_sdd* sdd);
void rgs_sdd_write_array_object_enter(rgs_sdd* sdd);
void rgs_sdd_write_array_object_leave(rgs_sdd* sdd);
void rgs_sdd_write_array_str(rgs_sdd* sdd, const char* value);
void rgs_sdd_write_array_int(rgs_sdd* sdd, int64_t value);
void rgs_sdd_write_array_bool(rgs_sdd* sdd, bool value);
void rgs_sdd_write_array_hash(rgs_sdd* sdd, uint32_t value);
void rgs_sdd_write_array_float(rgs_sdd* sdd, float value);
void rgs_sdd_write_array_enum(rgs_sdd* sdd, int32_t value, const char** strings, int32_t count);
void rgs_sdd_write_object_enter(rgs_sdd* sdd, const char* key);
void rgs_sdd_write_object_leave(rgs_sdd* sdd);
void rgs_sdd_write_object_str(rgs_sdd* sdd, const char* key, const char* value);
void rgs_sdd_write_object_int(rgs_sdd* sdd, const char* key, int64_t value);
void rgs_sdd_write_object_bool(rgs_sdd* sdd, const char* key, bool value);
void rgs_sdd_write_object_hash(rgs_sdd* sdd, const char* key, uint32_t value);
void rgs_sdd_write_object_float(rgs_sdd* sdd, const char* key, float value);
void rgs_sdd_write_object_enum(rgs_sdd* sdd, const char* key, int32_t value, const char** strings, int32_t count);

bool rgs_sdd_archive_array_enter(rgs_sdd* sdd, const char* key, int64_t* size);
void rgs_sdd_archive_array_leave(rgs_sdd* sdd);
bool rgs_sdd_archive_array_object_enter(rgs_sdd* sdd);
void rgs_sdd_archive_array_object_leave(rgs_sdd* sdd);
void rgs_sdd_archive_array_str(rgs_sdd* sdd, char** value);
void rgs_sdd_archive_array_fixed_str(rgs_sdd* sdd, char* value, int64_t buffer_size, bool truncate);
void rgs_sdd_archive_array_int(rgs_sdd* sdd, int64_t* value);
void rgs_sdd_archive_array_bool(rgs_sdd* sdd, bool* value);
void rgs_sdd_archive_array_hash(rgs_sdd* sdd, uint32_t* value);
void rgs_sdd_archive_array_float(rgs_sdd* sdd, float* value);
void rgs_sdd_archive_array_enum(rgs_sdd* sdd, int32_t* value, const char** strings, int32_t count);
bool rgs_sdd_archive_object_enter(rgs_sdd* sdd, const char* key);
void rgs_sdd_archive_object_leave(rgs_sdd* sdd);
bool rgs_sdd_archive_object_str(rgs_sdd* sdd, const char* key, char** value, const char* default_value);
bool rgs_sdd_archive_object_fixed_str(rgs_sdd* sdd, const char* key, char* value, const char* default_value, int64_t buffer_size, bool truncate);
bool rgs_sdd_archive_object_int(rgs_sdd* sdd, const char* key, int64_t* value, int64_t default_value);
bool rgs_sdd_archive_object_bool(rgs_sdd* sdd, const char* key, bool* value, bool default_value);
bool rgs_sdd_archive_object_hash(rgs_sdd* sdd, const char* key, uint32_t* value, uint32_t default_value);
bool rgs_sdd_archive_object_float(rgs_sdd* sdd, const char* key, float* value, float default_value);
bool rgs_sdd_archive_object_enum(rgs_sdd* sdd, const char* key, int32_t* value, int32_t default_value, const char** strings, int32_t count);