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

#include "../miral/basic_window_manager.h"

#include <miral/canonical_window_manager.h>

#include <mir/frontend/surface_id.h>
#include <mir/scene/surface_creation_parameters.h>
#include <mir/shell/display_layout.h>
#include <mir/shell/persistent_surface_store.h>

#include <mir/test/doubles/stub_session.h>
#include <mir/test/doubles/stub_surface.h>
#include <mir/test/fake_shared.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <atomic>

using namespace miral;
using namespace testing;
namespace mt = mir::test;

namespace
{
struct StubFocusController : mir::shell::FocusController
{
    void focus_next_session() override {}

    auto focused_session() const -> std::shared_ptr<mir::scene::Session> override { return {}; }

    void set_focus_to(
        std::shared_ptr<mir::scene::Session> const& /*focus_session*/,
        std::shared_ptr<mir::scene::Surface> const& /*focus_surface*/) override {}

    auto focused_surface() const -> std::shared_ptr<mir::scene::Surface> override { return {}; }

    void raise(mir::shell::SurfaceSet const& /*surfaces*/) override {}

    virtual auto surface_at(mir::geometry::Point /*cursor*/) const -> std::shared_ptr<mir::scene::Surface> override
        { return {}; }
};

struct StubDisplayLayout : mir::shell::DisplayLayout
{
    void clip_to_output(mir::geometry::Rectangle& /*rect*/) override {}

    void size_to_output(mir::geometry::Rectangle& /*rect*/) override {}

    bool place_in_output(mir::graphics::DisplayConfigurationOutputId /*id*/, mir::geometry::Rectangle& /*rect*/) override
        { return false; }
};

struct StubPersistentSurfaceStore : mir::shell::PersistentSurfaceStore
{
    Id id_for_surface(std::shared_ptr<mir::scene::Surface> const& /*surface*/) override { return {}; }

    auto surface_for_id(Id const& /*id*/) const -> std::shared_ptr<mir::scene::Surface> override { return {}; }
};

struct StubSurface : mir::test::doubles::StubSurface
{
    StubSurface(Point top_left, Size size) : top_left_{top_left}, size_{size} {}

    Point top_left() const override { return top_left_; }
    void move_to(Point const& top_left) override { top_left_ = top_left; }

    Size size() const override { return  size_; }
    void resize(Size const& size) override { size_ = size; }

    Point top_left_;
    Size size_;
};

struct StubStubSession : mir::test::doubles::StubSession
{
    mir::frontend::SurfaceId create_surface(
        mir::scene::SurfaceCreationParameters const& params,
        std::shared_ptr<mir::frontend::EventSink> const& /*sink*/) override
    {
        auto id = mir::frontend::SurfaceId{next_surface_id.fetch_add(1)};
        auto surface = std::make_shared<StubSurface>(params.top_left, params.size);
        surfaces[id] = surface;
        return id;
    }

    std::shared_ptr<mir::scene::Surface> surface(mir::frontend::SurfaceId surface) const override
    {
        return surfaces.at(surface);
    }

private:
    std::atomic<int> next_surface_id;
    std::map<mir::frontend::SurfaceId, std::shared_ptr<mir::scene::Surface>> surfaces;
};

struct MockWindowManagerPolicy : CanonicalWindowManagerPolicy
{
    using CanonicalWindowManagerPolicy::CanonicalWindowManagerPolicy;

    bool handle_touch_event(MirTouchEvent const* /*event*/) override { return false; }
    bool handle_pointer_event(MirPointerEvent const* /*event*/) override { return false; }
    bool handle_keyboard_event(MirKeyboardEvent const* /*event*/) override { return false; }

