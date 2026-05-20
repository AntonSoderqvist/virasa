#include "virasa/renderer/resources/ShaderModule.h"

#include <fstream>
#include <vector>

#include "quill/LogMacros.h"
#include "virasa/core/Logger.h"
#include "vulkan/vulkan.h"

namespace virasa
{

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------

ShaderModule::~ShaderModule() noexcept
{
	if (_shaderModule != VK_NULL_HANDLE)
	{
		vkDestroyShaderModule(_device, _shaderModule, nullptr);
		_shaderModule = VK_NULL_HANDLE;
		_device = VK_NULL_HANDLE;
	}
}

// ---------------------------------------------------------------------------
// Move semantics
// ---------------------------------------------------------------------------

ShaderModule::ShaderModule(ShaderModule&& other) noexcept
    : _device(other._device), _shaderModule(other._shaderModule)
{
	other._device = VK_NULL_HANDLE;
	other._shaderModule = VK_NULL_HANDLE;
}

ShaderModule& ShaderModule::operator=(ShaderModule&& other) noexcept
{
	if (this != &other)
	{
		// Destroy currently owned handle, if any
		if (_shaderModule != VK_NULL_HANDLE)
		{
			vkDestroyShaderModule(_device, _shaderModule, nullptr);
		}

		_device = other._device;
		_shaderModule = other._shaderModule;
		other._device = VK_NULL_HANDLE;
		other._shaderModule = VK_NULL_HANDLE;
	}
	return *this;
}

// ---------------------------------------------------------------------------
// Initialize
// ---------------------------------------------------------------------------

RenderError ShaderModule::Initialize(const Device& device, const char* file_path)
{
	// Pre-emptive cleanup: destroy any currently-owned handle unconditionally
	if (_shaderModule != VK_NULL_HANDLE)
	{
		vkDestroyShaderModule(_device, _shaderModule, nullptr);
		_shaderModule = VK_NULL_HANDLE;
	}
	_device = VK_NULL_HANDLE;

	auto* logger = Logger::GetLogger("renderer");

	// Validation 1: file_path must be non-null and non-empty
	if (file_path == nullptr || file_path[0] == '\0')
	{
		LOG_ERROR(logger, "ShaderModule::Initialize: file_path is null or empty");
		return RenderError::ShaderLoadFailed;
	}

	// Validation 2: file must be openable in binary mode
	std::ifstream file(file_path, std::ios::binary | std::ios::ate);
	if (!file.is_open())
	{
		LOG_ERROR(logger, "ShaderModule::Initialize: failed to open file '{}'", file_path);
		return RenderError::ShaderLoadFailed;
	}

	// Validation 3: file size must be > 0 and a multiple of sizeof(uint32_t)
	const std::streamoff fileSize = file.tellg();
	if (fileSize <= 0 || (static_cast<std::size_t>(fileSize) % sizeof(uint32_t)) != 0)
	{
		LOG_ERROR(logger,
			"ShaderModule::Initialize: file '{}' has invalid size {}",
			file_path,
			static_cast<long long>(fileSize));
		return RenderError::ShaderLoadFailed;
	}

	const std::size_t byteCount = static_cast<std::size_t>(fileSize);

	// Validation 4: read the file contents
	std::vector<char> buffer(byteCount);
	file.seekg(0);
	file.read(buffer.data(), static_cast<std::streamsize>(byteCount));
	if (!file || static_cast<std::size_t>(file.gcount()) != byteCount)
	{
		LOG_ERROR(logger,
			"ShaderModule::Initialize: failed to read file '{}' (read {} of {} bytes)",
			file_path,
			static_cast<long long>(file.gcount()),
			byteCount);
		return RenderError::ShaderLoadFailed;
	}

	// All validations passed — create the VkShaderModule
	VkDevice vkDevice = device.GetHandle();

	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = byteCount;
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
	createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

	VkShaderModule shaderModule = VK_NULL_HANDLE;
	const VkResult result = vkCreateShaderModule(vkDevice, &createInfo, nullptr, &shaderModule);
	if (result != VK_SUCCESS)
	{
		LOG_ERROR(logger,
			"ShaderModule::Initialize: vkCreateShaderModule failed for '{}' (VkResult {})",
			file_path,
			static_cast<int>(result));
		// _device and _shaderModule are already cleared from the pre-emptive cleanup
		return RenderError::ShaderCreateFailed;
	}

	_device = vkDevice;
	_shaderModule = shaderModule;
	return RenderError::None;
}

// ---------------------------------------------------------------------------
// Observers
// ---------------------------------------------------------------------------

VkShaderModule ShaderModule::GetHandle() const noexcept
{
	return _shaderModule;
}

bool ShaderModule::IsInitialized() const noexcept
{
	return _shaderModule != VK_NULL_HANDLE;
}

} // namespace virasa
