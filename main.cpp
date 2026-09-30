#include "chip8.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>
#include <cstdint>
#include <iostream>
#include <string>
#include <algorithm>

const int SCALE = 12; // Scaled up slightly to 768x384 for sharper graphics
const int WIDTH = 64 * SCALE;
const int HEIGHT = 32 * SCALE;

// Keyboard mapping
uint8_t keymap[16] = {
    SDLK_x, // 0
    SDLK_1, // 1
    SDLK_2, // 2
    SDLK_3, // 3
    SDLK_q, // 4
    SDLK_w, // 5
    SDLK_e, // 6
    SDLK_a, // 7
    SDLK_s, // 8
    SDLK_d, // 9
    SDLK_z, // A
    SDLK_c, // B
    SDLK_4, // C
    SDLK_r, // D
    SDLK_f, // E
    SDLK_v  // F
};

struct ColorScheme {
    const char* name;
    SDL_Color bg;
    SDL_Color fg;
};

const ColorScheme COLOR_SCHEMES[] = {
    {"Classic Black/White", {5, 5, 10, 255},       {240, 245, 255, 255}},
    {"Matrix Neon Green",   {5, 18, 5, 255},       {0, 255, 100, 255}},
    {"Cyberpunk Cyan",      {20, 5, 35, 255},      {0, 240, 255, 255}},
    {"GameBoy Olive",       {140, 175, 15, 255},   {15, 50, 15, 255}},
    {"Amber CRT",           {18, 8, 0, 255},       {255, 160, 0, 255}}
};
const int NUM_SCHEMES = sizeof(COLOR_SCHEMES) / sizeof(COLOR_SCHEMES[0]);
int current_scheme = 0;
bool enable_glow = true;

static float pixel_trail[64 * 32] = {0};

void audio_callback(void* userdata, uint8_t* stream, int len){
    static uint32_t sample_index = 0;
    int16_t* audio_buffer = (int16_t*) stream;
    int samples = len / 2;

    bool* beeping = (bool*) userdata;
    for(int i = 0; i < samples; i++){
        if(*beeping){
            int16_t value = ((sample_index++ / 100) % 2) ? 3000 : -3000;
            audio_buffer[i] = value;
        }
        else{
            audio_buffer[i] = 0;
            sample_index = 0;
        }
    }
}

void draw_graphics(SDL_Renderer* renderer, Chip8& chip8){
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    const ColorScheme& scheme = COLOR_SCHEMES[current_scheme];

    // Background
    SDL_SetRenderDrawColor(renderer, scheme.bg.r, scheme.bg.g, scheme.bg.b, 255);
    SDL_RenderClear(renderer);

    // Update pixel trail / persistence
    for(int i = 0; i < 64 * 32; i++){
        if(chip8.display[i] == 1){
            pixel_trail[i] = 1.0f;
        } else if(enable_glow){
            pixel_trail[i] *= 0.65f; // Smooth decay over frames
            if(pixel_trail[i] < 0.03f) pixel_trail[i] = 0.0f;
        } else {
            pixel_trail[i] = 0.0f;
        }
    }

    // Render pixels
    for(int y = 0; y < 32; y++){
        for(int x = 0; x < 64; x++){
            int idx = x + (y * 64);
            float val = pixel_trail[idx];

            if(val > 0.01f){
                // Compute color with smooth intensity decay
                uint8_t r = (uint8_t)(scheme.fg.r * val);
                uint8_t g = (uint8_t)(scheme.fg.g * val);
                uint8_t b = (uint8_t)(scheme.fg.b * val);
                uint8_t a = (uint8_t)(255 * std::min(1.0f, val + 0.15f));

                // 1. Neon Bloom Glow Layer for Active Pixels
                if(chip8.display[idx] == 1 && enable_glow){
                    SDL_SetRenderDrawColor(renderer, scheme.fg.r, scheme.fg.g, scheme.fg.b, 50);
                    SDL_Rect glow_rect = {x * SCALE - 2, y * SCALE - 2, SCALE + 4, SCALE + 4};
                    SDL_RenderFillRect(renderer, &glow_rect);
                }

                // 2. Crisp Matrix Pixel Cell with 1px margin
                SDL_SetRenderDrawColor(renderer, r, g, b, a);
                SDL_Rect cell_rect = {x * SCALE + 1, y * SCALE + 1, SCALE - 2, SCALE - 2};
                SDL_RenderFillRect(renderer, &cell_rect);
            }
        }
    }

    // 3. CRT Scanlines Overlay
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 25);
    for(int y = 0; y < HEIGHT; y += 3){
        SDL_Rect scanline = {0, y, WIDTH, 1};
        SDL_RenderFillRect(renderer, &scanline);
    }

    SDL_RenderPresent(renderer);
}

