#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Check compiler language version is supported
#ifdef __cplusplus
	// MSVC Still reports C++98 without /Zc:__cplusplus
	#ifdef _MSVC_LANG
		#if _MSVC_LANG < 201103L
			#error AASON requires C++11 of later
		#endif
	#elif __cplusplus < 201103L
		#error AASON requires C++11 of later
	#endif
#elif __STDC_VERSION__ < 201112L
	#error AASON requires C11 or later
#endif

#ifdef __cplusplus
	#define aason_restrict __restrict
#else
	#define aason_restrict restrict
#endif

#ifdef __cplusplus
	#define aason_noreturn [[noreturn]]
#elif __STDC_VERSION__ >= 202311L
	#define aason_noreturn [[noreturn]]
#else
	#define aason_noreturn _Noreturn // Deprecated in C23
#endif

#ifdef __cplusplus
	#define aason_alignof(type) alignof(type)
#elif __STDC_VERSION__ >= 202311L
	#define aason_alignof(type) alignof(type)
#else
	#define aason_alignof(type) _Alignof(type) // Deprecated in C23
#endif

#define aason_alloc_type(ctx, type)			aason_alloc_value_impl((ctx), sizeof(type), aason_alignof(type), __func__, __FILE__, __LINE__)
#define aason_alloc_value(ctx, size, align)	aason_alloc_value_impl((ctx), (size), (align), __func__, __FILE__, __LINE__)
#define aason_free_value(ctx, ptr)			aason_free_value_impl((ctx), (ptr), __func__, __FILE__, __LINE__)

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
	aason_error_recursion,
	aason_error_allocation,
	aason_error_empty_file,
	aason_error_invalid_key,
	aason_error_invalid_char,
	aason_error_key_expected,
	aason_error_out_of_range,
	aason_error_file_not_found,
	aason_error_multiple_roots,
	aason_error_colon_expected,
	aason_error_unexpected_eof,
	aason_error_trailing_comma,
	aason_error_unexpected_char,
	aason_error_unexpected_token,
	aason_error_expected_element,
	aason_error_expected_identifier,
	aason_error_unknown_constructor,
	aason_error_mismatched_array_depth,
	aason_error_mismatched_object_depth,
	// Read errors
	aason_error_wrong_type,
	aason_error_unused_field,
	aason_error_end_of_array,
	aason_error_invalid_enum,
	aason_error_key_not_found,
	aason_error_invalid_range,
	aason_error_array_too_large,
	aason_error_buffer_too_small,
	aason_error_invalid_constructor,

	aason_error_count
} aason_error_type;

typedef struct
{
	void*	user_data;
	void	(*error)(void* user_data, aason_error_type error, const char* file, uint32_t line, uint32_t column, const char* msg);
} aason_error_interface;

typedef struct
{
	void*	user_data;
	void*	(*open)(void* user_data, const char* path, size_t* size);
	void	(*read)(void* user_data, void* stream, void* buffer, size_t offset, size_t size);
	void	(*close)(void* user_data, void* stream);
} aason_read_interface;

typedef struct
{
	void*	user_data;
	void*	(*open)(void* user_data, const char* path);
	void	(*write)(void* user_data, void* stream, const void* buffer, size_t size);
	void	(*close)(void* user_data, void* stream);
} aason_write_interface;

typedef struct
{
	void*	user_data;
	void*	(*alloc)(void* user_data, size_t size, size_t align, const char* function, const char* file, int line);
	void*	(*realloc)(void* user_data, void* ptr, size_t old_size, size_t new_size, size_t align, const char* function, const char* file, int line);
	void	(*free)(void* user_data, void* ptr, const char* function, const char* file, int line);
	void*	(*push)(void* user_data);
	void	(*pop)(void* user_data, void* frame);
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

/*
	AASON supports reading and writing standard JSON using its strongly typed API. It is intended
	for applications that need to serialise and deserialise known data structures, rather than
	inspect or manipulate arbitrary JSON documents. Some features are disabled in JSON mode such as
	#include and user-defined constructors. Other features can optionally be enabled such as
	trailing commas. Comments are always supported despite not being supported by the format.
*/
typedef enum
{
	aason_mode_aason,
	aason_mode_json
} aason_mode;

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
	void					(*write)(aason_context* ctx, const void* value);
} aason_constructor_desc;

