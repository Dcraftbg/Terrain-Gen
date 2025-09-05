// Copyright (c) 2025 Dcraftbg
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#include "game.h"
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>
#include "darray.h"

#define FS_MALLOC malloc
#define FS_DEALLOC(ptr, size) ((void)(size), free(ptr))
#define defer_return(x) do { result = (x); goto DEFER; } while(0)
static char* read_entire_file(const char* path, size_t* size) {
    char* result = NULL;
    char* head = NULL;
    char* end = NULL;
    size_t buf_size = 0;
    long at = 0;
    FILE *f = fopen(path, "rb");

    if(!f) {
        fprintf(stderr, "ERROR Could not open file %s: %s\n",path,strerror(errno));
        return NULL;
    }
    if(fseek(f, 0, SEEK_END) != 0) {
        fprintf(stderr, "ERROR Could not fseek on file %s: %s\n",path,strerror(errno));
        defer_return(NULL);
    }
    at = ftell(f);
    if(at == -1L) {
        fprintf(stderr, "ERROR Could not ftell on file %s: %s\n",path,strerror(errno));
        defer_return(NULL);
    }
    *size = at;
    buf_size = at+1;
    rewind(f); // Fancy :D
    result = FS_MALLOC(buf_size);
    assert(result && "Ran out of memory");
    head = result;
    end = result+buf_size-1;
    while(head != end) {
        head += fread(head, 1, end-head, f);
        if(ferror(f)) {
            fprintf(stderr, "ERROR Could not fread on file %s: %s\n",path,strerror(errno));
            FS_DEALLOC(result, buf_size);
            defer_return(NULL);
        }
        if (feof(f)) break;
    }
    result[buf_size-1] = '\0';
DEFER:
    fclose(f);
    return result;
}

#define sym(type, name, ...) type name(__VA_ARGS__);
GAME_SYMBOLS

#define W_RATIO 16
#define H_RATIO 9
#define FPS 60
#define SCALE 70
#define WIDTH (W_RATIO*SCALE)
#define HEIGHT (H_RATIO*SCALE)
#define PI 3.141592653589f

typedef struct {
    float x, y;
} Vector2;
static inline Vector2 vec2_scale(Vector2 vec, float scale) {
    return (Vector2){vec.x * scale, vec.y * scale};
}
typedef struct {
    union {
        struct { float x, y, z; };
        float items[3];
    };
} Vector3;
typedef struct {
    Vector3* items;
    size_t len, cap;
} Vector3s;
typedef struct {
    unsigned int* items;
    size_t len, cap;
} Indecies;
typedef struct {
    // Animation delta
    uint32_t delta;
} State;
State* st = NULL;
Game game_main(void) {
    return (Game) {
        .width = WIDTH,
        .height = HEIGHT,
        .target_fps = FPS
    };
}
void* game_get_state(size_t* size) {
    *size = sizeof(*st);
    return st;
}

void regen_grid(void);
void game_reload_state(void* new_st, size_t size) {
    if(!sizeof(State)) return;
    new_st = realloc(new_st, sizeof(State));
    assert(new_st);
    if(size < sizeof(State)) memset(((char*)new_st) + size, 0, sizeof(State)-size);
    st = new_st;
    regen_grid();
}

static inline uint32_t abgr_to_argb(uint32_t agbr) {
    uint32_t r = agbr & 0xFF;
    uint32_t g = (agbr >> 8) & 0xFF;
    uint32_t b = (agbr >> 16) & 0xFF;
    uint32_t a = (agbr >> 24) & 0xFF;
    return (a << 24) | (r << 16) | (g << 8) | (b << 0);
}
#define CHECKER_MAX 512 
#define CHECKER_MIN 256

