#pragma once

#include "Bitmap.h"
#include "Common/DynamicArray.h"
#include "Event.h"
#include "Multitasking/Thread.h"
#include "Utilities.h"

namespace kernel {

class Window {
public:
    static RefPtr<Window> create(Thread& owner, const Rect& window_rect) { return new Window(owner, window_rect); }

    const Rect& rect() const { return m_rect; }

    void set_focused() { s_currently_focused = this; }

    static bool is_any_focused() { return s_currently_focused != nullptr; }

    static Window& focused()
    {
        ASSERT(s_currently_focused != nullptr);
        return *s_currently_focused;
    }

    Surface& surface()
    {
        ASSERT(m_front_surface.get() != nullptr);
        return *m_front_surface;
    }

    const Surface& surface() const
    {
        ASSERT(m_front_surface.get() != nullptr);
        return *m_front_surface;
    }

    size_t width() const { return surface().width(); }
    size_t height() const { return surface().height(); }

    void enqueue_event(const Event& event) { m_event_queue.append(event); }

private:
    Window(Thread& owner, const Rect& window_rect);

private:
    Thread&             m_owner;
    Rect                m_rect;
    RefPtr<Surface>     m_front_surface;
    DynamicArray<Event> m_event_queue;

    // TODO: this should be a weak ptr
    static Window*               s_currently_focused;
    static InterruptSafeSpinLock s_lock;
};
}
