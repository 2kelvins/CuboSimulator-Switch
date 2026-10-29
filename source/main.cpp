#include <switch.h>
#include <SDL.h>
#include <cmath>
#include <cstdint>
#include <algorithm>

struct Sound { int remaining = 0; int total = 0; double phase = 0; };
static void audioCallback(void* userdata, Uint8* stream, int len) {
    auto* s = static_cast<Sound*>(userdata);
    auto* samples = reinterpret_cast<int16_t*>(stream);
    int count = len / sizeof(int16_t);
    for (int i = 0; i < count; ++i) {
        int elapsed = s->total - s->remaining;
        double progress = s->total ? double(elapsed) / s->total : 1.0;
        double freq = 760.0 - 550.0 * progress;
        s->phase += 2.0 * 3.141592653589793 * freq / 48000.0;
        double wave = std::sin(s->phase) > 0 ? 1.0 : -1.0;
        samples[i] = s->remaining > 0 ? int16_t(wave * 4500 * (1.0 - progress)) : 0;
        if (s->remaining > 0) --s->remaining;
    }
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

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, 110, 110, 32, SDL_PIXELFORMAT_RGBA8888);
    if (!surface) { SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit(); return 4; }
    SDL_FillRect(surface, nullptr, SDL_MapRGBA(surface->format, 34, 174, 245, 255));
    SDL_Rect stripe{0, 0, 110, 18};
    SDL_FillRect(surface, &stripe, SDL_MapRGBA(surface->format, 115, 223, 255, 255));
    SDL_Rect mark{14, 30, 22, 22};
    SDL_FillRect(surface, &mark, SDL_MapRGBA(surface->format, 238, 245, 255, 255));
    SDL_Texture* cube = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (!cube) { SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit(); return 5; }

    Sound sound;
    SDL_AudioSpec want{};
    want.freq = 48000; want.format = AUDIO_S16SYS; want.channels = 1;
    want.samples = 1024; want.callback = audioCallback; want.userdata = &sound;
    SDL_AudioDeviceID audio = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
    if (audio) SDL_PauseAudioDevice(audio, 0);

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad; padInitializeDefault(&pad);

    constexpr float size = 110.0f, half = size / 2.0f;
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
        if ((down & HidNpadButton_A) && audio) {
            SDL_LockAudioDevice(audio);
            sound.total = sound.remaining = 48000 / 3;
            sound.phase = 0;
            SDL_UnlockAudioDevice(audio);
        }
        if ((down & HidNpadButton_B) && grounded) {
            velocityY = -jumpSpeed;
            grounded = false;
            jumpElapsed = 0.0f;
            startAngle = angle;
        }

        HidAnalogStickState stick = padGetStickPos(&pad, 0);
        float horizontal = stick.x / 32767.0f;
        if (std::fabs(horizontal) < 0.15f) horizontal = 0.0f;

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
        SDL_Rect dest{int(x - half), int(y - half), int(size), int(size)};
        SDL_RenderCopyEx(renderer, cube, nullptr, &dest, angle, nullptr, SDL_FLIP_NONE);
        SDL_RenderPresent(renderer);
    }

    if (audio) SDL_CloseAudioDevice(audio);
    SDL_DestroyTexture(cube);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
