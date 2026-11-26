
####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was BlockTypeSupportsSharedLibrarySupportConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

include(CMakeFindDependencyMacro)
find_dependency(PySysLinkBase REQUIRED)

# Specify the include directories and libraries
set_and_check(BLOCK_TYPE_SUPPORTS_SHARED_LIBRARY_SUPPORT_INCLUDE_DIRS "/usr/local/include")
set_and_check(BLOCK_TYPE_SUPPORTS_SHARED_LIBRARY_SUPPORT_LIBRARIES "/usr/local/lib/pysyslink_plugins/block_type_supports/shared_library_support_0_1_0")

# Set include directories for BlockTypeSupportsSharedLibrarySupport
set(BlockTypeSupportsSharedLibrarySupport_INCLUDE_DIRS "${CMAKE_INSTALL_PREFIX}/include")
set(BlockTypeSupportsSharedLibrarySupport_LIBRARIES "/usr/local/lib/pysyslink_plugins/block_type_supports/shared_library_support_0_1_0")

include("${CMAKE_CURRENT_LIST_DIR}/BlockTypeSupportsSharedLibrarySupportTargets.cmake")
