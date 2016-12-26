MEMORY
{
    /* We'll choose a conservative length to stay out of trouble in the event of
     * substantial changes to the default CPU/GPU RAM split in the future. */
    ram : ORIGIN = 0x00000000, LENGTH = 256M
}

SECTIONS
{
    /* We'll require the default config.txt boot parameters, and assume that
     * we're loaded and executed starting from 0x8000. */
    .stub 0x8000 : {
        caboose-platform/start.o(.text)
    } >ram

    .text : {
        *(.text)
    } >ram

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    } >ram

    .data : {
        *(.data)
    } >ram

    .bss : {
        bss_start = . ;
        *(.bss COMMON)
        bss_end = . ;
    } >ram

    /* Free system memory begins here. */
    pool_begin = ALIGN(8);

    /* Put the kernel stack at the end of system memory. */
    kernel_stack = 0x00000000 + 256M;
}
