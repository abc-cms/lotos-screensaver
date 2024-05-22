cmake_minimum_required(VERSION 3.16.0)
project(lotos)

set(CMAKE_CXX_STANDARD 20)

set(SOURCE_DIR "${PROJECT_SOURCE_DIR}/lotos-screensaver")
set(INCLUDE_DIR "${SOURCE_DIR}")
include(FetchContent)

find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP jsoncpp)
pkg_check_modules(CAIRO cairo)

add_executable(lotos ${SOURCE_DIR}/main.cpp)

target_include_directories(lotos PUBLIC ${INCLUDE_DIR})

target_compile_options(lotos PRIVATE -Wno-volatile)
find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
find_package(OpenCV REQUIRED )
find_package(XCB REQUIRED COMPONENTS IMAGE XCB SCREENSAVER)
find_package(spdlog REQUIRED)
target_link_libraries(lotos PUBLIC ${XCB_LIBRARIES} ${OpenCV_LIBS} spdlog::spdlog_header_only ${JSONCPP_LIBRARIES} ${CAIRO_LIBRARIES})
target_include_directories(lotos PUBLIC ${XCB_INCLUDE_DIRS} ${OpenCV_INCLUDE_DIRS} ${JSONCPP_INCLUDE_DIRS} ${CAIRO_INCLUDE_DIRS})

install(
    TARGETS lotos
    RUNTIME DESTINATION bin
)

install(
    DIRECTORY config
    USE_SOURCE_PERMISSIONS DESTINATION share/lotos-screensaver
)

install(
    DIRECTORY media
    USE_SOURCE_PERMISSIONS DESTINATION share/lotos-screensaver
)

# CPack Configuration
set(CPACK_PACKAGING_INSTALL_PREFIX /usr)

set(POSTINST_IN ${CMAKE_CURRENT_LIST_DIR}/debian/postinst.in)
set(POSTINST_OUT ${CMAKE_CURRENT_BINARY_DIR}/postinst)
configure_file(${POSTINST_IN} ${POSTINST_OUT} @ONLY)

set(POSTRM_IN ${CMAKE_CURRENT_LIST_DIR}/debian/postrm.in)
set(POSTRM_OUT ${CMAKE_CURRENT_BINARY_DIR}/postrm)
configure_file(${POSTRM_IN} ${POSTRM_OUT} @ONLY)

set(PRERM_IN ${CMAKE_CURRENT_LIST_DIR}/debian/prerm.in)
set(PRERM_OUT ${CMAKE_CURRENT_BINARY_DIR}/prerm)
configure_file(${PRERM_IN} ${PRERM_OUT} @ONLY)

set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${POSTINST_OUT};${POSTRM_OUT};${PRERM_OUT}")

set(CPACK_GENERATOR DEB)
set(CPACK_DEBIAN_PACKAGE_NAME "lotos")
set(CPACK_DEBIAN_PACKAGE_VERSION 1.0.0)
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "CPack deb lotos")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "a-kosak-mbx@gmail.com")
set(CPACK_PACKAGE_VENDOR "${CPACK_DEBIAN_PACKAGE_MAINTAINER}")
set(CPACK_PACKAGE_CONTACT "a-kosak-mbx@gmail.com")
set(CPACK_DEBIAN_PACKAGE_SECTION devel)

message(STATUS "  Package:      ${CPACK_DEBIAN_PACKAGE_NAME}")
message(STATUS "  Version:      ${CPACK_DEBIAN_PACKAGE_VERSION}")
message(STATUS "  Section:      ${CPACK_DEBIAN_PACKAGE_SECTION}")
message(STATUS "  Priority:     ${CPACK_DEBIAN_PACKAGE_PRIORITY}")
message(STATUS "  Architecture: ${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")
message(STATUS "  Depends:      ${CPACK_DEBIAN_PACKAGE_DEPENDS}")
message(STATUS "  Maintainer:   ${CPACK_DEBIAN_PACKAGE_MAINTAINER}")
message(STATUS "  Description:  ${CPACK_DEBIAN_PACKAGE_DESCRIPTION}")

# -- include cpack
include (CPack)
