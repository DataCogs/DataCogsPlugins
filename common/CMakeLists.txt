# DataCogs house UI, shared by every plugin in the suite.

# The gears logo, embedded as binary data. Namespaced DataCogsAssets (not
# JUCE's default BinaryData) so it can never collide with a plugin's own
# binary-data target.
juce_add_binary_data(DataCogsAssets
    HEADER_NAME DataCogsAssets.h
    NAMESPACE DataCogsAssets
    SOURCES assets/datacogs_logo.png
)
set_target_properties(DataCogsAssets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

# INTERFACE library: the look-and-feel sources compile inside each consuming
# plugin target, picking up that target's JUCE module configuration - which
# sidesteps every static-library-of-JUCE-code pitfall.
add_library(DataCogsUI INTERFACE)
target_sources(DataCogsUI INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/source/DataCogsLookAndFeel.cpp
)
target_include_directories(DataCogsUI INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(DataCogsUI INTERFACE DataCogsAssets)
