#include <SDL.h>
#include <SDL_opengl.h>
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <ctime>
#include <fstream>

#include "theme.h"
#include "particles.h"
#include "pi_worker.h"
#include "updater.h"
#include "config.h"

static ImU32 col(const float c[4]) {
    return IM_COL32((int)(c[0] * 255), (int)(c[1] * 255), (int)(c[2] * 255), (int)(c[3] * 255));
}

struct DigitsView {
    std::string text; // "3.14159..."
    std::vector<int> lineStart;
    int lineLen = 120;

    void rebuild() {
        lineStart.clear();
        for (size_t i = 0; i < text.size(); i += lineLen) lineStart.push_back((int)i);
    }

    int lineCount() const { return (int)lineStart.size(); }

    std::string line(int i) const {
        int start = lineStart[i];
        int len = std::min<int>(lineLen, (int)text.size() - start);
        return text.substr(start, len);
    }
};

static const long PRESETS[] = {1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};
static const char *PRESET_LABELS[] = {"1K", "10K", "100K", "1M", "10M", "100M", "1B"};

int main(int, char **) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

#if defined(__APPLE__)
    const char *glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    const char *glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_SHOWN;
    SDL_Window *window = SDL_CreateWindow("polyPi", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                           980, 760, windowFlags);
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, glContext);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    theme::apply();
    ImGui_ImplSDL2_InitForOpenGL(window, glContext);
    ImGui_ImplOpenGL3_Init(glsl_version);

    PiWorker worker;
    Updater updater;
    updater.checkAsync();

    ParticleField field;
    bool fieldBuilt = false;
    int animState = 0; // 0 idle, 1 forming/holding, 2 pulse

    static char digitsBuf[16] = "10000";
    int maxThreads = (int)std::max(1u, std::thread::hardware_concurrency());
    static int threadCount = maxThreads;
    int activePreset = 1;

    DigitsView digitsView;
    bool hasResult = false;
    std::string lastSavePath;

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(window))
                running = false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGuiViewport *vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::Begin("polyPi", nullptr,
                      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                          ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);

        ImGui::PushFont(nullptr);
        ImGui::SetWindowFontScale(1.7f);
        ImGui::TextColored(ImVec4(0.24f, 0.86f, 0.47f, 1.0f), "polyPi");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopFont();

        ImGui::TextDisabled("A tool that helps you count Pi.  -  v" POLYPI_VERSION);
        ImGui::Spacing();

        // ---- update banner ----
        {
            ImGui::BeginChild("update_bar", ImVec2(0, 34), true, ImGuiWindowFlags_NoScrollbar);
            ImGui::AlignTextToFramePadding();
            if (updater.isChecking()) {
                ImGui::TextDisabled("Checking for updates...");
            } else if (updater.hasError()) {
                ImGui::TextDisabled("Couldn't check for updates.");
            } else if (updater.updateAvailable()) {
                ImGui::TextColored(ImVec4(1, 0.85f, 0.3f, 1), "Update available: %s", updater.latestVersion().c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("Open release page")) {
#if defined(__APPLE__)
                    std::string cmd = "open \"" + updater.releaseUrl() + "\"";
                    system(cmd.c_str());
#elif defined(_WIN32)
                    std::string cmd = "start \"\" \"" + updater.releaseUrl() + "\"";
                    system(cmd.c_str());
#endif
                }
            } else if (updater.hasChecked()) {
                ImGui::TextDisabled("Up to date (v" POLYPI_VERSION ")");
            } else {
                ImGui::TextDisabled(" ");
            }
            ImGui::SameLine(ImGui::GetWindowWidth() - 110);
            if (ImGui::SmallButton("Check now")) updater.checkAsync();
            ImGui::EndChild();
        }

        ImGui::Spacing();

        // ---- particle stage ----
        ImVec2 stageSize = ImVec2(ImGui::GetContentRegionAvail().x, 260);
        ImGui::BeginChild("stage", stageSize, true, ImGuiWindowFlags_NoScrollbar);
        ImVec2 p0 = ImGui::GetWindowPos();
        ImVec2 p1 = ImVec2(p0.x + ImGui::GetWindowSize().x, p0.y + ImGui::GetWindowSize().y);
        if (!fieldBuilt) {
            ImVec2 margin(stageSize.x * 0.28f, 20);
            field.build(ImVec2(p0.x + margin.x, p0.y + margin.y), ImVec2(p1.x - margin.x, p1.y - margin.y));
            fieldBuilt = true;
        }

        double now = ImGui::GetTime();
        field.setState(animState);
        if (animState == 1) field.setProgress((float)worker.progress());
        field.update(io.DeltaTime, now);
        ImDrawList *dl = ImGui::GetWindowDrawList();
        field.draw(dl, col(theme::kPalette.green));

        if (worker.isRunning()) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%.0f%%", worker.progress() * 100.0);
            ImVec2 tsize = ImGui::CalcTextSize(buf);
            dl->AddText(ImVec2(p0.x + (p1.x - p0.x - tsize.x) * 0.5f, p1.y - 30), col(theme::kPalette.textDim), buf);
        }
        ImGui::EndChild();

        ImGui::Spacing();

        // ---- controls ----
        ImGui::Text("Digits");
        ImGui::SetNextItemWidth(200);
        bool enterPressed = ImGui::InputText("##digits", digitsBuf, sizeof(digitsBuf),
                                              ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        for (int i = 0; i < 7; i++) {
            ImGui::PushID(i);
            bool active = activePreset == i;
            if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(theme::kPalette.greenDim[0], theme::kPalette.greenDim[1], theme::kPalette.greenDim[2], 1));
            if (ImGui::SmallButton(PRESET_LABELS[i])) {
                std::snprintf(digitsBuf, sizeof(digitsBuf), "%ld", PRESETS[i]);
                activePreset = i;
            }
            if (active) ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::PopID();
        }
        ImGui::NewLine();

        long digitsWanted = std::atol(digitsBuf);
        if (digitsWanted < 1) digitsWanted = 1;
        if (digitsWanted > 2000000000L) digitsWanted = 2000000000L;

        if (digitsWanted > 50000000) {
            ImGui::TextColored(ImVec4(1, 0.75f, 0.3f, 1), "Large computation: expect significant time and RAM (several GB+).");
        }

        ImGui::Spacing();
        ImGui::Text("Speed (%d threads available)", maxThreads);
        ImGui::SetNextItemWidth(300);
        ImGui::SliderInt("##threads", &threadCount, 1, maxThreads, "%d threads");
        ImGui::TextDisabled("More threads = faster on large digit counts.");

        ImGui::Spacing();
        bool isRunning = worker.isRunning();
        if (!isRunning) {
            if (ImGui::Button(hasResult ? "COUNT AGAIN" : "COUNT PI", ImVec2(200, 42)) || enterPressed) {
                hasResult = false;
                animState = 1;
                worker.start(digitsWanted, threadCount);
            }
        } else {
            ImGui::BeginDisabled(false);
            if (ImGui::Button("CANCEL", ImVec2(200, 42))) {
                worker.cancel();
            }
            ImGui::EndDisabled();
        }

        if (isRunning) {
            ImGui::SameLine();
            ImGui::Text("%.1fs elapsed  |  ~%s digits/sec", worker.elapsedMs() / 1000.0,
                        std::to_string(worker.digitsPerSecEstimate()).c_str());
            ImGui::ProgressBar((float)worker.progress(), ImVec2(-1, 8), "");
        }

        if (worker.isDone() && !hasResult) {
            digitsView.text = worker.takeResult();
            digitsView.rebuild();
            hasResult = true;
            animState = 2;
        }

        if (hasResult) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            long shown = (long)digitsView.text.size() - 2;
            ImGui::TextColored(ImVec4(0.66f, 1.0f, 0.81f, 1.0f), "%ld digits in %.2fs  |  %d threads",
                                shown, worker.elapsedMs() / 1000.0, worker.lastThreads());

            ImGui::SameLine(ImGui::GetWindowWidth() - 220);
            if (ImGui::SmallButton("Copy")) {
                SDL_SetClipboardText(digitsView.text.c_str());
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Save .txt")) {
                std::time_t t = std::time(nullptr);
                char fname[64];
                std::strftime(fname, sizeof(fname), "pi_%Y%m%d_%H%M%S.txt", std::localtime(&t));
                std::ofstream f(fname);
                f << digitsView.text;
                lastSavePath = fname;
            }
            if (!lastSavePath.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("saved: %s", lastSavePath.c_str());
            }

            ImGui::BeginChild("digits_box", ImVec2(0, 220), true);
            ImGuiListClipper clipper;
            clipper.Begin(digitsView.lineCount());
            while (clipper.Step()) {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                    std::string ln = digitsView.line(i);
                    ImGui::TextUnformatted(ln.c_str());
                }
            }
            clipper.End();
            ImGui::EndChild();
        }

        ImGui::End();

        ImGui::Render();
        int dw, dh;
        SDL_GL_GetDrawableSize(window, &dw, &dh);
        glViewport(0, 0, dw, dh);
        glClearColor(0.02f, 0.03f, 0.024f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    worker.cancel();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
