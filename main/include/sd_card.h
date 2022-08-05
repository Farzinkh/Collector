#ifndef SD_CARD
#define SD_CARD

#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include <sys/stat.h>
#include <sys/unistd.h>
#include <string.h>

static const char *SDTAG = "sdcard";

#define MOUNT_POINT "/sdcard" 

void mount_sdcard(sdmmc_card_t ** Card,sdmmc_host_t * host);
void unmount_sdcard(sdmmc_card_t ** Card,sdmmc_host_t * host);
int search_in_spiffs(void);
int search_in_sdcard(void);

#endif