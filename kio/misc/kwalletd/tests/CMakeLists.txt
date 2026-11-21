kde3_add_executable(kwalletd_sstest NO_INSTALL
  SOURCES sstest.cpp ../kwalletsecretservice.cpp
  LINK kdecore kwalletbackend PkgConfig::LIBSECRET)
target_include_directories(kwalletd_sstest PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/.."
  "${PROJECT_SOURCE_DIR}/kwallet/backend"
  "${PROJECT_SOURCE_DIR}/kwallet/client")

kde3_add_executable(kwalletd_kwtest NO_INSTALL
  SOURCES kwtest.cpp
  LINK kdecore kwalletclient)
target_include_directories(kwalletd_kwtest PRIVATE "${PROJECT_SOURCE_DIR}/kwallet/client")

kde3_add_executable(kwalletd_mkkwl NO_INSTALL
  SOURCES mkkwl.cpp
  LINK kdecore kwalletbackend)
target_include_directories(kwalletd_mkkwl PRIVATE
  "${PROJECT_SOURCE_DIR}/kwallet/backend"
  "${PROJECT_SOURCE_DIR}/kwallet/client")
