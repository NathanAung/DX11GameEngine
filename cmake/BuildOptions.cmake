# Centralized build options and common CMake settings

# Use vcpkg-config packages when available
option(ENGINE_ENABLE_VCPKG "Use vcpkg-config packages when available" ON)

# Warning and sanitizers toggles (extend as needed)
option(ENGINE_ENABLE_WARNINGS "Enable strict compiler warnings" ON)
option(ENGINE_ENABLE_ASAN "Enable AddressSanitizer (non-MSVC)" OFF)
option(ENGINE_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer (non-MSVC)" OFF)

# Apply common policies/settings
cmake_policy(SET CMP0077 NEW) # Honor option() defaults via cache
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Configure warnings if requested
if(ENGINE_ENABLE_WARNINGS)
  if(MSVC)
    add_compile_options(/W4 /permissive-)
  else()
    add_compile_options(-Wall -Wextra -Wpedantic)
  endif()
endif()

# Optional sanitizers for non-MSVC
if(NOT MSVC)
  if(ENGINE_ENABLE_ASAN)
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
  endif()
  if(ENGINE_ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
  endif()
endif()