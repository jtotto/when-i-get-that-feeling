#include <stdarg.h>
#include <stdint.h>

#include <caboose/config.h>
#include <caboose/events.h>
#include <caboose/platform.h>
#include <caboose/util.h>

#include <uspi.h>
#include <uspios.h>

#include "barriers.h"
#include "bcm2835.h"
#include "bcm2835int.h"
#include "debug.h"
#include "ipi.h"
#include "irq.h"
#include "mempool.h"
#include "midi.h"
#include "mmu.h"
#include "platform-events.h"
#include "secondary.h"
#include "timer.h"
#include "usb.h"
#include "util.h"

/* Our business here is to shim USPi's USB-MIDI support into the wider system
 * while keeping interrupt latency on the main synth core sane.  Since USPi
 * needs to service thousands of USB interrupts per second even when the user
 * isn't actually providing any input (by no fault of its own - the DWC host
 * controller is well known in the RPi community for being insane), we do this
 * by nominating the USB interrupt to be the system FIQ interrupt, routing the
 * system FIQ line to one of the system secondary cores and delivering the
 * actual useful interrupts to the main core via inter-processor interrupts. */

#define USPI_PLATFORM_ASSERT(pred) do {                                     \
    if (!(pred)) {                                                          \
        uspi_assertion_failed(#pred, __FILE__, __LINE__);                   \
    }                                                                       \
} while (0)

uint8_t *usb_svc_stack;
uint8_t *usb_fiq_stack;
void *usb_pagetable;

/* State required for the USPi platform portability layer. */
struct {
    struct mempool mem;

    TInterruptHandler *handler;
    void *param;
    void *mailbuffer;
} uspios;

/* Buffer for passing MIDI packets between the main application core and the USB
 * core.  Access is synchronized using the USB IPI bit in the Core 0 mailbox -
 * the USB core owns it (and can write it) when the bit is clear, and the
 * application core owns it for as long as the bit is set.  MIDI/USB deadlines
 * are far more relaxed than the audio generation deadlines, so we'll have the
 * USB core spin to synchronize access. */
struct usbmidipkt midisync;

/* Buffer for MIDI packets waiting for delivery to userspace (that is, waiting
 * for the receiving task's AwaitEvent()). */
struct mempool midipool;

/* This is called by USPi in the FIQ handler when a new packet has been
 * received.  All we want to do here is put the packet somewhere that Core 0 can
 * find it and then prod it to have a look.
 *
 * Note that this is a handler for the altered semantics of our patched version
 * of USPi - what we're getting is a raw USB-MIDI packet that we need to parse
 * ourselves. */
static void uspi_packet_handler(unsigned cable, unsigned length, uint8_t *p)
{
    debug_printf("MIDI packet with length %u!", length);

    USPI_PLATFORM_ASSERT(length <= CONFIG_USB_PACKET_BUF_SIZE);

    /* Synchronize access to the inter-core buffer by waiting until the main
     * core has completed processing the last IPI we pinged them with. */
    while (ipi_pending(IPI_USB)) {
        /* spin */
    }

    /* Copy the new packet into the communication buffer. */
    midisync.len = length;
    memcpy(midisync.packet, p, length);

    /* Make sure that we're entirely finished writing it... */
    dmb();

    /* ... and signal the main core that there's a packet. */
    ipi_deliver(IPI_USB);
}

static void usb_ipi_handler(void)
{
    debug_printf("USB IPI!");

    /* Grab a packet buffer for the new packet. */
    struct usbmidipkt *pkt = mempool_alloc(&midipool);
    ASSERT(pkt);

    /* Fill it with the contents of the inter-core sync buffer. */
    memcpy(pkt, &midisync, USBMIDIPKT_SIZE(&midisync));

    /* Hand this packet off to userspace. */
    event_deliver(MIDIPKT_EVENTID, (int)pkt);

    /* Ensure that we're entirely finished reading the sync buffer... */
    dmb();

    /* ... and signal the USB core that we're done, implicitly, by returning
     * from this handler and clearing the bit for this IPI in the mailbox. */
}

/* Userspace pings us back here through AcknowledgeEvent() to return the packet
 * buffer when it's done. */
