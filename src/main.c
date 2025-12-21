#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#include "dynamic_string.h"

const char* filename = "apple.png";

void get_background_color(uint8_t* r, uint8_t* g, uint8_t* b) {
    *r = 12;
    *g = 12;
    *b = 12;
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetFileType(h) != FILE_TYPE_CHAR) {
        h = GetStdHandle(STD_ERROR_HANDLE);
        if (GetFileType(h) != FILE_TYPE_CHAR) {
            return;
        }
    }
    CONSOLE_SCREEN_BUFFER_INFOEX csbiex;
    csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    
    if (GetConsoleScreenBufferInfoEx(h, &csbiex)) {
        // Extract background color index (high 4 bits of wAttributes)
        WORD backgroundIndex = (csbiex.wAttributes & 0xF0) >> 4;

        // Each color index in the table maps to an RGB color
        COLORREF bgColor = csbiex.ColorTable[backgroundIndex];

        *r = GetRValue(bgColor);
        *g = GetGValue(bgColor);
        *b = GetBValue(bgColor);
    }
#endif
}

void get_console_size(int* w, int* h) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO i;
    if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &i)) {
        if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_ERROR_HANDLE), &i)) {
            HANDLE hn = CreateConsoleScreenBuffer(GENERIC_WRITE, 0, NULL, 
                    CONSOLE_TEXTMODE_BUFFER, 0);
            if (!GetConsoleScreenBufferInfo(hn, &i)) {
                *w = 80;
                *h = 20;
                return;
            }
            CloseHandle(hn);
        }
    }

    *w = i.dwSize.X;
    *h = i.dwSize.Y;
#else
    struct winsize sz;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &sz) == 0) {
        *w = sz.ws_col;
        *h = sz.ws_row;
    } else {
        *w = 80;
        *h = 20;
    }
#endif
}

uint8_t add_alhpa(int32_t c, int32_t a, int32_t bg) {
    return c + ((0xff - a) * (bg - c)) / 255;
}

void sample_pixel(SDL_Surface* s, uint32_t x, uint32_t y, uint32_t scale, 
                  uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) {
    uint64_t r_sum = 0;
    uint64_t g_sum = 0;
    uint64_t b_sum = 0;
    uint64_t a_sum = 0;
    uint64_t count = 0;
    for (uint64_t px = 0; px < scale; ++px) {
        for (uint64_t py = 0; py < scale; ++py) {
            if (x + px > s->w || y + py > s->h) {
                continue;
            }
            ++count;
            uint8_t r, g, b, a;
            SDL_ReadSurfacePixel(s, x, y, &r, &g, &b, &a);
            r_sum += r;
            g_sum += g;
            b_sum += b;
            a_sum += a;
        }
    }
    *r = r_sum / count;
    *g = g_sum / count;
    *b = b_sum / count;
    *a = a_sum / count;
}


void convert_frame(String* dest, SDL_Surface* s, int scale, SDL_Color bg) {
    int pw = (s->w + scale - 1) / scale;
    int ph = (s->h + scale - 1) / scale;
    ph = (ph + 1) / 2;

    for (uint32_t y = 0; y < ph; ++y) {
        String_format_append(dest, "\x1b[0m\n");
        uint32_t last_rgb1 = 0xffffffff, last_rgb2 = 0xffffffff;
        for (uint32_t x = 0; x < pw; ++x) {
            uint8_t r1, g1, b1, a1;
            uint8_t r2 = 0, g2 = 0, b2 = 0, a2 = 0;
            sample_pixel(s, x * scale, 2 * y * scale, scale, &r1, &g1, &b1, &a1);
            r1 = add_alhpa(r1, a1, bg.r);
            g1 = add_alhpa(g1, a1, bg.g);
            b1 = add_alhpa(b1, a1, bg.b);
            if ((2 * y + 1) * scale < s->h) {
                sample_pixel(s, x * scale, (2 * y + 1) * scale, 
                             scale, &r2, &g2, &b2, &a2);
                r2 = add_alhpa(r2, a2, bg.r);
                g2 = add_alhpa(g2, a2, bg.g);
                b2 = add_alhpa(b2, a2, bg.b);
            }
            uint32_t rgb1 = r1 | (g1 << 8) | (b1 << 16);
            uint32_t rgb2 = r2 | (g2 << 8) | (b2 << 16);
            if (rgb1 == last_rgb1) {
                if (rgb2 == last_rgb2) {
                    if (rgb1 == rgb2) {
                        String_append(dest, ' ');
                    } else {
                        String_append_count(dest,  "\xe2\x96\x80", 3);
                    }
                } else {
                    if (rgb1 == rgb2) {
                        String_format_append(dest, 
                            "\x1b[48;2;%u;%u;%um ",
                            (unsigned)r2, (unsigned)g2, (unsigned)b2);
                    } else {
                        String_format_append(dest, 
                            "\x1b[48;2;%u;%u;%um\xe2\x96\x80",
                            (unsigned)r2, (unsigned)g2, (unsigned)b2);
                    }
                }
            } else if (rgb2 == last_rgb2) {
                if (rgb1 == rgb2) {
                    String_format_append(dest, 
                        "\x1b[38;2;%u;%u;%um ",
                        (unsigned)r1, (unsigned)g1, (unsigned)b1);
                } else {
                    String_format_append(dest, 
                        "\x1b[38;2;%u;%u;%um\xe2\x96\x80",
                        (unsigned)r1, (unsigned)g1, (unsigned)b1);
                }
            } else if (rgb1 == rgb2) {
                String_format_append(dest, 
                    "\x1b[38;2;%u;%u;%um\x1b[48;2;%u;%u;%um ",
                    (unsigned)r1, (unsigned)g1, (unsigned)b1, 
                    (unsigned)r2, (unsigned)g2, (unsigned)b2);
            } else {
                String_format_append(dest, 
                    "\x1b[38;2;%u;%u;%um\x1b[48;2;%u;%u;%um\xe2\x96\x80",
                    (unsigned)r1, (unsigned)g1, (unsigned)b1, 
                    (unsigned)r2, (unsigned)g2, (unsigned)b2);
            }
            last_rgb1 = rgb1;
            last_rgb2 = rgb2;
        }
    }
    String_extend(dest, "\x1b[0m");
}

