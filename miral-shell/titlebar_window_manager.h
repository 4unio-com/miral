/*
 * Copyright © 2016 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#ifndef MIRAL_SHELL_TITLEBAR_WINDOW_MANAGER_H
#define MIRAL_SHELL_TITLEBAR_WINDOW_MANAGER_H

#include <miral/canonical_window_manager.h>
#include <miral/workspace_policy.h>

#include "spinner/splash.h"

#include <chrono>
#include <map>

namespace miral { class InternalClientLauncher; }

using namespace mir::geometry;

class TitlebarProvider;

class TitlebarWindowManagerPolicy : public miral::CanonicalWindowManagerPolicy, miral::WorkspacePolicy
{
public:
    TitlebarWindowManagerPolicy(miral::WindowManagerTools const& tools, SpinnerSplash const& spinner, miral::InternalClientLauncher const& launcher);
    ~TitlebarWindowManagerPolicy();

    virtual miral::WindowSpecification place_new_window(
        miral::ApplicationInfo const& app_info, miral::WindowSpecification const& request_parameters) override;

    /** @name example event handling:
     *  o Switch apps: Alt+Tab, tap or click on the corresponding window
     *  o Switch window: Alt+`, tap or click on the corresponding window
     *  o Move window: Alt-leftmousebutton drag (three finger drag)
     *  o Resize window: Alt-middle_button drag (three finger pinch)
     *  o Maximize/restore current window (to display size): Alt-F11
     *  o Maximize/restore current window (to display height): Shift-F11
     *  o Maximize/restore current window (to display width): Ctrl-F11
     *  @{ */
    bool handle_pointer_event(MirPointerEvent const* event) override;
    bool handle_touch_event(MirTouchEvent const* event) override;
    bool handle_keyboard_event(MirKeyboardEvent const* event) override;
    /** @} */

    /** @name track events that affect titlebar
     *  @{ */
    void advise_new_window(miral::WindowInfo const& window_info) override;
    void handle_window_ready(miral::WindowInfo& window_info) override;
    void advise_focus_lost(miral::WindowInfo const& info) override;
    void advise_focus_gained(miral::WindowInfo const& info) override;
    void advise_state_change(miral::WindowInfo const& window_info, MirWindowState state) override;
    void advise_resize(miral::WindowInfo const& window_info, Size const& new_size) override;
    void advise_delete_window(miral::WindowInfo const& window_info) override;
    /** @} */

protected:
    static const int modifier_mask =
        mir_input_event_modifier_alt |
        mir_input_event_modifier_shift |
        mir_input_event_modifier_sym |
        mir_input_event_modifier_ctrl |
        mir_input_event_modifier_meta;

private:
    void toggle(MirWindowState state);

    bool resize(miral::Window const& window, Point cursor, Point old_cursor);

    Point old_cursor{};

    bool resizing = false;
    bool left_resize = false;
    bool top_resize  = false;

    int old_touch_pinch_top = 0;
    int old_touch_pinch_left = 0;
    int old_touch_pinch_width = 0;
    int old_touch_pinch_height = 0;
    bool pinching = false;

    SpinnerSplash const spinner;

    std::unique_ptr<TitlebarProvider> const titlebar_provider;

    void end_resize();

    void keep_size_within_limits(
        miral::WindowInfo const& window_info,
        Displacement& delta,
        Width& new_width,
        Height& new_height) const;

    // Workaround for lp:1627697
    std::chrono::steady_clock::time_point last_resize;

    void advise_adding_to_workspace(
        std::shared_ptr<miral::Workspace> const& workspace,
        std::vector<miral::Window> const& windows) override;

    void advise_removing_from_workspace(
        std::shared_ptr<miral::Workspace> const& workspace,
        std::vector<miral::Window> const& windows) override;

    void switch_workspace_to(std::shared_ptr<miral::Workspace> const& workspace);

    std::shared_ptr<miral::Workspace> active_workspace;
    std::map<int, std::shared_ptr<miral::Workspace>> key_to_workspace;
};

#endif //MIRAL_SHELL_TITLEBAR_WINDOW_MANAGER_H
