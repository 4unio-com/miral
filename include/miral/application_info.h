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

#ifndef MIRAL_APPLICATION_INFO_H
#define MIRAL_APPLICATION_INFO_H

#include "miral/window.h"

#include <vector>


namespace miral
{
// TODO "Opaquify ApplicationInfo to provide a stable API
struct ApplicationInfo
{
    std::vector<Window> windows;

    /// This can be used by client code to store window manager specific information
    std::shared_ptr<void> userdata;
};
}

#endif //MIRAL_APPLICATION_INFO_H