typedef struct
{
	const char*						path;
	aason_mode						mode;
	bool							use_implicit_root;
	bool							check_unused_fields;
	bool							allow_trailing_commas;
	uint32_t						tab_size;
	const aason_error_interface*	error_interface;
	const aason_read_interface*		read_interface;
	const aason_allocator*			allocator;
	const aason_allocator*			value_allocator;
	const aason_allocator*			scratch_allocator;
	const aason_constructor_desc**	constructors;
	size_t							constructor_count;
} aason_read_desc;

/*
	TODO:
	* Add flag to use spaces instead of tabs.
	* Add flag to put opening braces/brackets on their own lines.
	* Add JSON writing mode.
*/
typedef struct
{
	const char*						path;
	uint32_t						tab_size;
	size_t							buffer_size;
	const aason_error_interface*	error_interface;
	const aason_write_interface*	write_interface;
	const aason_allocator*			allocator;
	const aason_allocator*			value_allocator;
	const aason_constructor_desc**	constructors;
	size_t							constructor_count;
} aason_write_desc;

aason_context*	aason_preparse(const aason_read_desc* desc);
bool			aason_parse(aason_context* ctx);
const char*		aason_get_path(const aason_context* ctx, size_t index);
size_t			aason_get_path_count(const aason_context* ctx);

aason_context* aason_read(const aason_read_desc* desc);
aason_context* aason_write(const aason_write_desc* desc);
void aason_destroy(aason_context* ctx);
bool aason_reading(const aason_context* ctx);
bool aason_writing(const aason_context* ctx);

void aason_error(aason_context* ctx, aason_error_type error, const char* format, ...);
const char* aason_get_error_string(aason_error_type error);

void* aason_alloc_value_impl(aason_context* ctx, size_t size, size_t align, const char* func, const char* file, int line);
void aason_free_value_impl(aason_context* ctx, void* value, const char* func, const char* file, int line);

bool aason_read_enter_root_array(aason_context* ctx, size_t* size, size_t max_size);
bool aason_read_enter_root_object(aason_context* ctx);
void aason_read_leave_array(aason_context* ctx);
void aason_read_leave_object(aason_context* ctx);
bool aason_read_array_enter_object(aason_context* ctx);
bool aason_read_array_fixed_str(aason_context* ctx, char* value, size_t buffer_size, bool truncate);
bool aason_read_array_borrowed_str(aason_context* ctx, const char** value, size_t* len);
bool aason_read_array_duplicated_str(aason_context* ctx, char** value);
bool aason_read_array_int(aason_context* ctx, int64_t* value);
bool aason_read_array_int_ranged(aason_context* ctx, int64_t* value, int64_t min, int64_t max);
bool aason_read_array_bool(aason_context* ctx, bool* value);
bool aason_read_array_float(aason_context* ctx, float* value);
bool aason_read_array_enum(aason_context* ctx, int32_t* value, const char** strings, int32_t count);
bool aason_read_array_constructor(aason_context* ctx, void* value, const char* type);
bool aason_read_object_enter_array(aason_context* ctx, const char* key, aason_flags flags, size_t* size, size_t max_size);
bool aason_read_object_enter_object(aason_context* ctx, const char* key, aason_flags flags);
bool aason_read_object_fixed_str(aason_context* ctx, const char* key, aason_flags flags, char* value, size_t buffer_size, bool truncate);
bool aason_read_object_borrowed_str(aason_context* ctx, const char* key, aason_flags flags, const char** value, size_t* len);
bool aason_read_object_duplicated_str(aason_context* ctx, const char* key, aason_flags flags, char** value);
bool aason_read_object_int(aason_context* ctx, const char* key, aason_flags flags, int64_t* value);
bool aason_read_object_int_ranged(aason_context* ctx, const char* key, aason_flags flags, int64_t* value, int64_t min, int64_t max);
bool aason_read_object_bool(aason_context* ctx, const char* key, aason_flags flags, bool* value);
bool aason_read_object_float(aason_context* ctx, const char* key, aason_flags flags, float* value);
bool aason_read_object_enum(aason_context* ctx, const char* key, aason_flags flags, int32_t* value, const char** strings, int32_t count);
bool aason_read_object_constructor(aason_context* ctx, const char* key, aason_flags flags, void* value, const char* type);

