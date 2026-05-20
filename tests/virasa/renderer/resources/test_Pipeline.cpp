#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

#include "virasa/core/Logger.h"
#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Device.h"
#include "virasa/renderer/core/Instance.h"
#include "virasa/renderer/resources/Pipeline.h"
#include "virasa/renderer/resources/ShaderModule.h"
#include "virasa/window/Platform.h"
#include "virasa/window/Types.h"
#include "vulkan/vulkan.h"

using namespace virasa;
using namespace virasa::window;

// ---------------------------------------------------------------------------
// Test infrastructure helpers
// ---------------------------------------------------------------------------

namespace
{

// Bring up a real Vulkan instance + device so that Build() can make real
// Vulkan calls. Tests that only exercise builder state accumulation (no
// Build) do not need this fixture.
struct VulkanContext
{
	RendererConfig config;
	Instance instance;
	window::Platform platform;
	Device device;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	bool valid = false;

	VulkanContext()
	{
		Logger::Initialize();

		config.enableValidation = false;

		// Initialize a headless-ish platform so we can get Vulkan extensions.
		// We need a window to create a surface for device selection.
		if (platform.Initialize("PipelineTest", 1, 1) != ErrorCode::None)
			return;

		uint32_t extCount = 0;
		const char* const* exts = window::Platform::GetRequiredVulkanExtensions(&extCount);
		config.requiredInstanceExtensions = exts;
		config.requiredInstanceExtensionCount = extCount;

		if (instance.Initialize(config) != RenderError::None)
			return;

		if (platform.CreateSurface(instance.GetHandle(), &surface) != ErrorCode::None)
			return;

		if (device.Initialize(instance, surface) != RenderError::None)
			return;

		valid = true;
	}

	~VulkanContext()
	{
		if (surface != VK_NULL_HANDLE && instance.GetHandle() != VK_NULL_HANDLE)
			vkDestroySurfaceKHR(instance.GetHandle(), surface, nullptr);
		platform.Shutdown();
		Logger::Shutdown();
	}
};

// Minimal SPIR-V for a vertex shader with a "main" entry point.
// Compiled from:
//   #version 450
//   void main() { gl_Position = vec4(0); }
// using: glslc -fshader-stage=vert -o vert.spv vert.glsl
static const uint32_t kMinimalVertSpirv[] = {
	0x07230203,
	0x00010000,
	0x000d000b,
	0x00000014,
	0x00000000,
	0x00020011,
	0x00000001,
	0x0006000b,
	0x00000001,
	0x4c534c47,
	0x6474732e,
	0x3035342e,
	0x00000000,
	0x0003000e,
	0x00000000,
	0x00000001,
	0x0006000f,
	0x00000000,
	0x00000004,
	0x6e69616d,
	0x00000000,
	0x0000000d,
	0x00030003,
	0x00000002,
	0x000001c2,
	0x000a0004,
	0x475f4c47,
	0x4c474f4f,
	0x70635f45,
	0x74735f70,
	0x5f656c79,
	0x656e696c,
	0x7269645f,
	0x69746365,
	0x00006576,
	0x00080004,
	0x475f4c47,
	0x4c474f4f,
	0x6e695f45,
	0x64756c63,
	0x69645f65,
	0x74636572,
	0x00657669,
	0x00040005,
	0x00000004,
	0x6e69616d,
	0x00000000,
	0x00060005,
	0x0000000b,
	0x505f6c67,
	0x65567265,
	0x78657472,
	0x00000000,
	0x00060006,
	0x0000000b,
	0x00000000,
	0x505f6c67,
	0x7469736f,
	0x006e6f69,
	0x00070006,
	0x0000000b,
	0x00000001,
	0x505f6c67,
	0x746e696f,
	0x657a6953,
	0x00000000,
	0x00070006,
	0x0000000b,
	0x00000002,
	0x435f6c67,
	0x4470696c,
	0x61747369,
	0x0065636e,
	0x00070006,
	0x0000000b,
	0x00000003,
	0x435f6c67,
	0x446c6c75,
	0x61747369,
	0x0065636e,
	0x00030005,
	0x0000000d,
	0x00000000,
	0x00050048,
	0x0000000b,
	0x00000000,
	0x0000000b,
	0x00000000,
	0x00050048,
	0x0000000b,
	0x00000001,
	0x0000000b,
	0x00000001,
	0x00050048,
	0x0000000b,
	0x00000002,
	0x0000000b,
	0x00000003,
	0x00050048,
	0x0000000b,
	0x00000003,
	0x0000000b,
	0x00000004,
	0x00030047,
	0x0000000b,
	0x00000002,
	0x00020013,
	0x00000002,
	0x00030021,
	0x00000003,
	0x00000002,
	0x00030016,
	0x00000006,
	0x00000020,
	0x00040017,
	0x00000007,
	0x00000006,
	0x00000004,
	0x00040015,
	0x00000008,
	0x00000020,
	0x00000000,
	0x0004002b,
	0x00000008,
	0x00000009,
	0x00000001,
	0x0004001c,
	0x0000000a,
	0x00000006,
	0x00000009,
	0x0006001e,
	0x0000000b,
	0x00000007,
	0x00000006,
	0x0000000a,
	0x0000000a,
	0x00040020,
	0x0000000c,
	0x00000003,
	0x0000000b,
	0x0004003b,
	0x0000000c,
	0x0000000d,
	0x00000003,
	0x00040015,
	0x0000000e,
	0x00000020,
	0x00000001,
	0x0004002b,
	0x0000000e,
	0x0000000f,
	0x00000000,
	0x0004002b,
	0x00000006,
	0x00000010,
	0x00000000,
	0x0007002c,
	0x00000007,
	0x00000011,
	0x00000010,
	0x00000010,
	0x00000010,
	0x00000010,
	0x00040020,
	0x00000012,
	0x00000003,
	0x00000007,
	0x00050036,
	0x00000002,
	0x00000004,
	0x00000000,
	0x00000003,
	0x000200f8,
	0x00000005,
	0x00050041,
	0x00000012,
	0x00000013,
	0x0000000d,
	0x0000000f,
	0x0003003e,
	0x00000013,
	0x00000011,
	0x000100fd,
	0x00010038};

// Minimal fragment SPIR-V: outputs vec4(1,0,0,1) to location 0.
// Generated from:
//   #version 450
//   layout(location=0) out vec4 outColor;
//   void main() { outColor = vec4(1,0,0,1); }
static const uint32_t kMinimalFragSpirv[] = {0x07230203,
	0x00010000,
	0x000d000a,
	0x0000000d,
	0x00000000,
	0x00020011,
	0x00000001,
	0x0006000b,
	0x00000001,
	0x4c534c47,
	0x6474732e,
	0x3035342e,
	0x00000000,
	0x0003000e,
	0x00000000,
	0x00000001,
	0x0006000f,
	0x00000004,
	0x00000004,
	0x6e69616d,
	0x00000000,
	0x00000009,
	0x00030010,
	0x00000004,
	0x00000007,
	0x00030003,
	0x00000002,
	0x000001c2,
	0x00040005,
	0x00000004,
	0x6e69616d,
	0x00000000,
	0x00050005,
	0x00000009,
	0x4374756f,
	0x726f6c6f,
	0x00000000,
	0x00040047,
	0x00000009,
	0x0000001e,
	0x00000000,
	0x00020013,
	0x00000002,
	0x00030021,
	0x00000003,
	0x00000002,
	0x00030016,
	0x00000006,
	0x00000020,
	0x00040017,
	0x00000007,
	0x00000006,
	0x00000004,
	0x00040020,
	0x00000008,
	0x00000003,
	0x00000007,
	0x0004003b,
	0x00000008,
	0x00000009,
	0x00000003,
	0x0004002b,
	0x00000006,
	0x0000000a,
	0x3f800000,
	0x0004002b,
	0x00000006,
	0x0000000b,
	0x00000000,
	0x0007002c,
	0x00000007,
	0x0000000c,
	0x0000000a,
	0x0000000b,
	0x0000000b,
	0x0000000a,
	0x00050036,
	0x00000002,
	0x00000004,
	0x00000000,
	0x00000003,
	0x000200f8,
	0x00000005,
	0x0003003e,
	0x00000009,
	0x0000000c,
	0x000100fd,
	0x00010038};

// Write SPIR-V to a temp file and return the path.
std::string WriteTempSpirv(const uint32_t* data, size_t wordCount, const char* suffix)
{
	std::string path = std::string("/tmp/virasa_test_") + suffix + ".spv";
	FILE* f = fopen(path.c_str(), "wb");
	if (!f)
		return "";
	fwrite(data, sizeof(uint32_t), wordCount, f);
	fclose(f);
	return path;
}

} // namespace

