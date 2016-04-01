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

#ifndef MIR_ABSTRACTION_WINDOW_MANAGEMENT_INFO_H
#define MIR_ABSTRACTION_WINDOW_MANAGEMENT_INFO_H

#include "miral/surface.h"

#include "mir/geometry/rectangles.h"
#include "mir/optional_value.h"
#include "mir/shell/surface_specification.h"

#include <vector>

namespace miral { class Surface; }

namespace mir
{
namespace scene { class Session; class SurfaceCreationParameters; }
namespace al
{
using ::miral::Surface;

struct SurfaceInfo
{
    SurfaceInfo(Surface const& surface, scene::SurfaceCreationParameters const& params);

    bool can_be_active() const;

    bool can_morph_to(MirSurfaceType new_type) const;

    bool must_have_parent() const;

    bool must_not_have_parent() const;

    bool is_visible() const;

    static bool needs_titlebar(MirSurfaceType type);

    void constrain_resize(geometry::Point& requested_pos, geometry::Size& requested_size) const;

    Surface surface;

    MirSurfaceType type;
    MirSurfaceState state;
    geometry::Rectangle restore_rect;
    Surface parent;
    std::vector <Surface> children;
    geometry::Width min_width;
    geometry::Height min_height;
    geometry::Width max_width;
    geometry::Height max_height;
    mir::optional_value<geometry::DeltaX> width_inc;
    mir::optional_value<geometry::DeltaY> height_inc;
    mir::optional_value<shell::SurfaceAspectRatio> min_aspect;
    mir::optional_value<shell::SurfaceAspectRatio> max_aspect;
    mir::optional_value<graphics::DisplayConfigurationOutputId> output_id;

    /// This can be ised by client code to store window manager specific information
    std::shared_ptr<void> userdata;
};

struct SessionInfo
{
    std::vector<std::weak_ptr<scene::Surface>> surfaces;

    // This is only used by the TilingWindowManagerPolicy,
    // perhaps we need a more extensible mechanism. (std::experimental::any?)
    geometry::Rectangle tile;
};
}
}

#endif //MIR_ABSTRACTION_WINDOW_MANAGEMENT_INFO_H
