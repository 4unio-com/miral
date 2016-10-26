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

#include "titlebar_provider.h"

#include <miral/toolkit/surface_spec.h>

#include <mir_toolkit/mir_buffer_stream.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cstring>
#include <sstream>

namespace
{
int const title_bar_height = 10;

void null_surface_callback(MirSurface*, void*) {}

struct Printer
{
    Printer();
    ~Printer();
    Printer(Printer const&) = delete;
    Printer& operator=(Printer const&) = delete;

    void print(MirGraphicsRegion const& region, std::wstring const& title, int const intensity);

private:
    bool working = false;
    FT_Library lib;
    FT_Face face;
};

void paint_surface(MirSurface* surface, std::string const& title, int const intensity)
{
    MirBufferStream* buffer_stream = mir_surface_get_buffer_stream(surface);

    // TODO sometimes buffer_stream is nullptr - find out why (and fix).
    // (Only observed when creating a lot of clients at once)
    if (!buffer_stream)
        return;

    MirGraphicsRegion region;
    mir_buffer_stream_get_graphics_region(buffer_stream, &region);

    char* row = region.vaddr;

    for (int j = 0; j != region.height; ++j)
    {
        memset(row, intensity, 4*region.width);
        row += region.stride;
    }

    static Printer printer;
    printer.print(region, std::wstring{begin(title), end(title)}, intensity);

    mir_buffer_stream_swap_buffers_sync(buffer_stream);
}

char const* const fontlist[] = {
    "/usr/share/fonts/truetype/ubuntu-font-family/Ubuntu-B.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
    nullptr // Sentinel value
};

Printer::Printer()
{
    if (FT_Init_FreeType(&lib))
        return;

    for (auto font : fontlist)
    {
        if (!font) // hit the sentinel? We're done!
        {
            FT_Done_FreeType(lib);
            return;
        }

        if (!FT_New_Face(lib, font, 0, &face))
        {
            puts(font);
            break;
        }
    }

    FT_Set_Pixel_Sizes(face, 0, 8);
    working = true;
}

Printer::~Printer()
{
    if (working)
    {
        FT_Done_Face(face);
        FT_Done_FreeType(lib);
    }
}

void Printer::print(MirGraphicsRegion const& region, std::wstring const& title, int const intensity)
{
    if (!working)
        return;

    int base_x = 2;
    int base_y = region.height-2;

    for (auto const& ch : title)
    {
        FT_Load_Glyph(face, FT_Get_Char_Index(face, ch), FT_LOAD_DEFAULT);
        auto const glyph = face->glyph;
        FT_Render_Glyph(glyph, FT_RENDER_MODE_NORMAL);

        auto const& bitmap = glyph->bitmap;
        auto const x = base_x + glyph->bitmap_left;

        if (static_cast<int>(x + bitmap.width) <= region.width)
        {
            unsigned char* src = bitmap.buffer;

            auto const y = base_y - glyph->bitmap_top;
            char* dest = region.vaddr + y*region.stride + 4*x;

            for (auto row = 0u; row != bitmap.rows; ++row)
            {
                for (auto col = 0u; col != bitmap.width; ++col)
                    memset(dest+ 4*col, intensity^src[col], 4);

                src += bitmap.pitch;
                dest += region.stride;
            }
        }

        base_x += glyph->advance.x >> 6;
        base_y += glyph->advance.y >> 6;
    }
}
}

using namespace miral::toolkit;
using namespace mir::geometry;

TitlebarProvider::TitlebarProvider(miral::WindowManagerTools const& tools) : tools{tools}
{

}

TitlebarProvider::~TitlebarProvider()
{
    stop();
}

void TitlebarProvider::stop()
{
    enqueue_work([this]
        {
            std::lock_guard<decltype(mutex)> lock{mutex};
            window_to_titlebar.clear();
        });

    enqueue_work([this]
        {
            connection.reset();
            stop_work();
        });
}

void TitlebarProvider::operator()(miral::toolkit::Connection connection)
{
    this->connection = connection;
    start_work();
}

void TitlebarProvider::operator()(std::weak_ptr<mir::scene::Session> const& session)
{
    std::lock_guard<decltype(mutex)> lock{mutex};
    this->weak_session = session;
}

auto TitlebarProvider::session() const -> std::shared_ptr<mir::scene::Session>
{
    std::lock_guard<decltype(mutex)> lock{mutex};
    return weak_session.lock();
}

void TitlebarProvider::create_titlebar_for(miral::Window const& window)
{
    enqueue_work([this, window]
        {
            std::ostringstream buffer;

            buffer << std::shared_ptr<mir::scene::Surface>(window).get();

            auto const spec = SurfaceSpec::for_normal_surface(
                connection, window.size().width.as_int(), title_bar_height, mir_pixel_format_xrgb_8888)
                .set_buffer_usage(mir_buffer_usage_software)
                .set_type(mir_surface_type_gloss)
                .set_name(buffer.str().c_str());

            std::lock_guard<decltype(mutex)> lock{mutex};
            windows_awaiting_titlebar[buffer.str()] = window;
            spec.create_surface(insert, &window_to_titlebar[window]);
        });
}