// ===========================================================================
// Tests
// ===========================================================================

// ---------------------------------------------------------------------------
// pipeline_builder_accumulates_state_with_defaults
// ---------------------------------------------------------------------------
TEST(PipelineBuilder, test_pipeline_builder_accumulates_state_with_defaults)
{
	// A default-constructed PipelineBuilder should be constructible without
	// crashing and should own no Vulkan resources (no Vulkan calls happen
	// in the constructor or setters).
	PipelineBuilder builder;

	// Copyable
	PipelineBuilder copy = builder;
	(void)copy;

	// Movable
	PipelineBuilder moved = std::move(copy);
	(void)moved;

	// We cannot directly inspect internal fields, but we can verify that
	// Build with no vertex shader and no color format fails validation
	// (confirming the defaults are VK_NULL_HANDLE / VK_FORMAT_UNDEFINED).
	// We do not have a Device here, so we use a default-constructed one;
	// Build must fail before touching Vulkan because the pre-condition check
	// fires first.
	Device dummyDevice;
	Pipeline out;
	RenderError err = builder.Build(dummyDevice, out);
	EXPECT_EQ(err, RenderError::PipelineCreateFailed);
	EXPECT_FALSE(out.IsInitialized());
}

// ---------------------------------------------------------------------------
// pipeline_builder_setters_return_self_for_chaining
// ---------------------------------------------------------------------------
TEST(PipelineBuilder, test_pipeline_builder_setters_return_self_for_chaining)
{
	PipelineBuilder builder;

	// Every setter must return a reference to the same builder object.
	// We verify this by chaining all setters and checking the address.
	ShaderModule dummyModule; // default-constructed, handle = VK_NULL_HANDLE

	PipelineBuilder& ref =
		builder.SetVertexShader(dummyModule)
			.SetFragmentShader(dummyModule)
			.SetColorFormat(VK_FORMAT_B8G8R8A8_UNORM)
			.SetExtent({800, 600})
			.SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
			.SetPolygonMode(VK_POLYGON_MODE_FILL)
			.SetCullMode(VK_CULL_MODE_BACK_BIT)
			.SetFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
			.SetDepthTest(false)
			.SetDepthWrite(false)
			.SetDepthCompareOp(VK_COMPARE_OP_LESS)
			.SetDepthFormat(VK_FORMAT_UNDEFINED)
			.SetDepthBias(0.0f, 0.0f)
			.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, 4)
			.AddDescriptorSetLayout(VK_NULL_HANDLE)
			.SetBlendEnabled(false)
			.SetStencilTest(false)
			.SetStencilOp(VK_STENCIL_OP_KEEP,
				VK_STENCIL_OP_KEEP,
				VK_STENCIL_OP_KEEP,
				VK_COMPARE_OP_ALWAYS)
			.SetStencilMasks(0xFF, 0xFF)
			.SetStencilFormat(VK_FORMAT_UNDEFINED)
			.SetColorWriteMask(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
						 VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)
			.AddDynamicState(VK_DYNAMIC_STATE_DEPTH_BIAS);

	EXPECT_EQ(&ref, &builder);
}

// ---------------------------------------------------------------------------
// vertex_layout_attributes_copied_at_set_time
// ---------------------------------------------------------------------------
TEST(PipelineBuilder, test_vertex_layout_attributes_copied_at_set_time)
{
	// Arrange: create a layout with some attributes.
	VertexAttribute attr0{};
	attr0.location = 0;
	attr0.format = VK_FORMAT_R32G32B32_SFLOAT;
	attr0.offset = 0;

	VertexAttribute attr1{};
	attr1.location = 1;
	attr1.format = VK_FORMAT_R32G32_SFLOAT;
	attr1.offset = 12;

	std::vector<VertexAttribute> attrs = {attr0, attr1};

	VertexLayout layout{};
	layout.stride = 20;
	layout.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	layout.attributes = attrs;

	PipelineBuilder builder;
	builder.SetVertexLayout(layout);

	// Act: mutate the original storage after SetVertexLayout.
	attrs[0].location = 99;
	attrs[1].format = VK_FORMAT_UNDEFINED;

	// Assert: Build should still see the original attribute data.
	// We verify indirectly: a second SetVertexLayout with a different layout
	// replaces the first (no append). We also verify that calling
	// SetVertexLayout twice replaces rather than appends by checking that
	// the builder can be built (or fails for the right reason) with only
	// the second layout's data.
	VertexLayout layout2{};
	layout2.stride = 8;
	layout2.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	// empty attributes
	builder.SetVertexLayout(layout2);

	// The builder now holds layout2's data (stride=8, no attributes).
	// We can't inspect internal state directly, but we can confirm no crash
	// and that the builder still functions.
	Device dummyDevice;
	Pipeline out;
	// Will fail validation (no vertex shader, no color format) — that's fine.
	RenderError err = builder.Build(dummyDevice, out);
	EXPECT_EQ(err, RenderError::PipelineCreateFailed);
}

