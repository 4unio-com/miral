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

#include "miral/window_info.h"

#include <mir/scene/surface.h>
#include <mir/scene/surface_creation_parameters.h>

namespace ms = mir::scene;
namespace mg = mir::graphics;
using namespace mir::geometry;

miral::WindowInfo::WindowInfo(
    Window const& window,
    ms::SurfaceCreationParameters const& params) :
    window{window},
    type{params.type.is_set() ? params.type.value() : mir_surface_type_normal},
    state{params.state.is_set() ? params.state.value() : mir_surface_state_restored},
    restore_rect{window.top_left(), window.size()},
    min_width{params.min_width.is_set() ? params.min_width.value()  : Width{5}},
    min_height{params.min_height.is_set() ? params.min_height.value() : Height{5}},
    max_width{params.max_width.is_set() ? params.max_width.value() : Width{std::numeric_limits<int>::max()}},
    max_height{params.max_height.is_set() ? params.max_height.value() : Height{std::numeric_limits<int>::max()}},
    width_inc{params.width_inc},
    height_inc{params.height_inc},
    min_aspect{},
    max_aspect{}
{
    if (min_aspect.is_set())
        min_aspect = AspectRatio{params.min_aspect.value().width, params.min_aspect.value().height};

    if (max_aspect.is_set())
        max_aspect = AspectRatio{params.max_aspect.value().width, params.max_aspect.value().height};

    if (params.output_id != mir::graphics::DisplayConfigurationOutputId{0})
        output_id = params.output_id.as_value();
}

bool miral::WindowInfo::can_be_active() const
{
    switch (type)
    {
    case mir_surface_type_normal:       /**< AKA "regular"                       */
    case mir_surface_type_utility:      /**< AKA "floating"                      */
    case mir_surface_type_dialog:
    case mir_surface_type_satellite:    /**< AKA "toolbox"/"toolbar"             */
    case mir_surface_type_freestyle:
    case mir_surface_type_menu:
    case mir_surface_type_inputmethod:  /**< AKA "OSK" or handwriting etc.       */
        return true;

    case mir_surface_type_gloss:
    case mir_surface_type_tip:          /**< AKA "tooltip"                       */
    default:
        // Cannot have input focus
        return false;
    }
}

bool miral::WindowInfo::must_have_parent() const
{
    switch (type)
    {
    case mir_surface_type_overlay:;
    case mir_surface_type_inputmethod:
    case mir_surface_type_satellite:
    case mir_surface_type_tip:
        return true;

    default:
        return false;
    }
}

bool miral::WindowInfo::can_morph_to(MirSurfaceType new_type) const
{
    switch (new_type)
    {
    case mir_surface_type_normal:
    case mir_surface_type_utility:
    case mir_surface_type_satellite:
        switch (type)
        {
        case mir_surface_type_normal:
        case mir_surface_type_utility:
        case mir_surface_type_dialog:
        case mir_surface_type_satellite:
            return true;

        default:
            break;
        }
        break;

    case mir_surface_type_dialog:
        switch (type)
        {
        case mir_surface_type_normal:
        case mir_surface_type_utility:
        case mir_surface_type_dialog:
        case mir_surface_type_popover:
        case mir_surface_type_satellite:
            return true;

        default:
            break;
        }
        break;

    default:
        break;
    }

    return false;
}

bool miral::WindowInfo::must_not_have_parent() const
{
    switch (type)
    {
    case mir_surface_type_normal:
    case mir_surface_type_utility:
        return true;

    default:
        return false;
    }
}

bool miral::WindowInfo::is_visible() const
{
    switch (state)
    {
    case mir_surface_state_hidden:
    case mir_surface_state_minimized:
        return false;
    default:
        break;
    }
    return true;
}
void miral::WindowInfo::constrain_resize(Point& requested_pos, Size& requested_size) const
{
    bool const left_resize = requested_pos.x != window.top_left().x;
    bool const top_resize  = requested_pos.y != window.top_left().y;

    Point new_pos = requested_pos;
    Size new_size = requested_size;

    if (min_aspect.is_set())
    {
        auto const ar = min_aspect.value();

        auto const error = new_size.height.as_int()*long(ar.width) - new_size.width.as_int()*long(ar.height);

        if (error > 0)
        {
            // Add (denominator-1) to numerator to ensure rounding up
            auto const width_correction  = (error+(ar.height-1))/ar.height;
            auto const height_correction = (error+(ar.width-1))/ar.width;

            if (width_correction < height_correction)
            {
                new_size.width = new_size.width + DeltaX(width_correction);
            }
            else
            {
                new_size.height = new_size.height - DeltaY(height_correction);
            }
        }
    }

    if (max_aspect.is_set())
    {
        auto const ar = max_aspect.value();

        auto const error = new_size.width.as_int()*long(ar.height) - new_size.height.as_int()*long(ar.width);

        if (error > 0)
        {
            // Add (denominator-1) to numerator to ensure rounding up
            auto const height_correction = (error+(ar.width-1))/ar.width;
            auto const width_correction  = (error+(ar.height-1))/ar.height;

            if (width_correction < height_correction)
            {
                new_size.width = new_size.width - DeltaX(width_correction);
            }
            else
            {
                new_size.height = new_size.height + DeltaY(height_correction);
            }
        }
    }

    if (min_width > new_size.width)
        new_size.width = min_width;

    if (min_height > new_size.height)
        new_size.height = min_height;

    if (max_width < new_size.width)
        new_size.width = max_width;

    if (max_height < new_size.height)
        new_size.height = max_height;

    if (width_inc.is_set())
    {
        auto const width = new_size.width.as_int() - min_width.as_int();
        auto inc = width_inc.value().as_int();
        if (width % inc)
            new_size.width = min_width + DeltaX{inc*(((2L*width + inc)/2)/inc)};
    }

    if (height_inc.is_set())
    {
        auto const height = new_size.height.as_int() - min_height.as_int();
        auto inc = height_inc.value().as_int();
        if (height % inc)
            new_size.height = min_height + DeltaY{inc*(((2L*height + inc)/2)/inc)};
    }

    if (left_resize)
        new_pos.x += new_size.width - requested_size.width;

    if (top_resize)
        new_pos.y += new_size.height - requested_size.height;

    // placeholder - constrain onscreen

    switch (state)
    {
    case mir_surface_state_restored:
        break;

        // "A vertically maximised window is anchored to the top and bottom of
        // the available workspace and can have any width."
    case mir_surface_state_vertmaximized:
        new_pos.y = window.top_left().y;
        new_size.height = window.size().height;
        break;

        // "A horizontally maximised window is anchored to the left and right of
        // the available workspace and can have any height"
    case mir_surface_state_horizmaximized:
        new_pos.x = window.top_left().x;
        new_size.width = window.size().width;
        break;

        // "A maximised window is anchored to the top, bottom, left and right of the
        // available workspace. For example, if the launcher is always-visible then
        // the left-edge of the window is anchored to the right-edge of the launcher."
    case mir_surface_state_maximized:
    default:
        new_pos.x = window.top_left().x;
        new_pos.y = window.top_left().y;
        new_size.width = window.size().width;
        new_size.height = window.size().height;
        break;
    }

    requested_pos = new_pos;
    requested_size = new_size;
}

bool miral::WindowInfo::needs_titlebar(MirSurfaceType type)
{
    switch (type)
    {
    case mir_surface_type_freestyle:
    case mir_surface_type_menu:
    case mir_surface_type_inputmethod:
    case mir_surface_type_gloss:
    case mir_surface_type_tip:
        // No decorations for these surface types
        return false;
    default:
        return true;
    }
}
