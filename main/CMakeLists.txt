# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES )

set(COMPONENT_SRCS "http_server.c" "sd_card" "ov2602" "main.c")
set(COMPONENT_ADD_INCLUDEDIRS "./include")

register_component()

#upload files                

set(WEB_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/front")
if(EXISTS ${WEB_SRC_DIR})
    spiffs_create_partition_image(storage ${WEB_SRC_DIR} FLASH_IN_PROJECT)
else()
    message(FATAL_ERROR "${WEB_SRC_DIR} doesn't exist. Please place it in ${WEB_SRC_DIR}")
endif()