#pragma once

#include "model_config.h"

#define FLASH_ADDRESS                 CONCAT(FW_NAME,_FLASH_ADDRESS)
#define FLASH_PAGE_SIZE               CONCAT(FW_NAME,_FLASH_PAGE_SIZE)

#define FLASH_MEASUREMENTS_PAGE       CONCAT(FW_NAME,_FLASH_MEASUREMENTS_PAGE)
#define FLASH_CONFIG_PAGE             CONCAT(FW_NAME,_FLASH_CONFIG_PAGE)
#define FW_PAGE                       CONCAT(FW_NAME,_FW_PAGE)
#define NEW_FW_PAGE                   CONCAT(FW_NAME,_NEW_FW_PAGE)

#define FW_PAGES                      CONCAT(FW_NAME,_FW_PAGES)
#define FW_MAX_SIZE                   CONCAT(FW_NAME,_FW_MAX_SIZE)
#define PAGE2ADDR(_page_)             CONCAT(FW_NAME,_PAGE2ADDR)(_page_)
#define FW_ADDR                       CONCAT(FW_NAME,_FW_ADDR)
#define NEW_FW_ADDR                   CONCAT(FW_NAME,_NEW_FW_ADDR)
#define PERSIST_RAW_DATA              CONCAT(FW_NAME,_PERSIST_RAW_DATA)
#define PERSIST_RAW_MEASUREMENTS      CONCAT(FW_NAME,_PERSIST_RAW_MEASUREMENTS)

#define PERSIST_VERSION               CONCAT(FW_NAME,_PERSIST_VERSION)

#define persist_model_config_t        CONCAT(FW_NAME,_PERSIST_MODEL_CONFIG_T)

