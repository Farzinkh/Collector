#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := client
EXTRA_COMPONENT_DIRS := $(IDF_WHO_PATH) #add for esp lib 
include $(IDF_PATH)/make/project.mk
