/* For the QEMU vexpress-a15 machine, based on the (terse) guidance of
 * http://wiki.osdev.org/ARMv7-A_Bare_Bones. */

MEMORY
{
    /* The authoritative definition of this platform's RAM can be found in
     * QEMU's hw/arm/vexpress.c within a15_daughterboard_init():
     * "RAM is from 0x80000000 upwards; there is no low-memory alias for it."
     * memory_region_add_subregion(sysmem, 0x80000000, ram);
     *
     * We'll assume we get the default 128MiB. */
    ram : ORIGIN = 0x80000000, LENGTH = 128M
}

SECTIONS
{
    /* Since we invoke QEMU with -kernel we're loaded according to the Linux
     * kernel ARM loading protocol, which loads and executes the kernel at an
     * offset of 0x10000 from the start of system memory.  This is documented in
     * Linux in Documentation/arm/Booting, and implemented in QEMU's
     * hw/arm/boot.c within arm_load_kernel_notify().  We explicitly place
     * start.o first to ensure execution begins with our stub. */
    .stub 0x80010000 : {
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
    kernel_stack = 0x80000000 + 128M;
}
