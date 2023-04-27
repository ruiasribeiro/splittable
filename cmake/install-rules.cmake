if(PROJECT_IS_TOP_LEVEL)
  set(
      CMAKE_INSTALL_INCLUDEDIR "include/splittable-${PROJECT_VERSION}"
      CACHE PATH ""
  )
endif()

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# find_package(<package>) call for consumers to find this project
set(package splittable)

install(
    DIRECTORY
    include/
    "${PROJECT_BINARY_DIR}/export/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT splittable_Development
)

install(
    TARGETS splittable_splittable
    EXPORT splittableTargets
    RUNTIME #
    COMPONENT splittable_Runtime
    LIBRARY #
    COMPONENT splittable_Runtime
    NAMELINK_COMPONENT splittable_Development
    ARCHIVE #
    COMPONENT splittable_Development
    INCLUDES #
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

write_basic_package_version_file(
    "${package}ConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion
)

# Allow package maintainers to freely override the path for the configs
set(
    splittable_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/${package}"
    CACHE PATH "CMake package config location relative to the install prefix"
)
mark_as_advanced(splittable_INSTALL_CMAKEDIR)

install(
    FILES cmake/install-config.cmake
    DESTINATION "${splittable_INSTALL_CMAKEDIR}"
    RENAME "${package}Config.cmake"
    COMPONENT splittable_Development
)

install(
    FILES "${PROJECT_BINARY_DIR}/${package}ConfigVersion.cmake"
    DESTINATION "${splittable_INSTALL_CMAKEDIR}"
    COMPONENT splittable_Development
)

install(
    EXPORT splittableTargets
    NAMESPACE splittable::
    DESTINATION "${splittable_INSTALL_CMAKEDIR}"
    COMPONENT splittable_Development
)

if(PROJECT_IS_TOP_LEVEL)
  include(CPack)
endif()