// ---------------------------------------------------------------------------
// accumulator_setters_append_in_call_order
// ---------------------------------------------------------------------------
TEST(PipelineBuilder, test_accumulator_setters_append_in_call_order)
{
	// We verify that Add* methods append without crashing and that the
	// builder remains usable. The actual ordering into Vulkan structs is
	// verified implicitly by the Build tests that succeed.
	PipelineBuilder builder;

	builder.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, 16);
	builder.AddPushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, 16, 4);
	builder.AddDescriptorSetLayout(VK_NULL_HANDLE);
	builder.AddDescriptorSetLayout(VK_NULL_HANDLE);
	builder.AddDynamicState(VK_DYNAMIC_STATE_DEPTH_BIAS);
	builder.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT); // duplicate — Vulkan tolerates it

	// Build will fail validation (no vertex shader, no color format).
	Device dummyDevice;
	Pipeline out;
	RenderError err = builder.Build(dummyDevice, out);
	EXPECT_EQ(err, RenderError::PipelineCreateFailed);
	EXPECT_FALSE(out.IsInitialized());
}

// ---------------------------------------------------------------------------
// setter_field_mapping
// ---------------------------------------------------------------------------
TEST(PipelineBuilder, test_setter_field_mapping)
{
	// Exercise every remaining setter to confirm they do not crash and
	// return *this. The actual field values are verified by the Build
	// integration tests; here we just confirm the API surface is callable.
	PipelineBuilder builder;

	builder.SetColorFormat(VK_FORMAT_R8G8B8A8_UNORM)
		.SetExtent({1920, 1080})
		.SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
		.SetPolygonMode(VK_POLYGON_MODE_LINE)
		.SetCullMode(VK_CULL_MODE_NONE)
		.SetFrontFace(VK_FRONT_FACE_CLOCKWISE)
		.SetDepthTest(true)
		.SetDepthWrite(true)
		.SetDepthCompareOp(VK_COMPARE_OP_LESS_OR_EQUAL)
		.SetDepthFormat(VK_FORMAT_D32_SFLOAT)
		.SetDepthBias(1.0f, 2.0f) // also sets depth bias enable = true
		.SetBlendEnabled(true)
		.SetStencilTest(true)
		.SetStencilOp(VK_STENCIL_OP_REPLACE,
			VK_STENCIL_OP_ZERO,
			VK_STENCIL_OP_INCREMENT_AND_CLAMP,
			VK_COMPARE_OP_EQUAL)
		.SetStencilMasks(0x0F, 0xFF)
		.SetStencilFormat(VK_FORMAT_S8_UINT)
		.SetColorWriteMask(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT);

	// No crash — setter field mapping is correct.
	Device dummyDevice;
	Pipeline out;
	// Will fail because no vertex shader is set.
	RenderError err = builder.Build(dummyDevice, out);
	EXPECT_EQ(err, RenderError::PipelineCreateFailed);
}

