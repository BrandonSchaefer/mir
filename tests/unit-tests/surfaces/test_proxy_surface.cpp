/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
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

#include "src/surfaces/proxy_surface.h"
#include "mir/surfaces/surface.h"
#include "mir/shell/surface_creation_parameters.h"
#include "mir/surfaces/surface_stack_model.h"
#include "mir/input/communication_package.h"

#include "mir_test_doubles/mock_buffer_bundle.h"
#include "mir_test_doubles/mock_buffer.h"
#include "mir_test_doubles/stub_buffer.h"
#include "mir_test/fake_shared.h"

#include <stdexcept>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace ms = mir::surfaces;
namespace msh = mir::shell;
namespace mc = mir::compositor;
namespace mi = mir::input;
namespace geom = mir::geometry;
namespace mt = mir::test;
namespace mtd = mt::doubles;

namespace
{
typedef testing::NiceMock<mtd::MockBufferBundle> StubBufferBundle;

class MockSurfaceStackModel : public ms::SurfaceStackModel
{
public:
    MockSurfaceStackModel()
    {
        using namespace testing;

        ON_CALL(*this, create_surface(_)).
            WillByDefault(Invoke(this, &MockSurfaceStackModel::do_create_surface));

        ON_CALL(*this, destroy_surface(_)).
            WillByDefault(Invoke(this, &MockSurfaceStackModel::do_destroy_surface));
    }

    MOCK_METHOD1(create_surface, std::weak_ptr<ms::Surface> (const msh::SurfaceCreationParameters&));

    MOCK_METHOD1(destroy_surface, void (std::weak_ptr<ms::Surface> const&));

private:
    std::weak_ptr<ms::Surface> do_create_surface(const msh::SurfaceCreationParameters& params)
    {
        surface = std::make_shared<ms::Surface>(
            params.name,
            std::make_shared<StubBufferBundle>());
        return surface;
    }

    void do_destroy_surface(std::weak_ptr<ms::Surface> const&)
    {
        surface.reset();
    }

    std::shared_ptr<ms::Surface> surface;
};

struct MockCommunicationPackage : public mi::CommunicationPackage
{
    MOCK_CONST_METHOD0(client_fd, int());
    MOCK_CONST_METHOD0(server_fd, int());
};

}

namespace
{
struct SurfaceProxySetup : public testing::Test
{
    MockSurfaceStackModel surface_stack;
    msh::SurfaceCreationParameters params;
    std::shared_ptr<mi::CommunicationPackage> null_communication_package;
};
}

TEST_F(SurfaceProxySetup, creation_and_destruction)
{
    using namespace testing;
    InSequence sequence;
    EXPECT_CALL(surface_stack, create_surface(_)).Times(1);
    EXPECT_CALL(surface_stack, destroy_surface(_)).Times(1);

    ms::ProxySurface test(&surface_stack, null_communication_package, params);
}

TEST_F(SurfaceProxySetup, destroy)
{
    using namespace testing;

    EXPECT_CALL(surface_stack, create_surface(_)).Times(1);
    EXPECT_CALL(surface_stack, destroy_surface(_)).Times(1);

    ms::ProxySurface test(&surface_stack, null_communication_package, params);
    test.destroy();

    Mock::VerifyAndClearExpectations(&surface_stack);
}

TEST_F(SurfaceProxySetup, client_input_fd)
{
    using namespace testing;
    const int testing_client_fd = 17;

    MockCommunicationPackage mock_package;
    EXPECT_CALL(mock_package, client_fd()).Times(1).WillOnce(Return(testing_client_fd));

    EXPECT_CALL(surface_stack, create_surface(_)).Times(1);
    EXPECT_CALL(surface_stack, destroy_surface(_)).Times(1);

    ms::ProxySurface test(&surface_stack, mt::fake_shared(mock_package), params);
 
    EXPECT_EQ(testing_client_fd, test.client_input_fd());

    test.destroy();

    Mock::VerifyAndClearExpectations(&surface_stack);
}

namespace
{
struct BasicSurfaceProxy : testing::Test
{
    std::shared_ptr<StubBufferBundle> buffer_bundle;
    std::shared_ptr<mi::CommunicationPackage> null_communication_package;

