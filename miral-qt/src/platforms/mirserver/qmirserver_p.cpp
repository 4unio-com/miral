/*
 * Copyright (C) 2015 Canonical, Ltd.
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

#include "qmirserver_p.h"

// Mir
#include <mir/main_loop.h>

// local
#include "logging.h"
#include "mirdisplayconfigurationpolicy.h"
#include "mirserver.h"
#include "promptsessionlistener.h"
#include "sessionlistener.h"
#include "sessionauthorizer.h"
#include "windowmanagementpolicy.h"

void MirServerThread::run()
{
    // This should eventually be replaced by miral::MirRunner::run()
    server->m_usingQtMirSessionAuthorizer(*server->server);
    server->m_usingQtMirSessionListener(*server->server);
    server->m_usingQtMirPromptSessionListener(*server->server);
    server->m_usingQtMirWindowManager(*server->server);
    mir_display_configuration_policy(*server->server);

    try {
        server->server->apply_settings();
    } catch (const std::exception &ex) {
        qCritical() << ex.what();
        exit(1);
    }

    auto const main_loop = server->server->the_main_loop();
    // By enqueuing the notification code in the main loop, we are
    // ensuring that the server has really and fully started before
    // leaving wait_for_startup().
    main_loop->enqueue(
        this,
        [&]
        {
            std::lock_guard<std::mutex> lock(mutex);
            mir_running = true;
            started_cv.notify_one();
        });

    server->server->run(); // blocks until Mir server stopped
    Q_EMIT stopped();
}

void MirServerThread::stop()
{
    server->server->stop();
}

bool MirServerThread::waitForMirStartup()
{
    std::unique_lock<decltype(mutex)> lock(mutex);
    started_cv.wait_for(lock, std::chrono::seconds{10}, [&]{ return mir_running; });
    return mir_running;
}

void UsingQtMirSessionListener::operator()(mir::Server& server)
{
    server.override_the_session_listener([this]
        {
            auto const result = std::make_shared<SessionListener>();
            m_sessionListener = result;
            return result;
        });
}

SessionListener *UsingQtMirSessionListener::sessionListener()
{
    return m_sessionListener.lock().get();
}

void UsingQtMirPromptSessionListener::operator()(mir::Server& server)
{
    server.override_the_prompt_session_listener([this]
        {
            auto const result = std::make_shared<PromptSessionListener>();
            m_promptSessionListener = result;
            return result;
        });
}

PromptSessionListener *UsingQtMirPromptSessionListener::promptSessionListener()
{
    return m_promptSessionListener.lock().get();
}

UsingQtMirWindowManager::UsingQtMirWindowManager(const QSharedPointer<ScreensModel> &model)
    : m_screensModel(model)
    , m_policy(miral::set_window_managment_policy<WindowManagementPolicy>(m_windowModel, m_windowController, m_screensModel))
{
}

void UsingQtMirWindowManager::operator()(mir::Server& server)
{
    m_policy(server);
}

qtmir::WindowModelInterface *UsingQtMirWindowManager::windowModel()
{
    return &m_windowModel;
}

qtmir::WindowControllerInterface *UsingQtMirWindowManager::windowController()
{
    return &m_windowController;
}

