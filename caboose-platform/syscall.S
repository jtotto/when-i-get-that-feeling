#include "syscalltable.h"

.macro syscall syscallname, syscallno
.global \syscallname

\syscallname:
    svc #(\syscallno)

.endm

syscall Create, SYSCALL_CREATE
syscall MyTid, SYSCALL_MYTID
syscall MyParentTid, SYSCALL_MYPARENTTID
syscall Pass, SYSCALL_PASS
syscall Exit, SYSCALL_EXIT
syscall Send, SYSCALL_SEND
syscall Receive, SYSCALL_RECEIVE
syscall Reply, SYSCALL_REPLY
syscall AsyncSend, SYSCALL_ASYNC_SEND
syscall AsyncReceive, SYSCALL_ASYNC_RECEIVE
syscall AwaitEvent, SYSCALL_AWAITEVENT
syscall Assert, SYSCALL_ASSERT
syscall Clean, SYSCALL_CLEAN
syscall Invalidate, SYSCALL_INVALIDATE
syscall CleanAndInvalidate, SYSCALL_CLEAN_AND_INVALIDATE
syscall AcknowledgeEvent, SYSCALL_ACKNOWLEDGEEVENT