int main(int argc, char** argv) {
    int status = 0;
    SDL_Color bg;
    get_background_color(&bg.r, &bg.g, &bg.b);
    int cw, ch;
    get_console_size(&cw, &ch);
#ifdef _WIN32
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    bool tty = GetFileType(out) == FILE_TYPE_CHAR;
    DWORD old_mode;
    if (tty && (!GetConsoleMode(out, &old_mode) ||
        !SetConsoleMode(out, old_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))) {
        fprintf(stderr, "Failed setting console mode\n");
        return 1;
    }
#endif
    SDL_Init(SDL_INIT_EVENTS);
    const char* file = filename;
    if (argc > 1) {
        file = argv[1];
    }

    IMG_Animation* a = IMG_LoadAnimation(file);
    if (a == NULL) {
        fprintf(stderr, "Failed converting %s: %s\n", file, SDL_GetError());
        status = 1;
        goto end;
    }

    ch = ch * 2; // Two pixels per row

    int scale = 1;
    int pw = a->frames[0]->w;
    int ph = a->frames[0]->h;

    while (pw > cw || ph > ch) {
        ++scale;
        pw = (a->frames[0]->w + scale - 1) / scale;
        ph = (a->frames[0]->h + scale - 1) / scale;
    }

    String* str;
#ifdef _WIN32
    WString* ws;
    if (tty) {
        ws = SDL_malloc(a->count * sizeof(WString));
    } else {
        str = SDL_malloc(a->count * sizeof(String));
    }
#else
    str = SDL_malloc(a->count * sizeof(String));
#endif
    for (uint32_t i = 0; i < a->count; ++i) {
        String dest;
        String_create(&dest);
        String_format_append(&dest, "\x1b[1;1H");
        convert_frame(&dest, a->frames[i], scale, bg);
#ifdef _WIN32
        if (tty) {
            WString_create(&ws[i]);
            WString_from_utf8_bytes(&ws[i], dest.buffer, dest.length);
            String_free(&dest);
        } else {
            str[i] = dest;
        }
#else
        str[i] = dest;
#endif
    }

    for (uint32_t i = 0; i < a->count; ++i) {
#ifdef _WIN32
        if (tty) {
            WriteConsoleW(out, ws[i % a->count].buffer, ws[i % a->count].length, NULL, NULL);
        } else {
            DWORD w;
            WriteFile(out, str[i].buffer, str[i].length, &w, NULL);
        }
#else
        fwrite(str[i % a->count].buffer, 1, str[i % a->count].length, stdout);
#endif
        Uint64 now = SDL_GetTicks();
        int delay = a->delays[i % a->count];
        while (SDL_GetTicks() - now < delay) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                switch (e.type) {
                case SDL_EVENT_QUIT:
                    goto end;
                }
            }
            SDL_Delay(1);
        }
    }
end:
#ifdef _WIN32
    if (tty) {
        WriteConsoleW(out, L"\n", 1, NULL, NULL);
        SetConsoleMode(out, old_mode);
    } else {
        DWORD w;
        WriteFile(out, "\n", 1, &w, NULL);
    }
#else
    fwrite("\n", 1, 1, stdout);
#endif
    SDL_Quit();
    return status;
}