void update_title(SDL_Window* window, int cycles_per_frame, bool paused) {
    std::string title = "CHIP-8 Pro | Speed: " + std::to_string(cycles_per_frame) +
                        " cyc/frame | Theme: " + COLOR_SCHEMES[current_scheme].name;
    if (enable_glow) title += " [Glow FX ON]";
    if (paused) title += " [PAUSED]";
    title += " | [+/-: Speed | P: Pause | G: Glow | T: Theme | K/F5: Save | L/F6: Load]";
    SDL_SetWindowTitle(window, title.c_str());
}

void handle_input(Chip8& chip8, bool& running, int& cycles_per_frame, bool& paused, SDL_Window* window){
    SDL_Event event;

    while(SDL_PollEvent(&event)){
        if(event.type == SDL_QUIT) running = false;
        if(event.type == SDL_KEYDOWN){
            if(event.key.keysym.sym == SDLK_ESCAPE) running = false;
            
            // Speed Controls
            if(event.key.keysym.sym == SDLK_EQUALS || event.key.keysym.sym == SDLK_PLUS || event.key.keysym.sym == SDLK_KP_PLUS){
                cycles_per_frame = std::min(100, cycles_per_frame + 2);
                update_title(window, cycles_per_frame, paused);
            }
            if(event.key.keysym.sym == SDLK_MINUS || event.key.keysym.sym == SDLK_KP_MINUS){
                cycles_per_frame = std::max(1, cycles_per_frame - 2);
                update_title(window, cycles_per_frame, paused);
            }

            // Pause
            if(event.key.keysym.sym == SDLK_p){
                paused = !paused;
                update_title(window, cycles_per_frame, paused);
            }

            // Glow FX Toggle
            if(event.key.keysym.sym == SDLK_g){
                enable_glow = !enable_glow;
                update_title(window, cycles_per_frame, paused);
            }

            // Theme Switch
            if(event.key.keysym.sym == SDLK_t || event.key.keysym.sym == SDLK_F1){
                current_scheme = (current_scheme + 1) % NUM_SCHEMES;
                update_title(window, cycles_per_frame, paused);
            }

            // Save / Load State (F5/F6 or K/L)
            if(event.key.keysym.sym == SDLK_F5 || event.key.keysym.sym == SDLK_k){
                if(chip8.save_state("savestate.bin")){
                    std::string status = "CHIP-8 Pro | [SAVED STATE SUCCESSFULLY!]";
                    SDL_SetWindowTitle(window, status.c_str());
                }
            }
            if(event.key.keysym.sym == SDLK_F6 || event.key.keysym.sym == SDLK_l){
                if(chip8.load_state("savestate.bin")){
                    std::string status = "CHIP-8 Pro | [LOADED STATE SUCCESSFULLY!]";
                    SDL_SetWindowTitle(window, status.c_str());
                }
            }

            // Check standard Chip-8 keypad mapping
            for(int i = 0; i < 16; i++){
                if(event.key.keysym.sym == keymap[i]) chip8.key[i] = 1;
            }

            // Universal WASD & Arrow Key Mappings (Overrides keypad for intuitive arcade play)
            if(event.key.keysym.sym == SDLK_w || event.key.keysym.sym == SDLK_UP){
                chip8.key[7] = 1; chip8.key[1] = 1; // UP in Blinky (7) & standard games
            }
            if(event.key.keysym.sym == SDLK_s || event.key.keysym.sym == SDLK_DOWN){
                chip8.key[7] = 0; // Clear Up
                chip8.key[8] = 1; chip8.key[0xA] = 1; chip8.key[2] = 1; // DOWN in Blinky (8/A) & Tetris (2)
            }
            if(event.key.keysym.sym == SDLK_a || event.key.keysym.sym == SDLK_LEFT){
                chip8.key[7] = 0; // Clear Up
                chip8.key[3] = 1; chip8.key[4] = 1; chip8.key[5] = 1; // LEFT in Blinky (3) & Tetris (5)
            }
            if(event.key.keysym.sym == SDLK_d || event.key.keysym.sym == SDLK_RIGHT){
                chip8.key[7] = 0; // Clear Up
                chip8.key[6] = 1; chip8.key[0xE] = 1; // RIGHT in Blinky (E) & Tetris (6)
            }
        }
        if(event.type == SDL_KEYUP){
            for(int i = 0; i < 16; i++){
                if(event.key.keysym.sym == keymap[i]) chip8.key[i] = 0;
            }
            if(event.key.keysym.sym == SDLK_w || event.key.keysym.sym == SDLK_UP){
                chip8.key[7] = 0; chip8.key[1] = 0;
            }
            if(event.key.keysym.sym == SDLK_s || event.key.keysym.sym == SDLK_DOWN){
                chip8.key[8] = 0; chip8.key[0xA] = 0; chip8.key[2] = 0;
            }
            if(event.key.keysym.sym == SDLK_a || event.key.keysym.sym == SDLK_LEFT){
                chip8.key[3] = 0; chip8.key[4] = 0; chip8.key[5] = 0;
            }
            if(event.key.keysym.sym == SDLK_d || event.key.keysym.sym == SDLK_RIGHT){
                chip8.key[6] = 0; chip8.key[0xE] = 0;
            }
        }
    }
}

