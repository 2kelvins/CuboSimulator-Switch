
#include <switch.h>
#include <SDL.h>
#include <cmath>
#include <cstdint>
#include <algorithm>

struct Sound {
    int remaining = 0;
    int total = 0;
    double phase = 0;
};

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

        samples[i] = s->remaining > 0
            ? int16_t(wave * 4500 * (1.0 - progress))
            : 0;

        if (s->remaining > 0) --s->remaining;
    }
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0)
        return 1;

    SDL_Window* window = SDL_CreateWindow(
        "Cubo Simulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_SHOWN
    );

    if (!window) {
        SDL_Quit();
        return 2;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!renderer)
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);

    if (!renderer) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 3;
    }

    Sound sound;
    SDL_AudioSpec want{};
    want.freq = 48000;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;
    want.callback = audioCallback;
    want.userdata = &sound;

    SDL_AudioDeviceID audio =
        SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);

    if (audio)
        SDL_PauseAudioDevice(audio, 0);

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    PadState pad;
    padInitializeDefault(&pad);

    float x = 640;
    float y = 360;
    bool running = true;

    while (appletMainLoop() && running) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = false;
        }

        padUpdate(&pad);
        u64 down = padGetButtonsDown(&pad);

        if (down & HidNpadButton_Plus)
            running = false;

        if ((down & HidNpadButton_A) && audio) {
            SDL_LockAudioDevice(audio);
            sound.total = sound.remaining = 48000 / 3;
            sound.phase = 0;
            SDL_UnlockAudioDevice(audio);
        }

        HidAnalogStickState stick = padGetStickPos(&pad, 0);

        x += (stick.x / 32767.0f) * 7.0f;
        y -= (stick.y / 32767.0f) * 7.0f;

        x = std::max(70.0f, std::min(1210.0f, x));
        y = std::max(70.0f, std::min(650.0f, y));

        SDL_SetRenderDrawColor(renderer, 17, 20, 34, 255);
        SDL_RenderClear(renderer);

        SDL_Rect shadow{int(x)-47, int(y)-32, 110, 110};
        SDL_SetRenderDrawColor(renderer, 7, 9, 16, 255);
        SDL_RenderFillRect(renderer, &shadow);

        SDL_Rect face{int(x)-55, int(y)-55, 110, 110};
        SDL_SetRenderDrawColor(renderer, 34, 174, 245, 255);
        SDL_RenderFillRect(renderer, &face);

        SDL_Rect shine{int(x)-55, int(y)-55, 110, 18};
        SDL_SetRenderDrawColor(renderer, 115, 223, 255, 255);
        SDL_RenderFillRect(renderer, &shine);

        SDL_SetRenderDrawColor(renderer, 238, 245, 255, 255);
        SDL_RenderDrawRect(renderer, &face);

        SDL_RenderPresent(renderer);
    }

    if (audio)
        SDL_CloseAudioDevice(audio);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
