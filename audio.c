#include <stdint.h>

#include <caboose/caboose.h>
#include <caboose/platform.h>
#include <caboose/util.h>

#include <caboose-platform/bcm2835.h>
#include <caboose-platform/bcm2835int.h>
#include <caboose-platform/debug.h>
#include <caboose-platform/irq.h>
#include <caboose-platform/mmu.h>
#include <caboose-platform/platform-events.h>
#include <caboose-platform/util.h>

#include "audio.h"
#include "messages.h"

/* The function of this driver is to turn buffers of 12-bit 44100Hz PCM audio
 * samples into sound from the 3.5mm headphone jack.  Doing so requires tying
 * together 4 separate BCM2835 peripherals, so this is really a number of
 * tightly-coupled drivers bundled together.
 *
 * In this explanation, I'll start from the physical headphone jack and work my
 * way back to the digital interface.  The first question is therefore "what
 * produces the analog audio signal emitted by the headphone jack?"  In [1], a
 * Raspberry Pi Foundation engineer doing audio work explains the DAC situation:
 * "The basic method of audio generation is the same across all models of Pi
 * (except Zero, which doesn't have it). The PWM peripheral is used to drive an
 * RC filter that "approximates" a DAC."  The fundamental business of our driver
 * is therefore to initialize the PWM peripheral and feed it our samples as we
 * get them.
 *
 * The PWM peripheral is documented in Chapter 9 of the BCM2835 datasheet.  It
 * has two output channels, which the documentation refers to as either PWM0 and
 * PWM1 or Channel 1 and Channel 2.  From Raspberry Pi forum communal wisdom like
 * [2] we know that "PWM channel 0 is fed to GPIO40 which is connected to the
 * (stereo) right channel, and PWM channel 1 is fed to GPIO45 which is connected
 * to the (stereo) left channel".  Section 9.5 of the datasheet describes the
 * assignment of GPIO pins to PWM channel outputs, and from there we can see
 * that in order to get PWM0 output on GPIO40 we need to put GPIO40 in ALT0 mode
 * and to get PWM1 output on GPIO45 we need to put GPIO45 in ALT0 mode.
 *
 * XXX NOTE: these pin assignments are different for RPi3 [3] XXX
 *
 * Once we've got the output of the PWM fed to the right GPIO pins, we come to
 * the more fundamental question: how do we produce the correct PWM output for a
 * buffer of input PCM samples?  To answer that, a quick primer on PWM would be
 * a good start: PWM, or pulse-width modulation, encodes a message in a
 * periodically pulsing signal.  In our case, the 'message' we're encoding is
 * the values of successive PCM samples, and the 'periodic pulsing signal' we've
 * got to encode them with is the voltage of the output GPIO pin (either high or
 * low) over time.  The data in the message is encoded by modulating the 'width'
 * of each periodic pulse in proportion to the full width of each period
 * according to the data value. (the ratio between the pulse width and full
 * period width is often called the 'duty cycle' and expressed as a percentage)
 *
 * As an example, consider a periodic pulse with a period (full width) of 3.
 * Each pulse can have 4 possible widths:
 *
 * |   |*  |** |***
 * +--- --- --- ---
 *   0   1   2   3
 *
 * And so PWM can encode a 2-bit value with each period.  The message "3021"
 * would be encoded over 4 periods as
 *
 * |***|   |** |*  |...
 * +-------------------
 *
 * In order to use the BCM2835 PWM, we therefore need answers to these more
 * specific questions:
 * 1) How can we configure the PWM to use the duration of 1 period of our audio
 *    format's sample rate as the width of 1 PWM period?
 * 2) How do we turn PCM samples into pulse widths and feed them to the PWM
 *    peripheral?
 *
 * For 1), we need to a) configure the PWM's clock and then b) define the PWM
 * period as the number of ticks of the PWM clock in 1 sample period.
 *
 * For a), the datasheet is kind of vague with respect to the PWM clock - in the
 * PWM chapter, all it has to say on the subject is "Both modes clocked by
 * clk_pwm which is nominally 100MHz, but can be varied by the clock manager."
 * The trouble is that there isn't really any documentation of the 'Clock
 * Manager' anywhere - other sections of the datasheet (like the UART) refer to
 * it, but it doesn't have its own section.  There is, however, some register
 * documentation on Clock Manager registers for the GPIO in Chapter 6, and based
 * on rsta2's Circle and Peter Lemon's repos I've inferred that the PWM portion
 * of the Clock Manager has the same register layout at a different
 * memory-mapped address.  Each peripheral's clock manager fundamentally serves
 * two purposes: it selects one of the onboard clock sources, and it divides
 * that clock source by a programmable value to reduce the frequency of the
 * clock seen by the peripheral (with some cleverness built in to get
 * frequencies that aren't pure integer divisors of the source frequency).
 *
 * From [5], we take it that PLLD clock source is a 500MHz clock that doesn't
 * vary with the CPU clock frequency, so it seems like a reasonable choice.
 * However, experimentally I've found that this clock source doesn't work unless
 * the clock manager is configured with a divider of at least 2, so really what
 * we have is a 250MHz clock.
 *
 * We can then move on to b) - how many clock ticks in a PWM period?  Well,
 * 44100Hz audio means each sample has a period of 22.675us, and the 250MHz
 * clock ticks ~5669 times in that time.  So, this is the value that we need to
 * configure the PWM peripheral with - each channel has a range register which
 * we program with the length of the PWM period, in PWM clock ticks.
 *
 * For 2), what we need to know is what sort of data the PWM peripheral is
 * expecting.  It turns out that what we feed it for each sample is our desired
 * pulse width, in clock ticks.  For example, to encode minimum amplitude we'd
 * feed it 0 (0% duty cycle), and to encode maximum amplitude we'd feed it 5669
 * (100% duty cycle).  This means that if we require that our input PCM format
 * be 12-bit unsigned, we can pass them directly to the PWM! (with 4095, the
 * maximum representable amplitude, being 72% of the maximum PWM duty cycle -
 * this is fine, because 100% duty cycle is uncomfortably loud)
 *
 * But wait!  How do we actually get our PCM(/PWM) samples into the PWM
 * peripheral?
 *
 * The PWM has a FIFO, from which the 2 channels take values to encode in a
 * round robin fashion.  One option would be to buffer input samples in the
 * driver and periodically poll the FIFO for capacity, feeding it samples when
 * it has space.  A better option is DMA - we can set up direct memory -> FIFO
 * transfers managed by the BCM2835 DMA peripheral (Chapter 4).
 *
 * Since we aim to produce uninterrupted/non-stop output, our DMA scheme will be
 * to maintain a pair of descriptors, each of the same size, that point to one
 * another cyclically.  We'll set the interrupt bit on both of them, so that
 * each time we receive the interrupt we can get to work filling the
 * just-finished descriptor's buffer back up with new samples while the DMA
 * engine is busy feeding the other descriptor's samples to the PWM.  Assuming
 * that we always meet our deadline (filling our current descriptor's buffer
 * with new samples before the other descriptor finishes its transfer), the
 * theoretical audio latency of our system will then be 22.675us * DMA sample
 * count.
 *
 * Aaaaaaand that's it, audio on the Raspberry Pi from PCM sample buffer to
 * 3.5mm analog signal.
 *
 * [1] https://www.raspberrypi.org/forums/viewtopic.php?f=29&t=136445
 * [2] http://raspberrypi.stackexchange.com/questions/49600/how-to-output-audio
 *     -signals-through-gpio
 * [3] https://github.com/PeterLemon/RaspberryPi/issues/10#issuecomment-260231186
 * [4] https://en.wikipedia.org/wiki/Pulse-width_modulation
 * [5] http://raspberrypi.stackexchange.com/questions/1153/what-are-the-different
 *     -clock-sources-for-the-general-purpose-clocks
 */

