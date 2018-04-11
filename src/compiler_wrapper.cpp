//--------------------------------------------------------------------------------------------------
// Copyright (c) 2018 Marcus Geelnard
//
// This software is provided 'as-is', without any express or implied warranty. In no event will the
// authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose, including commercial
// applications, and to alter it and redistribute it freely, subject to the following restrictions:
//
//  1. The origin of this software must not be misrepresented; you must not claim that you wrote
//     the original software. If you use this software in a product, an acknowledgment in the
//     product documentation would be appreciated but is not required.
//
//  2. Altered source versions must be plainly marked as such, and must not be misrepresented as
//     being the original software.
//
//  3. This notice may not be removed or altered from any source distribution.
//--------------------------------------------------------------------------------------------------

#include "compiler_wrapper.hpp"

#include "debug_utils.hpp"
#include "hasher.hpp"
#include "sys_utils.hpp"

namespace bcache {
compiler_wrapper_t::compiler_wrapper_t(cache_t& cache) : m_cache(cache) {
}

compiler_wrapper_t::~compiler_wrapper_t() {
}

bool compiler_wrapper_t::handle_command(const string_list_t& args, int& return_code) {
  return_code = 1;

  try {
    // Start a hash.
    hasher_t hasher;

    // Hash the preprocessed file contents.
    hasher.update(preprocess_source(args));

    // Hash the (filtered) command line flags.
    hasher.update(filter_arguments(args).join(" ", true));

    // Hash the compiler version string.
    hasher.update(get_compiler_id(args));

    // Finalize the hash.
    const auto hash = hasher.final();

    // Get the object (target) file for this compilation command.
    const auto object_file = get_object_file(args);

    // Look up the entry in the cache.
    const auto cached_file = m_cache.lookup(hash);
    if (!cached_file.empty()) {
      debug::log(debug::INFO) << "Cache hit: " << hash.as_string() << ": " << cached_file << " => "
                              << object_file;

      return_code = 0;
      file::link_or_copy(cached_file, object_file);
      return true;
    }

    debug::log(debug::INFO) << "Cache miss: " << hash.as_string() << ": " << object_file;

    // Run the actual compiler command to produce the object file.
    const auto result = sys::run_with_prefix(args, false);
    return_code = result.return_code;

    // Create a new entry in the cache.
    // TODO(m): If we could store stdout, stderr and the return value in the cache, we might want
    // to create cache entries for failed compilations too. Right now we just don't add cache
    // entries for failed compilations.
    if (return_code == 0) {
      m_cache.add(hash, object_file);
    }

    // Everything's ok!
    // Note: Even if the compilation failed, we've done the expected job (running the compiler again
    // would just take twice the time and give the same errors).
    return true;
  } catch (std::exception& e) {
    debug::log(debug::DEBUG) << "Exception: " << e.what();
  } catch (...) {
    // Catch-all in order to not propagate exceptions any higher up (we'll return false).
    debug::log(debug::ERROR) << "UNEXPECTED EXCEPTION";
  }

  return false;
}

file::tmp_file_t compiler_wrapper_t::get_temp_file(const std::string& extension) const {
  // Use a suitable folder for temporary files.
  const auto tmp_path = file::append_path(m_cache.root_folder(), "tmp");
  if (!file::dir_exists(tmp_path)) {
    file::create_dir(tmp_path);
  }

  // Generate a fairly unique file name.
  return file::tmp_file_t(tmp_path, extension);
}
}  // namespace bcache
