// This file is part of Asteria.
// Copyleft 2018 - 2020, LH_Mouse. All wrongs reserved.

#ifndef ASTERIA_LIBRARY_FILESYSTEM_HPP_
#define ASTERIA_LIBRARY_FILESYSTEM_HPP_

#include "../fwd.hpp"

namespace Asteria {

// `std.filesystem.get_working_directory`
V_string
std_filesystem_get_working_directory();

// `std.filesystem.get_real_path`
V_string
std_filesystem_get_real_path(V_string path);

// `std.filesystem.get_information`
optV_object
std_filesystem_get_information(V_string path);

// `std.filesystem.move_from`
void
std_filesystem_move_from(V_string path_new, V_string path_old);

// `std.filesystem.remove_recursive`
V_integer
std_filesystem_remove_recursive(V_string path);

// `std.filesystem.directory_list`
optV_object
std_filesystem_directory_list(V_string path);

// `std.filesystem.directory_create`
V_integer
std_filesystem_directory_create(V_string path);

// `std.filesystem.directoryr_remove`
V_integer
std_filesystem_directory_remove(V_string path);

// `std.filesystem.file_read`
V_string
std_filesystem_file_read(V_string path, optV_integer offset, optV_integer limit);

// `std.filesystem.file_stream`
V_integer
std_filesystem_file_stream(Global_Context& global, V_string path, V_function callback,
                           optV_integer offset, optV_integer limit);

// `std.filesystem.file_write`
void
std_filesystem_file_write(V_string path, V_string data, optV_integer offset);

// `std.filesystem.file_append`
void
std_filesystem_file_append(V_string path, V_string data, optV_boolean exclusive);

// `std.filesystem.file_copy_from`
void
std_filesystem_file_copy_from(V_string path_new, V_string path_old);

// `std.filesystem.file_remove`
V_integer
std_filesystem_file_remove(V_string path);

// Create an object that is to be referenced as `std.filesystem`.
void
create_bindings_filesystem(V_object& result, API_Version version);

}  // namespace Asteria

#endif
