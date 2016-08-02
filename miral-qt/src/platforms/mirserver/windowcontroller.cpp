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

#include "windowcontroller.h"

#include "windowmanagementpolicy.h"

using namespace qtmir;


WindowController::WindowController()
    : m_policy(nullptr)
{
}

void WindowController::focus(const miral::Window &window)
{
    if (m_policy) {
        m_policy->focus(window);
    }
}

void WindowController::resize(const miral::Window &window, const mir::geometry::Size size)
{
    if (m_policy) {
        m_policy->resize(window, size);
    }
}

void WindowController::move(const miral::Window &window, const mir::geometry::Point topLeft)
{
    if (m_policy) {
        m_policy->move(window, topLeft);
    }
}

void WindowController::deliver_keyboard_event(const MirKeyboardEvent *event, const miral::Window &window)
{
    if (m_policy) {
        m_policy->deliver_keyboard_event(event, window);
    }
}

void WindowController::deliver_touch_event(const MirTouchEvent *event, const miral::Window &window)
{
    if (m_policy) {
        m_policy->deliver_touch_event(event, window);
    }
}

void WindowController::deliver_pointer_event(const MirPointerEvent *event, const miral::Window &window)
{
    if (m_policy) {
        m_policy->deliver_pointer_event(event, window);
    }
}

void WindowController::setPolicy(WindowManagementPolicy * const policy)
{
    m_policy = policy;
}