#define DMA_SAMPLE_CNT 32 /* 32 * 22.675us = 725.6us theoretical latency */

/* I'm omitting the registers I'm not using for now (which is most of them).
 * The full set is described in the datasheet, and a mostly complete subset of
 * those are defined in rsta2's bcm2835.h. */
struct gpioregs {
    LEADPAD(ARM_GPIO_BASE, ARM_GPIO_GPFSEL4);
    uint32_t gpfsel4;
} __packed;

#define GPIO_GPFSEL4_BASE 40
#define GPIO_GPFSEL_BITS 3
#define GPIO_ALT0 0b100

struct clkregs {
    uint32_t ctl;
    uint32_t div;
} __packed;

/* eff this, why on earth is this a thing? */
#define CLK_PASSWD (0x5a << 24)

#define CLKCTL_BUSY (1 << 7)
#define CLKCTL_KILL (1 << 5)
#define CLKCTL_ENAB (1 << 4)
#define CLKCTL_PLLD 6

#define CLKDIV_DIVI_SHIFT 12

struct pwmregs {
    uint32_t ctl;
    uint32_t sta;
    uint32_t dmac;
    PAD(ARM_PWM_DMAC, ARM_PWM_RNG1);
    uint32_t rng1;
    uint32_t dat1;
    uint32_t fif1;
    PAD(ARM_PWM_FIF1, ARM_PWM_RNG2);
    uint32_t rng2;
    uint32_t dat2;
} __packed;