#define sign(v) ((v) < 0 ? -1 : 1)
#define absi(v) ((v) < 0 ? -(v) : (v))
static inline bool in_bounds(int x, int y) {
    return x >= 0 && x < WIDTH &&
           y >= 0 && y < HEIGHT;
}
void draw_line(uint32_t* pixels, int x1, int y1, int x2, int y2, uint32_t color) {
    int dx = x2 - x1,
        dy = y2 - y1;

    if(dx == 0 && dy == 0) {
        if(in_bounds(x1, y1)) {
            pixels[y1 * WIDTH + x1] = color;
        }
        return;
    }
    if(absi(dx) < absi(dy)) {
        if(y1 > y2) {
            int tmp = x1;
            x1 = x2;
            x2 = tmp;

            tmp = y1;
            y1 = y2;
            y2 = tmp;
        }
        for(int y = y1; y <= y2; ++y) {
            int x = dx * (y - y1) / dy + x1;
            if(in_bounds(x, y)) {
                pixels[y * WIDTH + x] = color;
            }
        }
    } else {
        if(x1 > x2) {
            int tmp = x1;
            x1 = x2;
            x2 = tmp;

            tmp = y1;
            y1 = y2;
            y2 = tmp;
        }
        for(int x = x1; x <= x2; ++x) {
            int y = dy * (x - x1) / dx + y1;
            if(in_bounds(x, y)) {
                pixels[y * WIDTH + x] = color;
            }
        }
    }
}
void draw_rect(uint32_t* pixels, size_t x, size_t y, size_t w, size_t h, uint32_t color) {
    for(size_t dy = 0; dy < h; ++dy) {
        for(size_t dx = 0; dx < w; ++dx) {
            if(in_bounds(x + dx, y + dy)) {
                pixels[(y + dy) * WIDTH + x + dx] = color;
            }
        }
    }
}
typedef struct {
    uint32_t value;
    int32_t delta;
} GridEntry;

#define GRID_WIDTH  (WIDTH/2 )
#define GRID_HEIGHT (HEIGHT/2 )
#define GRID_SIZE (GRID_WIDTH*GRID_HEIGHT)
static GridEntry grid[GRID_WIDTH*GRID_HEIGHT];

