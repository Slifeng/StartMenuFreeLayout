#pragma once
#include <cmath>
namespace input {
enum class Phase { Idle, Pending, Dragging };
enum class Release { None, Click, Drop };
struct Gesture {
    Phase phase=Phase::Idle;
    void Begin(bool threshold) {phase=threshold?Phase::Pending:Phase::Dragging;}
    bool Active() const {return phase!=Phase::Idle;}
    bool Dragging() const {return phase==Phase::Dragging;}
    bool Move(double dx,double dy,double threshold) {
        if(phase==Phase::Pending&&(std::abs(dx)>=threshold||std::abs(dy)>=threshold)) {
            phase=Phase::Dragging;return true;
        }
        return false;
    }
    Release End() {
        auto result=phase==Phase::Dragging?Release::Drop:(phase==Phase::Pending?Release::Click:Release::None);
        phase=Phase::Idle;return result;
    }
    void Cancel() {phase=Phase::Idle;}
};
}