    MOCK_METHOD1(advise_new_window, void (WindowInfo const& window_info));
    MOCK_METHOD2(advise_move_to, void(WindowInfo const& window_info, Point top_left));
};

X const display_left{0};
Y const display_top{0};
Width  const display_width{640};
Height const display_height{480};

Rectangle const display_area{{display_left, display_top}, {display_width, display_height}};

auto const null_window = Window{};

auto create_surface(std::shared_ptr<mir::scene::Session> const& session, mir::scene::SurfaceCreationParameters const& params)
-> mir::frontend::SurfaceId
{
    // This type is Mir-internal, I hope we don't need to create it here
    std::shared_ptr<mir::frontend::EventSink> const sink;

    return session->create_surface(params, sink);
}

mir::shell::SurfaceSpecification edge_attachment(Rectangle const& aux_rect, MirEdgeAttachment attachment)
{
    mir::shell::SurfaceSpecification result;
    result.aux_rect = aux_rect;
    result.edge_attachment = attachment;
    return result;
}

struct WindowPlacement : testing::Test
{
    StubFocusController focus_controller;
    StubDisplayLayout display_layout;
    StubPersistentSurfaceStore persistent_surface_store;
    std::shared_ptr<StubStubSession> session{std::make_shared<StubStubSession>()};

    MockWindowManagerPolicy* window_manager_policy;
    BasicWindowManager basic_window_manager{
        &focus_controller,
        mt::fake_shared(display_layout),
        mt::fake_shared(persistent_surface_store),
        [this](WindowManagerTools const& tools) -> std::unique_ptr<WindowManagementPolicy> 
            {
                auto policy = std::make_unique<MockWindowManagerPolicy>(tools);
                window_manager_policy = policy.get();
                return std::move(policy);
            }
        };

    Size const initial_parent_size{600, 400};
    Size const initial_child_size{300, 300};
    Rectangle const rectangle_away_from_rhs{{20, 20}, {20, 20}};
    Rectangle const rectangle_near_rhs{{600, 20}, {20, 20}};
    Rectangle const rectangle_away_from_bottom{{20, 20}, {20, 20}};
    Rectangle const rectangle_near_bottom{{20, 400}, {20, 20}};
    Rectangle const rectangle_near_both_sides{{0, 20}, {600, 20}};
    Rectangle const rectangle_near_both_sides_and_bottom{{0, 400}, {600, 20}};
    Rectangle const rectangle_near_all_sides{{0, 20}, {600, 400}};
    Rectangle const rectangle_near_both_bottom_right{{400, 400}, {200, 20}};

    Window parent;
    Window child;

    WindowSpecification modification;

    void SetUp() override
    {
        basic_window_manager.add_display(display_area);

        mir::scene::SurfaceCreationParameters creation_parameters;
        basic_window_manager.add_session(session);

        EXPECT_CALL(*window_manager_policy, advise_new_window(_))
            .WillOnce(Invoke([this](WindowInfo const& window_info){ parent = window_info.window(); }))
            .WillOnce(Invoke([this](WindowInfo const& window_info){ child = window_info.window(); }));

        creation_parameters.size = initial_parent_size;
        basic_window_manager.add_surface(session, creation_parameters, &create_surface);

        creation_parameters.type = mir_surface_type_menu;
        creation_parameters.parent = parent;
        creation_parameters.size = initial_child_size;
        basic_window_manager.add_surface(session, creation_parameters, &create_surface);

        // Clear the expectations used to capture parent & child
        Mock::VerifyAndClearExpectations(window_manager_policy);
    }

    void TearDown() override
    {
//        std::cerr << "DEBUG parent position:" << Rectangle{parent.top_left(), parent.size()} << '\n';
//        std::cerr << "DEBUG child position :" << Rectangle{child.top_left(), child.size()} << '\n';
    }

    auto aux_rect_position() -> Rectangle
    {
        auto const rectangle = modification.aux_rect().value();
        return {rectangle.top_left + (parent.top_left() - Point{}), rectangle.size};
    }

    auto on_top_edge() -> Point
    {
        return aux_rect_position().top_left - as_displacement(child.size()).dy;
    }

    auto on_right_edge() -> Point
    {
        return aux_rect_position().top_right();
    }

