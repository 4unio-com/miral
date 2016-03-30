/*
 * Copyright © 2014 Canonical Ltd.
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

#ifndef MIR_ABSTRACTION_DISPLAY_CONFIGURATION_POLICY_H_
#define MIR_ABSTRACTION_DISPLAY_CONFIGURATION_POLICY_H_

#include "mir/graphics/display_configuration_policy.h"

namespace mir
{
class Server;

namespace al
{
void add_display_configuration_options_to(Server& server);
}
}

#endif /* MIR_ABSTRACTION_DISPLAY_CONFIGURATION_POLICY_H_ */
