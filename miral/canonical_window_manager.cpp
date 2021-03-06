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
 * Authored By: Alan Griffiths <alan@octopull.co.uk>
 */

#include "miral/canonical_window_manager.h"

#include "miral/window_info.h"
#include "miral/window_manager_tools.h"

namespace ms = mir::scene;

// Based on "Mir and Unity: Surfaces, input, and displays (v0.3)"

miral::CanonicalWindowManagerPolicy::CanonicalWindowManagerPolicy(WindowManagerTools const& tools) :
    tools{tools}
{
}

#ifndef __clang__
extern "C" __attribute__((alias("_ZN5miral28CanonicalWindowManagerPolicy16place_new_windowERKNS_15ApplicationInfoERKNS_19WindowSpecificationE"))) void _ZN5miral28CanonicalWindowManagerPolicy17place_new_surfaceERKNS_15ApplicationInfoERKNS_19WindowSpecificationE();
__asm__(".symver _ZN5miral28CanonicalWindowManagerPolicy17place_new_surfaceERKNS_15ApplicationInfoERKNS_19WindowSpecificationE,_ZN5miral28CanonicalWindowManagerPolicy17place_new_surfaceERKNS_15ApplicationInfoERKNS_19WindowSpecificationE@MIRAL_1.0");
#else
__asm__(".symver _ZN5miral28CanonicalWindowManagerPolicy16place_new_windowERKNS_15ApplicationInfoERKNS_19WindowSpecificationE,_ZN5miral28CanonicalWindowManagerPolicy17place_new_surfaceERKNS_15ApplicationInfoERKNS_19WindowSpecificationE@MIRAL_1.0");
__asm__(".symver _ZN5miral28CanonicalWindowManagerPolicy16place_new_windowERKNS_15ApplicationInfoERKNS_19WindowSpecificationE,_ZN5miral28CanonicalWindowManagerPolicy16place_new_windowERKNS_15ApplicationInfoERKNS_19WindowSpecificationE@@@MIRAL_1.1");
#endif
auto miral::CanonicalWindowManagerPolicy::place_new_window(
    miral::ApplicationInfo const& /*app_info*/,
    miral::WindowSpecification const& request_parameters)
    -> miral::WindowSpecification
{
    return request_parameters;
}

void miral::CanonicalWindowManagerPolicy::handle_window_ready(WindowInfo& window_info)
{
    if (window_info.can_be_active())
    {
        tools.select_active_window(window_info.window());
    }
}

void miral::CanonicalWindowManagerPolicy::handle_modify_window(
    WindowInfo& window_info,
    WindowSpecification const& modifications)
{
    tools.modify_window(window_info, modifications);
}

void miral::CanonicalWindowManagerPolicy::handle_raise_window(WindowInfo& window_info)
{
    tools.select_active_window(window_info.window());
}

void miral::CanonicalWindowManagerPolicy::advise_focus_gained(WindowInfo const& info)
{
    tools.raise_tree(info.window());
}

auto miral::CanonicalWindowManagerPolicy::confirm_inherited_move(WindowInfo const& window_info, Displacement movement)
-> Rectangle
{
    return {window_info.window().top_left()+movement, window_info.window().size()};
}