void midi_event_ack_handler(int ack)
{
    /* Just put it back on the free list. */
    mempool_free(&midipool, (void *)ack);
}

void usb_start(void);

/* This is the vanilla initialization routine called on the main core during
 * platform startup (interrupts disabled, scheduler not yet started). */
uint8_t *usb_init(uint8_t *pool)
{
    mempool_init(&midipool,
                 pool,
                 sizeof (struct usbmidipkt),
                 CONFIG_MIDI_EVENT_PACKET_COUNT);
    pool += sizeof (struct usbmidipkt) * CONFIG_MIDI_EVENT_PACKET_COUNT;

    event_register_ack_handler(MIDIPKT_EVENTID, midi_event_ack_handler);

    ipi_register(IPI_USB, usb_ipi_handler);

    pool = (uint8_t *)ALIGN((uintptr_t)pool, 8);
    mempool_init(&uspios.mem,
                 pool,
                 CONFIG_USPI_MEMPOOL_SIZE,
                 CONFIG_USPI_MEMPOOL_COUNT);
    pool += CONFIG_USPI_MEMPOOL_SIZE * CONFIG_USPI_MEMPOOL_COUNT;

    /* Bring up Core 1, to which we'll delegate the handling of USB FIQs. */

    /* Start by allocating it stacks to use in SVC and FIQ mode. */
    pool = (uint8_t *)ALIGN((uintptr_t)pool, 8);
    pool += CONFIG_USB_STACK_SIZE;
    usb_svc_stack = pool;

    pool += CONFIG_FIQ_STACK_SIZE;
    usb_fiq_stack = pool;

    /* Allocate memory for Core 1's page table. */
    pool = mmu_pagetable_alloc(pool, &usb_pagetable);

    /* Allocate a section for the mailbox communication code. */
    uspios.mailbuffer = (void *)ALIGN((uintptr_t)pool, SECTION_SIZE);
    pool += SECTION_SIZE;

    /* Ensure that all of the writes to global state read by Core 1 are complete
     * and have made it back to main memory, and then bring it up. */
    cache_clean_range(&usb_svc_stack, sizeof usb_svc_stack);
    cache_clean_range(&usb_fiq_stack, sizeof usb_fiq_stack);
    cache_clean_range(&usb_pagetable, sizeof usb_pagetable);
    cache_clean_range(&uspios, sizeof uspios);

    secondary_start(1, usb_start);

    return pool;
}

/* This routine initializes USPi on Core 1. */
void usb_uspi_init(void)
{
    /* Initialize the MMU on this core. */
    mmu_init(usb_pagetable);

    /* Mark the mailbox section as uncacheable, unbufferable and strongly
     * ordered so we can use it to communicate with the VideoCore without
     * dealing with cache insanity. */
    mmu_mark_strongly_ordered(usb_pagetable, uspios.mailbuffer);

    debug_printf("Starting USPi initialization.");
    int rc = USPiInitialize();
    USPI_PLATFORM_ASSERT(rc != 0);

    debug_printf("USPi initialization complete!");

    if (USPiMIDIAvailable()) {
        USPiMIDIRegisterPacketHandler(uspi_packet_handler);
    } else {
        debug_printf("No USB-MIDI device available :(");
    }

    /* That's it!  We can just drop back to the stub and spin waiting for FIQs
     * now. */
}

void usb_handle_fiq(void)
{
    /* Deliver the interrupt to USPi. */
    uspios.handler(uspios.param);
}

/* ----- Implementations of the USPi portability layer services follow ---- */

/* We're either in the SVC-mode usb_init() code or a USB FIQ, so rather than
 * using the normal platform ASSERT() machinery we should just dump the message
 * straight to the UART and spin.  Don't make any attempts to shut down other
 * cores. */
void uspi_assertion_failed (const char *pExpr,
                            const char *pFile,
                            unsigned nLine)
{
    debug_printf("Assertion %s in %s:%u failed", pExpr, pFile, nLine);
    while (true) {
        /* spin */
    }
}

void DebugHexdump (const void *pBuffer, unsigned nBufLen, const char *pSource)
{
    debug_printf("uspi hexdump follows");
    debug_hexdump(pBuffer, nBufLen);
}

