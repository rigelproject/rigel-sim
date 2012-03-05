# $Id: 3847682ab3c892d66e27bf3070faabf1bbb0fd40 $

# Build the library...

# Include all the .cpp files in the library.
file (GLOB SOURCES *.cpp)

# Define the library and specify whether it is shared or not.
if (RANDOM_SHARED_LIB)
  add_library (Random SHARED ${SOURCES})
else (RANDOM_SHARED_LIB)
  add_library (Random STATIC ${SOURCES})
endif (RANDOM_SHARED_LIB)

# Set the version number on the library
if (WIN32)
  set_target_properties (Random PROPERTIES VERSION "${LIBVERSIONFULL}" )
else (WIN32)
  set_target_properties (Random PROPERTIES
    VERSION "${LIBVERSIONFULL}" SOVERSION "${LIBVERSION}" )
endif (WIN32)

# Specify where the library is installed, adding it to the export
# RandomLibDepends.
install (TARGETS Random
  EXPORT RandomLibDepends
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)