    auto on_left_edge() -> Point
    {
        return aux_rect_position().top_left - as_displacement(child.size()).dx;
    }

    auto on_bottom_edge() -> Point
    {
        return aux_rect_position().bottom_left();
    }
};
}

TEST_F(WindowPlacement, fixture_sets_up_parent_and_child)
{
    ASSERT_THAT(parent, Ne(null_window));
    ASSERT_THAT(parent.size(), Eq(initial_parent_size));
    ASSERT_THAT(basic_window_manager.info_for(parent).children(), ElementsAre(child));
    ASSERT_THAT(basic_window_manager.info_for(parent).type(), Eq(mir_surface_type_normal));

    ASSERT_THAT(child, Ne(null_window));
    ASSERT_THAT(child.size(), Eq(initial_child_size));
    ASSERT_THAT(basic_window_manager.info_for(child).parent(), Eq(parent));
    ASSERT_THAT(basic_window_manager.info_for(child).type(), Eq(mir_surface_type_menu));
}


/* From the Mir toolkit API:
 * Positioning of the surface is specified with respect to the parent surface
 * via an adjacency rectangle. The server will attempt to choose an edge of the
 * adjacency rectangle on which to place the surface taking in to account
 * screen-edge proximity or similar constraints. In addition, the server can use
 * the edge affinity hint to consider only horizontal or only vertical adjacency
 * edges in the given rectangle.
 */

TEST_F(WindowPlacement, given_aux_rect_away_from_right_side_edge_attachment_vertical_attaches_to_right_edge)
{
    modification = edge_attachment(rectangle_away_from_rhs, mir_edge_attachment_vertical);

    auto const expected_position = on_right_edge();

    EXPECT_CALL(*window_manager_policy, advise_move_to(_, expected_position));
    basic_window_manager.modify_window(basic_window_manager.info_for(child), modification);
    ASSERT_THAT(child.top_left(), Eq(expected_position));
}

TEST_F(WindowPlacement, given_aux_rect_near_right_side_edge_attachment_vertical_attaches_to_left_edge)
{
    modification = edge_attachment(rectangle_near_rhs, mir_edge_attachment_vertical);

    auto const expected_position = on_left_edge();

    EXPECT_CALL(*window_manager_policy, advise_move_to(_, expected_position));
    basic_window_manager.modify_window(basic_window_manager.info_for(child), modification);
    ASSERT_THAT(child.top_left(), Eq(expected_position));
}

TEST_F(WindowPlacement, given_aux_rect_near_both_sides_edge_attachment_vertical_attaches_to_right_edge)
{
    modification = edge_attachment(rectangle_near_both_sides, mir_edge_attachment_vertical);

    auto const expected_position = on_right_edge();

    EXPECT_CALL(*window_manager_policy, advise_move_to(_, expected_position));
    basic_window_manager.modify_window(basic_window_manager.info_for(child), modification);
    ASSERT_THAT(child.top_left(), Eq(expected_position));
}

TEST_F(WindowPlacement, given_aux_rect_away_from_bottom_edge_attachment_horizontal_attaches_to_bottom_edge)
{
    modification = edge_attachment(rectangle_away_from_bottom, mir_edge_attachment_horizontal);

    auto const expected_position = on_bottom_edge();

    EXPECT_CALL(*window_manager_policy, advise_move_to(_, expected_position));
    basic_window_manager.modify_window(basic_window_manager.info_for(child), modification);
    ASSERT_THAT(child.top_left(), Eq(expected_position));
}

TEST_F(WindowPlacement, given_aux_rect_near_bottom_edge_attachment_horizontal_attaches_to_top_edge)
{
    modification = edge_attachment(rectangle_near_bottom, mir_edge_attachment_horizontal);

    auto const expected_position = on_top_edge();

    EXPECT_CALL(*window_manager_policy, advise_move_to(_, expected_position));
    basic_window_manager.modify_window(basic_window_manager.info_for(child), modification);
    ASSERT_THAT(child.top_left(), Eq(expected_position));
}

