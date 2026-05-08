// Snake game for YOTOS
// Build: make snake

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef int            i32;

// -------------------------------------------------------
// Syscall interface
// -------------------------------------------------------

#define SYS_EXIT    1
#define SYS_WRITE   2
#define SYS_READKEY 3
#define SYS_SLEEP   4

static inline void sys_exit(i32 code) {
    asm volatile("int $0x80" :: "a"(SYS_EXIT), "b"(code));
    __builtin_unreachable();
}

static inline char sys_readkey(void) {
    char c;
    asm volatile("int $0x80" : "=a"(c) : "0"(SYS_READKEY));
    return c;
}

static inline void sys_sleep(u32 ms) {
    asm volatile("int $0x80" :: "a"(SYS_SLEEP), "b"(ms));
}

// -------------------------------------------------------
// VGA text buffer  (80x25, 2 bytes per cell: char | attr)
// -------------------------------------------------------

#define VGA   ((volatile u16 *)0xB8000)
#define VGA_W 80
#define VGA_H 25

#define BLACK        0x00
#define WHITE        0x0F
#define GRAY         0x07
#define YELLOW       0x0E
#define GREEN        0x0A
#define RED          0x0C
#define CYAN         0x0B

static void vput(i32 x, i32 y, char c, u8 col) {
    VGA[y * VGA_W + x] = (u16)((col << 8) | (u8)c);
}

static void vclear(void) {
    for (i32 i = 0; i < VGA_W * VGA_H; i++)
        VGA[i] = (u16)(WHITE << 8 | ' ');
}

static void vstr(i32 x, i32 y, const char *s, u8 col) {
    while (*s) vput(x++, y, *s++, col);
}

// Print number n right-aligned in a 5-char field at (x,y)
static void vnum(i32 x, i32 y, u32 n, u8 col) {
    char buf[6];
    i32 i = 5;
    buf[i] = '\0';
    if (n == 0) {
        buf[--i] = '0';
    } else {
        while (n && i > 0) {
            buf[--i] = '0' + (char)(n % 10);
            n /= 10;
        }
    }
    // print digits then pad with spaces to fill 5 chars
    i32 pos = x;
    for (i32 j = i; j < 5; j++)
        vput(pos++, y, buf[j], col);
    while (pos < x + 5)
        vput(pos++, y, ' ', col);
}

// -------------------------------------------------------
// Play area
// -------------------------------------------------------

#define PX  1    // left edge (inside border)
#define PY  2    // top edge  (inside border)
#define PW  78   // width
#define PH  21   // height

static void draw_border(void) {
    for (i32 x = PX - 1; x <= PX + PW; x++) {
        vput(x, PY - 1,   '-', CYAN);
        vput(x, PY + PH,  '-', CYAN);
    }
    for (i32 y = PY; y < PY + PH; y++) {
        vput(PX - 1,  y, '|', CYAN);
        vput(PX + PW, y, '|', CYAN);
    }
    vput(PX - 1,  PY - 1,  '+', CYAN);
    vput(PX + PW, PY - 1,  '+', CYAN);
    vput(PX - 1,  PY + PH, '+', CYAN);
    vput(PX + PW, PY + PH, '+', CYAN);
}

// -------------------------------------------------------
// Game state
// -------------------------------------------------------

#define MAX_LEN 512

typedef struct { i32 x, y; } Pt;

static Pt  snake[MAX_LEN];
static i32 slen;
static i32 dx, dy;
static Pt  food;
static u32 score;
static u32 seed = 0xDEADBEEF;

static u32 rng(void) {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}

static void place_food(void) {
    i32 fx, fy, ok;
    do {
        fx = PX + (i32)(rng() % (u32)PW);
        fy = PY + (i32)(rng() % (u32)PH);
        ok = 1;
        for (i32 i = 0; i < slen; i++)
            if (snake[i].x == fx && snake[i].y == fy) { ok = 0; break; }
    } while (!ok);
    food.x = fx;
    food.y = fy;
}

// -------------------------------------------------------
// Entry point
// -------------------------------------------------------

void _start(void) {
    vclear();
    vstr(36, 0, "SNAKE",              YELLOW);
    vstr(2,  1, "Score:",             WHITE);
    vstr(30, 1, "WASD: move  Q: quit", GRAY);
    draw_border();

    // Initial snake: 3 segments in the centre, heading right
    i32 cx = PX + PW / 2;
    i32 cy = PY + PH / 2;
    snake[0].x = cx;     snake[0].y = cy;
    snake[1].x = cx - 1; snake[1].y = cy;
    snake[2].x = cx - 2; snake[2].y = cy;
    slen = 3;
    dx = 1; dy = 0;
    score = 0;

    for (i32 i = 0; i < slen; i++)
        vput(snake[i].x, snake[i].y, '#', GREEN);

    place_food();
    vput(food.x, food.y, '*', RED);
    vnum(9, 1, 0, WHITE);

    u32 speed = 130;   // ms per tick; decreases as score grows

    for (;;) {
        sys_sleep(speed);

        // Drain the key buffer; last valid direction wins
        char key;
        while ((key = sys_readkey()) != 0) {
            if ((key == 'w' || key == 'W') && dy == 0) { dx =  0; dy = -1; }
            if ((key == 's' || key == 'S') && dy == 0) { dx =  0; dy =  1; }
            if ((key == 'a' || key == 'A') && dx == 0) { dx = -1; dy =  0; }
            if ((key == 'd' || key == 'D') && dx == 0) { dx =  1; dy =  0; }
            if (key == 'q' || key == 'Q') sys_exit(0);
        }

        // Compute new head
        Pt nh;
        nh.x = snake[0].x + dx;
        nh.y = snake[0].y + dy;

        // Wall collision
        if (nh.x < PX || nh.x >= PX + PW || nh.y < PY || nh.y >= PY + PH)
            break;

        // Self collision — skip the tail (it moves away this tick)
        i32 dead = 0;
        for (i32 i = 0; i < slen - 1; i++)
            if (snake[i].x == nh.x && snake[i].y == nh.y) { dead = 1; break; }
        if (dead) break;

        i32 ate      = (nh.x == food.x && nh.y == food.y);
        Pt  old_tail = snake[slen - 1];

        if (ate && slen < MAX_LEN) slen++;   // grow before shifting so the tail is preserved

        // Shift the whole body forward one slot
        for (i32 i = slen - 1; i > 0; i--)
            snake[i] = snake[i - 1];
        snake[0] = nh;

        if (!ate) vput(old_tail.x, old_tail.y, ' ', BLACK);
        vput(nh.x, nh.y, '#', GREEN);

        if (ate) {
            score++;
            if (speed > 60) speed -= 4;
            vnum(9, 1, score, WHITE);
            place_food();
            vput(food.x, food.y, '*', RED);
        }
    }

    // Game over screen
    vstr(32, 11, "GAME OVER!", RED);
    vstr(27, 13, "Press Q to exit...", WHITE);
    char k;
    while ((k = sys_readkey()) != 'q' && k != 'Q')
        sys_sleep(50);
    sys_exit(0);
}
