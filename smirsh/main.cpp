/*
 * Copyright © 2016 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
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

#include "tiling_window_manager.h"
#include "canonical_window_manager.h"

#include "miral/display_configuration_option.h"
#include "miral/runner.h"
#include "miral/window_management_options.h"
#include "miral/quit_on_ctrl_alt_bksp.h"

namespace
{
inline auto filename(std::string path) -> std::string
{
    return path.substr(path.find('/')+1);
}
}

int main(int argc, char const* argv[])
{
    auto const config_file = filename(argv[0]) + ".config";

    using namespace miral;

    MirRunner runner(argc, argv, config_file.c_str());

    return runner.run_with(
        {
            WindowManagerOptions
            {
                add_window_manager_policy<CanonicalWindowManagerPolicy>("canonical"),
                add_window_manager_policy<TilingWindowManagerPolicy>("tiling"),
            },
            display_configuration_options,
            QuitOnCtrlAltBkSp{}
        });
}