/* This malloc() and free() are used _only_ by the USPi portability layer.  I
 * wish they weren't named so generally, but don't really feel like patching the
 * library to deal with it. */
void *malloc (unsigned nSize)
{
    USPI_PLATFORM_ASSERT(nSize <= CONFIG_USPI_MEMPOOL_SIZE);
    return mempool_alloc(&uspios.mem);
}

void free (void *pBlock)
{
    mempool_free(&uspios.mem, pBlock);
}

void MsDelay (unsigned nMilliSeconds)
{
    while (nMilliSeconds--) {
        usDelay(1000);
    }
}

void usDelay (unsigned nMicroSeconds)
{
    unsigned ticks_per_microsecond =
        CABOOSE_PLATFORM_TIMER_CLOCK_FREQ / 1000000;

    uint32_t start = timer_read();
    while (start + ticks_per_microsecond * nMicroSeconds > timer_read()) {
        /* wait */
    }
}

/* Taking a look at the library, it appears that only interrupt transfers make
 * use of the kernel timer facilities, while for my use case (USB-MIDI) I need
 * only control and bulk transfers.  To keep my life simple, I therefore omit
 * these platform functions. */
unsigned StartKernelTimer (unsigned nHzDelay,
                           TKernelTimerHandler *pHandler,
                           void *pParam,
                           void *pContext)
{
    USPI_PLATFORM_ASSERT(false);
    return 0;
}

void CancelKernelTimer (unsigned hTimer)
{
    USPI_PLATFORM_ASSERT(false);
}

void ConnectInterrupt (unsigned nIRQ, TInterruptHandler *pHandler, void *pParam)
{
    USPI_PLATFORM_ASSERT(nIRQ == ARM_IRQ_USB);
    USPI_PLATFORM_ASSERT(!uspios.handler);
    USPI_PLATFORM_ASSERT(!uspios.param);

    uspios.handler = pHandler;
    uspios.param = pParam;
    fiq_register(ARM_IRQ_USB);
}

