#pragma once
#include <SDL.h>
#include <string>

class SearchKeyboard {
public:
    enum class State { Idle, Visible, Disappearing };
    bool Initialize();
    void Shutdown();
    bool Request(const std::string& initialText, const std::string& hintText);
    void Update();
    bool IsActive() const { return state_ != State::Idle; }
    bool ConsumeResult(bool& confirmed, std::string& text);
private:
    static int SDLCALL EventWatch(void* userdata, SDL_Event* event);
    int HandleEvent(SDL_Event* event);
    State state_{State::Idle};
    std::string inputBuffer_;
    bool pendingConfirmed_{};
    bool resultReady_{};
    bool initialized_{};
};