#define VALUE_RANGE 1024
int32_t rand_delta(void) {
    return (rand() % VALUE_RANGE) - (VALUE_RANGE/2);
}
void regen_deltas(void) {
    for(size_t i = 0; i < GRID_SIZE; ++i) {
        grid[i].delta = rand_delta();
    }
    for(size_t i = 0; i < GRID_SIZE; ++i) {
        int32_t delta = grid[i].delta;
        size_t count = 1;
        size_t x = i % WIDTH, y = i / WIDTH;

        if(x > 0) {
            if(y > 0) {
                count++;
                delta += grid[i-WIDTH-1].delta;
            }
            count++;
            delta += grid[i-1].delta;
        } else if(x < GRID_WIDTH-1) {
            if(y < GRID_HEIGHT-1) {
                count++;
                delta += grid[i+WIDTH+1].delta;
            }
            count++;
            delta += grid[i+1].delta;
        } else if(y > 0) {
            if(x < GRID_WIDTH-1) {
                count++;
                delta += grid[i-WIDTH+1].delta;
            }
            count++;
            delta += grid[i-WIDTH].delta;
        } else if(y < GRID_HEIGHT-1) {
            if(x > 0) {
                count++;
                delta += grid[i+WIDTH-1].delta;
            }
            count++;
            delta += grid[i-WIDTH].delta;
        } 
        grid[i].delta = delta / (int32_t)count;
        grid[i].value = grid[i].delta + (VALUE_RANGE/2);
        // grid[i].value = grid[i].delta + (VALUE_RANGE/2);
    }
}
void regen_balls(void) {
    const size_t MIN_BALLS = (128 * 4 * 4); 
    const size_t MAX_BALLS = (256 * 4 * 4);
    const size_t MIN_BALL_RADIUS = 2;
    const size_t MAX_BALL_RADIUS = 20;

    size_t num_balls = (rand() % (MAX_BALLS-MIN_BALLS+1)) + MIN_BALLS;
    size_t bump = VALUE_RANGE/(MAX_BALL_RADIUS-MIN_BALL_RADIUS);
    for(size_t i = 0; i < num_balls; ++i) {
        int top_left = (rand() % GRID_SIZE);
        int top_left_x = top_left % GRID_WIDTH, top_left_y = top_left / GRID_WIDTH;
        int radius = (rand() % (MAX_BALL_RADIUS-MIN_BALL_RADIUS+1)) + MIN_BALL_RADIUS;
        int diam = radius * 2;
        assert(diam > 0);
        for(int dy = 0; dy <= diam; ++dy) {
            for(int dx = 0; dx <= diam; ++dx) {
                int sq_d = ((dx - radius) * (dx - radius)) + ((dy - radius) * (dy - radius));
                if(sq_d <= radius*radius) {
                    int x = top_left_x + dx;
                    int y = top_left_y + dy;
                    int idx = y * GRID_WIDTH + x;
                    if(x >= 0 && x < GRID_WIDTH &&
                       y >= 0 && y < GRID_HEIGHT) {
                        grid[idx].value += bump;
                        if(grid[idx].value >= VALUE_RANGE) grid[idx].value = VALUE_RANGE-1;
                    }
                }
            }
        }
    }
}
void regen_balls_distance(void) {
    const size_t MIN_BALLS = (128 * 4 * 1 * 2); 
    const size_t MAX_BALLS = (256 * 4 * 1 * 2);
    const size_t MIN_BALL_RADIUS = 2;
    const size_t MAX_BALL_RADIUS = 20;
    size_t num_balls = (rand() % (MAX_BALLS-MIN_BALLS+1)) + MIN_BALLS;
    size_t bump = VALUE_RANGE/(MAX_BALL_RADIUS-MIN_BALL_RADIUS) * 4;
    for(size_t i = 0; i < num_balls; ++i) {
        int top_left = (rand() % GRID_SIZE);
        int top_left_x = top_left % GRID_WIDTH, top_left_y = top_left / GRID_WIDTH;
        int radius = (rand() % (MAX_BALL_RADIUS-MIN_BALL_RADIUS+1)) + MIN_BALL_RADIUS;
        int diam = radius * 2;
        assert(diam > 0);
        for(int dy = 0; dy <= diam; ++dy) {
            for(int dx = 0; dx <= diam; ++dx) {
                int sq_d = ((dx - radius) * (dx - radius)) + ((dy - radius) * (dy - radius));
                if(sq_d <= radius*radius) {
                    int x = top_left_x + dx;
                    int y = top_left_y + dy;
                    int idx = y * GRID_WIDTH + x;
                    if(x >= 0 && x < GRID_WIDTH &&
                       y >= 0 && y < GRID_HEIGHT) {
                        grid[idx].value += bump * (radius*radius-sq_d) / (radius*radius);
                        if(grid[idx].value >= VALUE_RANGE) grid[idx].value = VALUE_RANGE-1;
                    }
                }
            }
        }
    }
}
void regen_grid(void) {
    memset(grid, 0, sizeof(grid));
    // regen_balls();
    regen_balls_distance();
    // regen_deltas();
    // for(size_t i = 0; i < GRID_SIZE; ++i) {
    //     grid[i].value = rand() % VALUE_RANGE;
    // }
}

enum {
    TILE_WATER,
    TILE_SAND,
    TILE_GRASS,
    TILE_STONE,
    TILE_SNOW,
    TILES_COUNT
};

