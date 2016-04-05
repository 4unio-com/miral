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

#include "miral/runner.h"

#include <mir/server.h>
#include <mir/report_exception.h>

namespace
{
inline auto filename(std::string path) -> std::string
{
    return path.substr(path.rfind('/')+1);
}
}

miral::MirRunner::MirRunner(int argc, char const* argv[]) :
    argc(argc), argv(argv), config_file(filename(argv[0]) + ".config")
{
}

miral::MirRunner::MirRunner(int argc, char const* argv[], char const* config_file) :
    argc(argc), argv(argv), config_file{config_file}
{
}

auto miral::MirRunner::run_with(std::initializer_list<std::function<void(::mir::Server&)>> options)
-> int
try
{
    mir::Server server;

    server.set_config_filename(config_file);

    for (auto& option : options)
        option(server);

    // Provide the command line and run the server
    server.set_command_line(argc, argv);
    server.apply_settings();
    server.run();

    return server.exited_normally() ? EXIT_SUCCESS : EXIT_FAILURE;
}
catch (...)
{
    mir::report_exception();
    return EXIT_FAILURE;
}