int main(int argc, char** argv){
    if(argc < 2){
        std::cerr << "Usage: " << argv[0] << " <ROM file>" << std::endl;
        return 1;
    }
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0){
        std::cerr << "SDL Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    bool beeping = false;
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 2048;
    want.callback = audio_callback;
    want.userdata = &beeping;

    SDL_AudioDeviceID audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if(audio_device == 0) std::cerr << "Failed to open audio: " << SDL_GetError() << std::endl;
    else SDL_PauseAudioDevice(audio_device, 0);

    SDL_Window* window = SDL_CreateWindow("CHIP-8 Pro", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
    if(!window){
        std::cerr << "Window error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if(!renderer){
        std::cerr << "Renderer error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Chip8 chip8;
    chip8.load_rom(argv[1]);
    
    int cycles_per_frame = 10;
    bool paused = false;
    update_title(window, cycles_per_frame, paused);

    uint64_t perf_freq = SDL_GetPerformanceFrequency();
    const double target_frame_ms = 1000.0 / 60.0; // Exact 60 FPS (16.666 ms)

    bool running = true;
    while(running){
        uint64_t frame_start = SDL_GetPerformanceCounter();

        handle_input(chip8, running, cycles_per_frame, paused, window);

        if(!paused){
            for(int i = 0; i < cycles_per_frame; i++){
                chip8.emulate_cycle();
            }
        }

        beeping = (!paused && chip8.get_sound_timer() > 0);

        draw_graphics(renderer, chip8);

        // High-precision performance counter pacing for buttery smooth 60 FPS
        uint64_t frame_end = SDL_GetPerformanceCounter();
        double elapsed_ms = (double)(frame_end - frame_start) * 1000.0 / (double)perf_freq;
        if(elapsed_ms < target_frame_ms){
            SDL_Delay((Uint32)(target_frame_ms - elapsed_ms));
        }
    }

    if(audio_device != 0) SDL_CloseAudioDevice(audio_device);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}