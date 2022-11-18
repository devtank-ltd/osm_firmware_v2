#pragma once

#include "env01_config.h"

#define FLASH_ADDRESS                 ENV01_FLASH_ADDRESS
#define FLASH_PAGE_SIZE               ENV01_FLASH_PAGE_SIZE

#define FLASH_MEASUREMENTS_PAGE       ENV01_FLASH_MEASUREMENTS_PAGE
#define FLASH_CONFIG_PAGE             ENV01_FLASH_CONFIG_PAGE
#define FW_PAGE                       ENV01_FW_PAGE
#define NEW_FW_PAGE                   ENV01_NEW_FW_PAGE

#define FW_PAGES                      ENV01_FW_PAGES
#define FW_MAX_SIZE                   ENV01_FW_MAX_SIZE
#define PAGE2ADDR(_page_)             ENV01_PAGE2ADDR(_page_)
#define FW_ADDR                       ENV01_FW_ADDR
#define NEW_FW_ADDR                   ENV01_NEW_FW_ADDR
#define PERSIST_RAW_DATA              ENV01_PERSIST_RAW_DATA
#define PERSIST_RAW_MEASUREMENTS      ENV01_PERSIST_RAW_MEASUREMENTS

#define PERSIST_VERSION               ENV01_PERSIST_VERSION

#define persist_model_config_t        persist_env01_config_v1_t

#define MODEL_NUM                     MODEL_NUM_ENV01
