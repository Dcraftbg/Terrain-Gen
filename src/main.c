// Copyright (c) 2025 Dcraftbg
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#include "game.h"

#define eprintfln(fmt, ...) (fprintf(stderr, fmt "\n", ##__VA_ARGS__))
#define die(fmt, ...) (eprintfln(fmt "\n", ##__VA_ARGS__), exit(1))


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <dlfcn.h>


#include <ctype.h>
#include <X11/Xlib.h>
#include <unistd.h>
#include <time.h>

uint32_t* pixels = NULL;

uint64_t time_milis_unspec(void) {
    struct timespec timespec = { 0 };
    clock_gettime(CLOCK_MONOTONIC, &timespec);
    return (timespec.tv_sec * 1000) + timespec.tv_nsec / (1000*1000);
}
void sleep_milis(uint64_t milis) {
    usleep(milis * 1000);
}

#define sym(type, name, ...) type (*name)(__VA_ARGS__);
GAME_SYMBOLS
#undef sym

void* game_dll = NULL;
int game_dll_number = 0;
bool load_game_symbols(void) {
    // if it already exists, close it
    if(game_dll) dlclose(game_dll);
    game_dll = dlopen("./game.so", RTLD_NOW);
    if(!game_dll) {
        eprintfln("Failed to open ./game.so: %s", dlerror());
        return false;
    }
#define sym(type, name, ...) \
    name = dlsym(game_dll, #name); \
    if(!name) { \
        dlclose(game_dll); \
        die("Failed to load " #name " %s", dlerror()); \
        return false; \
    }
    GAME_SYMBOLS
    return true;
}

int main(void) {
    if(!load_game_symbols()) return 1;
    Game game = game_main();
    game_reload_state(NULL, 0);

    Display* display = XOpenDisplay(NULL);
    if(display == NULL) die("No display");
    Window window = XCreateSimpleWindow(
            display, 
            XDefaultRootWindow(display),
            0, 0, game.width, game.height,
            0, 0, 0
        );
    XWindowAttributes wa = { 0 };
    XGetWindowAttributes(display, window, &wa);

    pixels = calloc(game.width*game.height, sizeof(*pixels));
    assert(pixels);
    XImage *image = XCreateImage(display, wa.visual, wa.depth, ZPixmap, 0, (char*)pixels, game.width, game.height, 32, game.width*sizeof(pixels[0]));
    GC gc = XCreateGC(display, window, 0, NULL);
    Atom wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", 0);
    XSetWMProtocols(display, window, &wm_delete_window, 1);
    XSelectInput(display, window, KeyPressMask /*| ...*/);
    XMapWindow(display, window);

    bool should_close = false;

    uint32_t delta = 0;

    uint64_t frame_time_now = time_milis_unspec();
    uint64_t frame_time_last = frame_time_now;
    uint64_t target_milis = (1*1000)/game.target_fps;
    while(!should_close) {
        while(XPending(display) > 0) {
            XEvent event = { 0 };
            XNextEvent(display, &event);
            switch(event.type) {
            case KeyPress: {
                int key = XLookupKeysym(&event.xkey, 0);
                switch(key) {
                case 'r': {
                    int ret = system("./build.sh");
                    if(ret < 0) {
                        eprintfln("Failed to rebuild");
                        break;
                    }
                    size_t old_state_size;
                    void* old_state = game_get_state(&old_state_size);
                    if(!load_game_symbols()) break;
                    game = game_main();
                    game_reload_state(old_state, old_state_size);
                    target_milis = (1*1000)/game.target_fps;
                    eprintfln("Successfully reloaded!");
                    // TODO: destroy and recreate the image if the size doesn't match
                } break;
                default:
                    game_keydown(key);
                }
            } break;
            case ClientMessage:
                if ((Atom) event.xclient.data.l[0] == wm_delete_window) {
                    should_close = true;
                }
                break;
            }
        }
        // Do drawing logic
        uint64_t delta_time_milis = frame_time_now - frame_time_last;
        game_update(pixels, delta_time_milis / 1000.f);
        XPutImage(display, window, gc, image, 0, 0, 0, 0, game.width, game.height);
        frame_time_last = frame_time_now;
        frame_time_now = time_milis_unspec();
        uint64_t time_diff = frame_time_now - frame_time_last;
        if(time_diff < target_milis) sleep_milis(target_milis - time_diff);
    }
    XCloseDisplay(display);
    return 0;
}