int SetPowerStateOn (unsigned nDeviceId)
{
    /* As hinted by the documentation comment in uspios.h, in order to turn on
     * power to @nDeviceId (i.e. the USB host controller) we need to ask the
     * VideoCore nicely to do it on our behalf using the mailbox interface that
     * the Raspberry Pi Foundation helpfully documents at [1].
     *
     * [1] https://github.com/raspberrypi/firmware/wiki/Mailbox-property
     *     -interface
     *
     * Honestly this should probably have its own driver, but thus far I haven't
     * needed it anywhere else so for now this great big lump of glue is going
     * to live here.
     *
     * Reproducing the relevant bits of the documentation here so that this code
     * explains itself:
     *
     * "Only mailbox 0's status can trigger interrupts on the ARM, so MB 0 is
     * always for communication from VC to ARM and MB 1 is for ARM to VC. The
     * ARM should never write MB 0 or read MB 1."
     *
     * "General procedure
     *
     *      To read from a mailbox:
     *          Read the status register until the empty flag is not set
     *          Read data from the read register
     *
     *          If the lower four bits do not match the channel number desired
     *          then repeat from 1
     *
     *          The upper 28 bits are the returned data
     *
     *      To write to a mailbox
     *          Read the status register until the full flag is not set
     *
     *          Write the data (shifted into the upper 28 bits) combined with
     *          the channel (in the lower four bits) to the write register
     *
     * ...
     *
     * Mailbox messages:
     *
     * The mailbox interface has 28 bits (MSB) available for the value and 4
     * bits (LSB) for the channel.
     *      Request message: 28 bits (MSB) buffer address
     *      Response message: 28 bits (MSB) buffer address
     * Channels 8 and 9 are used.
     *      Channel 8: Request from ARM for response by VC
     *      Channel 9: Request from VC for response by ARM (none currently
     *      defined)
     *
     * Buffer contents:
     *      u32: buffer size in bytes (including the header values, the end tag
     *           and padding)
     *      u32: buffer request/response code
     *             Request codes:
     *                  0x00000000: process request
     *                  All other values reserved
     *             Response codes:
     *                  0x80000000: request successful
     *                  0x80000001: error parsing request buffer (partial
     *                              response)
     *                  All other values reserved
     *      u8...: sequence of concatenated tags
     *      u32: 0x0 (end tag)
     *      u8...: padding"
     *
     * ...
     *
     * "Tag format:
     *      u32: tag identifier
     *      u32: value buffer size in bytes
     *      u32: 1 bit (MSB) request/response indicator (0=request, 1=response),
     *           31 bits (LSB) value length in bytes
     *      u8...: value buffer
     *
     *      Tag value is padded at the end to make it aligned to 32 bits."
     *
     * ...
     *
     * "Set power state
     *
     * Tag: 0x00028001
     * Request:
     *      Length: 8
     *      Value:
     *          u32: device id
     *          u32: state
     *      State:
     *          Bit 0: 0=off, 1=on
     *          Bit 1: 0=do not wait, 1=wait
     *          Bits 2-31: reserved for future use (set to 0)
     *
     * Response:
     *      Length: 8
     *      Value:
     *          u32: device id
     *          u32: state
     *      State:
     *          Bit 0: 0=off, 1=on
     *          Bit 1: 0=device exists, 1=device does not exist
     *          Bits 2-31: reserved for future use
     *
     * Response indicates new state, with/without waiting for the power to
     * become stable."
     */
    struct mailbuffer {
        uint32_t bufsize;
        uint32_t code;
        struct __packed {
            uint32_t id;
            uint32_t bufsize;
            uint32_t len;
            struct __packed {
                uint32_t device;
                uint32_t state;
            } setpower;
        } tag;

        uint32_t endtag;
    } __packed;

    volatile struct mailbuffer *mailbuffer =
        (struct mailbuffer *)uspios.mailbuffer;
    *mailbuffer = (struct mailbuffer) {
        .bufsize = sizeof *mailbuffer,
        .code = 0x00000000, /* 'process request' */
        .tag = {
            .id = 0x00028001, /* set power state */
            .bufsize = 8,
            .len = 8,
            .setpower = {
                .device = nDeviceId,
                .state = (/* wait */ 1 << 1) | (/* on */ 1 << 0)
            }
        },
        .endtag = 0
    };

    struct mailboxregs {
        uint32_t read0;
        PAD(MAILBOX0_READ, MAILBOX0_STATUS);
        uint32_t status0;
        PAD(MAILBOX0_STATUS, MAILBOX1_WRITE);
        uint32_t write1;
        PAD(MAILBOX1_WRITE, MAILBOX1_STATUS);
        uint32_t status1;
    } __packed;

    /* Flush any junk sitting in the read FIFO out. */
    volatile struct mailboxregs *mb = (struct mailboxregs *)MAILBOX_BASE;
    while (!(mb->status0 & MAILBOX_STATUS_EMPTY)) {
        (void)mb->read0;
        /* The spec doesn't say anything about it, but the USPi env version of
         * this sleeps and I never get a response from the VideoCore if I don't,
         * so... */
        MsDelay(20);
    }

    /* Wait until the write mailbox isn't full. */
    while (mb->status1 & MAILBOX_STATUS_FULL) {
        /* wait */
    }

    /* Write the address of our SetPowerState property tag buffer, setting the
     * least significant bits to channel 8. */
    uint32_t gpuaddr = (uint32_t)mailbuffer | 0xc0000000;
    mb->write1 = gpuaddr | 0x8;

    /* Now wait for the response. */
    uint32_t response;
    do {
        while (mb->status0 & MAILBOX_STATUS_EMPTY) {
            /* wait */
        }

        response = mb->read0;
    } while ((response & 0xf) != 8);

    USPI_PLATFORM_ASSERT((response & ~0xf) == gpuaddr);
    USPI_PLATFORM_ASSERT(mailbuffer->code == 0x80000000);
    USPI_PLATFORM_ASSERT(mailbuffer->tag.setpower.state & (1 << 0));

    return 1;
}

int GetMACAddress (unsigned char Buffer[6])
{
    /* yolo */
    memset(Buffer, 0, 6);
    return 1;
}

void LogWrite (const char *pSource,
               unsigned Severity,
               const char *pMessage,
               ...)
{
    va_list va;
    va_start(va, pMessage);
    debug_vprintf(pMessage, va);
    va_end(va);
}
