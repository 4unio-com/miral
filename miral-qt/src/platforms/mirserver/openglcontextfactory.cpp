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

#include "openglcontextfactory.h"

// local
#include "mirglconfig.h"
#include "miropenglcontext.h"

// mir
#include <mir/server.h>

struct qtmir::OpenGLContextFactory::Self
{
    std::shared_ptr<mir::graphics::GLConfig> m_mirGLConfig;
};

qtmir::OpenGLContextFactory::OpenGLContextFactory() :
    self{std::make_shared<Self>()}
{}

void qtmir::OpenGLContextFactory::operator()(mir::Server& server)
{
    server.override_the_gl_config([this]
        {
            auto result = std::make_shared<MirGLConfig>();
            self->m_mirGLConfig = result;
            return result;
        });


}

QPlatformOpenGLContext *qtmir::OpenGLContextFactory::createPlatformOpenGLContext(QSurfaceFormat format, mir::graphics::Display &mirDisplay) const
{
    return new MirOpenGLContext(mirDisplay, *self->m_mirGLConfig, format);

}