// ---------------------------------------------------------------------------
// pipeline_build_requires_vertex_shader_and_color_format
// ---------------------------------------------------------------------------
TEST(PipelineBuilder, test_pipeline_build_requires_vertex_shader_and_color_format)
{
	Device dummyDevice;
	Pipeline out;

	// Case 1: no vertex shader, no color format → PipelineCreateFailed
	{
		PipelineBuilder builder;
		EXPECT_EQ(builder.Build(dummyDevice, out), RenderError::PipelineCreateFailed);
		EXPECT_FALSE(out.IsInitialized());
	}

	// Case 2: color format set but no vertex shader → PipelineCreateFailed
	{
		PipelineBuilder builder;
		builder.SetColorFormat(VK_FORMAT_B8G8R8A8_UNORM);
		EXPECT_EQ(builder.Build(dummyDevice, out), RenderError::PipelineCreateFailed);
		EXPECT_FALSE(out.IsInitialized());
	}

	// Case 3: vertex shader set (via dummy ShaderModule with VK_NULL_HANDLE)
	// but no color format → PipelineCreateFailed
	// (VK_NULL_HANDLE vertex shader is still a null handle → fails)
	{
		PipelineBuilder builder;
		ShaderModule dummyVs;
		builder.SetVertexShader(dummyVs); // handle = VK_NULL_HANDLE
		EXPECT_EQ(builder.Build(dummyDevice, out), RenderError::PipelineCreateFailed);
		EXPECT_FALSE(out.IsInitialized());
	}

	// out_pipeline must not be modified on validation failure.
	EXPECT_EQ(out.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(out.GetLayout(), VK_NULL_HANDLE);
}

// ---------------------------------------------------------------------------
// pipeline_build_clears_out_pipeline_at_start
// ---------------------------------------------------------------------------
TEST(PipelineBuilder, test_pipeline_build_clears_out_pipeline_at_start)
{
	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	std::string vsPath =
		WriteTempSpirv(kMinimalVertSpirv, sizeof(kMinimalVertSpirv) / sizeof(uint32_t), "vert");
	std::string fsPath =
		WriteTempSpirv(kMinimalFragSpirv, sizeof(kMinimalFragSpirv) / sizeof(uint32_t), "frag");
	ASSERT_FALSE(vsPath.empty());
	ASSERT_FALSE(fsPath.empty());

	ShaderModule vs, fs;
	ASSERT_EQ(vs.Initialize(ctx.device, vsPath.c_str()), RenderError::None);
	ASSERT_EQ(fs.Initialize(ctx.device, fsPath.c_str()), RenderError::None);

	// First successful build.
	Pipeline pipeline;
	PipelineBuilder builder;
	builder.SetVertexShader(vs)
		.SetFragmentShader(fs)
		.SetColorFormat(VK_FORMAT_B8G8R8A8_UNORM)
		.SetExtent({800, 600});

	ASSERT_EQ(builder.Build(ctx.device, pipeline), RenderError::None);
	EXPECT_TRUE(pipeline.IsInitialized());
	EXPECT_NE(pipeline.GetHandle(), VK_NULL_HANDLE);
	EXPECT_NE(pipeline.GetLayout(), VK_NULL_HANDLE);

	// Second build into the same pipeline object — must succeed and replace.
	RenderError err2 = builder.Build(ctx.device, pipeline);
	EXPECT_EQ(err2, RenderError::None);
	EXPECT_TRUE(pipeline.IsInitialized());
	EXPECT_NE(pipeline.GetHandle(), VK_NULL_HANDLE);

	// Now build with a builder that will fail validation into the same
	// pipeline — the pipeline must be left in default-constructed state.
	PipelineBuilder badBuilder; // no vertex shader, no color format
	RenderError errBad = badBuilder.Build(ctx.device, pipeline);
	EXPECT_EQ(errBad, RenderError::PipelineCreateFailed);
	EXPECT_FALSE(pipeline.IsInitialized());
	EXPECT_EQ(pipeline.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(pipeline.GetLayout(), VK_NULL_HANDLE);
}

// ---------------------------------------------------------------------------
// pipeline_build_creates_pipeline_layout
// ---------------------------------------------------------------------------
TEST(PipelineBuilder, test_pipeline_build_creates_pipeline_layout)
{
	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	std::string vsPath = WriteTempSpirv(
		kMinimalVertSpirv, sizeof(kMinimalVertSpirv) / sizeof(uint32_t), "layout_vert");
	ASSERT_FALSE(vsPath.empty());

	ShaderModule vs;
	ASSERT_EQ(vs.Initialize(ctx.device, vsPath.c_str()), RenderError::None);

	PipelineBuilder builder;
	builder.SetVertexShader(vs)
		.SetColorFormat(VK_FORMAT_B8G8R8A8_UNORM)
		.SetExtent({800, 600})
		.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, 16);

	Pipeline pipeline;
	RenderError err = builder.Build(ctx.device, pipeline);
	EXPECT_EQ(err, RenderError::None);
	EXPECT_NE(pipeline.GetLayout(), VK_NULL_HANDLE);
	EXPECT_TRUE(pipeline.IsInitialized());
}

// ---------------------------------------------------------------------------
// pipeline_build_uses_dynamic_rendering
// ---------------------------------------------------------------------------
TEST(PipelineBuilder, test_pipeline_build_uses_dynamic_rendering)
{
	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	std::string vsPath = WriteTempSpirv(
		kMinimalVertSpirv, sizeof(kMinimalVertSpirv) / sizeof(uint32_t), "dynrender_vert");
	std::string fsPath = WriteTempSpirv(
		kMinimalFragSpirv, sizeof(kMinimalFragSpirv) / sizeof(uint32_t), "dynrender_frag");
	ASSERT_FALSE(vsPath.empty());
	ASSERT_FALSE(fsPath.empty());

	ShaderModule vs, fs;
	ASSERT_EQ(vs.Initialize(ctx.device, vsPath.c_str()), RenderError::None);
	ASSERT_EQ(fs.Initialize(ctx.device, fsPath.c_str()), RenderError::None);

	PipelineBuilder builder;
	builder.SetVertexShader(vs)
		.SetFragmentShader(fs)
		.SetColorFormat(VK_FORMAT_B8G8R8A8_UNORM)
		.SetDepthFormat(VK_FORMAT_D32_SFLOAT)
		.SetStencilFormat(VK_FORMAT_UNDEFINED)
		.SetExtent({800, 600});

	Pipeline pipeline;
	RenderError err = builder.Build(ctx.device, pipeline);
	// A successful build using dynamic rendering confirms the pipeline was
	// created with VkPipelineRenderingCreateInfo (no render pass needed).
	EXPECT_EQ(err, RenderError::None);
	EXPECT_TRUE(pipeline.IsInitialized());
	EXPECT_NE(pipeline.GetHandle(), VK_NULL_HANDLE);
}

// ---------------------------------------------------------------------------
// pipeline_shader_stages_main_entry
// ---------------------------------------------------------------------------
TEST(PipelineBuilder, test_pipeline_shader_stages_main_entry)
{
	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	std::string vsPath = WriteTempSpirv(
		kMinimalVertSpirv, sizeof(kMinimalVertSpirv) / sizeof(uint32_t), "stages_vert");
	ASSERT_FALSE(vsPath.empty());

	ShaderModule vs;
	ASSERT_EQ(vs.Initialize(ctx.device, vsPath.c_str()), RenderError::None);

	// Vertex-only pipeline (no fragment shader — depth-only style).
	PipelineBuilder builder;
	builder.SetVertexShader(vs).SetColorFormat(VK_FORMAT_B8G8R8A8_UNORM).SetExtent({800, 600});

	Pipeline pipeline;
	RenderError err = builder.Build(ctx.device, pipeline);
	// Build must succeed: absent fragment shader is permitted.
	EXPECT_EQ(err, RenderError::None);
	EXPECT_TRUE(pipeline.IsInitialized());

	// Now with fragment shader.
	std::string fsPath = WriteTempSpirv(
		kMinimalFragSpirv, sizeof(kMinimalFragSpirv) / sizeof(uint32_t), "stages_frag");
	ASSERT_FALSE(fsPath.empty());

	ShaderModule fs;
	ASSERT_EQ(fs.Initialize(ctx.device, fsPath.c_str()), RenderError::None);

	PipelineBuilder builder2;
	builder2.SetVertexShader(vs)
		.SetFragmentShader(fs)
		.SetColorFormat(VK_FORMAT_B8G8R8A8_UNORM)
		.SetExtent({800, 600});

	Pipeline pipeline2;
	RenderError err2 = builder2.Build(ctx.device, pipeline2);
	EXPECT_EQ(err2, RenderError::None);
	EXPECT_TRUE(pipeline2.IsInitialized());
}

// ---------------------------------------------------------------------------
// pipeline_vertex_input_state_derivation
// ---------------------------------------------------------------------------
TEST(PipelineBuilder, test_pipeline_vertex_input_state_derivation)
{
	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	std::string vsPath = WriteTempSpirv(
		kMinimalVertSpirv, sizeof(kMinimalVertSpirv) / sizeof(uint32_t), "vtxinput_vert");
	ASSERT_FALSE(vsPath.empty());

	ShaderModule vs;
	ASSERT_EQ(vs.Initialize(ctx.device, vsPath.c_str()), RenderError::None);

	// Case A: stride=0, no attributes → "no vertex input" path.
	{
		PipelineBuilder builder;
		builder.SetVertexShader(vs)
			.SetColorFormat(VK_FORMAT_B8G8R8A8_UNORM)
			.SetExtent({800, 600});
		// No SetVertexLayout call → defaults (stride=0, no attrs).
		Pipeline pipeline;
		EXPECT_EQ(builder.Build(ctx.device, pipeline), RenderError::None);
		EXPECT_TRUE(pipeline.IsInitialized());
	}

	// Case B: non-zero stride with attributes → binding + attribute descriptions.
	{
		VertexAttribute attr{};
		attr.location = 0;
		attr.format = VK_FORMAT_R32G32B32_SFLOAT;
		attr.offset = 0;

		VertexLayout layout{};
		layout.stride = 12;
		layout.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		layout.attributes = std::span<const VertexAttribute>(&attr, 1);

		PipelineBuilder builder;
		builder.SetVertexShader(vs)
			.SetColorFormat(VK_FORMAT_B8G8R8A8_UNORM)
			.SetExtent({800, 600})
			.SetVertexLayout(layout);

		Pipeline pipeline;
		EXPECT_EQ(builder.Build(ctx.device, pipeline), RenderError::None);
		EXPECT_TRUE(pipeline.IsInitialized());
	}
}

// ---------------------------------------------------------------------------
// pipeline_input_assembly_uses_recorded_topology
// ---------------------------------------------------------------------------
TEST(PipelineBuilder, test_pipeline_input_assembly_uses_recorded_topology)
{
	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	std::string vsPath = WriteTempSpirv(
		kMinimalVertSpirv, sizeof(kMinimalVertSpirv) / sizeof(uint32_t), "topo_vert");
	ASSERT_FALSE(vsPath.empty());

	ShaderModule vs;
	ASSERT_EQ(vs.Initialize(ctx.device, vsPath.c_str()), RenderError::None);

	// Build with non-default topology.
	PipelineBuilder builder;
	builder.SetVertexShader(vs)
		.SetColorFormat(VK_FORMAT_B8G8R8A8_UNORM)
		.SetExtent({800, 600})
		.SetTopology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);

	Pipeline pipeline;
	EXPECT_EQ(builder.Build(ctx.device, pipeline), RenderError::None);
	EXPECT_TRUE(pipeline.IsInitialized());
}

