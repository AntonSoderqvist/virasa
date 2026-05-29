#include <concepts>
#include <gtest/gtest.h>
#include <type_traits>
#include <utility>

#include "virasa/core/Logger.h"
#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Context.h"
#include "virasa/renderer/resources/Mesh.h"
#include "virasa/window/Platform.h"

namespace
{

using namespace virasa;

MeshData MakeTestMeshData()
{
	MeshData meshData;

	meshData.vertices = {
		{glm::vec3{0.0F, 0.0F, 0.0F}, glm::vec3{0.0F, 0.0F, 1.0F}, glm::vec4{1.0F, 0.0F, 0.0F, 1.0F}, glm::vec2{0.0F, 0.0F}},
		{glm::vec3{1.0F, 0.0F, 0.0F}, glm::vec3{0.0F, 0.0F, 1.0F}, glm::vec4{1.0F, 0.0F, 0.0F, 1.0F}, glm::vec2{1.0F, 0.0F}},
		{glm::vec3{0.0F, 1.0F, 0.0F}, glm::vec3{0.0F, 0.0F, 1.0F}, glm::vec4{1.0F, 0.0F, 0.0F, 1.0F}, glm::vec2{0.0F, 1.0F}}};
	meshData.indices = {0U, 1U, 2U};

	return meshData;
}

class MeshIntegrationTest : public ::testing::Test
{
	protected:
	static void SetUpTestSuite()
	{
		if (!Logger::IsInitialized())
		{
			Logger::Initialize();
		}
	}

	void SetUp() override
	{
		ASSERT_EQ(platform.Initialize("MeshTest", 128U, 128U), virasa::ErrorCode::None);
		RendererConfig config;
		config.enableValidation = false;
		uint32_t extCount = 0;
		const char* const* exts = virasa::window::Platform::GetRequiredVulkanExtensions(&extCount);
		config.requiredInstanceExtensions = exts;
		config.requiredInstanceExtensionCount = extCount;
		ASSERT_EQ(context.Initialize(platform, config), RenderError::None);
	}

	void TearDown() override
	{
		context.Shutdown();
		platform.Shutdown();
	}

	window::Platform platform;
	Context context;
};

} // namespace

TEST(Mesh, test_mesh_is_raii_movable_non_copyable)
{
	using virasa::Mesh;

	static_assert(std::is_final_v<Mesh>);
	static_assert(std::is_default_constructible_v<Mesh>);
	static_assert(!std::copy_constructible<Mesh>);
	static_assert(!std::is_copy_assignable_v<Mesh>);
	static_assert(std::is_move_constructible_v<Mesh>);
	static_assert(std::is_move_assignable_v<Mesh>);
	static_assert(std::is_nothrow_destructible_v<Mesh>);

	Mesh mesh;
	EXPECT_FALSE(mesh.IsInitialized());
	EXPECT_EQ(mesh.GetIndexCount(), 0U);

	Mesh movedConstructed(std::move(mesh));
	EXPECT_FALSE(mesh.IsInitialized());
	EXPECT_EQ(mesh.GetIndexCount(), 0U);
	EXPECT_FALSE(movedConstructed.IsInitialized());
	EXPECT_EQ(movedConstructed.GetIndexCount(), 0U);

	Mesh moveAssigned;
	moveAssigned = std::move(movedConstructed);
	EXPECT_FALSE(movedConstructed.IsInitialized());
	EXPECT_EQ(movedConstructed.GetIndexCount(), 0U);
	EXPECT_FALSE(moveAssigned.IsInitialized());
	EXPECT_EQ(moveAssigned.GetIndexCount(), 0U);
}

TEST(Mesh, test_mesh_default_constructed_state)
{
	using virasa::Mesh;

	Mesh mesh;

	EXPECT_FALSE(mesh.IsInitialized());
	EXPECT_EQ(mesh.GetIndexCount(), 0U);
}

