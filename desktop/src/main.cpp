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
#include "widgets.h"
#include "config.h"

#include "fonts/font_ui.h"
#include "fonts/font_display.h"
#include "fonts/font_mono.h"

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
    // the layout never scrolls, so don't let the window shrink below it
    SDL_SetWindowMinimumSize(window, 760, 640);
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, glContext);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // ImGui lays out in window points but renders into the (possibly 2x)
    // drawable, so rasterize glyphs at the pixel scale and scale the layout
    // back down - otherwise every label is a blurry upscale.
    int winW = 0, winH = 0, drawW = 0, drawH = 0;
    SDL_GetWindowSize(window, &winW, &winH);
    SDL_GL_GetDrawableSize(window, &drawW, &drawH);
    float dpi = (winW > 0) ? (float)drawW / (float)winW : 1.0f;
    if (dpi < 1.0f) dpi = 1.0f;

    ImFont *fontUI = io.Fonts->AddFontFromMemoryCompressedTTF(
        SpaceGroteskMedium_compressed_data, SpaceGroteskMedium_compressed_size, 17.0f * dpi);
    ImFont *fontDisplay = io.Fonts->AddFontFromMemoryCompressedTTF(
        SpaceGroteskBold_compressed_data, SpaceGroteskBold_compressed_size, 34.0f * dpi);
    ImFont *fontMono = io.Fonts->AddFontFromMemoryCompressedTTF(
        JetBrainsMono_compressed_data, JetBrainsMono_compressed_size, 14.0f * dpi);
    io.FontDefault = fontUI;
    io.FontGlobalScale = 1.0f / dpi;

    theme::apply();
    ImGui_ImplSDL2_InitForOpenGL(window, glContext);
    ImGui_ImplOpenGL3_Init(glsl_version);

    PiWorker worker;
    Updater updater;
    updater.checkAsync();

    ParticleField field;
    bool fieldBuilt = false;
    ImVec2 lastStage(0, 0);
    int animState = 0; // 0 idle, 1 forming/holding, 2 pulse

    static char digitsBuf[16] = "10000";
    int maxThreads = (int)std::max(1u, std::thread::hardware_concurrency());
    static int threadCount = maxThreads;
    int activePreset = 1;

    DigitsView digitsView;
    bool hasResult = false;
    bool cancelPending = false;
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
                          ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        // two-tone wordmark, matching the web app
        ImGui::PushFont(fontDisplay);
        ImGui::TextColored(ImVec4(0.43f, 0.60f, 0.49f, 1.0f), "poly");
        ImGui::SameLine(0, 0);
        ImGui::TextColored(ImVec4(0.24f, 0.86f, 0.47f, 1.0f), "Pi");
        ImGui::PopFont();

        ImGui::TextDisabled("A tool that helps you count Pi.  -  v" POLYPI_VERSION);
        ImGui::Spacing();

        // ---- update banner ----
        {
            // The window-wide 18px padding is far too generous for a one-line
            // bar: with it, the row gets clipped. Size the bar from real metrics.
            const ImVec2 barPad(12, 7);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, barPad);
            float barH = ImGui::GetFrameHeight() + barPad.y * 2.0f;
            ImGui::BeginChild("update_bar", ImVec2(0, barH), true, ImGuiWindowFlags_NoScrollbar);
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
            const char *checkLabel = "Check now";
            float checkW = ImGui::CalcTextSize(checkLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
            ImGui::SameLine(ImGui::GetContentRegionMax().x - checkW);
            if (ImGui::Button(checkLabel)) updater.checkAsync();
            ImGui::EndChild();
            ImGui::PopStyleVar();
        }

        ImGui::Spacing();

        // ---- particle stage ----
        // The window never scrolls, so the stage yields height to whatever
        // else needs to be on screen instead of pushing content off the edge.
        const float kControlsHeight = 250.0f; // digits row + speed + action button
        float availForStage = ImGui::GetContentRegionAvail().y - kControlsHeight -
                              (hasResult ? 150.0f : 0.0f);
        float stageH = availForStage;
        if (stageH < 110.0f) stageH = 110.0f;
        if (stageH > 260.0f) stageH = 260.0f;
        ImVec2 stageSize = ImVec2(ImGui::GetContentRegionAvail().x, stageH);
        ImGui::BeginChild("stage", stageSize, true, ImGuiWindowFlags_NoScrollbar);
        ImVec2 p0 = ImGui::GetWindowPos();
        ImVec2 p1 = ImVec2(p0.x + ImGui::GetWindowSize().x, p0.y + ImGui::GetWindowSize().y);
        // stage height flexes with the layout, so rebuild when it actually moves
        if (!fieldBuilt || std::fabs(p1.x - p0.x - lastStage.x) > 1.0f ||
            std::fabs(p1.y - p0.y - lastStage.y) > 1.0f) {
            field.build(p0, p1);
            lastStage = ImVec2(p1.x - p0.x, p1.y - p0.y);
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
        ImGui::PushFont(fontMono);
        bool enterPressed = ImGui::InputText("##digits", digitsBuf, sizeof(digitsBuf),
                                              ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopFont();
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

        // Measured peak is ~21 bytes per digit, dominated by the merge phase
        // of the binary splitting - so the ceiling is RAM, not patience, and
        // overshooting it kills the run mid-count. Say so up front.
        {
            double needGB = digitsWanted * 21.0 / 1073741824.0;
            int ramMB = SDL_GetSystemRAM();
            double ramGB = ramMB > 0 ? ramMB / 1024.0 : 0.0;
            if (needGB >= 0.5) {
                bool overRam = ramGB > 0.0 && needGB > ramGB * 0.75;
                ImGui::TextColored(overRam ? ImVec4(1, 0.42f, 0.42f, 1) : ImVec4(1, 0.75f, 0.3f, 1),
                                   "Needs roughly %.1f GB of RAM%s%s", needGB,
                                   ramGB > 0.0 ? " of your " : "",
                                   ramGB > 0.0 ? (std::to_string((int)(ramGB + 0.5)) + " GB").c_str() : "");
                if (overRam) {
                    ImGui::TextColored(ImVec4(1, 0.42f, 0.42f, 1),
                                       "That likely won't fit alongside your other apps - the run would fail part-way.");
                }
            }
        }

        ImGui::Spacing();
        ImGui::Text("Speed");
        widgets::SmoothSliderInt("threads", &threadCount, 1, maxThreads, 320.0f, io.DeltaTime);
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImVec4(0.24f, 0.86f, 0.47f, 1.0f), "%d / %d threads", threadCount, maxThreads);
        ImGui::TextDisabled("More threads = faster on large digit counts.");

        ImGui::Spacing();
        bool isRunning = worker.isRunning();
        if (!isRunning) {
            if (ImGui::Button(hasResult ? "COUNT AGAIN" : "COUNT PI", ImVec2(200, 42)) || enterPressed) {
                hasResult = false;
                // drop the old digits first - at these sizes holding the
                // previous result while allocating the next one is what
                // pushes a big run into an out-of-memory failure
                std::string().swap(digitsView.text);
                std::vector<int>().swap(digitsView.lineStart);
                lastSavePath.clear();
                animState = 1;
                worker.start(digitsWanted, threadCount);
            }
        } else {
            // Formatting is a few big opaque GMP calls; a cancel there can't
            // land until the current one ends, so don't offer a button that
            // would look broken - say what's happening instead.
            const bool finalizing = worker.isFinalizing();
            ImGui::BeginDisabled(finalizing);
            if (ImGui::Button("CANCEL", ImVec2(200, 42))) {
                worker.cancel();
                cancelPending = true;
            }
            ImGui::EndDisabled();
            if (finalizing && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Writing out the digits - this last step can't be interrupted.");
            }
        }

        if (isRunning) {
            ImGui::SameLine();
            if (worker.isFinalizing()) {
                ImGui::TextDisabled("writing out the digits...");
            } else if (cancelPending) {
                ImGui::TextDisabled("stopping...");
            } else {
                ImGui::Text("%.1fs elapsed  |  ~%s digits/sec", worker.elapsedMs() / 1000.0,
                            std::to_string(worker.digitsPerSecEstimate()).c_str());
            }
            ImGui::ProgressBar((float)worker.progress(), ImVec2(-1, 8), "");
        }

        // a cancelled run leaves the glyph half-assembled; settle it back to idle
        if (cancelPending && !isRunning) {
            cancelPending = false;
            if (worker.wasCancelled()) animState = 0;
        }

        if (worker.hasError() && !isRunning) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1, 0.42f, 0.42f, 1),
                               "Ran out of memory at that size - try fewer digits.");
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

            const ImGuiStyle &st = ImGui::GetStyle();
            float copyW = ImGui::CalcTextSize("Copy").x + st.FramePadding.x * 2.0f;
            float saveW = ImGui::CalcTextSize("Save .txt").x + st.FramePadding.x * 2.0f;
            ImGui::SameLine(ImGui::GetContentRegionMax().x - copyW - saveW - st.ItemSpacing.x);
            if (ImGui::Button("Copy")) {
                SDL_SetClipboardText(digitsView.text.c_str());
            }
            ImGui::SameLine();
            if (ImGui::Button("Save .txt")) {
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

            // take exactly what's left so the window never needs to scroll
            float boxH = ImGui::GetContentRegionAvail().y;
            if (boxH < 80.0f) boxH = 80.0f;
            ImGui::BeginChild("digits_box", ImVec2(0, boxH), true);
            ImGui::PushFont(fontMono); // digits have to line up in columns
            ImGuiListClipper clipper;
            clipper.Begin(digitsView.lineCount());
            while (clipper.Step()) {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                    std::string ln = digitsView.line(i);
                    ImGui::TextUnformatted(ln.c_str());
                }
            }
            clipper.End();
            ImGui::PopFont();
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
