/* Linker script for the STM32F303RE chip (512K flash, 64K RAM). */

/* Define memory regions. */
MEMORY
{
	rom (rx) : ORIGIN = 0x08000000, LENGTH = 512K
	ram (rwx) : ORIGIN = 0x20000000, LENGTH = 64K
	ccm (rwx) : ORIGIN = 0x10000000, LENGTH = 16K
}

/* Include the common ld script. */
INCLUDE cortex-m-generic.ld

