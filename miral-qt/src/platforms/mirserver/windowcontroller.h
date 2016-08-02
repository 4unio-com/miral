/*
 * Copyright (C) 2016 Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef WINDOWCONTROLLER_H
#define WINDOWCONTROLLER_H

#include "windowcontrollerinterface.h"

class WindowManagementPolicy;

namespace qtmir {

class WindowController : public WindowControllerInterface
{
public:
    WindowController();
    virtual ~WindowController() = default;

    void focus (const miral::Window &window);
    void resize(const miral::Window &window, const mir::geometry::Size size);
    void move  (const miral::Window &window, const mir::geometry::Point topLeft);

    void deliver_keyboard_event(const MirKeyboardEvent *event, const miral::Window &window);
    void deliver_touch_event   (const MirTouchEvent *event,    const miral::Window &window);
    void deliver_pointer_event (const MirPointerEvent *event,  const miral::Window &window);

    void setPolicy(WindowManagementPolicy *policy);

protected:
    WindowManagementPolicy *m_policy;
};

} // namespace qtmir

#endif // WINDOWCONTROLLER_H