    BasicSurfaceProxy() :
        buffer_bundle(std::make_shared<StubBufferBundle>())
    {
        using namespace testing;

        ON_CALL(*buffer_bundle, bundle_size()).WillByDefault(Return(geom::Size()));
        ON_CALL(*buffer_bundle, get_bundle_pixel_format()).WillByDefault(Return(geom::PixelFormat::abgr_8888));
        ON_CALL(*buffer_bundle, secure_client_buffer()).WillByDefault(Return(std::shared_ptr<mtd::StubBuffer>()));
    }
};
}

TEST_F(BasicSurfaceProxy, client_buffer_throw_behavior)
{
    auto surface = std::make_shared<ms::Surface>(__PRETTY_FUNCTION__, buffer_bundle);

    ms::BasicProxySurface proxy_surface(surface, null_communication_package);

    EXPECT_NO_THROW({
        proxy_surface.client_buffer();
    });

    surface.reset();

    EXPECT_THROW({
        proxy_surface.client_buffer();
    }, std::runtime_error);
}

TEST_F(BasicSurfaceProxy, size_throw_behavior)
{
    auto surface = std::make_shared<ms::Surface>(__PRETTY_FUNCTION__, buffer_bundle);

    ms::BasicProxySurface proxy_surface(surface, null_communication_package);

    EXPECT_NO_THROW({
        proxy_surface.size();
    });

    surface.reset();

    EXPECT_THROW({
        proxy_surface.size();
    }, std::runtime_error);
}

TEST_F(BasicSurfaceProxy, pixel_format_throw_behavior)
{
    auto surface = std::make_shared<ms::Surface>(__PRETTY_FUNCTION__, buffer_bundle);

    ms::BasicProxySurface proxy_surface(surface, null_communication_package);

    EXPECT_NO_THROW({
        proxy_surface.pixel_format();
    });

    surface.reset();

    EXPECT_THROW({
        proxy_surface.pixel_format();
    }, std::runtime_error);
}

TEST_F(BasicSurfaceProxy, hide_throw_behavior)
{
    auto surface = std::make_shared<ms::Surface>(__PRETTY_FUNCTION__, buffer_bundle);

    ms::BasicProxySurface proxy_surface(surface, null_communication_package);

    EXPECT_NO_THROW({
        proxy_surface.hide();
    });

    surface.reset();

    EXPECT_NO_THROW({
        proxy_surface.hide();
    });
}

TEST_F(BasicSurfaceProxy, show_throw_behavior)
{
    auto surface = std::make_shared<ms::Surface>(__PRETTY_FUNCTION__, buffer_bundle);

    ms::BasicProxySurface proxy_surface(surface, null_communication_package);

    EXPECT_NO_THROW({
        proxy_surface.show();
    });

    surface.reset();

    EXPECT_NO_THROW({
        proxy_surface.show();
    });
}

TEST_F(BasicSurfaceProxy, destroy_throw_behavior)
{
    auto surface = std::make_shared<ms::Surface>(__PRETTY_FUNCTION__, buffer_bundle);

    ms::BasicProxySurface proxy_surface(surface, null_communication_package);

    EXPECT_NO_THROW({
        proxy_surface.destroy();
    });

    surface.reset();

    EXPECT_NO_THROW({
        proxy_surface.destroy();
    });
}

TEST_F(BasicSurfaceProxy, shutdown_throw_behavior)
{
    auto surface = std::make_shared<ms::Surface>(__PRETTY_FUNCTION__, buffer_bundle);

    ms::BasicProxySurface proxy_surface(surface, null_communication_package);

    EXPECT_NO_THROW({
        proxy_surface.shutdown();
    });

    surface.reset();

    EXPECT_NO_THROW({
        proxy_surface.shutdown();
    });
}

TEST_F(BasicSurfaceProxy, advance_client_buffer_throw_behavior)
{
    auto surface = std::make_shared<ms::Surface>(__PRETTY_FUNCTION__, buffer_bundle);

    ms::BasicProxySurface proxy_surface(surface, null_communication_package);

    EXPECT_NO_THROW({
        proxy_surface.advance_client_buffer();
    });

    surface.reset();

    EXPECT_NO_THROW({
        proxy_surface.advance_client_buffer();
    });
}