#define PWMCTL_PWEN1 (1 << 0)
#define PWMCTL_USEF1 (1 << 5)
#define PWMCTL_CLRF1 (1 << 6)
#define PWMCTL_PWEN2 (1 << 8)
#define PWMCTL_USEF2 (1 << 13)

#define PWMDMAC_ENAB (1 << 31)

void dump_pwmregs(void)
{
    volatile struct pwmregs *pwm = (struct pwmregs *)ARM_PWM_BASE;
    debug_printf("0x%x 0x%x 0x%x 0x%x 0x%x",
                 pwm->ctl,
                 pwm->sta,
                 pwm->dmac,
                 pwm->rng1,
                 pwm->rng2);
}

struct dmaconblk {
    uint32_t ti;
    uint32_t sourcead;
    uint32_t destad;
    uint32_t txfrlen;
    uint32_t stride;
    uint32_t nextconblk;
    /* Pad the struct out with its reserved members so that all members of a
     * 32-byte aligned array are themselves aligned. */
    uint32_t rsvd1;
    uint32_t rsvd2;
} __packed;

#define TI_INTEN (1 << 0)
#define TI_DEST_DREQ (1 << 6)
#define TI_SRC_INC (1 << 8)
#define TI_PERMAP_SHIFT 16

struct dmaregs {
    uint32_t cs;
    uint32_t conblkad;
    struct dmaconblk conblk;
} __packed;

void dump_dmaconblk(struct dmaconblk *conblk)
{
    debug_printf("(conblk 0x%x) 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
                 (uint32_t)conblk,
                 conblk->ti,
                 conblk->sourcead,
                 conblk->destad,
                 conblk->txfrlen,
                 conblk->stride,
                 conblk->nextconblk);
}

void dump_dmaregs(void)
{
    volatile struct dmaregs *dma = (struct dmaregs *)ARM_DMA_BASE;
    debug_printf("(dma 0x%x) 0x%x 0x%x", (uint32_t)dma, dma->cs, dma->conblkad);
    dump_dmaconblk((struct dmaconblk *)&dma->conblk);
}

#define DMA_CS_ACTIVE (1 << 0)
#define DMA_CS_END (1 << 1)
#define DMA_CS_INT (1 << 2)

static void dma_irq_handler(void)
{
    volatile struct dmaregs *dma = (struct dmaregs *)ARM_DMA_BASE;

    /* The cs register requires that you manually clear the interrupt and
     * transfer-end bits.  Make sure to write active back to avoid accidentally
     * pausing the current transfer. */
    dma->cs = DMA_CS_ACTIVE | DMA_CS_END | DMA_CS_INT;

    /* It isn't explicitly documented in the data sheet, but you also need to
     * acknowledge the interrupt by writing to the channel's bit in the global
     * interrupt status register. */
    volatile uint32_t *dmastat = (uint32_t *)ARM_DMA_STAT;
    *dmastat = 1 << 0;

    /* Poke the audio task to fill the next DMA buffer. */
    event_deliver(DMA0_EVENTID, 0xcab005e);
}

