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
	aason_error_buffer_too_small
} aason_error;

typedef void* (*aason_allocator)(void* user_data, void* ptr, size_t old_size, size_t new_size);
typedef void (*aason_format)(char* out, size_t size, const char* format, va_list args);
typedef void (*aason_error_callback)(void* user_data, aason_error error, uint32_t line, uint32_t column, const char* str);
typedef void (*aason_write_callback)(void* user_data, const char* str, int64_t size);

typedef struct
{
	char*					source;
	size_t					length;
	uint32_t				tab_size;
	aason_error_callback	error_callback;
	void*					error_data;
	aason_allocator			allocator;
	void*					allocator_data;
	aason_allocator			scratch;
	void*					scratch_data;
	aason_format			format_string;
} aason_read_desc;

typedef struct
{
	void*					buffer;
	size_t					buffer_size;
	aason_write_callback	callback;
	void*					user_data;
} aason_write_desc;

aason_context* aason_read(const aason_read_desc* desc);
aason_context* aason_write(const aason_write_desc* desc);
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

#ifdef __cplusplus
}
#endif