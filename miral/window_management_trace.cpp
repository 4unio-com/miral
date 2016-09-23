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

#include "window_management_trace.h"

#include <miral/application_info.h>
#include <miral/window_info.h>

#include <mir/scene/session.h>
#include <mir/scene/surface.h>
#include <mir/event_printer.h>

#include <sstream>

#define MIR_LOG_COMPONENT "miral::Window Management"
#include <mir/log.h>

using mir::operator<<;

namespace 
{
std::string const null_ptr{"(null)"};

struct BracedItemStream
{
    BracedItemStream(std::ostream& out) : out{out} { out << '{'; }
    ~BracedItemStream() { out << '}'; }
    bool mutable first_field = true;
    std::ostream& out;

    template<typename Type>
    auto append(Type const& item) const -> BracedItemStream const&
    {
        if (!first_field) out << ", ";
        out << item;
        first_field = false;
        return *this;
    }

    template<typename Type>
    auto append(char const* name, Type const& item) const -> BracedItemStream const&
    {
        if (!first_field) out << ", ";
        out << name << '=' << item;
        first_field = false;
        return *this;
    }
};

auto operator<< (std::ostream& out, miral::WindowSpecification::AspectRatio const& ratio) -> std::ostream&
{
    BracedItemStream{out}.append(ratio.width).append(ratio.height);
    return out;
}

auto dump_of(miral::Window const& window) -> std::string
{
    if (std::shared_ptr<mir::scene::Surface> surface = window)
        return surface->name();
    else
        return null_ptr;
}

auto dump_of(miral::Application const& application) -> std::string
{
    if (application)
        return application->name();
    else
        return null_ptr;
}

auto dump_of(std::vector<miral::Window> const& windows) -> std::string;

auto dump_of(miral::WindowInfo const& info) -> std::string
{
    std::stringstream out;
    {
        BracedItemStream bout{out};

#define APPEND(field) bout.append(#field, info.field());
        APPEND(name);
        APPEND(type);
        APPEND(state);
        APPEND(restore_rect);
        APPEND(parent);
        bout.append("children", dump_of(info.children()));
        APPEND(min_width);
        APPEND(min_height);
        APPEND(max_width);
        APPEND(max_height);
        APPEND(width_inc);
        APPEND(height_inc);
        APPEND(min_aspect);
        APPEND(max_aspect);
        APPEND(preferred_orientation);

#define APPEND_IF_SET(field) if (info.has_##field()) bout.append(#field, info.field());
        APPEND_IF_SET(output_id);
#undef  APPEND_IF_SET
#undef  APPEND
    }

    return out.str();
}

auto dump_of(miral::WindowSpecification const& specification) -> std::string
{
    std::stringstream out;

    {
        BracedItemStream bout{out};

#define APPEND_IF_SET(field) if (specification.field().is_set()) bout.append(#field, specification.field().value());
        APPEND_IF_SET(name);
        APPEND_IF_SET(type);
        APPEND_IF_SET(top_left);
        APPEND_IF_SET(size);
        APPEND_IF_SET(output_id);
        APPEND_IF_SET(state);
        APPEND_IF_SET(preferred_orientation);
        APPEND_IF_SET(aux_rect);
        APPEND_IF_SET(placement_hints);
        APPEND_IF_SET(window_placement_gravity);
        APPEND_IF_SET(aux_rect_placement_gravity);
        APPEND_IF_SET(aux_rect_placement_offset);
        APPEND_IF_SET(min_width);
        APPEND_IF_SET(min_height);
        APPEND_IF_SET(max_width);
        APPEND_IF_SET(max_height);
        APPEND_IF_SET(width_inc);
        APPEND_IF_SET(height_inc);
        APPEND_IF_SET(min_aspect);
        APPEND_IF_SET(max_aspect);
        if (specification.parent().is_set())
            if (auto const& parent = specification.parent().value().lock())
                bout.append("parent", parent->name());
//        APPEND_IF_SET(input_shape);
//        APPEND_IF_SET(input_mode);
        APPEND_IF_SET(shell_chrome);
        APPEND_IF_SET(top_left);
        APPEND_IF_SET(size);
#undef  APPEND_IF_SET
    }

    return out.str();
}

auto dump_of(std::vector<miral::Window> const& windows) -> std::string
{
    std::stringstream out;

    {
        BracedItemStream bout{out};

        for (auto const& window: windows)
            bout.append(dump_of(window));
    }

    return out.str();
}

auto dump_of(miral::ApplicationInfo const& app_info) -> std::string
{
    std::stringstream out;

    BracedItemStream{out}
        .append("application", dump_of(app_info.application()))
        .append("windows", dump_of(app_info.windows()));

    return out.str();
}
}

