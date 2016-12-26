#include "offsets.h"

.global engine
.global irq_except
.global software_except

#define SYSTEM_MODE 0b11111
#define IRQ_MODE    0b10010
#define SVC_MODE    0b10011

/* Do a bunch of work to save the active task stack pointer. */
/* Modifies @scratch, stores @to_save */
.macro save_sp scratch to_save
    ldr \scratch, =(caboose + caboose_tasks_offset + tasks_active_offset)
    ldr \scratch, [\scratch]
    str \to_save, [\scratch, #task_sp_offset]
.endm

software_except:
    /********* SUPERVISOR ***********/
    cps #SYSTEM_MODE                    /* Jank immediately into system mode. */
    /********* SYSTEM ***********/
    stmfd sp!, { r4-r11, lr }           /* Save the callee-saved registers. */
    sub r4, sp, #4                      /* Make space for r0 and place the
                                         * resulting sp value in r4, where we
                                         * can touch it from SVC_MODE. */

    cps #SVC_MODE                       /* Jank back for good. */
    /********* SUPERVISOR ***********/
    mrs r5, spsr
    stmfd r4!, { r5 }                   /* Stack spsr. */
    save_sp r5 r4                       /* Using r5 as scratch, save the final
                                         * sp value (in r4) into the active
                                         * task. */

    ldr r5, [lr, #-4]
    bic r5, r5, #0xff000000 /* Remove the opcode from the svc instruction
                             * so we can get the index in syscall table. */

    ldr r6, =syscalls        /* Put the base of the syscall table in r6. */
    ldr r6, [r6, r5, lsl #2] /* r6 now points to the syscall. */
    blx r6                   /* Call the computed syscall handler. */
    str r0, [r4, #svcframe_sf_r0_offset] /* Save the return value to the frame
                                          * on the caller's stack. */

activate:
    /********* SUPERVISOR ***********/
    bl schedule             /* Find the next task to activate. */
    tst r0, #0x80000000     /* Did we enter through an IRQ, or an SVC? */
    bne irq_return          /* => IRQ */

    /* If we're here, we're returning from SVC. */
    ldmia r0!, { r5 }       /* Load the spsr from the SVC frame into r5. */
    msr spsr, r5            /* Restore spsr. */
    ldmia r0!, {r3-r11, lr} /* Restore the user registers. */
    cps #SYSTEM_MODE        /* Jank into system mode for sp. */
    /********* SYSTEM ***********/
    mov sp, r0
    cps #SVC_MODE           /* Jank back to return. */
    /********* SUPERVISOR ***********/
    mov r0, r3                          /* r3 contains the saved return value */
    movs pc, lr                         /* This also restores cpsr from spsr. */

engine:
    push { fp, lr }
    b activate

irq_return:
    cps #IRQ_MODE
    /********* IRQ ***********/
    bic r0, #0x80000000     /* Make sp great again. */
    ldmia r0!, { r1, r2 }   /* Fetch the spsr and lr from the stack. */
    msr spsr, r1            /* Restore spsr. */
    mov lr, r2              /* Restore lr (the interrupted instruction). */
    cps #SYSTEM_MODE
    /********* SYSTEM ***********/
    /* Pop what's left of the irqframe. */
    mov sp, r0
    ldmia sp!, { r0-r12, lr }
    cps #IRQ_MODE
    movs pc, lr

irq_except:
    /********* IRQ ***********/
    sub lr, #4                      /* Fix up the link register. */
    cps #SYSTEM_MODE
    /********* SYSTEM ***********/
    stmfd sp!, {r0-r12, lr}         /* Save r0-r12 and lr on the user stack. */

    cps #IRQ_MODE                   /* Back to IRQ mode to get the SPSR and link
                                     * register. */
    /********* IRQ ***********/
    mrs r0, spsr
    mov r1, lr
    cps #SYSTEM_MODE                /* Back to system mode to stack them. */
    /********* SYSTEM ***********/
    stmfd sp!, { r0, r1 }           /* Actually save them. */
    orr sp, #0x80000000             /* Set the most significant bit of sp so
                                     * that we remember we need to resume an
                                     * irqframe rather than an svcframe. */
    save_sp r0 sp                   /* Save sp to the active task. */

    cps #SVC_MODE
    /********* SUPERVISOR ***********/
    bl irq_service
    b activate