TEST_F(WindowPlacement, given_aux_rect_near_both_sides_edge_attachment_any_attaches_to_bottom_edge)
{
    modification = edge_attachment(rectangle_near_both_sides, mir_edge_attachment_any);

    auto const expected_position = on_bottom_edge();

    EXPECT_CALL(*window_manager_policy, advise_move_to(_, expected_position));
    basic_window_manager.modify_window(basic_window_manager.info_for(child), modification);
    ASSERT_THAT(child.top_left(), Eq(expected_position));
}

TEST_F(WindowPlacement, given_aux_rect_near_both_sides_and_bottom_edge_attachment_any_attaches_to_top_edge)
{
    modification = edge_attachment(rectangle_near_both_sides_and_bottom, mir_edge_attachment_any);

    auto const expected_position = on_top_edge();

    EXPECT_CALL(*window_manager_policy, advise_move_to(_, expected_position));
    basic_window_manager.modify_window(basic_window_manager.info_for(child), modification);
    ASSERT_THAT(child.top_left(), Eq(expected_position));
}

TEST_F(WindowPlacement, given_aux_rect_near_all_sides_attachment_any_attaches_to_right_edge)
{
    modification = edge_attachment(rectangle_near_all_sides, mir_edge_attachment_any);

    auto const expected_position = on_right_edge();

    EXPECT_CALL(*window_manager_policy, advise_move_to(_, expected_position));
    basic_window_manager.modify_window(basic_window_manager.info_for(child), modification);
    ASSERT_THAT(child.top_left(), Eq(expected_position));
}

namespace
{
MirPlacementGravity const all_gravities[] =
{
    mir_placement_gravity_northwest,
    mir_placement_gravity_north,
    mir_placement_gravity_northeast,
    mir_placement_gravity_west,
    mir_placement_gravity_centre,
    mir_placement_gravity_east,
    mir_placement_gravity_southwest,
    mir_placement_gravity_south,
    mir_placement_gravity_southeast,
};

auto position_of(MirPlacementGravity rect_gravity, Rectangle rectangle) -> Point
{
    auto const displacement = as_displacement(rectangle.size);

    switch (rect_gravity)
    {
    case mir_placement_gravity_northwest:
        return rectangle.top_left;

    case mir_placement_gravity_north:
        return rectangle.top_left + Displacement{0.5 * displacement.dx, 0};

    case mir_placement_gravity_northeast:
        return rectangle.top_right();

    case mir_placement_gravity_west:
        return rectangle.top_left + Displacement{0, 0.5 * displacement.dy};

    case mir_placement_gravity_centre:
        return rectangle.top_left + 0.5 * displacement;

    case mir_placement_gravity_east:
        return rectangle.top_right() + Displacement{0, 0.5 * displacement.dy};

    case mir_placement_gravity_southwest:
        return rectangle.bottom_left();

    case mir_placement_gravity_south:
        return rectangle.bottom_left() + Displacement{0.5 * displacement.dx, 0};

    case mir_placement_gravity_southeast:
        return rectangle.bottom_right();

    default:
        throw std::runtime_error("bad placement gravity");
    }

}
}

TEST_F(WindowPlacement, given_no_hints_can_attach_by_every_gravity)
{
    modification.aux_rect() = Rectangle{{100, 50}, { 20, 20}};
    modification.placement_hints() = MirPlacementHints{};

    for (auto const rect_gravity : all_gravities)
    {
        modification.aux_rect_placement_gravity() = rect_gravity;

        auto const rect_anchor = position_of(rect_gravity, aux_rect_position());

        for (auto const window_gravity : all_gravities)
        {
            modification.window_placement_gravity() = window_gravity;

            EXPECT_CALL(*window_manager_policy, advise_move_to(_, _));
            basic_window_manager.modify_window(basic_window_manager.info_for(child), modification);

            Rectangle child_rect{child.top_left(), child.size()};

            EXPECT_THAT(position_of(window_gravity, child_rect), Eq(rect_anchor))
                        << "rect_gravity=" << rect_gravity << ", window_gravity=" << window_gravity;
            Mock::VerifyAndClearExpectations(window_manager_policy);
        }
    }
}

