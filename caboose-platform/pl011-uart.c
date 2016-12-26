#include <stdint.h>

#include "bcm2835.h"

/* PL011 register bits from Xen. */

/* CR bits */
#define CTSEN  (1<<15) /* automatic CTS hardware flow control */
#define RTSEN  (1<<14) /* automatic RTS hardware flow control */
#define RTS    (1<<11) /* RTS signal */
#define DTR    (1<<10) /* DTR signal */
#define RXE    (1<<9) /* Receive enable */
#define TXE    (1<<8) /* Transmit enable */
#define UARTEN (1<<0) /* UART enable */

/* FR bits */
#define TXFE   (1<<7) /* TX FIFO empty */
#define RXFE   (1<<4) /* RX FIFO empty */
#define BUSY   (1<<3) /* Transmit is not complete */

/* LCR_H bits */
#define SPS    (1<<7) /* Stick parity select */
#define FEN    (1<<4) /* FIFO enable */
#define STP2   (1<<3) /* Two stop bits select */
#define EPS    (1<<2) /* Even parity select */
#define PEN    (1<<1) /* Parity enable */
#define BRK    (1<<0) /* Send break */

/* Interrupt bits (IMSC, MIS, ICR) */
#define OEI   (1<<10) /* Overrun Error interrupt mask */
#define BEI   (1<<9)  /* Break Error interrupt mask */
#define PEI   (1<<8)  /* Parity Error interrupt mask */
#define FEI   (1<<7)  /* Framing Error interrupt mask */
#define RTI   (1<<6)  /* Receive Timeout interrupt mask */
#define TXI   (1<<5)  /* Transmit interrupt mask */
#define RXI   (1<<4)  /* Receive interrupt mask */
#define DSRMI (1<<3)  /* nUARTDSR Modem interrupt mask */
#define DCDMI (1<<2)  /* nUARTDCD Modem interrupt mask */
#define CTSMI (1<<1)  /* nUARTCTS Modem interrupt mask */
#define RIMI  (1<<0)  /* nUARTRI Modem interrupt mask */
#define ALLI  OEI|BEI|PEI|FEI|RTI|TXI|RXI|DSRMI|DCDMI|CTSMI|RIMI

static void delay(unsigned int count)
{
    while (count--) {
        asm volatile ("nop");
    }
}

/* The helpful comments in this initialization routine are from uart.cc in the
 * raspbootin serial bootloader. */

uint8_t *uart0_init(uint8_t *pool)
{
    volatile uint32_t *cr, *icr, *ibrd, *fbrd, *lcrh,  *imsc;
    cr = (uint32_t *)ARM_UART0_CR;
    icr = (uint32_t *)ARM_UART0_ICR;
    ibrd = (uint32_t *)ARM_UART0_IBRD;
    fbrd = (uint32_t *)ARM_UART0_FBRD;
    lcrh = (uint32_t *)ARM_UART0_LCRH;
    imsc = (uint32_t *)ARM_UART0_IMSC;

    volatile uint32_t *gppud, *gppudclk0;
    gppud = (uint32_t *)ARM_GPIO_GPPUD;
    gppudclk0 = (uint32_t *)ARM_GPIO_GPPUDCLK0;

    /* Disable UART0 by clearing all CR bits. */
    *cr = 0;

    /* Setup GPIO pins 14 && 15. */

    /* Disable pull up/down for all GPIO pins & delay */
    *gppud = 0x00000000;
    delay(200);

    /* Disable pull up/down for pin 14,15 & delay */
    *gppudclk0 = (1 << 14) | (1 << 15);
    delay(200);

    /* Write 0 to GPPUDCLK0 to make it take effect. */
    *gppudclk0 = 0x00000000;

    /* Clear pending interrupts. */
    *icr = ALLI;

    /* Set integer & fractional part of baud rate.
     * Divider = UART_CLOCK/(16 * Baud)
     * Fraction part register = (Fractional part * 64) + 0.5
     * UART_CLOCK = 3000000; Baud = 115200.
     *
     * Divider = 3000000/(16 * 115200) = 1.627 = ~1.
     * Fractional part register = (.627 * 64) + 0.5 = 40.6 = ~40.
     *
     * XXX ^^ this comment (and the calculated values) are no longer valid - a
     * bootcode.bin or start.elf upgrade within the past year or so changed
     * UART_CLOCK (I discovered after much head scratching and LED blinking).
     * The below values work for my current boot firmware revisions - it looks
     * like rsta2 has a clever strategy for dynamically determining UART_CLOCK,
     * but for now this is good enough for me. */
    *ibrd = 26;
    *fbrd = 3;

    /* Enable FIFO & 8 bit data transmission (1 stop bit, no parity). */
    *lcrh = FEN | ((8 - 5) << 5);

    /* Mask all interrupts. */
    *imsc = ALLI;

    /* Enable UART0, receive & transmit part of UART. */
    *cr = UARTEN | TXE | RXE;

    return pool;
}

void uart0_putc(uint8_t c)
{
    volatile uint32_t *fr, *dr;
    fr = (uint32_t *)ARM_UART0_FR;
    dr = (uint32_t *)ARM_UART0_DR;

    while (!(*fr & TXFE)) {
        /* wait */
    }

    *dr = c;
}
