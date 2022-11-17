#pragma once

#include "sens01_config.h"

#define FLASH_ADDRESS                SENS01_FLASH_ADDRESS
#define FLASH_PAGE_SIZE              SENS01_FLASH_PAGE_SIZE
#define FLASH_MEASUREMENTS_PAGE      SENS01_FLASH_MEASUREMENTS_PAGE
#define FLASH_CONFIG_PAGE            SENS01_FLASH_CONFIG_PAGE
#define FW_PAGE                      SENS01_FW_PAGE
#define NEW_FW_PAGE                  SENS01_NEW_FW_PAGE
#define FW_PAGES                     SENS01_FW_PAGES
#define FW_MAX_SIZE                  SENS01_FW_MAX_SIZE
#define PAGE2ADDR(_page_)            SENS01_PAGE2ADDR(_page_)
#define FW_ADDR                      SENS01_FW_ADDR
#define NEW_FW_ADDR                  SENS01_NEW_FW_ADDR
#define PERSIST_RAW_DATA             SENS01_PERSIST_RAW_DATA
#define PERSIST_RAW_MEASUREMENTS     SENS01_PERSIST_RAW_MEASUREMENTS


#define PERSIST_VERSION              SENS01_PERSIST_VERSION

#define persist_model_config_t       persist_sens01_config_v1_t
