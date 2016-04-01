/*
 * Copyright © 2015-2016 Canonical Ltd.
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

#include "mir/al/basic_window_manager.h"

#include "mir/scene/session.h"
#include "mir/scene/surface.h"
#include "mir/scene/surface_creation_parameters.h"

namespace ma = mir::al;

ma::BasicWindowManager::BasicWindowManager(
    shell::FocusController* focus_controller,
    std::unique_ptr<WindowManagementPolicy> policy) :
    focus_controller(focus_controller),
    policy(std::move(policy)),
    surface_builder([](std::shared_ptr<scene::Session> const&, scene::SurfaceCreationParameters const&) -> Surface
        { throw std::logic_error{"Can't create a surface yet"};})
{
}

auto ma::BasicWindowManager::build_surface(std::shared_ptr<scene::Session> const& session, scene::SurfaceCreationParameters const& parameters)
-> SurfaceInfo&
{
    auto result = surface_builder(session, parameters);
    auto& info = surface_info.emplace(result, SurfaceInfo{result, parameters}).first->second;
    if (auto const parent = parameters.parent.lock())
        info.parent = info_for(parent).surface;
    return info;
}

void ma::BasicWindowManager::add_session(std::shared_ptr<scene::Session> const& session)
{
    std::lock_guard<decltype(mutex)> lock(mutex);
    session_info[session] = SessionInfo();
    policy->handle_session_info_updated(displays);
}

void ma::BasicWindowManager::remove_session(std::shared_ptr<scene::Session> const& session)
{
    std::lock_guard<decltype(mutex)> lock(mutex);
    session_info.erase(session);
    policy->handle_session_info_updated(displays);
}

auto ma::BasicWindowManager::add_surface(
    std::shared_ptr<scene::Session> const& session,
    scene::SurfaceCreationParameters const& params,
    std::function<frontend::SurfaceId(std::shared_ptr<scene::Session> const& session, scene::SurfaceCreationParameters const& params)> const& build)
-> frontend::SurfaceId
{
    std::lock_guard<decltype(mutex)> lock(mutex);
    surface_builder = [build](std::shared_ptr<scene::Session> const& session, scene::SurfaceCreationParameters const& params)
        { return Surface{session, build(session, params)}; };
    auto const placed_params = policy->handle_place_new_surface(session, params);

    auto& info = build_surface(session, placed_params);

    policy->handle_new_surface(info);
    policy->generate_decorations_for(info);

    return info.surface.surface_id();
}

void ma::BasicWindowManager::modify_surface(
    std::shared_ptr<scene::Session> const& /*session*/,
    std::shared_ptr<scene::Surface> const& surface,
    shell::SurfaceSpecification const& modifications)
{
    std::lock_guard<decltype(mutex)> lock(mutex);
    policy->handle_modify_surface(info_for(surface), modifications);
}

void ma::BasicWindowManager::remove_surface(
    std::shared_ptr<scene::Session> const& /*session*/,
    std::weak_ptr<scene::Surface> const& surface)
{
    std::lock_guard<decltype(mutex)> lock(mutex);
    policy->handle_delete_surface(info_for(surface));

    surface_info.erase(surface);
}

void ma::BasicWindowManager::forget(Surface const& surface)
{
    surface_info.erase(surface);
}

void ma::BasicWindowManager::add_display(geometry::Rectangle const& area)
{
    std::lock_guard<decltype(mutex)> lock(mutex);
    displays.add(area);
    policy->handle_displays_updated(displays);
}

void ma::BasicWindowManager::remove_display(geometry::Rectangle const& area)
{
    std::lock_guard<decltype(mutex)> lock(mutex);
    displays.remove(area);
    policy->handle_displays_updated(displays);
}

bool ma::BasicWindowManager::handle_keyboard_event(MirKeyboardEvent const* event)
{
    std::lock_guard<decltype(mutex)> lock(mutex);
    update_event_timestamp(event);
    return policy->handle_keyboard_event(event);
}

bool ma::BasicWindowManager::handle_touch_event(MirTouchEvent const* event)
{
    std::lock_guard<decltype(mutex)> lock(mutex);
    update_event_timestamp(event);
    return policy->handle_touch_event(event);
}

bool ma::BasicWindowManager::handle_pointer_event(MirPointerEvent const* event)
{
    std::lock_guard<decltype(mutex)> lock(mutex);
    update_event_timestamp(event);

    cursor = {
        mir_pointer_event_axis_value(event, mir_pointer_axis_x),
        mir_pointer_event_axis_value(event, mir_pointer_axis_y)};

    return policy->handle_pointer_event(event);
}

void ma::BasicWindowManager::handle_raise_surface(
    std::shared_ptr<scene::Session> const& /*session*/,
    std::shared_ptr<scene::Surface> const& surface,
    uint64_t timestamp)
{
    std::lock_guard<decltype(mutex)> lock(mutex);
    if (timestamp >= last_input_event_timestamp)
        policy->handle_raise_surface(info_for(surface));
}

int ma::BasicWindowManager::set_surface_attribute(
    std::shared_ptr<scene::Session> const& /*session*/,
    std::shared_ptr<scene::Surface> const& surface,
    MirSurfaceAttrib attrib,
    int value)
{
    std::lock_guard<decltype(mutex)> lock(mutex);
    switch (attrib)
    {
    case mir_surface_attrib_state:
    {
        auto const state = policy->handle_set_state(info_for(surface), MirSurfaceState(value));
        return surface->configure(attrib, state);
    }
    default:
        return surface->configure(attrib, value);
    }
}