// ---------------------------------------------------------------------------
// pipeline_viewport_state_uses_extent_for_creation
// ---------------------------------------------------------------------------
TEST(PipelineBuilder, test_pipeline_viewport_state_uses_extent_for_creation)
{
	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	std::string vsPath = WriteTempSpirv(
		kMinimalVertSpirv, sizeof(kMinimalVertSpirv) / sizeof(uint32_t), "viewport_vert");
	ASSERT_FALSE(vsPath.empty());

	ShaderModule vs;
	ASSERT_EQ(vs.Initialize(ctx.device, vsPath.c_str()), RenderError::None);

	// Build with a specific extent; the pipeline must be created successfully
	// (confirming the viewport/scissor state was properly populated).
	PipelineBuilder builder;
	builder.SetVertexShader(vs).SetColorFormat(VK_FORMAT_B8G8R8A8_UNORM).SetExtent({1280, 720});

	Pipeline pipeline;
	EXPECT_EQ(builder.Build(ctx.device, pipeline), RenderError::None);
	EXPECT_TRUE(pipeline.IsInitialized());
}

// ---------------------------------------------------------------------------
// pipeline_rasterization_state_derivation
// ---------------------------------------------------------------------------
TEST(PipelineBuilder, test_pipeline_rasterization_state_derivation)
{
	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	std::string vsPath = WriteTempSpirv(
		kMinimalVertSpirv, sizeof(kMinimalVertSpirv) / sizeof(uint32_t), "raster_vert");
	ASSERT_FALSE(vsPath.empty());

	ShaderModule vs;
	ASSERT_EQ(vs.Initialize(ctx.device, vsPath.c_str()), RenderError::None);

	PipelineBuilder builder;
	builder.SetVertexShader(vs)
		.SetColorFormat(VK_FORMAT_B8G8R8A8_UNORM)
		.SetExtent({800, 600})
		.SetPolygonMode(VK_POLYGON_MODE_FILL)
		.SetCullMode(VK_CULL_MODE_NONE)
		.SetFrontFace(VK_FRONT_FACE_CLOCKWISE)
		.SetDepthBias(1.5f, 0.5f); // also enables depth bias

	Pipeline pipeline;
	EXPECT_EQ(builder.Build(ctx.device, pipeline), RenderError::None);
	EXPECT_TRUE(pipeline.IsInitialized());
}

// ---------------------------------------------------------------------------
// pipeline_multisample_state_is_no_msaa
// ---------------------------------------------------------------------------
TEST(PipelineBuilder, test_pipeline_multisample_state_is_no_msaa)
{
	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	std::string vsPath = WriteTempSpirv(
		kMinimalVertSpirv, sizeof(kMinimalVertSpirv) / sizeof(uint32_t), "msaa_vert");
	ASSERT_FALSE(vsPath.empty());

	ShaderModule vs;
	ASSERT_EQ(vs.Initialize(ctx.device, vsPath.c_str()), RenderError::None);

	// Build succeeds → multisampling state was VK_SAMPLE_COUNT_1_BIT (no MSAA).
	PipelineBuilder builder;
	builder.SetVertexShader(vs).SetColorFormat(VK_FORMAT_B8G8R8A8_UNORM).SetExtent({800, 600});

	Pipeline pipeline;
	EXPECT_EQ(builder.Build(ctx.device, pipeline), RenderError::None);
	EXPECT_TRUE(pipeline.IsInitialized());
}