TEST_F(MeshIntegrationTest, test_mesh_initialize_uploads_geometry)
{
	using virasa::Mesh;

	const MeshData meshData = MakeTestMeshData();
	Mesh mesh;

	ASSERT_EQ(mesh.Initialize(context.GetDevice(), context, meshData), RenderError::None);
	EXPECT_TRUE(mesh.IsInitialized());
	EXPECT_EQ(mesh.GetIndexCount(), static_cast<uint32_t>(meshData.indices.size()));

	const VkDeviceAddress vertexAddress = mesh.GetVertexBufferAddress(context.GetDevice());
	const VkDeviceAddress indexAddress = mesh.GetIndexBufferAddress(context.GetDevice());
	EXPECT_NE(vertexAddress, 0U);
	EXPECT_NE(indexAddress, 0U);

	const MeshData replacementMeshData = []()
	{
		MeshData data;
		data.vertices = {{glm::vec3{-1.0F, -1.0F, 0.0F},
					     glm::vec3{0.0F, 1.0F, 0.0F},
					     glm::vec4{1.0F, 0.0F, 0.0F, 1.0F},
					     glm::vec2{0.0F, 0.0F}},
			{glm::vec3{1.0F, -1.0F, 0.0F},
				glm::vec3{0.0F, 1.0F, 0.0F},
				glm::vec4{1.0F, 0.0F, 0.0F, 1.0F},
				glm::vec2{1.0F, 0.0F}},
			{glm::vec3{1.0F, 1.0F, 0.0F},
				glm::vec3{0.0F, 1.0F, 0.0F},
				glm::vec4{1.0F, 0.0F, 0.0F, 1.0F},
				glm::vec2{1.0F, 1.0F}},
			{glm::vec3{-1.0F, 1.0F, 0.0F},
				glm::vec3{0.0F, 1.0F, 0.0F},
				glm::vec4{1.0F, 0.0F, 0.0F, 1.0F},
				glm::vec2{0.0F, 1.0F}}};
		data.indices = {0U, 1U, 2U, 2U, 3U, 0U};
		return data;
	}();

	ASSERT_EQ(
		mesh.Initialize(context.GetDevice(), context, replacementMeshData), RenderError::None);
	EXPECT_TRUE(mesh.IsInitialized());
	EXPECT_EQ(mesh.GetIndexCount(), static_cast<uint32_t>(replacementMeshData.indices.size()));
	EXPECT_NE(mesh.GetVertexBufferAddress(context.GetDevice()), 0U);
	EXPECT_NE(mesh.GetIndexBufferAddress(context.GetDevice()), 0U);
}

TEST_F(MeshIntegrationTest, test_mesh_buffer_address_accessors)
{
	using virasa::Mesh;

	const MeshData meshData = MakeTestMeshData();
	Mesh mesh;
	ASSERT_EQ(mesh.Initialize(context.GetDevice(), context, meshData), RenderError::None);
	ASSERT_TRUE(mesh.IsInitialized());

	const VkDeviceAddress vertexAddressA = mesh.GetVertexBufferAddress(context.GetDevice());
	const VkDeviceAddress vertexAddressB = mesh.GetVertexBufferAddress(context.GetDevice());
	const VkDeviceAddress indexAddressA = mesh.GetIndexBufferAddress(context.GetDevice());
	const VkDeviceAddress indexAddressB = mesh.GetIndexBufferAddress(context.GetDevice());

	EXPECT_NE(vertexAddressA, 0U);
	EXPECT_NE(indexAddressA, 0U);
	EXPECT_EQ(vertexAddressA, vertexAddressB);
	EXPECT_EQ(indexAddressA, indexAddressB);
}

TEST_F(MeshIntegrationTest, test_mesh_observers)
{
	using virasa::Mesh;

	Mesh mesh;
	EXPECT_FALSE(mesh.IsInitialized());
	EXPECT_EQ(mesh.GetIndexCount(), 0U);

	const MeshData meshData = MakeTestMeshData();
	ASSERT_EQ(mesh.Initialize(context.GetDevice(), context, meshData), RenderError::None);
	EXPECT_TRUE(mesh.IsInitialized());
	EXPECT_EQ(mesh.GetIndexCount(), static_cast<uint32_t>(meshData.indices.size()));

	Mesh movedTo = std::move(mesh);
	EXPECT_FALSE(mesh.IsInitialized());
	EXPECT_EQ(mesh.GetIndexCount(), 0U);
	EXPECT_TRUE(movedTo.IsInitialized());
	EXPECT_EQ(movedTo.GetIndexCount(), static_cast<uint32_t>(meshData.indices.size()));
}

TEST(Mesh, test_mesh_is_not_thread_safe_per_instance)
{
	using virasa::Mesh;

	static_assert(noexcept(
		std::declval<const Mesh&>().GetVertexBufferAddress(std::declval<const Device&>())));
	static_assert(noexcept(
		std::declval<const Mesh&>().GetIndexBufferAddress(std::declval<const Device&>())));
	static_assert(noexcept(std::declval<const Mesh&>().GetIndexCount()));
	static_assert(noexcept(std::declval<const Mesh&>().IsInitialized()));

	SUCCEED();
}