static void audio_init(struct dmaconblk conblks[2],
                       uint32_t bufs[2][DMA_SAMPLE_CNT * 2])
{
    /* Configure the left and right audio GPIOs to be PWM outputs.
     *
     * GPIO is covered in Chapter 6 of the datasheet.  There are 6 function
     * select registers, each of which covers 10 pins with 3 bits per pin with
     * these semantics:
     *
     *     000 = GPIO Pin is an input
     *     001 = GPIO Pin is an output
     *     100 = GPIO Pin takes alternate function 0
     *     101 = GPIO Pin takes alternate function 1
     *     110 = GPIO Pin takes alternate function 2
     *     111 = GPIO Pin takes alternate function 3
     *     011 = GPIO Pin takes alternate function 4
     *     010 = GPIO Pin takes alternate function 5
     *
     * Pins 40 and 45 are covered by GPFSEL4.
     *
     * XXX BIG SCARY WARNING XXX
     *
     * Turns out the USB host controller depends on the (apparently non-zero)
     * reset value of function select register 4, so we need to carefully
     * preserve the values of the bits we're not touching here.  USPi doesn't
     * directly manipulate the register and it's the only thing running on the
     * other core, so for now we're not going to bother with a spinlock or
     * anything. */
    volatile struct gpioregs *gpio = (struct gpioregs *)ARM_GPIO_BASE;
    gpio->gpfsel4 |=
        (GPIO_ALT0 << ((40 - GPIO_GPFSEL4_BASE) * GPIO_GPFSEL_BITS))
        | (GPIO_ALT0 << ((45 - GPIO_GPFSEL4_BASE) * GPIO_GPFSEL_BITS));

    /* Configure the PWM clock to use PLLD with an integer divisor of 1. */
    volatile struct clkregs *clk = (struct clkregs *)ARM_CM_PWM_BASE;

    /* Obey the warnings in the datasheet and ensure the clock isn't busy before
     * configuring it. */
    clk->ctl |= CLK_PASSWD | CLKCTL_KILL;
    while (clk->ctl & CLKCTL_BUSY) {
        /* wait */
    }

    /* Choose a divisor of 2, experimentally the lowest that works. */
    clk->div = CLK_PASSWD | (2 << CLKDIV_DIVI_SHIFT);
    /* Select PLLD and enable the clock. */
    clk->ctl = CLK_PASSWD | CLKCTL_ENAB | CLKCTL_PLLD;

    /* Configure the range register of each channel to use a 22.675us period
     * (5669 ticks of the 250MHz PWM clock). */
    volatile struct pwmregs *pwm = (struct pwmregs *)ARM_PWM_BASE;
    pwm->rng1 = 5669;
    pwm->rng2 = 5669;

    /* Enable both channels, configure them both to use the FIFO (sharing it
     * round robin as described in the datasheet), and clear the FIFO as
     * recommended by the datasheet ("If the set of channels to share the FIFO
     * has been modified after a configuration change, FIFO should be cleared
     * before writing new data.") */
    pwm->ctl = PWMCTL_PWEN1
               | PWMCTL_USEF1
               | PWMCTL_PWEN2
               | PWMCTL_USEF2
               | PWMCTL_CLRF1;

    /* Initialize the PWM side of the DMA transfer by enabling DMA and
     * configuring the DREQ signal, which the PWM uses to signal to the DMA
     * engine that it should be fed. (I'm honestly not exactly sure what the
     * threshold _value_ means - maybe the number of words remaining in the FIFO
     * before it's exhausted?) */
    pwm->dmac = PWMDMAC_ENAB | 1;

    /* Configure DMA Channel 0 to transfer to the PWM FIFO. */

    /* Enable channel 0 in the global channel enable register. */
    volatile uint32_t *dmaenab = (uint32_t *)ARM_DMA_ENAB;
    *dmaenab = 1 << 0;

    /* Set up the two control blocks. */
    for (int i = 0; i < 2; i++) {
        struct dmaconblk *conblk = &conblks[i];
        uint32_t *buf = &bufs[i][0];

        conblk->ti = TI_INTEN /* we want an interrupt on each completion */
                     | TI_DEST_DREQ /* follow the PWM's pacing signal */
                     | TI_SRC_INC /* the transfer source is just memory, so each
                                   * subsequent word is located one word later
                                   * in memory */
                     | (5 << TI_PERMAP_SHIFT); /* from section 9.5 */
        /* From Section 1.2.4 in the datasheet: "Software accessing RAM using
         * the DMA engines must use bus addresses (based at 0xC0000000)".  Fun
         * fact: this seems to have something to do with caching in the DMA
         * engine, because (as I learned the hard way) if you use the CPU
         * physical address instead the DMA engine never observes subsequent
         * writes to the buffers and just plays the first sample buffer over and
         * over and over again... :( */
        conblk->sourcead = 0xC0000000 | (uint32_t)buf;
        /* "Beware that the DMA controller is direcly connected to the
         * peripherals. Thus the DMA controller must be set-up to use the
         * Physical (harware) addresses of the peripherals." */
        conblk->destad = GPU_IO_BASE + (ARM_PWM_FIF1 - ARM_IO_BASE);
        conblk->txfrlen = sizeof bufs[0];
        /* We're doing a 1d transfer, no need to skip any bytes. */
        conblk->stride = 0;
        /* Form a cycle. */
        conblk->nextconblk = (uint32_t)&conblks[!i];

        /* Now make sure all of our modifications to the control block structure
         * are visible from the DMA engine before handing it off. */
        Clean(conblk, sizeof *conblk);
    }

    /* Start off pointing to the first control block. */
    volatile struct dmaregs *dma = (struct dmaregs *)ARM_DMA_BASE;
    dma->conblkad = (uint32_t)&conblks[0];

    /* Wire up the channel-0 IRQ. */
    irq_register(ARM_IRQ_DMA0, dma_irq_handler);

    /* The task will actually activate the channel by writing the active bit once
     * it has some samples to send.  We're done! */
}

