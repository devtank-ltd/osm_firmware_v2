
/* Linker script for STM32L452RE, 512K flash, 160K RAM. */
/* 8k for config split out of the flash */

/* Define memory regions. */
MEMORY
{
	rom (rx) : ORIGIN  = 0x08000000, LENGTH = 512K
	ram (rwx) : ORIGIN = 0x20000000, LENGTH = 160K
}


/* Include the common ld script. */
INCLUDE cortex-m-generic.ld