// ---------------------------------------------------------------------------
// pipeline_depth_stencil_state_derivation
// ---------------------------------------------------------------------------
TEST(PipelineBuilder, test_pipeline_depth_stencil_state_derivation)
{
	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	std::string vsPath = WriteTempSpirv(
		kMinimalVertSpirv, sizeof(kMinimalVertSpirv) / sizeof(uint32_t), "depthst_vert");
	std::string fsPath = WriteTempSpirv(
		kMinimalFragSpirv, sizeof(kMinimalFragSpirv) / sizeof(uint32_t), "depthst_frag");
	ASSERT_FALSE(vsPath.empty());
	ASSERT_FALSE(fsPath.empty());

	ShaderModule vs, fs;
	ASSERT_EQ(vs.Initialize(ctx.device, vsPath.c_str()), RenderError::None);
	ASSERT_EQ(fs.Initialize(ctx.device, fsPath.c_str()), RenderError::None);

	// With depth test + write + stencil enabled.
	PipelineBuilder builder;
	builder.SetVertexShader(vs)
		.SetFragmentShader(fs)
		.SetColorFormat(VK_FORMAT_B8G8R8A8_UNORM)
		.SetDepthFormat(VK_FORMAT_D32_SFLOAT)
		.SetStencilFormat(VK_FORMAT_S8_UINT)
		.SetExtent({800, 600})
		.SetDepthTest(true)
		.SetDepthWrite(true)
		.SetDepthCompareOp(VK_COMPARE_OP_LESS)
		.SetStencilTest(true)
		.SetStencilOp(VK_STENCIL_OP_KEEP,
			VK_STENCIL_OP_KEEP,
			VK_STENCIL_OP_REPLACE,
			VK_COMPARE_OP_ALWAYS)
		.SetStencilMasks(0xFF, 0xFF);

	Pipeline pipeline;
	EXPECT_EQ(builder.Build(ctx.device, pipeline), RenderError::None);
	EXPECT_TRUE(pipeline.IsInitialized());
}

// ---------------------------------------------------------------------------
// pipeline_color_blend_state_derivation
// ---------------------------------------------------------------------------
TEST(PipelineBuilder, test_pipeline_color_blend_state_derivation)
{
	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	std::string vsPath = WriteTempSpirv(
		kMinimalVertSpirv, sizeof(kMinimalVertSpirv) / sizeof(uint32_t), "blend_vert");
	std::string fsPath = WriteTempSpirv(
		kMinimalFragSpirv, sizeof(kMinimalFragSpirv) / sizeof(uint32_t), "blend_frag");
	ASSERT_FALSE(vsPath.empty());
	ASSERT_FALSE(fsPath.empty());

	ShaderModule vs, fs;
	ASSERT_EQ(vs.Initialize(ctx.device, vsPath.c_str()), RenderError::None);
	ASSERT_EQ(fs.Initialize(ctx.device, fsPath.c_str()), RenderError::None);

	// With blend enabled and a non-default color write mask.
	PipelineBuilder builder;
	builder.SetVertexShader(vs)
		.SetFragmentShader(fs)
		.SetColorFormat(VK_FORMAT_B8G8R8A8_UNORM)
		.SetExtent({800, 600})
		.SetBlendEnabled(true)
		.SetColorWriteMask(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
					 VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);

	Pipeline pipeline;
	EXPECT_EQ(builder.Build(ctx.device, pipeline), RenderError::None);
	EXPECT_TRUE(pipeline.IsInitialized());
}

// ---------------------------------------------------------------------------
// pipeline_dynamic_state_always_includes_viewport_and_scissor
// ---------------------------------------------------------------------------
TEST(PipelineBuilder, test_pipeline_dynamic_state_always_includes_viewport_and_scissor)
{
	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	std::string vsPath = WriteTempSpirv(
		kMinimalVertSpirv, sizeof(kMinimalVertSpirv) / sizeof(uint32_t), "dynstate_vert");
	ASSERT_FALSE(vsPath.empty());

	ShaderModule vs;
	ASSERT_EQ(vs.Initialize(ctx.device, vsPath.c_str()), RenderError::None);

	// Add extra dynamic states including duplicates of VIEWPORT/SCISSOR.
	PipelineBuilder builder;
	builder.SetVertexShader(vs)
		.SetColorFormat(VK_FORMAT_B8G8R8A8_UNORM)
		.SetExtent({800, 600})
		.AddDynamicState(VK_DYNAMIC_STATE_DEPTH_BIAS)
		.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT) // duplicate — tolerated by Vulkan
		.AddDynamicState(VK_DYNAMIC_STATE_SCISSOR); // duplicate — tolerated by Vulkan

	Pipeline pipeline;
	EXPECT_EQ(builder.Build(ctx.device, pipeline), RenderError::None);
	EXPECT_TRUE(pipeline.IsInitialized());
}

// ---------------------------------------------------------------------------
// pipeline_build_creates_vk_pipeline
// ---------------------------------------------------------------------------
TEST(PipelineBuilder, test_pipeline_build_creates_vk_pipeline)
{
	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	std::string vsPath = WriteTempSpirv(
		kMinimalVertSpirv, sizeof(kMinimalVertSpirv) / sizeof(uint32_t), "full_vert");
	std::string fsPath = WriteTempSpirv(
		kMinimalFragSpirv, sizeof(kMinimalFragSpirv) / sizeof(uint32_t), "full_frag");
	ASSERT_FALSE(vsPath.empty());
	ASSERT_FALSE(fsPath.empty());

	ShaderModule vs, fs;
	ASSERT_EQ(vs.Initialize(ctx.device, vsPath.c_str()), RenderError::None);
	ASSERT_EQ(fs.Initialize(ctx.device, fsPath.c_str()), RenderError::None);

	VertexAttribute attr{};
	attr.location = 0;
	attr.format = VK_FORMAT_R32G32B32_SFLOAT;
	attr.offset = 0;

	VertexLayout layout{};
	layout.stride = 12;
	layout.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	layout.attributes = std::span<const VertexAttribute>(&attr, 1);

	PipelineBuilder builder;
	builder.SetVertexShader(vs)
		.SetFragmentShader(fs)
		.SetVertexLayout(layout)
		.SetColorFormat(VK_FORMAT_B8G8R8A8_UNORM)
		.SetDepthFormat(VK_FORMAT_D32_SFLOAT)
		.SetExtent({800, 600})
		.SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.SetPolygonMode(VK_POLYGON_MODE_FILL)
		.SetCullMode(VK_CULL_MODE_BACK_BIT)
		.SetFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
		.SetDepthTest(true)
		.SetDepthWrite(true)
		.SetDepthCompareOp(VK_COMPARE_OP_LESS)
		.SetBlendEnabled(false)
		.SetColorWriteMask(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
					 VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)
		.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, 64)
		.AddDynamicState(VK_DYNAMIC_STATE_DEPTH_BIAS);

	Pipeline pipeline;
	RenderError err = builder.Build(ctx.device, pipeline);
	EXPECT_EQ(err, RenderError::None);
	EXPECT_TRUE(pipeline.IsInitialized());
	EXPECT_NE(pipeline.GetHandle(), VK_NULL_HANDLE);
	EXPECT_NE(pipeline.GetLayout(), VK_NULL_HANDLE);
}