auto ma::BasicWindowManager::count_sessions() const
-> unsigned int
{
    return session_info.size();
}


void ma::BasicWindowManager::for_each_session(std::function<void(SessionInfo& info)> const& functor)
{
    for(auto& info : session_info)
    {
        functor(info.second);
    }
}

auto ma::BasicWindowManager::find_session(std::function<bool(SessionInfo const& info)> const& predicate)
-> std::shared_ptr<scene::Session>
{
    for(auto& info : session_info)
    {
        if (predicate(info.second))
        {
            return info.first.lock();
        }
    }

    return std::shared_ptr<scene::Session>{};
}

auto ma::BasicWindowManager::info_for(std::weak_ptr<scene::Session> const& session) const
-> SessionInfo&
{
    return const_cast<SessionInfo&>(session_info.at(session));
}

auto ma::BasicWindowManager::info_for(std::weak_ptr<scene::Surface> const& surface) const
-> SurfaceInfo&
{
    return const_cast<SurfaceInfo&>(surface_info.at(surface));
}

auto ma::BasicWindowManager::info_for(Surface const& surface) const
-> SurfaceInfo&
{
    return info_for(std::weak_ptr<mir::scene::Surface>(surface));
}

auto ma::BasicWindowManager::focused_session() const
-> Session
{
    return Session{this, focus_controller->focused_session() };
}

auto ma::BasicWindowManager::focused_surface() const
-> Surface
{
    auto focussed_surface = focus_controller->focused_surface();
    return focussed_surface ? info_for(focussed_surface).surface :Surface{};
}

void ma::BasicWindowManager::focus_next_session()
{
    focus_controller->focus_next_session();
}

void ma::BasicWindowManager::set_focus_to(Surface const& surface)
{
    focus_controller->set_focus_to(surface.session(), surface);
}

auto ma::BasicWindowManager::surface_at(geometry::Point cursor) const
-> Surface
{
    auto surface_at = focus_controller->surface_at(cursor);
    return surface_at ? info_for(surface_at).surface : Surface{};
}

auto ma::BasicWindowManager::active_display()
-> geometry::Rectangle const
{
    geometry::Rectangle result;

    // 1. If a window has input focus, whichever display contains the largest
    //    proportion of the area of that window.
    if (auto const surface = focused_surface())
    {
        auto const surface_rect = surface.input_bounds();
        int max_overlap_area = -1;

        for (auto const& display : displays)
        {
            auto const intersection = surface_rect.intersection_with(display).size;
            if (intersection.width.as_int()*intersection.height.as_int() > max_overlap_area)
            {
                max_overlap_area = intersection.width.as_int()*intersection.height.as_int();
                result = display;
            }
        }
        return result;
    }

    // 2. Otherwise, if any window previously had input focus, for the window that had
    //    it most recently, the display that contained the largest proportion of the
    //    area of that window at the moment it closed, as long as that display is still
    //    available.

    // 3. Otherwise, the display that contains the pointer, if there is one.
    for (auto const& display : displays)
    {
        if (display.contains(cursor))
        {
            // Ignore the (unspecified) possiblity of overlapping displays
            return display;
        }
    }

    // 4. Otherwise, the primary display, if there is one (for example, the laptop display).

    // 5. Otherwise, the first display.
    if (displays.size())
        result = *displays.begin();

    return result;
}

void ma::BasicWindowManager::raise_tree(std::shared_ptr<scene::Surface> const& root)
{
    SurfaceSet surfaces;
    std::function<void(std::weak_ptr<scene::Surface> const& surface)> const add_children =
        [&,this](std::weak_ptr<scene::Surface> const& surface)
            {
            auto const& info = info_for(surface);
            surfaces.insert(begin(info.children), end(info.children));
            for (auto const& child : info.children)
                add_children(child);
            };

    surfaces.insert(root);
    add_children(root);

    focus_controller->raise(surfaces);
}

void ma::BasicWindowManager::update_event_timestamp(MirKeyboardEvent const* kev)
{
    auto iev = mir_keyboard_event_input_event(kev);
    last_input_event_timestamp = mir_input_event_get_event_time(iev);
}

void ma::BasicWindowManager::update_event_timestamp(MirPointerEvent const* pev)
{
    auto iev = mir_pointer_event_input_event(pev);
    auto pointer_action = mir_pointer_event_action(pev);

    if (pointer_action == mir_pointer_action_button_up ||
        pointer_action == mir_pointer_action_button_down)
    {
        last_input_event_timestamp = mir_input_event_get_event_time(iev);
    }
}

void ma::BasicWindowManager::update_event_timestamp(MirTouchEvent const* tev)
{
    auto iev = mir_touch_event_input_event(tev);
    auto touch_count = mir_touch_event_point_count(tev);
    for (unsigned i = 0; i < touch_count; i++)
    {
        auto touch_action = mir_touch_event_action(tev, i);
        if (touch_action == mir_touch_action_up ||
            touch_action == mir_touch_action_down)
        {
            last_input_event_timestamp = mir_input_event_get_event_time(iev);
            break;
        }
    }
}