TEST_F(WindowPlacement, given_no_hints_can_attach_by_offset_at_every_gravity)
{
    auto const offset = Displacement{42, 13};

    modification.aux_rect() = Rectangle{{100, 50}, { 20, 20}};
    modification.placement_hints() = MirPlacementHints{};
    modification.aux_rect_placement_offset() = offset;

    for (auto const rect_gravity : all_gravities)
    {
        modification.aux_rect_placement_gravity() = rect_gravity;

        auto const rect_anchor = position_of(rect_gravity, aux_rect_position()) + offset;

        for (auto const window_gravity : all_gravities)
        {
            modification.window_placement_gravity() = window_gravity;

            EXPECT_CALL(*window_manager_policy, advise_move_to(_, _));
            basic_window_manager.modify_window(basic_window_manager.info_for(child), modification);

            Rectangle child_rect{child.top_left(), child.size()};

            EXPECT_THAT(position_of(window_gravity, child_rect), Eq(rect_anchor))
                        << "rect_gravity=" << rect_gravity << ", window_gravity=" << window_gravity;
            Mock::VerifyAndClearExpectations(window_manager_policy);
        }
    }
}

TEST_F(WindowPlacement, given_aux_rect_near_right_side_and_offset_placement_is_flipped)
{
    DeltaX const x_offset{42};
    DeltaY const y_offset{13};

    modification.aux_rect() = rectangle_near_rhs;
    modification.placement_hints() = mir_placement_hints_flip_x;
    modification.aux_rect_placement_offset() = Displacement{x_offset, y_offset};
    modification.window_placement_gravity() = mir_placement_gravity_northwest;
    modification.aux_rect_placement_gravity() = mir_placement_gravity_northeast;

    auto const expected_position = on_left_edge() + Displacement{-1*x_offset, y_offset};

    EXPECT_CALL(*window_manager_policy, advise_move_to(_, expected_position));
    basic_window_manager.modify_window(basic_window_manager.info_for(child), modification);
    ASSERT_THAT(child.top_left(), Eq(expected_position));
}

TEST_F(WindowPlacement, given_aux_rect_near_bottom_and_offset_placement_is_flipped)
{
    DeltaX const x_offset{42};
    DeltaY const y_offset{13};

    modification.aux_rect() = rectangle_near_bottom;
    modification.placement_hints() = mir_placement_hints_flip_y;
    modification.aux_rect_placement_offset() = Displacement{x_offset, y_offset};
    modification.window_placement_gravity() = mir_placement_gravity_northwest;
    modification.aux_rect_placement_gravity() = mir_placement_gravity_southwest;

    auto const expected_position = on_top_edge() + Displacement{x_offset, -1*y_offset};

    EXPECT_CALL(*window_manager_policy, advise_move_to(_, expected_position));
    basic_window_manager.modify_window(basic_window_manager.info_for(child), modification);
    ASSERT_THAT(child.top_left(), Eq(expected_position));
}

TEST_F(WindowPlacement, given_aux_rect_near_bottom_right_and_offset_placement_is_flipped_both_ways)
{
    Displacement const displacement{42, 13};

    modification.aux_rect() = rectangle_near_both_bottom_right;
    modification.placement_hints() = mir_placement_hints_flip_any;
    modification.aux_rect_placement_offset() = displacement;
    modification.window_placement_gravity() = mir_placement_gravity_northwest;
    modification.aux_rect_placement_gravity() = mir_placement_gravity_southeast;

    auto const expected_position = aux_rect_position().top_left - as_displacement(child.size()) - displacement;

    EXPECT_CALL(*window_manager_policy, advise_move_to(_, expected_position));
    basic_window_manager.modify_window(basic_window_manager.info_for(child), modification);
    ASSERT_THAT(child.top_left(), Eq(expected_position));
}