void TitlebarProvider::paint_titlebar_for(miral::WindowInfo const& info, int intensity)
{
    if (auto data = find_titlebar_data(info.window()))
    {
        auto const title = info.name();

        if (auto surface = data->titlebar.load())
        {
            enqueue_work([this, surface, title, intensity]{ paint_surface(surface, title, intensity); });
        }
        else
        {
            data->on_create = [this, title, intensity](MirSurface* surface)
                { enqueue_work([this, surface, title, intensity]{ paint_surface(surface, title, intensity); }); };
        }
    }
}

void TitlebarProvider::destroy_titlebar_for(miral::Window const& window)
{
    if (auto data = find_titlebar_data(window))
    {
        if (auto surface = data->titlebar.exchange(nullptr))
        {
            enqueue_work([surface]
                 {
                     mir_surface_release(surface, &null_surface_callback, nullptr);
                 });
        }

        enqueue_work([this, window]
            {
                std::lock_guard<decltype(mutex)> lock{mutex};
                window_to_titlebar.erase(window);
            });
    }
}

void TitlebarProvider::resize_titlebar_for(miral::Window const& window, Size const& size)
{
    if (window.size().width == size.width)
        return;

    if (auto titlebar_window = find_titlebar_window(window))
    {
        titlebar_window.resize({size.width, title_bar_height});
    }
}

void TitlebarProvider::place_new_titlebar(miral::WindowSpecification& window_spec)
{
    auto const name = window_spec.name().value();

    std::lock_guard<decltype(mutex)> lock{mutex};

    auto const scene_surface = windows_awaiting_titlebar[name].lock();
    windows_awaiting_titlebar.erase(name);

    auto& parent_info = tools.info_for(scene_surface);
    auto const parent_window = parent_info.window();

    window_spec.parent() = scene_surface;
    window_spec.size() = Size{parent_window.size().width, Height{title_bar_height}};
    window_spec.top_left() = parent_window.top_left() - Displacement{0, title_bar_height};
}

void TitlebarProvider::advise_new_titlebar(miral::WindowInfo const& window_info)
{
    {
        std::lock_guard<decltype(mutex)> lock{mutex};

        window_to_titlebar[window_info.parent()].window = window_info.window();
    }

    tools.raise_tree(window_info.parent());
}

void TitlebarProvider::advise_state_change(miral::WindowInfo const& window_info, MirSurfaceState state)
{
    if (auto titlebar = find_titlebar_window(window_info.window()))
    {
        miral::WindowSpecification modifications;
        switch (state)
        {
        case mir_surface_state_maximized:
        case mir_surface_state_vertmaximized:
        case mir_surface_state_hidden:
        case mir_surface_state_minimized:
        case mir_surface_state_fullscreen:
            modifications.state() = mir_surface_state_hidden;
            break;

        default:
            modifications.state() = mir_surface_state_restored;
            break;
        }

        tools.modify_window(titlebar, modifications);
    }
}

TitlebarProvider::Data::~Data()
{
    if (auto surface = titlebar.exchange(nullptr))
        mir_surface_release(surface, &null_surface_callback, nullptr);
}

void TitlebarProvider::insert(MirSurface* surface, Data* data)
{
    data->on_create(surface);
    data->titlebar = surface;
}

TitlebarProvider::Data* TitlebarProvider::find_titlebar_data(miral::Window const& window)
{
    std::lock_guard<decltype(mutex)> lock{mutex};

    auto const find = window_to_titlebar.find(window);

    return (find != window_to_titlebar.end()) ? &find->second : nullptr;
}

miral::Window TitlebarProvider::find_titlebar_window(miral::Window const& window) const
{
    std::lock_guard<decltype(mutex)> lock{mutex};

    auto const find = window_to_titlebar.find(window);

    return (find != window_to_titlebar.end()) ? find->second.window : miral::Window{};
}

Worker::~Worker()
{
    if (worker.joinable()) worker.join();
}

void Worker::do_work()
{
    while (!work_done)
    {
        WorkQueue::value_type work;
        {
            std::unique_lock<decltype(work_mutex)> lock{work_mutex};
            work_cv.wait(lock, [this] { return !work_queue.empty(); });
            work = work_queue.front();
            work_queue.pop();
        }

        work();
    }
}

void Worker::enqueue_work(std::function<void()> const& functor)
{
    std::lock_guard<decltype(work_mutex)> lock{work_mutex};
    work_queue.push(functor);
    work_cv.notify_one();
}

void Worker::start_work()
{
    worker = std::thread{[this] { do_work(); }};
}

void Worker::stop_work()
{
    enqueue_work([this] { work_done = true; });
}