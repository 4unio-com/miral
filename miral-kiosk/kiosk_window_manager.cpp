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
 * Authored By: Alan Griffiths <alan@octopull.co.uk>
 */

#include "kiosk_window_manager.h"

#include "miral/application.h"
#include "miral/window_manager_tools.h"

#include "mir/scene/session.h"

#include <linux/input.h>
#include <sys/types.h>
#include <unistd.h>

namespace ms = mir::scene;
using namespace miral;

KioskWindowManagerPolicy::KioskWindowManagerPolicy(WindowManagerTools* const tools) :
    tools{tools}
{
}

void KioskWindowManagerPolicy::handle_app_info_updated(Rectangles const& /*displays*/)
{
}

void KioskWindowManagerPolicy::handle_displays_updated(Rectangles const& /*displays*/)
{
}

auto KioskWindowManagerPolicy::handle_place_new_surface(
    ApplicationInfo const& /*app_info*/,
    ms::SurfaceCreationParameters const& request_parameters)
-> ms::SurfaceCreationParameters
{
    auto parameters = request_parameters;

    Rectangle const active_display = tools->active_display();
    parameters.top_left = parameters.top_left + (active_display.top_left - Point{0, 0});

    if (parameters.parent.lock())
    {
        auto parent = tools->info_for(parameters.parent).window;
        auto const width = parameters.size.width.as_int();
        auto const height = parameters.size.height.as_int();

        if (parameters.aux_rect.is_set() && parameters.edge_attachment.is_set())
        {
            auto const edge_attachment = parameters.edge_attachment.value();
            auto const aux_rect = parameters.aux_rect.value();
            auto const parent_top_left = parent.top_left();
            auto const top_left = aux_rect.top_left     -Point{} + parent_top_left;
            auto const top_right= aux_rect.top_right()  -Point{} + parent_top_left;
            auto const bot_left = aux_rect.bottom_left()-Point{} + parent_top_left;

            if (edge_attachment & mir_edge_attachment_vertical)
            {
                if (active_display.contains(top_right + Displacement{width, height}))
                {
                    parameters.top_left = top_right;
                }
                else if (active_display.contains(top_left + Displacement{-width, height}))
                {
                    parameters.top_left = top_left + Displacement{-width, 0};
                }
            }

            if (edge_attachment & mir_edge_attachment_horizontal)
            {
                if (active_display.contains(bot_left + Displacement{width, height}))
                {
                    parameters.top_left = bot_left;
                }
                else if (active_display.contains(top_left + Displacement{width, -height}))
                {
                    parameters.top_left = top_left + Displacement{0, -height};
                }
            }
        }
        else
        {
            auto const parent_top_left = parent.top_left();
            auto const centred = parent_top_left
                                 + 0.5*(as_displacement(parent.size()) - as_displacement(parameters.size))
                                 - DeltaY{(parent.size().height.as_int()-height)/6};

            parameters.top_left = centred;
        }
    }
    else
    {
        parameters.size = active_display.size;
    }

    return parameters;
}

void KioskWindowManagerPolicy::generate_decorations_for(WindowInfo& /*window_info*/)
{
}

void KioskWindowManagerPolicy::handle_new_window(WindowInfo& window_info)
{
    auto const window = window_info.window;
    auto const session = window.session();

    tools->info_for(session).windows.push_back(window);

    if (auto const parent = window_info.parent)
    {
        tools->info_for(parent).children.push_back(window);
    }
}

void KioskWindowManagerPolicy::handle_window_ready(WindowInfo& window_info)
{
    select_active_surface(window_info.window);
}

void KioskWindowManagerPolicy::handle_modify_window(
    WindowInfo& window_info,
    mir::shell::SurfaceSpecification const& modifications)
{
    if (modifications.name.is_set())
        window_info.window.rename(modifications.name.value());
}

