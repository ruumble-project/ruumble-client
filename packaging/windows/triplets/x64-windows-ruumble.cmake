# vcpkg triplet of the Windows build: shared libraries like x64-windows (Qt WebEngine needs a shared build), except
# protobuf and the libraries it is built from. Those are linked statically and built as C++20 like Mumble: protobuf as
# a DLL does not export fixed_address_empty_string, and its type depends on the C++ standard (GlobalEmptyStringConstexpr
# only with C++20), so a C++17 protobuf misses the symbol the generated Mumble.pb.cc needs (LNK2019 on mumble.exe).
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
if(PORT MATCHES "^(protobuf|abseil|utf8-range)$")
  set(VCPKG_LIBRARY_LINKAGE static)
  set(VCPKG_CMAKE_CONFIGURE_OPTIONS "-DCMAKE_CXX_STANDARD=20")
endif()
