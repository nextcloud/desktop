project(gui)
set(CMAKE_AUTOMOC TRUE)

set(MOCKSERVER_NAME mockserver)

#qt_wrap_ui(mockserver_UI_SRCS ${mockserver_UI})

set(mockserver_SRCS
  main.cpp
  httpserver.cpp
)

set(mockserver_HDRS
  httpserver.h
)

# add_executable( ${MOCKSERVER_NAME} main.cpp ${final_src})
add_executable(${MOCKSERVER_NAME} WIN32 ${mockserver_SRCS} ${mockserver_HDRS})
qt5_use_modules(${MOCKSERVER_NAME} Network Xml)
target_link_libraries(${MOCKSERVER_NAME} ${QT_LIBRARIES})