void KioskWindowManagerPolicy::handle_delete_window(WindowInfo& window_info)
{
    auto const session = window_info.window.session();
    auto const& window = window_info.window;

    if (auto const parent = window_info.parent)
    {
        auto& siblings = tools->info_for(parent).children;

        for (auto i = begin(siblings); i != end(siblings); ++i)
        {
            if (window == *i)
            {
                siblings.erase(i);
                break;
            }
        }
    }

    auto& windows = tools->info_for(session).windows;

    for (auto i = begin(windows); i != end(windows); ++i)
    {
        if (window == *i)
        {
            windows.erase(i);
            break;
        }
    }

    window_info.window.destroy_surface();

    if (windows.empty() && session == tools->focused_session())
    {
        tools->focus_next_session();
        select_active_surface(tools->focused_surface());
    }
}

auto KioskWindowManagerPolicy::handle_set_state(WindowInfo& window_info, MirSurfaceState value)
-> MirSurfaceState
{
    auto state = transform_set_state(window_info, value);
    window_info.window.set_state(state);
    return state;
}

auto KioskWindowManagerPolicy::transform_set_state(WindowInfo& window_info, MirSurfaceState /*value*/)
-> MirSurfaceState
{
    return window_info.state;
}

void KioskWindowManagerPolicy::handle_raise_window(WindowInfo& window_info)
{
    select_active_surface(window_info.window);
}

bool KioskWindowManagerPolicy::handle_keyboard_event(MirKeyboardEvent const* event)
{
    auto const action = mir_keyboard_event_action(event);
    auto const scan_code = mir_keyboard_event_scan_code(event);
    auto const modifiers = mir_keyboard_event_modifiers(event) & modifier_mask;

    if (action == mir_keyboard_action_down &&
            modifiers == mir_input_event_modifier_alt &&
            scan_code == KEY_TAB)
    {
        tools->focus_next_session();
        select_active_surface(tools->focused_surface());

        return true;
    }
    else if (action == mir_keyboard_action_down &&
            modifiers == mir_input_event_modifier_alt &&
            scan_code == KEY_GRAVE)
    {
        if (auto const prev = tools->focused_surface())
        {
            if (auto const app = tools->focused_session())
                if (auto const window = app.surface_after(prev))
                {
                    select_active_surface(tools->info_for(window).window);
                }
        }

        return true;
    }

    return false;
}

bool KioskWindowManagerPolicy::handle_touch_event(MirTouchEvent const* event)
{
    auto const count = mir_touch_event_point_count(event);

    long total_x = 0;
    long total_y = 0;

    for (auto i = 0U; i != count; ++i)
    {
        total_x += mir_touch_event_axis_value(event, i, mir_touch_axis_x);
        total_y += mir_touch_event_axis_value(event, i, mir_touch_axis_y);
    }

    Point const cursor{total_x/count, total_y/count};

    old_cursor = cursor;
    return false;
}

bool KioskWindowManagerPolicy::handle_pointer_event(MirPointerEvent const* event)
{
    auto const action = mir_pointer_event_action(event);

    Point const cursor{
        mir_pointer_event_axis_value(event, mir_pointer_axis_x),
        mir_pointer_event_axis_value(event, mir_pointer_axis_y)};

    if (action == mir_pointer_action_button_down)
    {
        select_active_surface(tools->surface_at(cursor));
    }

    old_cursor = cursor;
    return false;
}

auto KioskWindowManagerPolicy::select_active_surface(Window const& window) -> Window
{
    if (!window)
    {
        tools->set_focus_to({});
        return window;
    }

    auto const& info_for = tools->info_for(window);

    if (info_for.can_be_active())
    {
        tools->set_focus_to(info_for.window);
        tools->raise_tree(window);

        raise_internal_sessions();

        return window;
    }
    else
    {
        // Cannot have input focus - try the parent
        if (auto const parent = info_for.parent)
            return select_active_surface(parent);

        return {};
    }
}

void KioskWindowManagerPolicy::raise_internal_sessions() const
{// Look for any internal sessions and raise its window(s)
    tools->for_each_session([this](ApplicationInfo const& app_info)
        {
            if (!app_info.windows.empty())
            {
                if (auto const& first_surface = app_info.windows[0])
                {
                    if (auto const scene_session = first_surface.session())
                    {
                        if (scene_session->process_id() == getpid())
                        {
                            for (auto const& s : app_info.windows)
                                tools->raise_tree(s);
                        }
                    }
                }
            }
        });
}