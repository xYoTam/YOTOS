#define SYS_EXIT    1
#define SYS_WRITE   2
#define SYS_READKEY 3

static void sys_write(const char *buf, int len) {
    asm volatile("int $0x80" :: "a"(SYS_WRITE), "b"(buf), "c"(len));
}

static char sys_readkey(void) {
    char c;
    asm volatile("int $0x80" : "=a"(c) : "0"(SYS_READKEY));
    return c;
}

static void sys_exit(int code) {
    asm volatile("int $0x80" :: "a"(SYS_EXIT), "b"(code));
    __builtin_unreachable();
}

static int strlen(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void print(const char *s) {
    sys_write(s, strlen(s));
}

void _start(void) {
    print("Hello from YOTOS!\n");
    print("Press any key to exit...\n");
    while (!sys_readkey());
    sys_exit(0);
}
