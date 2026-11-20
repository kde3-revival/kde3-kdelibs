# libkICE.la (convenience library of libDCOP)
kde3_add_library(kICE OBJECT
  SOURCES
    accept.c
    authutil.c
    connect.c
    error.c
    getauth.c
    iceauth.c
    listen.c
    listenwk.c
    locking.c
    misc.c
    ping.c
    process.c
    protosetup.c
    register.c
    replywait.c
    setauth.c
    shutdown.c
    watch.c
    transport.c
    globals.c
)
target_compile_definitions(kICE PRIVATE ICE_t TRANS_SERVER TRANS_CLIENT UNIXCONN ANSICPP)
target_include_directories(kICE PRIVATE "${PROJECT_SOURCE_DIR}/dcop")
