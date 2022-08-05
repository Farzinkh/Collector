#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := client
EXTRA_COMPONENT_DIRS := $(/home/farzin/esp/esp32-camera) #add for esp32cam 
include $(IDF_PATH)/make/project.mk