void audio(void)
{
    /* "Control Blocks (CB) are 8 words (256 bits) in length and must start at a
     * 256-bit aligned address. */
    struct dmaconblk conblks[2] __aligned(32);
    /* The samples for the left and right channel are interleaved, so we need
     * two actual samples for each logical 'sample' in time. */
    uint32_t bufs[2][DMA_SAMPLE_CNT * 2];

    /* Initialize all of the hardware required for audio output. */
    audio_init(conblks, bufs);

    /* Find the source of system audio. */
    tid_t audio_source = WhoIs(AUDIO_SOURCE);

    /* Before beginning audio output, fill both of the buffers. */
    struct audioreq req = {
        .hdr = {
            .type = GET_AUDIO
        },
        .len = DMA_SAMPLE_CNT
    };

    for (int i = 0; i < 2; i++) {
        int replylen = Send(audio_source,
                            &req,
                            sizeof req,
                            &bufs[i][0],
                            sizeof bufs[0]);
        ASSERT(replylen == sizeof bufs[0]);

        Clean(&bufs[i][0], sizeof bufs[0]);
    }

    /* Activate the DMA channel. */
    volatile struct dmaregs *dma = (struct dmaregs *)ARM_DMA_BASE;
    dma->cs = DMA_CS_ACTIVE;

    /* Buffer 0 will be consumed first. */
    int i = 0;
    while (true) {
        /* Wait until the current buffer has been fully consumed by DMA. */
        int rc = AwaitEvent(DMA0_EVENTID);
        ASSERT(rc == 0xcab005e);

        /* Assert that the engine is processing the other buffer right now. */
        ASSERT(dma->conblkad == (uint32_t)&conblks[!i]);

        /* Refill it with new audio data. */
        int replylen = Send(audio_source,
                            &req,
                            sizeof req,
                            &bufs[i][0],
                            sizeof bufs[0]);
        ASSERT(replylen == sizeof bufs[0]);

        /* Make sure the DMA engine observes our writes. */
        Clean(&bufs[i][0], sizeof bufs[0]);

        /* Assert that the engine is _still_ processing the other buffer right
         * now. */
        ASSERT(dma->conblkad == (uint32_t)&conblks[!i]);

        /* Flip the buffer. */
        i = !i;
    }
}
