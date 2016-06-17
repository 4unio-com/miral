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

#ifndef MIRAL_TOOLKIT_CONNECTION_H
#define MIRAL_TOOLKIT_CONNECTION_H

#include <mir_toolkit/mir_connection.h>

#include <memory>

namespace miral
{
namespace toolkit
{
class Connection
{
public:
    Connection() = default;
    explicit Connection(MirConnection* connection) : self{connection, deleter} {}

    operator MirConnection*() const { return self.get(); }

private:
    static void deleter(MirConnection* connection) { mir_connection_release(connection); }
    std::shared_ptr<MirConnection> self;
};
}
}

#endif //MIRAL_TOOLKIT_CONNECTION_H
