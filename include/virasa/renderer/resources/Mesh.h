#ifndef VIRASA_RENDERER_RESOURCES_MESH_H
#define VIRASA_RENDERER_RESOURCES_MESH_H

#include <vulkan/vulkan.h>

#include <cstdint>
#include <utility>

#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Context.h"
#include "virasa/renderer/core/Device.h"
#include "virasa/renderer/resources/Buffer.h"

namespace virasa
{

/**
 * @brief Owns GPU vertex and index buffers for a mesh.
 */
class Mesh final
{
	public:
	Mesh() = default;
	Mesh(const Mesh&) = delete;
	Mesh& operator=(const Mesh&) = delete;
	Mesh(Mesh&& other) noexcept
		: _vertexBuffer(std::move(other._vertexBuffer))
		, _indexBuffer(std::move(other._indexBuffer))
		, _indexCount(std::exchange(other._indexCount, 0))
	{
	}
	Mesh& operator=(Mesh&& other) noexcept
	{
		if (this != &other)
		{
			_vertexBuffer = std::move(other._vertexBuffer);
			_indexBuffer = std::move(other._indexBuffer);
			_indexCount = std::exchange(other._indexCount, 0);
		}
		return *this;
	}
	~Mesh() = default;

	/**
	 * @brief Upload mesh geometry into device-local vertex and index buffers.
	 * @param device Device used to create buffers and query device addresses.
	 * @param context Rendering context providing transfer resources.
	 * @param mesh_data CPU-side mesh vertex and index data.
	 * @return RenderError::None on success, otherwise the buffer creation or upload error.
	 */
	[[nodiscard]] RenderError Initialize(
		const Device& device, const Context& context, const MeshData& mesh_data);

	/**
	 * @brief Get the device address of the vertex buffer.
	 * @param device Device the mesh was initialized against.
	 * @return Vertex buffer device address.
	 */
	[[nodiscard]] VkDeviceAddress GetVertexBufferAddress(const Device& device) const noexcept;

	/**
	 * @brief Get the device address of the index buffer.
	 * @param device Device the mesh was initialized against.
	 * @return Index buffer device address.
	 */
	[[nodiscard]] VkDeviceAddress GetIndexBufferAddress(const Device& device) const noexcept;

	/**
	 * @brief Get the cached number of indices.
	 * @return Index count from the most recent successful initialization.
	 */
	[[nodiscard]] uint32_t GetIndexCount() const noexcept;

	/**
	 * @brief Check whether both mesh buffers are initialized.
	 * @return True when both vertex and index buffers are initialized.
	 */
	[[nodiscard]] bool IsInitialized() const noexcept;

	private:
	Buffer _vertexBuffer;
	Buffer _indexBuffer;
	uint32_t _indexCount = 0;
};

} // namespace virasa

#endif