miral::WindowManagementTrace::WindowManagementTrace(
    WindowManagerTools const& wrapped,
    WindowManagementPolicyBuilder const& builder) :
    wrapped{wrapped},
    policy(builder(WindowManagerTools{this}))
{
}

auto miral::WindowManagementTrace::count_applications() const -> unsigned int
{
    auto const result = wrapped.count_applications();
    mir::log_info("%s -> %d", __func__, result);
    return result;
}

void miral::WindowManagementTrace::for_each_application(std::function<void(miral::ApplicationInfo&)> const& functor)
{
    mir::log_info("%s", __func__);
    wrapped.for_each_application(functor);
}

auto miral::WindowManagementTrace::find_application(std::function<bool(ApplicationInfo const& info)> const& predicate)
-> Application
{
    auto result = wrapped.find_application(predicate);
    mir::log_info("%s -> %s", __func__, dump_of(result).c_str());
    return result;
}

auto miral::WindowManagementTrace::info_for(std::weak_ptr<mir::scene::Session> const& session) const -> ApplicationInfo&
{

    auto& result = wrapped.info_for(session);
    mir::log_info("%s -> %s", __func__, result.application()->name().c_str());
    return result;
}

auto miral::WindowManagementTrace::info_for(std::weak_ptr<mir::scene::Surface> const& surface) const -> WindowInfo&
{
    auto& result = wrapped.info_for(surface);
    mir::log_info("%s -> %s", __func__, result.name().c_str());
    return result;
}

auto miral::WindowManagementTrace::info_for(Window const& window) const -> WindowInfo&
{
    auto& result = wrapped.info_for(window);
    mir::log_info("%s -> %s", __func__, result.name().c_str());
    return result;
}

void miral::WindowManagementTrace::ask_client_to_close(miral::Window const& window)
{
    mir::log_info("%s -> %s", __func__, dump_of(window).c_str());
    wrapped.ask_client_to_close(window);
}

auto miral::WindowManagementTrace::active_window() const -> Window
{
    auto result = wrapped.active_window();
    mir::log_info("%s -> %s", __func__, dump_of(result).c_str());
    return result;
}

auto miral::WindowManagementTrace::select_active_window(Window const& hint) -> Window
{
    auto result = wrapped.select_active_window(hint);
    mir::log_info("%s hint=%s -> %s", __func__, dump_of(hint).c_str(), dump_of(result).c_str());
    return result;
}

auto miral::WindowManagementTrace::window_at(mir::geometry::Point cursor) const -> Window
{
    auto result = wrapped.window_at(cursor);
    std::stringstream out;
    out << cursor << " -> " << dump_of(result);
    mir::log_info("%s cursor=%s", __func__, out.str().c_str());
    return result;
}

auto miral::WindowManagementTrace::active_display() -> mir::geometry::Rectangle const
{
    auto result = wrapped.active_display();
    std::stringstream out;
    out << result;
    mir::log_info("%s -> ", __func__, out.str().c_str());
    return result;
}

auto miral::WindowManagementTrace::info_for_window_id(std::string const& id) const -> WindowInfo&
{
    auto& result = wrapped.info_for_window_id(id);
    mir::log_info("%s id=%s -> %s", __func__, id.c_str(), dump_of(result).c_str());
    return result;
}

auto miral::WindowManagementTrace::id_for_window(Window const& window) const -> std::string
{
    auto result = wrapped.id_for_window(window);
    mir::log_info("%s window=%s -> %s", __func__, dump_of(window).c_str(), result.c_str());
    return result;
}

void miral::WindowManagementTrace::drag_active_window(mir::geometry::Displacement movement)
{
    std::stringstream out;
    out << movement;
    mir::log_info("%s movement=%s", __func__, out.str().c_str());
    wrapped.drag_active_window(movement);
}

void miral::WindowManagementTrace::focus_next_application()
{
    mir::log_info("%s", __func__);
    wrapped.focus_next_application();
}

void miral::WindowManagementTrace::focus_next_within_application()
{
    mir::log_info("%s", __func__);
    wrapped.focus_next_within_application();
}

void miral::WindowManagementTrace::raise_tree(miral::Window const& root)
{
    mir::log_info("%s root=%s", __func__, dump_of(root).c_str());
    wrapped.raise_tree(root);
}

void miral::WindowManagementTrace::modify_window(
    miral::WindowInfo& window_info, miral::WindowSpecification const& modifications)
{
    mir::log_info("%s window_info=%s, modifications=%s", 
                  __func__, dump_of(window_info).c_str(), dump_of(modifications).c_str());
    
    wrapped.modify_window(window_info, modifications);
}

