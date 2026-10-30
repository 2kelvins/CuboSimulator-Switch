#include <switch.h>
#include <SDL.h>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include "laura_voice.h"
#include "laura_frames.h"

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
    SDL_Window* window = SDL_CreateWindow("Laura Adventure 0.6", SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_SHOWN);
    if (!window) { SDL_Quit(); return 2; }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) { SDL_DestroyWindow(window); SDL_Quit(); return 3; }

    auto loadFrame = [&](const FrameData& frame) -> SDL_Texture* {
        SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, frame.w, frame.h, 32, SDL_PIXELFORMAT_RGBA8888);
        if (!s) return nullptr;
        SDL_LockSurface(s);
        Uint32* px = static_cast<Uint32*>(s->pixels);
        int pos = 0;
        for (int i = 0; i < frame.runCount; ++i) {
            const SpriteRun& r = frame.runs[i];
            Uint32 color = SDL_MapRGBA(s->format, Uint8(r.rgba >> 24), Uint8(r.rgba >> 16),
                Uint8(r.rgba >> 8), Uint8(r.rgba));
            for (int j = 0; j < r.count; ++j) px[pos++] = color;
        }
        SDL_UnlockSurface(s);
        SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
        SDL_FreeSurface(s);
        if (t) SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        return t;
    };
    const FrameData* frames[] = {
        &frame_parado_001, &frame_corrida_001, &frame_corrida_002, &frame_corrida_003,
        &frame_corrida_004, &frame_corrida_005, &frame_corrida_006, &frame_salto_001,
        &frame_salto_002, &frame_salto_003, &frame_salto_004, &frame_aterrissagem_001,
        &frame_aterrissagem_002, &frame_aterrissagem_003, &frame_aterrissagem_004, &frame_dano_001,
        &frame_dano_002, &frame_dano_003
    };
    constexpr int frameCount = sizeof(frames) / sizeof(frames[0]);
    SDL_Texture* animation[frameCount]{};
    for (int i=0; i<frameCount; ++i) {
        animation[i] = loadFrame(*frames[i]);
        if (!animation[i]) {
            for (int j=0; j<i; ++j) SDL_DestroyTexture(animation[j]);
            SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit(); return 6;
        }
    }
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
    constexpr float fallGravityMultiplier = 1.18f; // Acelera levemente apenas a descida
    constexpr float maxFallSpeed = 1050.0f;
    constexpr float groundY = 620.0f;
    constexpr float worldWidth = 6100.0f;
    constexpr float fallDamageThreshold = 504.0f;
    float cameraX = 0.0f, cameraY = 0.0f;
    float peakFeetY = groundY;
    float hurtTimer = 0.0f, landTimer = 0.0f;
    float runClock = 0.0f;
    float horizontal = 0.0f;
    // Cada degrau sobe 105 px: 74,6% da altura maxima do salto (140,8 px).
    const SDL_Rect platforms[] = {
        {245, 515, 215, 22},
        {520, 410, 215, 22},
        {795, 305, 215, 22},
        {1100, 410, 230, 22},
        {1400, 515, 230, 22},
        {1730, 410, 215, 22},
        {2010, 305, 215, 22},
        {2320, 410, 230, 22},
        {2640, 515, 230, 22},
        {2950, 410, 215, 22},
        {3230, 305, 215, 22},
        // Torre: degraus de 105px, subida acessivel e queda final perigosa.
        {3500, 200, 215, 22},
        {3770, 95, 215, 22},
        {4040, -10, 215, 22},
        {4310, -115, 215, 22},
        {4580, -220, 215, 22},
        {4850, -325, 215, 22}
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
        if ((down & HidNpadButton_A) && hurtTimer <= 0.0f)
            playVoice(audio, sound, laura_effort, laura_effort_count);
        if ((down & HidNpadButton_B) && grounded && hurtTimer <= 0.0f) {
            velocityY = -jumpSpeed;
            grounded = false;
            jumpElapsed = 0.0f;
            startAngle = angle;
            peakFeetY = y + half;
            playVoice(audio, sound, laura_jump, laura_jump_count);
        }

        HidAnalogStickState stick = padGetStickPos(&pad, 0);
        horizontal = stick.x / 32767.0f;
        if (std::fabs(horizontal) < 0.15f) horizontal = 0.0f;
        if (horizontal < 0) facingLeft = true;
        if (horizontal > 0) facingLeft = false;

        // Colisao horizontal com as laterais das plataformas.
        if (hurtTimer > 0.0f) { hurtTimer = std::max(0.0f, hurtTimer - dt); horizontal = 0.0f; }
        if (landTimer > 0.0f) landTimer = std::max(0.0f, landTimer - dt);
        float oldX = x;
        float nextX = std::max(half, std::min(worldWidth - half, x + horizontal * moveSpeed * dt));
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
            velocityY = std::min(velocityY + gravity * (velocityY > 0.0f ? fallGravityMultiplier : 1.0f) * dt, maxFallSpeed);
            y += velocityY * dt;
            peakFeetY = std::min(peakFeetY, y + half);
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
            float fallDistance = (y + half) - peakFeetY;
            if (!grounded && fallDistance >= fallDamageThreshold) {
                hurtTimer = 0.54f;
                playVoice(audio, sound, laura_effort, laura_effort_count);
            } else if (!grounded) {
                landTimer = 0.24f;
            }
            peakFeetY = y + half;
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
            if (grounded && !supported) { grounded = false; velocityY = 0.0f; peakFeetY = y + half; }
        }

        if (grounded && std::fabs(horizontal) > 0.0f) runClock += dt * 11.0f;
        else runClock = 0.0f;
        cameraX = std::max(0.0f, std::min(worldWidth - 1280.0f, x - 420.0f));
        // A camera sobe na torre e volta ao nivel original ao descer.
        float targetCameraY = std::min(0.0f, y - 350.0f);
        cameraY += (targetCameraY - cameraY) * std::min(1.0f, dt * 7.0f);

        SDL_SetRenderDrawColor(renderer, 17, 20, 34, 255);
        SDL_RenderClear(renderer);
        // Linhas de fundo deslocam mais devagar (parallax simples).
        SDL_SetRenderDrawColor(renderer, 26, 32, 51, 255);
        for (int i = 0; i < 24; ++i) {
            SDL_Rect pillar{int(i * 210 - cameraX * 0.35f), 170 + (i % 3) * 35 - int(cameraY * 0.2f), 75, 450};
            SDL_RenderFillRect(renderer, &pillar);
        }
        SDL_Rect floor{0, int(groundY - cameraY), 1280, 100};
        SDL_SetRenderDrawColor(renderer, 44, 51, 70, 255);
        SDL_RenderFillRect(renderer, &floor);
        SDL_Rect floorLine{0, int(groundY - cameraY), 1280, 4};
        SDL_SetRenderDrawColor(renderer, 100, 116, 145, 255);
        SDL_RenderFillRect(renderer, &floorLine);
        for (int i = 0; i < platformCount; ++i) {
            SDL_SetRenderDrawColor(renderer, 60, 79, 106, 255);
            SDL_Rect screenPlatform = platforms[i];
            screenPlatform.x -= int(cameraX);
            screenPlatform.y -= int(cameraY);
            if (screenPlatform.x + screenPlatform.w < 0 || screenPlatform.x > 1280) continue;
            SDL_RenderFillRect(renderer, &screenPlatform);
            SDL_Rect top{screenPlatform.x, screenPlatform.y, screenPlatform.w, 5};
            SDL_SetRenderDrawColor(renderer, 117, 213, 195, 255);
            SDL_RenderFillRect(renderer, &top);
        }
        SDL_Rect shadow{int(x - cameraX) - 28, int(groundY - cameraY) - 9, 56, 9};
        SDL_SetRenderDrawColor(renderer, 7, 9, 16, 255);
        SDL_RenderFillRect(renderer, &shadow);
        // Frames ordenados exatamente pelos nomes enviados:
        // parado 0; corrida 1..6; salto 7..10; aterrissagem 11..14; dano 15..17.
        bool moving = grounded && std::fabs(horizontal) > 0.0f;
        int selected = 0;
        if (hurtTimer > 0.0f) {
            selected = 15 + std::min(2, int((0.54f - hurtTimer) / 0.18f));
        } else if (!grounded) {
            // Salto: preparacao, subida, apice e descida.
            if (jumpElapsed < 0.09f) selected = 7;
            else if (velocityY < -90.0f) selected = 8;
            else if (velocityY < 90.0f) selected = 9;
            else selected = 10;
        } else if (landTimer > 0.0f) {
            selected = 11 + std::min(3, int((0.24f - landTimer) / 0.06f));
        } else if (moving) {
            selected = 1 + int(runClock * 1.2f) % 6;
        }
        // Corrige a margem transparente abaixo dos pes em cada PNG.
        // Assim o contato visual com o solo permanece estavel entre as poses.
        constexpr int bottomPadding[frameCount] = {3, 2, 4, 1, 7, 5, 2, 27, 8, 15, 11, 47, 34, 20, 24, 30, 30, 20};
        SDL_Texture* current = animation[selected];
        int frameW = frames[selected]->w, frameH = frames[selected]->h;
        // Todos os quadros ancorados pelos pes no nivel da colisao.
        float spriteHeight = 85.0f;
        float spriteWidth = spriteHeight * float(frameW) / float(frameH);
        SDL_Rect dest{int(x - cameraX - spriteWidth / 2.0f),
            int(y + half - cameraY - spriteHeight + spriteHeight * float(bottomPadding[selected]) / float(frameH)),
            int(spriteWidth), int(spriteHeight)};
        SDL_RenderCopyEx(renderer, current, nullptr, &dest, 0.0, nullptr,
            facingLeft ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
        SDL_RenderPresent(renderer);
    }

    if (audio) SDL_CloseAudioDevice(audio);
    for (auto* t : animation) SDL_DestroyTexture(t);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
