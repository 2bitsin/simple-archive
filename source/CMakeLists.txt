add_executable(archive archive.cpp)
target_compile_features(archive PRIVATE cxx_std_23)

add_executable(unarchive unarchive.cpp)
target_compile_features(unarchive PRIVATE cxx_std_23)

install(TARGETS unarchive archive DESTINATION bin)

