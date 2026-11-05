# vcpkg triplet of the Windows build: shared libraries like x64-windows (Qt WebEngine needs a shared build), except
# protobuf and the libraries it is built from, which are linked statically: protobuf as a DLL does not export
# fixed_address_empty_string, which the generated Mumble.pb.cc needs (LNK2019 when linking mumble.exe).
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
if(PORT MATCHES "^(protobuf|abseil|utf8-range)$")
  set(VCPKG_LIBRARY_LINKAGE static)
endif()
