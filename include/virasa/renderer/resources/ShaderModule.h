#ifndef VIRASA_RENDERER_RESOURCES_SHADER_MODULE_H
#define VIRASA_RENDERER_RESOURCES_SHADER_MODULE_H

#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Device.h"

#include "vulkan/vulkan.h"

namespace virasa
{

/**
 * @brief Owns a single VkShaderModule handle loaded from a SPIR-V file on disk.
 *
 * ShaderModule is a movable, non-copyable RAII wrapper around a VkShaderModule.
 * It borrows the VkDevice from the Device passed to Initialize and uses it
 * during destruction to call vkDestroyShaderModule. The Device must outlive
 * any ShaderModule initialized against it.
 */
class ShaderModule final
{
public:
	/// @brief Default-constructs a ShaderModule that owns no Vulkan handle.
	ShaderModule() noexcept = default;

	/// @brief Destroys the owned VkShaderModule, if any.
	~ShaderModule() noexcept;

	// Not copyable
	ShaderModule(const ShaderModule&) = delete;
	ShaderModule& operator=(const ShaderModule&) = delete;

	/// @brief Move-constructs, transferring ownership from \p other.
	ShaderModule(ShaderModule&& other) noexcept;

	/// @brief Move-assigns, destroying any currently-owned handle first, then transferring ownership.
	ShaderModule& operator=(ShaderModule&& other) noexcept;

	/**
	 * @brief Loads a SPIR-V file and creates a VkShaderModule.
	 *
	 * If this ShaderModule already owns a handle it is destroyed before the new
	 * one is created. On failure the ShaderModule is left in the default-constructed
	 * state.
	 *
	 * @param device    The logical device to create the shader module against.
	 * @param file_path Path to a compiled SPIR-V binary (.spv).
	 * @return RenderError::None on success.
	 * @return RenderError::ShaderLoadFailed if the file cannot be opened, has an
	 *         invalid size, or cannot be read.
	 * @return RenderError::ShaderCreateFailed if vkCreateShaderModule fails.
	 */
	[[nodiscard]] RenderError Initialize(const Device& device, const char* file_path);

	/**
	 * @brief Returns the owned VkShaderModule handle, or VK_NULL_HANDLE.
	 * @return The VkShaderModule handle, or VK_NULL_HANDLE if not initialized.
	 */
	[[nodiscard]] VkShaderModule GetHandle() const noexcept;

	/**
	 * @brief Returns true if this ShaderModule currently owns a VkShaderModule handle.
	 * @return true if initialized, false otherwise.
	 */
	[[nodiscard]] bool IsInitialized() const noexcept;

private:
	VkDevice       _device        = VK_NULL_HANDLE;
	VkShaderModule _shaderModule  = VK_NULL_HANDLE;
};

} // namespace virasa

#endif // VIRASA_RENDERER_RESOURCES_SHADER_MODULE_H
