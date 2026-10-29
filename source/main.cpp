#include <switch.h>
#include <SDL.h>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include "laura_idle.h"
#include "laura_voice.h"

struct Sound {
    const int16_t* data = nullptr;
    int remaining = 0;
    int position = 0;
};
static void audioCallback(void* userdata, Uint8* stream, int len) {
    auto* s = static_cast<Sound*>(userdata);
    auto* samples = reinterpret_cast<int16_t*>(stream);
    int count = len / sizeof(int16_t);
    for (int i = 0; i < count; ++i) {
        samples[i] = (s->data && s->remaining > 0) ? s->data[s->position++] : 0;
        if (s->remaining > 0) --s->remaining;
    }
}
static void playVoice(SDL_AudioDeviceID audio, Sound& sound, const int16_t* data, int count) {
    if (!audio) return;
    SDL_LockAudioDevice(audio);
    sound.data = data;
    sound.position = 0;
    sound.remaining = count;
    SDL_UnlockAudioDevice(audio);
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) return 1;
    SDL_Window* window = SDL_CreateWindow("Cubo Simulator 2.0", SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_SHOWN);
    if (!window) { SDL_Quit(); return 2; }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) { SDL_DestroyWindow(window); SDL_Quit(); return 3; }

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, LAURA_W, LAURA_H, 32, SDL_PIXELFORMAT_RGBA8888);
    if (!surface) { SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit(); return 4; }
    SDL_LockSurface(surface);
    auto* pixels = static_cast<Uint32*>(surface->pixels);
    int pos = 0;
    for (const auto& run : LAURA_PIXELS) {
        Uint8 r = Uint8(run.rgba >> 24), g = Uint8(run.rgba >> 16);
        Uint8 b = Uint8(run.rgba >> 8), a = Uint8(run.rgba);
        Uint32 color = SDL_MapRGBA(surface->format, r, g, b, a);
        for (int j=0; j<run.count; ++j) pixels[pos++] = color;
    }
    SDL_UnlockSurface(surface);
    SDL_Texture* cube = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (!cube) { SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit(); return 5; }
    SDL_SetTextureBlendMode(cube, SDL_BLENDMODE_BLEND);

    Sound sound;
    SDL_AudioSpec want{};
    want.freq = 48000; want.format = AUDIO_S16SYS; want.channels = 1;
    want.samples = 1024; want.callback = audioCallback; want.userdata = &sound;
    SDL_AudioDeviceID audio = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
    if (audio) SDL_PauseAudioDevice(audio, 0);

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad; padInitializeDefault(&pad);

    constexpr float size = 70.0f, half = size / 2.0f;
    constexpr float gravity = 1500.0f, jumpSpeed = 650.0f, moveSpeed = 420.0f;
    constexpr float groundY = 620.0f;
    // Cada degrau sobe 105 px: 74,6% da altura maxima do salto (140,8 px).
    const SDL_Rect platforms[] = {
        {245, 515, 215, 22},
        {520, 410, 215, 22},
        {795, 305, 215, 22}
    };
    constexpr int platformCount = sizeof(platforms) / sizeof(platforms[0]);

    float x = 165.0f, y = groundY - half;
    float velocityY = 0.0f;
    bool facingLeft = false;
    bool grounded = true, running = true;
    double angle = 0.0, startAngle = 0.0;
    float jumpElapsed = 0.0f;
    constexpr float flightTime = 2.0f * jumpSpeed / gravity;
    Uint64 lastCounter = SDL_GetPerformanceCounter();
    double frequency = double(SDL_GetPerformanceFrequency());

    while (appletMainLoop() && running) {
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = std::min(float((now - lastCounter) / frequency), 0.033f);
        lastCounter = now;
        SDL_Event event;
        while (SDL_PollEvent(&event)) if (event.type == SDL_QUIT) running = false;
        padUpdate(&pad);
        u64 down = padGetButtonsDown(&pad);
        if (down & HidNpadButton_Plus) running = false;
        if (down & HidNpadButton_A)
            playVoice(audio, sound, laura_effort, laura_effort_count);
        if ((down & HidNpadButton_B) && grounded) {
            velocityY = -jumpSpeed;
            grounded = false;
            jumpElapsed = 0.0f;
            startAngle = angle;
            playVoice(audio, sound, laura_jump, laura_jump_count);
        }

        HidAnalogStickState stick = padGetStickPos(&pad, 0);
        float horizontal = stick.x / 32767.0f;
        if (std::fabs(horizontal) < 0.15f) horizontal = 0.0f;
        if (horizontal < 0) facingLeft = true;
        if (horizontal > 0) facingLeft = false;

        // Colisao horizontal com as laterais das plataformas.
        float oldX = x;
        float nextX = std::max(half, std::min(1280.0f - half, x + horizontal * moveSpeed * dt));
        for (int i = 0; i < platformCount; ++i) {
            const SDL_Rect& p = platforms[i];
            if (y + half > p.y && y - half < p.y + p.h) {
                if (nextX > oldX && oldX + half <= p.x && nextX + half > p.x)
                    nextX = float(p.x) - half;
                if (nextX < oldX && oldX - half >= p.x + p.w && nextX - half < p.x + p.w)
                    nextX = float(p.x + p.w) + half;
            }
        }
        x = nextX;

        float oldY = y;
        if (!grounded) {
            velocityY += gravity * dt;
            y += velocityY * dt;
            jumpElapsed += dt;
            angle = startAngle + 90.0 * std::min(jumpElapsed / flightTime, 1.0f);
        }

        // Colisao vertical: pousar por cima ou bater a cabeca por baixo.
        bool landed = false;
        for (int i = 0; i < platformCount; ++i) {
            const SDL_Rect& p = platforms[i];
            if (x + half <= p.x || x - half >= p.x + p.w) continue;
            if (velocityY >= 0.0f && oldY + half <= p.y && y + half >= p.y) {
                y = float(p.y) - half;
                velocityY = 0.0f;
                landed = true;
            } else if (velocityY < 0.0f && oldY - half >= p.y + p.h && y - half <= p.y + p.h) {
                y = float(p.y + p.h) + half;
                velocityY = 0.0f;
            }
        }
        if (velocityY >= 0.0f && oldY + half <= groundY && y + half >= groundY) {
            y = groundY - half;
            velocityY = 0.0f;
            landed = true;
        }
        if (landed) {
            grounded = true;
            angle = startAngle + 90.0;
        } else {
            // Verificar se ainda existe piso sob o cubo.
            bool supported = std::fabs((y + half) - groundY) < 0.1f;
            for (int i = 0; i < platformCount; ++i) {
                const SDL_Rect& p = platforms[i];
                if (std::fabs((y + half) - p.y) < 0.1f &&
                    x + half > p.x && x - half < p.x + p.w) supported = true;
            }
            if (grounded && !supported) { grounded = false; velocityY = 0.0f; }
        }

        SDL_SetRenderDrawColor(renderer, 17, 20, 34, 255);
        SDL_RenderClear(renderer);
        SDL_Rect floor{0, int(groundY), 1280, 100};
        SDL_SetRenderDrawColor(renderer, 44, 51, 70, 255);
        SDL_RenderFillRect(renderer, &floor);
        SDL_Rect floorLine{0, int(groundY), 1280, 4};
        SDL_SetRenderDrawColor(renderer, 100, 116, 145, 255);
        SDL_RenderFillRect(renderer, &floorLine);
        for (int i = 0; i < platformCount; ++i) {
            SDL_SetRenderDrawColor(renderer, 60, 79, 106, 255);
            SDL_RenderFillRect(renderer, &platforms[i]);
            SDL_Rect top{platforms[i].x, platforms[i].y, platforms[i].w, 5};
            SDL_SetRenderDrawColor(renderer, 117, 213, 195, 255);
            SDL_RenderFillRect(renderer, &top);
        }
        SDL_Rect shadow{int(x) - 45, int(groundY) - 9, 90, 9};
        SDL_SetRenderDrawColor(renderer, 7, 9, 16, 255);
        SDL_RenderFillRect(renderer, &shadow);
        SDL_Rect dest{int(x - 31), int(y + half - 85), 62, 85};
        SDL_RenderCopyEx(renderer, cube, nullptr, &dest, 0.0, nullptr, facingLeft ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
        SDL_RenderPresent(renderer);
    }

    if (audio) SDL_CloseAudioDevice(audio);
    SDL_DestroyTexture(cube);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
