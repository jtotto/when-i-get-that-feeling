#include <stdint.h>

#include <caboose/util.h>

#include "bcm2835.h"
#include "util.h"

/* PL011 register bits from Xen. */

struct pl011regs {
    uint32_t dr;
    PAD(ARM_UART0_DR, ARM_UART0_FR);
    uint32_t fr;
    PAD(ARM_UART0_FR, ARM_UART0_IBRD);
    uint32_t ibrd;
    uint32_t fbrd;
    uint32_t lcrh;
    uint32_t cr;
    uint32_t ifls;
    uint32_t imsc;
    uint32_t ris;
    uint32_t mis;
    uint32_t icr;
} __packed;

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

/* The helpful comments in this initialization routine are from uart.cc in the
 * raspbootin serial bootloader. */

uint8_t *uart0_init(uint8_t *pool)
{
    volatile struct pl011regs *uart = (struct pl011regs *)ARM_UART0_BASE;

    /* Disable UART0 by clearing all CR bits. */
    uart->cr = 0;

    /* Clear pending interrupts. */
    uart->icr = ALLI;

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
    uart->ibrd = 26;
    uart->fbrd = 3;

    /* Enable FIFO & 8 bit data transmission (1 stop bit, no parity). */
    uart->lcrh = FEN | ((8 - 5) << 5);

    /* Mask all interrupts. */
    uart->imsc = ALLI;

    /* Enable UART0, receive & transmit part of UART. */
    uart->cr = UARTEN | TXE | RXE;

    return pool;
}

void uart0_putc(uint8_t c)
{
    volatile struct pl011regs *uart = (struct pl011regs *)ARM_UART0_BASE;

    while (!(uart->fr & TXFE)) {
        /* wait */
    }

    uart->dr = c;
}
