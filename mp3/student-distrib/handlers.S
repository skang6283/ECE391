#define ASM


/* given name and handler function, creates an assembly linkage */
#define HANDLER(name,handler)   \
.global name                    ;\
name:                           ;\
    pushal                      ;\
    call handler                ;\
    popal                       ;\
    iret                        ;\

HANDLER(KEYBOARD_WRAPPER, keyboard_handler);
HANDLER(RTC_WRAPPER, rtc_handler);


