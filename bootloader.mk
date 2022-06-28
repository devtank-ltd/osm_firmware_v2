bootloader_SOURCES := bootloader/bootloader.c


ifeq ($(DEV), STM32L451RE)
	bootloader_LINK_SCRIPT := bootloader/stm32l452.ld
else ifeq ($(DEV), STM32L433VTC6)
	bootloader_LINK_SCRIPT := bootloader/stm32l433.ld
else
	bootloader_LINK_SCRIPT :=
endif
