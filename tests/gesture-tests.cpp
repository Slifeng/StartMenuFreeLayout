#include "PointerGesture.h"
#include <cassert>
#include <iostream>
int main() {
    input::Gesture g;
    assert(g.End()==input::Release::None);
    g.Begin(true);assert(!g.Dragging());
    assert(!g.Move(2,-3,6));assert(g.End()==input::Release::Click);
    g.Begin(true);assert(g.Move(6,0,6));assert(g.Dragging());
    assert(!g.Move(30,10,6));assert(g.End()==input::Release::Drop);
    assert(g.End()==input::Release::None); // release + capture-lost cannot double-drop
    g.Begin(true);assert(g.Move(0,-7,6));g.Cancel();assert(g.End()==input::Release::None);
    g.Begin(false);assert(g.Dragging());assert(g.End()==input::Release::Drop);
    g.Begin(true);g.Cancel();assert(!g.Active());
    std::cout<<"PASS: native click, jitter, threshold, drag, duplicate release, mode cancellation\n";
}