void aason_write_enter_root_array(aason_context* ctx);
void aason_write_enter_root_object(aason_context* ctx);
void aason_write_leave_array(aason_context* ctx);
void aason_write_leave_object(aason_context* ctx);
void aason_write_array_enter_object(aason_context* ctx);
void aason_write_array_str(aason_context* ctx, const char* value);
void aason_write_array_int(aason_context* ctx, int64_t value);
void aason_write_array_dec(aason_context* ctx, int64_t value, int min_digits);
void aason_write_array_hex(aason_context* ctx, int64_t value, int min_digits);
void aason_write_array_bin(aason_context* ctx, int64_t value, int min_digits);
void aason_write_array_bool(aason_context* ctx, bool value);
void aason_write_array_float(aason_context* ctx, float value);
void aason_write_array_enum(aason_context* ctx, int32_t value, const char** strings, int32_t count);
void aason_write_array_constructor(aason_context* ctx, const void* value, const char* type);
void aason_write_object_enter_array(aason_context* ctx, const char* key);
void aason_write_object_enter_object(aason_context* ctx, const char* key);
void aason_write_object_str(aason_context* ctx, const char* key, const char* value);
void aason_write_object_int(aason_context* ctx, const char* key, int64_t value);
void aason_write_object_dec(aason_context* ctx, const char* key, int64_t value, int min_digits);
void aason_write_object_hex(aason_context* ctx, const char* key, int64_t value, int min_digits);
void aason_write_object_bin(aason_context* ctx, const char* key, int64_t value, int min_digits);
void aason_write_object_bool(aason_context* ctx, const char* key, bool value);
void aason_write_object_float(aason_context* ctx, const char* key, float value);
void aason_write_object_enum(aason_context* ctx, const char* key, int32_t value, const char** strings, int32_t count);
void aason_write_object_constructor(aason_context* ctx, const char* key, const void* value, const char* type);
void aason_write_arg_str(aason_context* ctx, const char* value);
void aason_write_arg_int(aason_context* ctx, int64_t value);
void aason_write_arg_dec(aason_context* ctx, int64_t value, int min_digits);
void aason_write_arg_hex(aason_context* ctx, int64_t value, int min_digits);
void aason_write_arg_bin(aason_context* ctx, int64_t value, int min_digits);
void aason_write_arg_bool(aason_context* ctx, bool value);
void aason_write_arg_float(aason_context* ctx, float value);
void aason_write_arg_enum(aason_context* ctx, int32_t value, const char** strings, int32_t count);

bool aason_archive_enter_root_array(aason_context* ctx, size_t* size);
bool aason_archive_enter_root_object(aason_context* ctx);
void aason_archive_leave_array(aason_context* ctx);
void aason_archive_leave_object(aason_context* ctx);
bool aason_archive_array_enter_object(aason_context* ctx);
void aason_archive_array_fixed_str(aason_context* ctx, char* value, size_t buffer_size, bool truncate);
void aason_archive_array_duplicated_str(aason_context* ctx, char** value);
void aason_archive_array_int(aason_context* ctx, int64_t* value);
void aason_archive_array_dec(aason_context* ctx, int64_t* value, int min_digits);
void aason_archive_array_hex(aason_context* ctx, int64_t* value, int min_digits);
void aason_archive_array_bin(aason_context* ctx, int64_t* value, int min_digits);
void aason_archive_array_bool(aason_context* ctx, bool* value);
void aason_archive_array_float(aason_context* ctx, float* value);
void aason_archive_array_enum(aason_context* ctx, int32_t* value, const char** strings, int32_t count);
void aason_archive_array_constructor(aason_context* ctx, void* value, const char* type);
bool aason_archive_object_enter_array(aason_context* ctx, const char* key, size_t* size);
bool aason_archive_object_enter_object(aason_context* ctx, const char* key);
bool aason_archive_object_fixed_str(aason_context* ctx, const char* key, char* value, size_t buffer_size, bool truncate);
bool aason_archive_object_duplicated_str(aason_context* ctx, const char* key, char** value);
bool aason_archive_object_int(aason_context* ctx, const char* key, int64_t* value);
bool aason_archive_object_dec(aason_context* ctx, const char* key, int64_t* value, int min_digits);
bool aason_archive_object_hex(aason_context* ctx, const char* key, int64_t* value, int min_digits);
bool aason_archive_object_bin(aason_context* ctx, const char* key, int64_t* value, int min_digits);
bool aason_archive_object_bool(aason_context* ctx, const char* key, bool* value);
bool aason_archive_object_float(aason_context* ctx, const char* key, float* value);
bool aason_archive_object_enum(aason_context* ctx, const char* key, int32_t* value, const char** strings, int32_t count);
bool aason_archive_object_constructor(aason_context* ctx, const char* key, void* value, const char* type);

#ifdef __cplusplus
}
#endif