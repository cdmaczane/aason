#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct aason_context aason_context;

typedef enum
{
	aason_optional,
	aason_required
} aason_flags;

// TODO: Currently unused
typedef enum
{
	aason_base_bin,
	aason_base_dec,
	aason_base_hex
} aason_base;

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
	aason_error_invalid_range,
	aason_error_array_too_large,
	aason_error_buffer_too_small
} aason_error_type;

typedef struct
{
	void*	user_data;
	void	(*error)(void* user_data, aason_error_type error, const char* file, uint32_t line, uint32_t column, const char* msg);
} aason_error_interface;

typedef struct
{
	void*	self;
	void*	(*open)(void* self, const char* path, size_t* size);
	void	(*read)(void* self, void* stream, void* buffer, size_t offset, size_t size);
	void	(*close)(void* self, void* stream);
} aason_read_interface;

typedef struct
{
	void*	self;
	void*	(*open)(void* self, const char* path);
	void	(*write)(void* self, void* stream, const void* buffer, size_t size);
	void	(*close)(void* self, void* stream);
} aason_write_interface;

typedef struct
{
	void*	self;
	void*	(*alloc)(void* self, size_t size, size_t align, const char* function, const char* file, int line);
	void*	(*realloc)(void* self, void* ptr, size_t size, size_t align, const char* function, const char* file, int line);
	void	(*free)(void* self, void* ptr, const char* function, const char* file, int line);
	void*	(*push)(void* self);
	void	(*pop)(void* self, void* frame);
} aason_allocator;

typedef enum
{
	aason_arg_type_str,
	aason_arg_type_bin,
	aason_arg_type_dec,
	aason_arg_type_hex,
	aason_arg_type_bool,
	aason_arg_type_enum,
	aason_arg_type_float
} aason_arg_type;

typedef struct
{
	aason_arg_type	type;
	union
	{
		const char*	str_value;
		int64_t		int_value;
		bool		bool_value;
		const char*	enum_value;
		float		float_value;
	};
} aason_arg;

typedef struct
{
	const char*				name;
	const aason_arg_type*	args;
	size_t					count;
	void					(*read)(aason_context* ctx, void* value, const aason_arg* args, size_t count);
	void					(*write)(aason_context* ctx, void* value);
} aason_constructor_desc;

typedef void (*aason_format_callback)(char* out, size_t size, const char* format, va_list args);

typedef struct
{
	const char*						path;
	uint32_t						tab_size;
	aason_format_callback			format;
	const aason_error_interface*	error_interface;
	const aason_read_interface*		read_interface;
	const aason_allocator*			allocator;
	const aason_allocator*			value_allocator;
	const aason_allocator*			scratch_allocator;
	const aason_constructor_desc**	constructors;
	size_t							constructor_count;
} aason_read_desc;

typedef struct
{
	const char*						path;
	uint32_t						tab_size;
	size_t							buffer_size;
	aason_format_callback			format;
	const aason_error_interface*	error_interface;
	const aason_write_interface*	write_interface;
	const aason_allocator*			allocator;
	const aason_constructor_desc*	constructors;
	size_t							constructor_count;
} aason_write_desc;

aason_context* aason_read(const aason_read_desc* desc);
aason_context* aason_write(const aason_write_desc* desc);
void aason_destroy(aason_context* ctx);
bool aason_reading(const aason_context* ctx);
bool aason_writing(const aason_context* ctx);

void aason_error(aason_context* ctx, aason_error_type type, const char* format, ...);

bool aason_read_array_enter(aason_context* ctx, const char* key, aason_flags flags, int64_t* size, int64_t max_size);
void aason_read_array_leave(aason_context* ctx);
bool aason_read_array_enter_object(aason_context* ctx);
void aason_read_array_leave_object(aason_context* ctx);
bool aason_read_array_str(aason_context* ctx, const char** value, int64_t* len);
bool aason_read_array_fixed_str(aason_context* ctx, char* value, int64_t buffer_size, bool truncate);
bool aason_read_array_int(aason_context* ctx, int64_t* value);
bool aason_read_array_int_ranged(aason_context* ctx, int64_t* value, int64_t min, int64_t max);
bool aason_read_array_bool(aason_context* ctx, bool* value);
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
bool aason_read_object_constructor(aason_context* ctx, const char* key, aason_flags flags, void* value, const char* type);

void aason_write_array_enter(aason_context* ctx, const char* key);
void aason_write_array_leave(aason_context* ctx);
void aason_write_array_object_enter(aason_context* ctx);
void aason_write_array_object_leave(aason_context* ctx);
void aason_write_array_str(aason_context* ctx, const char* value);
void aason_write_array_int(aason_context* ctx, int64_t value);
void aason_write_array_bool(aason_context* ctx, bool value);
void aason_write_array_float(aason_context* ctx, float value);
void aason_write_array_enum(aason_context* ctx, int32_t value, const char** strings, int32_t count);
void aason_write_object_enter(aason_context* ctx, const char* key);
void aason_write_object_leave(aason_context* ctx);
void aason_write_object_str(aason_context* ctx, const char* key, const char* value);
void aason_write_object_int(aason_context* ctx, const char* key, int64_t value);
void aason_write_object_bool(aason_context* ctx, const char* key, bool value);
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
void aason_archive_array_float(aason_context* ctx, float* value);
void aason_archive_array_enum(aason_context* ctx, int32_t* value, const char** strings, int32_t count);
bool aason_archive_object_enter(aason_context* ctx, const char* key);
void aason_archive_object_leave(aason_context* ctx);
bool aason_archive_object_str(aason_context* ctx, const char* key, char** value, const char* default_value);
bool aason_archive_object_fixed_str(aason_context* ctx, const char* key, char* value, const char* default_value, int64_t buffer_size, bool truncate);
bool aason_archive_object_int(aason_context* ctx, const char* key, int64_t* value, int64_t default_value);
bool aason_archive_object_bool(aason_context* ctx, const char* key, bool* value, bool default_value);
bool aason_archive_object_float(aason_context* ctx, const char* key, float* value, float default_value);
bool aason_archive_object_enum(aason_context* ctx, const char* key, int32_t* value, int32_t default_value, const char** strings, int32_t count);

#ifdef __cplusplus
}
#endif