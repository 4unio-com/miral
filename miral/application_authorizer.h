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

#ifndef MIRAL_APPLICATION_AUTHORIZER_H
#define MIRAL_APPLICATION_AUTHORIZER_H

#include <sys/types.h>
#include <memory>

namespace mir { class Server; }
namespace mir { namespace frontend { class SessionCredentials; } }

namespace miral
{
class ApplicationCredentials
{
public:
    ApplicationCredentials(mir::frontend::SessionCredentials const& creds);

    pid_t pid() const;
    uid_t uid() const;
    gid_t gid() const;

private:
    ApplicationCredentials() = delete;

    mir::frontend::SessionCredentials const& creds;
};

class ApplicationAuthorizer
{
public:
    ApplicationAuthorizer() = default;
    virtual ~ApplicationAuthorizer() = default;
    ApplicationAuthorizer(ApplicationAuthorizer const&) = delete;
    ApplicationAuthorizer& operator=(ApplicationAuthorizer const&) = delete;

    virtual bool connection_is_allowed(ApplicationCredentials const& creds) = 0;
    virtual bool configure_display_is_allowed(ApplicationCredentials const& creds) = 0;
    virtual bool set_base_display_configuration_is_allowed(ApplicationCredentials const& creds) = 0;
    virtual bool screencast_is_allowed(ApplicationCredentials const& creds) = 0;
    virtual bool prompt_session_is_allowed(ApplicationCredentials const& creds) = 0;
};

class BasicSetApplicationAuthorizer
{
public:
    BasicSetApplicationAuthorizer(std::function<std::shared_ptr<ApplicationAuthorizer>()> const& builder);
    ~BasicSetApplicationAuthorizer();

    void operator()(mir::Server& server);
    auto the_application_authorizer() const -> std::shared_ptr<ApplicationAuthorizer>;

private:
    std::function<std::shared_ptr<ApplicationAuthorizer>()> const builder;
    std::weak_ptr<ApplicationAuthorizer> my_authorizer;
};

template<typename Policy>
class SetApplicationAuthorizer : public BasicSetApplicationAuthorizer
{
public:
    template<typename ...Args>
    SetApplicationAuthorizer(Args& ... args) :
        BasicSetApplicationAuthorizer{[this, &args...]() -> std::shared_ptr<ApplicationAuthorizer>
            { return std::make_shared<Policy>(args...); }} {}

    auto the_custom_application_authorizer() const -> std::shared_ptr<Policy>
        { return std::static_pointer_cast<Policy>(the_application_authorizer()); }
};
}

#endif //MIRAL_APPLICATION_AUTHORIZER_H