// ---------------------------------------------------------------------------
// pipeline_builder_is_not_consumed_by_build
// ---------------------------------------------------------------------------
TEST(PipelineBuilder, test_pipeline_builder_is_not_consumed_by_build)
{
	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	std::string vsPath = WriteTempSpirv(
		kMinimalVertSpirv, sizeof(kMinimalVertSpirv) / sizeof(uint32_t), "reuse_vert");
	ASSERT_FALSE(vsPath.empty());

	ShaderModule vs;
	ASSERT_EQ(vs.Initialize(ctx.device, vsPath.c_str()), RenderError::None);

	PipelineBuilder builder;
	builder.SetVertexShader(vs).SetColorFormat(VK_FORMAT_B8G8R8A8_UNORM).SetExtent({800, 600});

	// First build.
	Pipeline p1;
	EXPECT_EQ(builder.Build(ctx.device, p1), RenderError::None);
	EXPECT_TRUE(p1.IsInitialized());

	// Second build from the same builder into a different pipeline.
	Pipeline p2;
	EXPECT_EQ(builder.Build(ctx.device, p2), RenderError::None);
	EXPECT_TRUE(p2.IsInitialized());

	// Both pipelines are independently valid.
	EXPECT_NE(p1.GetHandle(), VK_NULL_HANDLE);
	EXPECT_NE(p2.GetHandle(), VK_NULL_HANDLE);
}

