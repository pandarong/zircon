#include <inttypes.h>

#include <zxcpp/new.h>

#include <object/channel_dispatcher.h>
#include <object/dispatcher.h>
#include <object/event_dispatcher.h>
#include <object/handle.h>
#include <object/handle_reaper.h>
#include <object/state_tracker.h>

#include <zircon/types.h>

Handle* MakeHandle(fbl::RefPtr<Dispatcher> dispatcher, zx_rights_t rights) {
    return new Handle(dispatcher, rights, /*base_value=*/0x55555555);
}

namespace internal {
void TearDownHandle(Handle* handle) {
    delete handle;
}
} // namespace internal

void DeleteHandle(Handle* handle) {
    fbl::RefPtr<Dispatcher> dispatcher(handle->dispatcher());
    auto state_tracker = dispatcher->get_state_tracker();

    if (state_tracker) {
        state_tracker->Cancel(handle);
    }

    internal::TearDownHandle(handle);
}

void ReapHandles(Handle** handles, uint32_t num_handles) {
    while (num_handles > 0) {
        DeleteHandle(*handles++);
    }
}

int main(int argc, char** argv) {
    Handle* h = MakeHandle(nullptr, ZX_RIGHT_READ);
    StateTracker st(0x5);
    printf("signals 0x%x\n", st.GetSignalsState());
    printf("rights 0x%x\n", h->rights());

    fbl::RefPtr<Dispatcher> ev;
    zx_rights_t evr;
    EventDispatcher::Create(0u, &ev, &evr);
    printf("ev koid %" PRIu64 "\n", ev->get_koid());

    fbl::RefPtr<Dispatcher> ch0;
    fbl::RefPtr<Dispatcher> ch1;
    zx_rights_t chr;
    ChannelDispatcher::Create(&ch0, &ch1, &chr);
    printf("ch0 koid %" PRIu64 ", related %" PRIu64 "\n",
           ch0->get_koid(), ch0->get_related_koid());
    printf("ch1 koid %" PRIu64 ", related %" PRIu64 "\n",
           ch1->get_koid(), ch1->get_related_koid());
    return 0;
}
