//-----------------------------------------------------------------------------
// Cross-platform handling of spnavdev 6-dof input.
//
// Copyright 2021 Collabora, Ltd.
//-----------------------------------------------------------------------------

#ifndef SOLVESPACE_SPNAVDEVICE_H
#define SOLVESPACE_SPNAVDEVICE_H
#include <vector>
#include <array>

namespace SolveSpace {
namespace Platform {
    struct SixDofEvent;
}
} // namespace SolveSpace
struct spndev;

class NavDeviceWrapper {
public:
    explicit NavDeviceWrapper(const char* device = nullptr);
    ~NavDeviceWrapper();

    // no copy, no move
    NavDeviceWrapper(NavDeviceWrapper const&) = delete;
    NavDeviceWrapper& operator=(NavDeviceWrapper const&) = delete;
    NavDeviceWrapper(NavDeviceWrapper&&)                 = delete;
    NavDeviceWrapper& operator=(NavDeviceWrapper&&) = delete;

    bool active() const noexcept {
        return dev != nullptr;
    }

    //! true when the event has data in it to deal with.
    bool process(SolveSpace::Platform::SixDofEvent& event);

    enum class NavButton {
        UNUSED,
        FIT,
        CTRL,
        SHIFT,
    };
    struct AxisData {
        AxisData() = default;
        AxisData(int spnavdevIndex_) : spnavdevIndex(spnavdevIndex_) {
        }

        int spnavdevIndex = -1;
    };

private:
    void populateAxes();
    void populateButtons();
    struct spndev* dev = nullptr;
    //! indexed by the spnavdev index
    std::vector<NavButton> buttons;
    //! indexed by our internal axis index.
    std::array<AxisData, 6> axes;
    bool ctrlPressed  = false;
    bool shiftPressed = false;
};
#endif // !SOLVESPACE_SPNAVDEVICE_H