TEST_F(WindowPlacement, given_aux_rect_near_right_side_placement_can_slide_in_x)
{
    modification.aux_rect() = rectangle_near_rhs;
    modification.placement_hints() = mir_placement_hints_slide_x;
    modification.window_placement_gravity() = mir_placement_gravity_northwest;
    modification.aux_rect_placement_gravity() = mir_placement_gravity_northeast;

    Point const expected_position{display_area.top_right().x - as_displacement(child.size()).dx, aux_rect_position().top_left.y};

    EXPECT_CALL(*window_manager_policy, advise_move_to(_, expected_position));
    basic_window_manager.modify_window(basic_window_manager.info_for(child), modification);
    ASSERT_THAT(child.top_left(), Eq(expected_position));
}

TEST_F(WindowPlacement, given_aux_rect_near_left_side_placement_can_slide_in_x)
{
    Rectangle const rectangle_near_left_side{{0, 20}, {20, 20}};

    modification.aux_rect() = rectangle_near_left_side;
    modification.placement_hints() = mir_placement_hints_slide_x;
    modification.window_placement_gravity() = mir_placement_gravity_northeast;
    modification.aux_rect_placement_gravity() = mir_placement_gravity_northwest;

    Point const expected_position{display_area.top_left.x, aux_rect_position().top_left.y};

    EXPECT_CALL(*window_manager_policy, advise_move_to(_, expected_position));
    basic_window_manager.modify_window(basic_window_manager.info_for(child), modification);
    ASSERT_THAT(child.top_left(), Eq(expected_position));
}

TEST_F(WindowPlacement, given_aux_rect_near_bottom_placement_can_slide_in_y)
{
    modification.aux_rect() = rectangle_near_bottom;
    modification.placement_hints() = mir_placement_hints_slide_y;
    modification.window_placement_gravity() = mir_placement_gravity_northwest;
    modification.aux_rect_placement_gravity() = mir_placement_gravity_southwest;

    Point const expected_position{
        aux_rect_position().top_left.x,
        (display_area.bottom_left() - as_displacement(child.size())).y};

    EXPECT_CALL(*window_manager_policy, advise_move_to(_, expected_position));
    basic_window_manager.modify_window(basic_window_manager.info_for(child), modification);
    ASSERT_THAT(child.top_left(), Eq(expected_position));
}

TEST_F(WindowPlacement, given_aux_rect_near_top_placement_can_slide_in_y)
{
    modification.aux_rect() = rectangle_near_all_sides;
    modification.placement_hints() = mir_placement_hints_slide_y;
    modification.window_placement_gravity() = mir_placement_gravity_southwest;
    modification.aux_rect_placement_gravity() = mir_placement_gravity_northwest;

    Point const expected_position{aux_rect_position().top_left.x, display_area.top_left.y};

    EXPECT_CALL(*window_manager_policy, advise_move_to(_, expected_position));
    basic_window_manager.modify_window(basic_window_manager.info_for(child), modification);
    ASSERT_THAT(child.top_left(), Eq(expected_position));
}

TEST_F(WindowPlacement, given_aux_rect_near_bottom_right_and_offset_placement_can_slide_in_x_and_y)
{
    modification.aux_rect() = rectangle_near_both_bottom_right;
    modification.placement_hints() = mir_placement_hints_slide;
    modification.window_placement_gravity() = mir_placement_gravity_northwest;
    modification.aux_rect_placement_gravity() = mir_placement_gravity_southwest;

    auto const expected_position = display_area.bottom_right() - as_displacement(child.size());

    EXPECT_CALL(*window_manager_policy, advise_move_to(_, expected_position));
    basic_window_manager.modify_window(basic_window_manager.info_for(child), modification);
    ASSERT_THAT(child.top_left(), Eq(expected_position));
}