void miral::WindowManagementTrace::invoke_under_lock(std::function<void()> const& callback)
{
    mir::log_info("%s", __func__);
    wrapped.invoke_under_lock(callback);
}

auto miral::WindowManagementTrace::place_new_surface(
    ApplicationInfo const& app_info,
    WindowSpecification const& requested_specification) -> WindowSpecification
{
    auto const result = policy->place_new_surface(app_info, requested_specification);
    mir::log_info("%s app_info=%s, requested_specification=%s -> %s",
              __func__, dump_of(app_info).c_str(), dump_of(requested_specification).c_str(), dump_of(result).c_str());

    return result;
}

void miral::WindowManagementTrace::handle_window_ready(miral::WindowInfo& window_info)
{
    mir::log_info("%s window_info=%s", __func__, dump_of(window_info).c_str());
    policy->handle_window_ready(window_info);
}

void miral::WindowManagementTrace::handle_modify_window(
    miral::WindowInfo& window_info, miral::WindowSpecification const& modifications)
{
    mir::log_info("%s window_info=%s, modifications=%s",
                  __func__, dump_of(window_info).c_str(), dump_of(modifications).c_str());

    policy->handle_modify_window(window_info, modifications);
}

void miral::WindowManagementTrace::handle_raise_window(miral::WindowInfo& window_info)
{
    mir::log_info("%s window_info=%s", __func__, dump_of(window_info).c_str());
    policy->handle_raise_window(window_info);
}

bool miral::WindowManagementTrace::handle_keyboard_event(MirKeyboardEvent const* event)
{
    mir::log_debug("%s", __func__);
    return policy->handle_keyboard_event(event);
}

bool miral::WindowManagementTrace::handle_touch_event(MirTouchEvent const* event)
{
    mir::log_debug("%s", __func__);
    return policy->handle_touch_event(event);
}

bool miral::WindowManagementTrace::handle_pointer_event(MirPointerEvent const* event)
{
    mir::log_debug("%s", __func__);
    return policy->handle_pointer_event(event);
}

void miral::WindowManagementTrace::advise_begin()
{
    mir::log_info("%s", __func__);
    policy->advise_begin();
}

void miral::WindowManagementTrace::advise_end()
{
    mir::log_info("%s", __func__);
    policy->advise_end();
}

void miral::WindowManagementTrace::advise_new_app(miral::ApplicationInfo& application)
{
    mir::log_info("%s application=%s", __func__, dump_of(application).c_str());
    policy->advise_new_app(application);
}

void miral::WindowManagementTrace::advise_delete_app(miral::ApplicationInfo const& application)
{
    mir::log_info("%s application=%s", __func__, dump_of(application).c_str());
    policy->advise_delete_app(application);
}

void miral::WindowManagementTrace::advise_new_window(miral::WindowInfo const& window_info)
{
    mir::log_info("%s window_info=%s", __func__, dump_of(window_info).c_str());
    policy->advise_new_window(window_info);
}

void miral::WindowManagementTrace::advise_focus_lost(miral::WindowInfo const& window_info)
{
    mir::log_info("%s window_info=%s", __func__, dump_of(window_info).c_str());
    policy->advise_focus_lost(window_info);
}

void miral::WindowManagementTrace::advise_focus_gained(miral::WindowInfo const& window_info)
{
    mir::log_info("%s window_info=%s", __func__, dump_of(window_info).c_str());
    policy->advise_focus_gained(window_info);
}

void miral::WindowManagementTrace::advise_state_change(miral::WindowInfo const& window_info, MirSurfaceState state)
{
    mir::log_info("%s window_info=%s", __func__, dump_of(window_info).c_str());
    policy->advise_state_change(window_info, state);
}

void miral::WindowManagementTrace::advise_move_to(miral::WindowInfo const& window_info, mir::geometry::Point top_left)
{
    mir::log_info("%s window_info=%s", __func__, dump_of(window_info).c_str());
    policy->advise_move_to(window_info, top_left);
}

void miral::WindowManagementTrace::advise_resize(miral::WindowInfo const& window_info, mir::geometry::Size const& new_size)
{
    mir::log_info("%s window_info=%s", __func__, dump_of(window_info).c_str());
    policy->advise_resize(window_info, new_size);
}

void miral::WindowManagementTrace::advise_delete_window(miral::WindowInfo const& window_info)
{
    mir::log_info("%s window_info=%s", __func__, dump_of(window_info).c_str());
    policy->advise_delete_window(window_info);
}

void miral::WindowManagementTrace::advise_raise(std::vector<miral::Window> const& windows)
{
    mir::log_info("%s window_info=%s", __func__, dump_of(windows).c_str());
    policy->advise_raise(windows);
}