#include "platform/wiiu/SearchKeyboard.hpp"

#include <SDL_system.h>
#include <SDL_syswm.h>
#include <nn/swkbd.h>
#include <vpad/input.h>

bool SearchKeyboard::Initialize()
{
    if (initialized_) return true;
    SDL_SetHint(SDL_HINT_ENABLE_SCREEN_KEYBOARD, "1");
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
    SDL_AddEventWatch(EventWatch, this);
    state_ = State::Idle;
    inputBuffer_.clear();
    pendingConfirmed_ = false;
    resultReady_ = false;
    initialized_ = true;
    return true;
}

void SearchKeyboard::Shutdown()
{
    if (!initialized_) return;
    SDL_DelEventWatch(EventWatch, this);
    if (state_ != State::Idle) SDL_StopTextInput();
    state_ = State::Idle;
    inputBuffer_.clear();
    pendingConfirmed_ = false;
    resultReady_ = false;
    initialized_ = false;
}

bool SearchKeyboard::Request(const std::string& initialText, const std::string& hintText)
{
    if (!initialized_ || state_ != State::Idle) return false;
    inputBuffer_.clear();
    pendingConfirmed_ = false;
    resultReady_ = false;
    SDL_WiiUSetSWKBDInitialText(initialText.empty() ? nullptr : initialText.c_str());
    SDL_WiiUSetSWKBDHintText(hintText.empty() ? nullptr : hintText.c_str());
    SDL_WiiUSetSWKBDHighlightInitialText(!initialText.empty() ? SDL_TRUE : SDL_FALSE);
    SDL_WiiUSetSWKBDShowCopyPasteButtons(SDL_TRUE);
    SDL_WiiUSetSWKBDKeyboardMode(SDL_WIIU_SWKBD_KEYBOARD_MODE_RESTRICTED);
    SDL_StartTextInput();
    state_ = State::Visible;
    return true;
}

void SearchKeyboard::Update()
{
    if (state_ == State::Idle) return;
    VPADStatus vpadStatus{};
    VPADReadError vpadError{};
    if (VPADRead(VPAD_CHAN_0, &vpadStatus, 1, &vpadError) > 0 &&
        vpadError == VPAD_READ_SUCCESS)
    {
        VPADTouchData calibrated{};
        VPADGetTPCalibratedPoint(VPAD_CHAN_0, &calibrated, &vpadStatus.tpFiltered1);
        vpadStatus.tpNormal = calibrated;
        vpadStatus.tpFiltered1 = calibrated;
        vpadStatus.tpFiltered2 = calibrated;
        SDL_WiiUSetSWKBDVPAD(&vpadStatus);
    }
    SDL_PumpEvents();
    if (state_ == State::Disappearing &&
        nn::swkbd::GetStateInputForm() == nn::swkbd::State::Hidden)
    {
        state_ = State::Idle;
        resultReady_ = true;
    }
}

bool SearchKeyboard::ConsumeResult(bool& confirmed, std::string& text)
{
    if (!resultReady_) return false;
    confirmed = pendingConfirmed_;
    text = inputBuffer_;
    resultReady_ = false;
    return true;
}

int SDLCALL SearchKeyboard::EventWatch(void* userdata, SDL_Event* event)
{
    if (userdata == nullptr) return 1;
    return static_cast<SearchKeyboard*>(userdata)->HandleEvent(event);
}

int SearchKeyboard::HandleEvent(SDL_Event* event)
{
    if (state_ == State::Idle) return 1;
    if (event->type == SDL_TEXTINPUT) {
        if (state_ == State::Visible) inputBuffer_ += event->text.text;
        return 0;
    }
#if defined(SDL_VIDEO_DRIVER_WIIU)
    if (event->type == SDL_SYSWMEVENT && event->syswm.msg != nullptr) {
        const unsigned int keyboardEvent = event->syswm.msg->msg.wiiu.event;
        if (keyboardEvent == SDL_WIIU_SYSWM_SWKBD_OK_START_EVENT) {
            inputBuffer_.clear();
            return 0;
        }
        if (keyboardEvent == SDL_WIIU_SYSWM_SWKBD_OK_FINISH_EVENT) {
            pendingConfirmed_ = true;
            state_ = State::Disappearing;
            SDL_StopTextInput();
            return 0;
        }
        if (keyboardEvent == SDL_WIIU_SYSWM_SWKBD_CANCEL_EVENT) {
            pendingConfirmed_ = false;
            state_ = State::Disappearing;
            SDL_StopTextInput();
            return 0;
        }
    }
#endif
    return 1;
}