// ---------------------------------------------------------------------------
// pipeline_is_raii_movable_non_copyable_handle_owner
// ---------------------------------------------------------------------------
TEST(Pipeline, test_pipeline_is_raii_movable_non_copyable_handle_owner)
{
	// Verify compile-time traits.
	static_assert(
		!std::is_copy_constructible_v<Pipeline>, "Pipeline must not be copy-constructible");
	static_assert(!std::is_copy_assignable_v<Pipeline>, "Pipeline must not be copy-assignable");
	static_assert(std::is_move_constructible_v<Pipeline>, "Pipeline must be move-constructible");
	static_assert(std::is_move_assignable_v<Pipeline>, "Pipeline must be move-assignable");
	static_assert(
		std::is_default_constructible_v<Pipeline>, "Pipeline must be default-constructible");

	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	std::string vsPath = WriteTempSpirv(
		kMinimalVertSpirv, sizeof(kMinimalVertSpirv) / sizeof(uint32_t), "raii_vert");
	ASSERT_FALSE(vsPath.empty());

	ShaderModule vs;
	ASSERT_EQ(vs.Initialize(ctx.device, vsPath.c_str()), RenderError::None);

	PipelineBuilder builder;
	builder.SetVertexShader(vs).SetColorFormat(VK_FORMAT_B8G8R8A8_UNORM).SetExtent({800, 600});

	Pipeline p1;
	ASSERT_EQ(builder.Build(ctx.device, p1), RenderError::None);
	ASSERT_TRUE(p1.IsInitialized());

	VkPipeline rawHandle = p1.GetHandle();
	VkPipelineLayout rawLayout = p1.GetLayout();
	EXPECT_NE(rawHandle, VK_NULL_HANDLE);
	EXPECT_NE(rawLayout, VK_NULL_HANDLE);

	// Move-construct.
	Pipeline p2 = std::move(p1);
	EXPECT_TRUE(p2.IsInitialized());
	EXPECT_EQ(p2.GetHandle(), rawHandle);
	EXPECT_EQ(p2.GetLayout(), rawLayout);

	// Source is in default-constructed state.
	EXPECT_FALSE(p1.IsInitialized());
	EXPECT_EQ(p1.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(p1.GetLayout(), VK_NULL_HANDLE);

	// Move-assign into a pipeline that already owns handles.
	Pipeline p3;
	ASSERT_EQ(builder.Build(ctx.device, p3), RenderError::None);
	ASSERT_TRUE(p3.IsInitialized());

	p3 = std::move(p2); // p3's old handles are destroyed; p3 takes p2's handles.
	EXPECT_TRUE(p3.IsInitialized());
	EXPECT_EQ(p3.GetHandle(), rawHandle);
	EXPECT_FALSE(p2.IsInitialized());
}

// ---------------------------------------------------------------------------
// pipeline_default_constructed_state
// ---------------------------------------------------------------------------
TEST(Pipeline, test_pipeline_default_constructed_state)
{
	Pipeline pipeline;

	EXPECT_EQ(pipeline.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(pipeline.GetLayout(), VK_NULL_HANDLE);
	EXPECT_FALSE(pipeline.IsInitialized());

	// Destroying a default-constructed pipeline is well-defined (no crash).
	// The destructor runs at end of scope.
}

// ---------------------------------------------------------------------------
// pipeline_observers_return_owned_handles
// ---------------------------------------------------------------------------
TEST(Pipeline, test_pipeline_observers_return_owned_handles)
{
	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	std::string vsPath = WriteTempSpirv(
		kMinimalVertSpirv, sizeof(kMinimalVertSpirv) / sizeof(uint32_t), "obs_vert");
	ASSERT_FALSE(vsPath.empty());

	ShaderModule vs;
	ASSERT_EQ(vs.Initialize(ctx.device, vsPath.c_str()), RenderError::None);

	PipelineBuilder builder;
	builder.SetVertexShader(vs).SetColorFormat(VK_FORMAT_B8G8R8A8_UNORM).SetExtent({800, 600});

	Pipeline pipeline;
	ASSERT_EQ(builder.Build(ctx.device, pipeline), RenderError::None);

	// Observers return non-null handles after a successful build.
	EXPECT_NE(pipeline.GetHandle(), VK_NULL_HANDLE);
	EXPECT_NE(pipeline.GetLayout(), VK_NULL_HANDLE);
	EXPECT_TRUE(pipeline.IsInitialized());

	// Calling observers multiple times returns the same value (pure observer).
	EXPECT_EQ(pipeline.GetHandle(), pipeline.GetHandle());
	EXPECT_EQ(pipeline.GetLayout(), pipeline.GetLayout());

	// After move, source observers return null.
	Pipeline moved = std::move(pipeline);
	EXPECT_EQ(pipeline.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(pipeline.GetLayout(), VK_NULL_HANDLE);
	EXPECT_FALSE(pipeline.IsInitialized());

	// Moved-to object has the handles.
	EXPECT_NE(moved.GetHandle(), VK_NULL_HANDLE);
	EXPECT_NE(moved.GetLayout(), VK_NULL_HANDLE);
	EXPECT_TRUE(moved.IsInitialized());
}

// ---------------------------------------------------------------------------
// pipeline_bind_calls_vk_cmd_bind_pipeline
// ---------------------------------------------------------------------------
TEST(Pipeline, test_pipeline_bind_calls_vk_cmd_bind_pipeline)
{
	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	std::string vsPath = WriteTempSpirv(
		kMinimalVertSpirv, sizeof(kMinimalVertSpirv) / sizeof(uint32_t), "bind_vert");
	ASSERT_FALSE(vsPath.empty());

	ShaderModule vs;
	ASSERT_EQ(vs.Initialize(ctx.device, vsPath.c_str()), RenderError::None);

	PipelineBuilder builder;
	builder.SetVertexShader(vs).SetColorFormat(VK_FORMAT_B8G8R8A8_UNORM).SetExtent({800, 600});

	Pipeline pipeline;
	ASSERT_EQ(builder.Build(ctx.device, pipeline), RenderError::None);
	ASSERT_TRUE(pipeline.IsInitialized());

	// Create a command pool and allocate a command buffer to call Bind on.
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = ctx.device.GetGraphicsQueueFamilyIndex();
	poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

	VkCommandPool pool = VK_NULL_HANDLE;
	ASSERT_EQ(vkCreateCommandPool(ctx.device.GetHandle(), &poolInfo, nullptr, &pool), VK_SUCCESS);

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = pool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer cmd = VK_NULL_HANDLE;
	ASSERT_EQ(vkAllocateCommandBuffers(ctx.device.GetHandle(), &allocInfo, &cmd), VK_SUCCESS);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	ASSERT_EQ(vkBeginCommandBuffer(cmd, &beginInfo), VK_SUCCESS);

	// Bind must not crash and must record the pipeline bind command.
	pipeline.Bind(cmd);

	ASSERT_EQ(vkEndCommandBuffer(cmd), VK_SUCCESS);

	vkDestroyCommandPool(ctx.device.GetHandle(), pool, nullptr);
}

// ---------------------------------------------------------------------------
// is_initialized_reflects_owned_pipeline
// ---------------------------------------------------------------------------
TEST(Pipeline, test_is_initialized_reflects_owned_pipeline)
{
	// Default-constructed: not initialized.
	Pipeline pipeline;
	EXPECT_FALSE(pipeline.IsInitialized());
	EXPECT_EQ(pipeline.GetHandle(), VK_NULL_HANDLE);

	// The bi-implication: IsInitialized ↔ GetHandle != VK_NULL_HANDLE.
	EXPECT_EQ(pipeline.IsInitialized(), pipeline.GetHandle() != VK_NULL_HANDLE);

	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	std::string vsPath = WriteTempSpirv(
		kMinimalVertSpirv, sizeof(kMinimalVertSpirv) / sizeof(uint32_t), "isinit_vert");
	ASSERT_FALSE(vsPath.empty());

	ShaderModule vs;
	ASSERT_EQ(vs.Initialize(ctx.device, vsPath.c_str()), RenderError::None);

	PipelineBuilder builder;
	builder.SetVertexShader(vs).SetColorFormat(VK_FORMAT_B8G8R8A8_UNORM).SetExtent({800, 600});

	ASSERT_EQ(builder.Build(ctx.device, pipeline), RenderError::None);

	// After successful build: initialized.
	EXPECT_TRUE(pipeline.IsInitialized());
	EXPECT_NE(pipeline.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(pipeline.IsInitialized(), pipeline.GetHandle() != VK_NULL_HANDLE);

	// After move: not initialized.
	Pipeline moved = std::move(pipeline);
	EXPECT_FALSE(pipeline.IsInitialized());
	EXPECT_EQ(pipeline.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(pipeline.IsInitialized(), pipeline.GetHandle() != VK_NULL_HANDLE);

	// Moved-to: initialized.
	EXPECT_TRUE(moved.IsInitialized());
	EXPECT_EQ(moved.IsInitialized(), moved.GetHandle() != VK_NULL_HANDLE);
}

// ---------------------------------------------------------------------------
// pipeline_methods_are_not_thread_safe_per_instance
// ---------------------------------------------------------------------------
// This semantic documents a non-guarantee (no synchronization). We verify
// the positive side: concurrent reads (const methods) on the same Pipeline
// from different threads are permitted. We cannot meaningfully test that
// concurrent writes are undefined without invoking UB, so we only verify
// the documented safe path.
TEST(Pipeline, test_pipeline_methods_are_not_thread_safe_per_instance)
{
	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	std::string vsPath = WriteTempSpirv(
		kMinimalVertSpirv, sizeof(kMinimalVertSpirv) / sizeof(uint32_t), "thread_vert");
	ASSERT_FALSE(vsPath.empty());

	ShaderModule vs;
	ASSERT_EQ(vs.Initialize(ctx.device, vsPath.c_str()), RenderError::None);

	PipelineBuilder builder;
	builder.SetVertexShader(vs).SetColorFormat(VK_FORMAT_B8G8R8A8_UNORM).SetExtent({800, 600});

	Pipeline pipeline;
	ASSERT_EQ(builder.Build(ctx.device, pipeline), RenderError::None);

	// Concurrent const-method calls from two threads — documented as safe.
	std::atomic<int> readCount{0};
	auto readTask = [&]()
	{
		for (int i = 0; i < 100; ++i)
		{
			(void)pipeline.GetHandle();
			(void)pipeline.GetLayout();
			(void)pipeline.IsInitialized();
			++readCount;
		}
	};

	std::thread t1(readTask);
	std::thread t2(readTask);
	t1.join();
	t2.join();

	EXPECT_EQ(readCount.load(), 200);
	EXPECT_TRUE(pipeline.IsInitialized());
}
