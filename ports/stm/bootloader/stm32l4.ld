
/* Linker script for STM32L4 bootloader, 4K flash, 64K RAM. */
/* 8k for config split out of the flash */

/* Define memory regions. */
MEMORY
{
    rom (rx) : ORIGIN  = 0x08000000, LENGTH = 4K
    ram (rwx) : ORIGIN = 0x20000000, LENGTH = 64K
}


/* Include the common ld script. */
INCLUDE cortex-m-generic.ld