static const size_t tile_weights[] = {
#if 0
    [TILE_WATER] = 5, // 8,
    [TILE_SAND]  = 10, // 2,
    [TILE_GRASS] = 30, // 4,
    [TILE_STONE] = 30,// 5,
    [TILE_SNOW]  = 15,// 2,
#elif 0
    [TILE_WATER] = 2000, 
    [TILE_SAND]  = 700,
    [TILE_GRASS] = 500,
    [TILE_STONE] = 100,
    [TILE_SNOW]  = 50,
#elif 0
    [TILE_WATER] = 800,
    [TILE_SAND]  = 200,
    [TILE_GRASS] = 400,
    [TILE_STONE] = 500,
    [TILE_SNOW]  = 200,
#else
    [TILE_WATER] = 8,
    [TILE_SAND]  = 2,
    [TILE_GRASS] = 4,
    [TILE_STONE] = 5,
    [TILE_SNOW]  = 2,
#endif
};
static inline const size_t tile_weights_sum(void) {
    size_t sum = 0;
    for(size_t i = 0; i < sizeof(tile_weights)/sizeof(*tile_weights); ++i) {
        sum += tile_weights[i];
    }
    return sum;
}
// Range: 0..tile weight
static inline const int get_tile_for_value(uint32_t value, size_t* range) {
    size_t sum = tile_weights_sum();
    size_t idx = value * sum / VALUE_RANGE;
    size_t acum = 0;
    for(size_t i = 0; i < sizeof(tile_weights)/sizeof(*tile_weights); ++i) {
        acum += tile_weights[i];
        if(idx < acum) {
            *range = idx - (acum - tile_weights[i]);
            return i;
        }
    }
    return -1;
}
static bool blue_view = false;
void game_update(uint32_t* pixels, float dt) {
    for(size_t i = 0; i < WIDTH*HEIGHT; ++i) {
        pixels[i] = 0xff212121;
    }
#if 0
    for(size_t i = 0; i < GRID_SIZE; ++i) {
        uint32_t value = grid[i].value;
        size_t x = i * (WIDTH/GRID_SIZE), y = 0;
        size_t w = (WIDTH/GRID_SIZE), h = HEIGHT/4;

        uint32_t color = (0xFF << 24) | (value << 16) | (value << 8) | (value << 0);
        draw_rect(pixels, x, y, w, h, color);
    }
#else
    uint32_t max_value = 1;
    for(size_t i = 0; i < GRID_SIZE; ++i) {
        if(max_value < grid[i].value) max_value = grid[i].value;
    }
    size_t w = WIDTH/GRID_WIDTH;
    size_t h = HEIGHT/GRID_HEIGHT;
    for(size_t i = 0; i < GRID_SIZE; ++i) {
        uint32_t value = grid[i].value;
        uint32_t color;
        size_t range;
        switch(get_tile_for_value(value, &range)) {
        case TILE_WATER:
            color = (range * 0x77) / tile_weights[TILE_WATER] + 0xFF-0x77;
            break;
        case TILE_SAND:
            color = 0xFFDA00;
            break;
        case TILE_GRASS:
            color = ((range * 0x66) / tile_weights[TILE_GRASS] + 0xFF-0x66) << 8;
            break;
        case TILE_STONE: {
            uint32_t gray = (range * (0xFF-0x21)) / tile_weights[TILE_STONE] + 0x21;
            color = (gray << 16) | (gray << 8) | (gray << 0);
        } break;
        case TILE_SNOW:
            color = 0xFFFFFF;    
            break;
        default:
            color = 0xFF0000;
        }
        color |= 0xFF000000;
#if 0
        uint32_t color;
        if(blue_view) {
            color = (0xFF << 24) | value;
        } else {
            uint32_t gray = value * 0xFF / max_value; // (((uint64_t)value) * 0xFF) / 0xFFFFFFFF;
            color = (0xFF << 24) | (gray << 16) | (gray << 8) | (gray << 0);
        }
#endif
        size_t x = (i % GRID_WIDTH) * w; 
        size_t y = (i / GRID_WIDTH) * h;
        draw_rect(pixels, x, y, w, h, color);
    }
#endif
}
void game_keydown(int key) {
    switch(key) {
    case 'b':
        blue_view = !blue_view;
        break;
    case 'g':
        regen_grid();
        break;
    }
}
