#include "virasa/renderer/core/Context.h"

#include <cassert>
#include <utility>

#include "quill/LogMacros.h"
#include "virasa/core/Logger.h"
#include "virasa/window/Platform.h"
#include "vulkan/vulkan.h"

namespace virasa
{
// ---------------------------------------------------------------------------
// Destructor / move
// ---------------------------------------------------------------------------

Context::~Context()
{
	Shutdown();
}

Context::Context(Context&& other) noexcept
    : _platform(other._platform), _config(other._config), _instance(std::move(other._instance)),
	_surface(std::move(other._surface)), _device(std::move(other._device)),
	_swapchain(std::move(other._swapchain)), _commandPool(other._commandPool),
	_commandBuffers(std::move(other._commandBuffers)), _frameData(std::move(other._frameData)),
	_currentFrameIndex(other._currentFrameIndex), _acquiredImageIndex(other._acquiredImageIndex),
	_isReady(other._isReady)
{
	other._platform = nullptr;
	other._commandPool = VK_NULL_HANDLE;
	other._currentFrameIndex = 0;
	other._acquiredImageIndex = 0;
	other._isReady = false;
}

Context& Context::operator=(Context&& other) noexcept
{
	if (this != &other)
	{
		Shutdown();

		_platform = other._platform;
		_config = other._config;
		_instance = std::move(other._instance);
		_surface = std::move(other._surface);
		_device = std::move(other._device);
		_swapchain = std::move(other._swapchain);
		_commandPool = other._commandPool;
		_commandBuffers = std::move(other._commandBuffers);
		_frameData = std::move(other._frameData);
		_currentFrameIndex = other._currentFrameIndex;
		_acquiredImageIndex = other._acquiredImageIndex;
		_isReady = other._isReady;

		other._platform = nullptr;
		other._commandPool = VK_NULL_HANDLE;
		other._currentFrameIndex = 0;
		other._acquiredImageIndex = 0;
		other._isReady = false;
	}
	return *this;
}

// ---------------------------------------------------------------------------
// Initialize
// ---------------------------------------------------------------------------

RenderError Context::Initialize(virasa::window::Platform& platform, const RendererConfig& config)
{
	_platform = &platform;
	_config = config;

	// Step 1: Initialize Instance
	{
		RenderError err = _instance.Initialize(config);
		if (err != RenderError::None)
		{
			return err;
		}
	}

	// Step 2: Initialize Surface (creates VkSurfaceKHR only)
	{
		RenderError err = _surface.Initialize(_instance, platform);
		if (err != RenderError::None)
		{
			return err;
		}
	}

	// Step 3: Initialize Device
	{
		RenderError err = _device.Initialize(_instance, _surface.GetHandle());
		if (err != RenderError::None)
		{
			return err;
		}
	}

	// Step 4: Query surface formats and present modes
	_surface.QueryFormatsAndModes(_device.GetPhysicalDevice(), config);

	// Step 5: Initialize Swapchain
	{
		Size2D windowSize = platform.GetWindowSize();
		RenderError err = _swapchain.Initialize(_device, _surface, windowSize, config);
		if (err != RenderError::None)
		{
			return err;
		}
	}

	// Step 6: Create graphics command pool and allocate command buffers
	{
		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = _device.GetQueueFamilies().graphicsFamily;

		VkResult result =
			vkCreateCommandPool(_device.GetHandle(), &poolInfo, nullptr, &_commandPool);
		if (result != VK_SUCCESS)
		{
			return RenderError::CommandPoolCreateFailed;
		}

		_commandBuffers.resize(config.maxFramesInFlight);

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = _commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = config.maxFramesInFlight;

		result =
			vkAllocateCommandBuffers(_device.GetHandle(), &allocInfo, _commandBuffers.data());
		if (result != VK_SUCCESS)
		{
			return RenderError::CommandPoolCreateFailed;
		}
	}

	// Step 7: Create per-frame synchronization objects
	{
		_frameData.resize(config.maxFramesInFlight);

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (uint32_t i = 0; i < config.maxFramesInFlight; ++i)
		{
			VkResult r1 = vkCreateSemaphore(_device.GetHandle(),
				&semaphoreInfo,
				nullptr,
				&_frameData[i].imageAvailableSemaphore);
			VkResult r2 = vkCreateSemaphore(_device.GetHandle(),
				&semaphoreInfo,
				nullptr,
				&_frameData[i].renderFinishedSemaphore);
			VkResult r3 = vkCreateFence(
				_device.GetHandle(), &fenceInfo, nullptr, &_frameData[i].inFlightFence);

			if (r1 != VK_SUCCESS || r2 != VK_SUCCESS || r3 != VK_SUCCESS)
			{
				return RenderError::FenceCreateFailed;
			}
		}
	}

	_isReady = true;
	_currentFrameIndex = 0;

	auto* logger = Logger::GetLogger("renderer");
	LOG_INFO(logger, "Context initialized successfully.");

	return RenderError::None;
}

// ---------------------------------------------------------------------------
// Shutdown
// ---------------------------------------------------------------------------

void Context::DestroyPerFrameResources()
{
	VkDevice device = _device.GetHandle();
	if (device == VK_NULL_HANDLE)
	{
		return;
	}

	for (auto& frame : _frameData)
	{
		if (frame.inFlightFence != VK_NULL_HANDLE)
		{
			vkDestroyFence(device, frame.inFlightFence, nullptr);
			frame.inFlightFence = VK_NULL_HANDLE;
		}
		if (frame.renderFinishedSemaphore != VK_NULL_HANDLE)
		{
			vkDestroySemaphore(device, frame.renderFinishedSemaphore, nullptr);
			frame.renderFinishedSemaphore = VK_NULL_HANDLE;
		}
		if (frame.imageAvailableSemaphore != VK_NULL_HANDLE)
		{
			vkDestroySemaphore(device, frame.imageAvailableSemaphore, nullptr);
			frame.imageAvailableSemaphore = VK_NULL_HANDLE;
		}
	}
	_frameData.clear();

	if (_commandPool != VK_NULL_HANDLE)
	{
		vkDestroyCommandPool(device, _commandPool, nullptr);
		_commandPool = VK_NULL_HANDLE;
		_commandBuffers.clear();
	}
}

void Context::Shutdown()
{
	if (!_isReady && _device.GetHandle() == VK_NULL_HANDLE)
	{
		// Nothing to do — not even partially initialized
		_platform = nullptr;
		_currentFrameIndex = 0;
		return;
	}

	_device.WaitIdle();

	DestroyPerFrameResources();

	// Release owned stack in reverse dependency order
	_swapchain = Swapchain{};
	_device = Device{};
	_surface = Surface{};
	_instance = Instance{};

	_isReady = false;
	_currentFrameIndex = 0;
	_platform = nullptr;
}

// ---------------------------------------------------------------------------
// Internal swapchain recreation
// ---------------------------------------------------------------------------

void Context::RecreateSwapchain()
{
	_device.WaitIdle();

	Size2D windowSize = _platform->GetWindowSize();
	RenderError err = _swapchain.Recreate(_device, _surface, windowSize, _config);
	if (err != RenderError::None)
	{
		auto* logger = Logger::GetLogger("renderer");
		// Log using ostream insertion operator via a stringstream
		// We format the error into a string first
		LOG_ERROR(logger, "Swapchain recreation failed.");
	}
}

// ---------------------------------------------------------------------------
// BeginFrame
// ---------------------------------------------------------------------------

SwapchainStatus Context::BeginFrame()
{
	// Step 1: minimize check
	if (_platform->IsMinimized())
	{
		return SwapchainStatus::NotReady;
	}

	const FrameData& frame = _frameData[_currentFrameIndex];

	// Step 2: fence wait
	vkWaitForFences(_device.GetHandle(), 1, &frame.inFlightFence, VK_TRUE, UINT64_MAX);

	// Step 3: acquire with retry-once
	SwapchainStatus acquireStatus = _swapchain.AcquireNextImage(
		_device, frame.imageAvailableSemaphore, &_acquiredImageIndex);

	bool wasRecreated = false;

	if (acquireStatus == SwapchainStatus::Recreated)
	{
		RecreateSwapchain();
		wasRecreated = true;

		acquireStatus = _swapchain.AcquireNextImage(
			_device, frame.imageAvailableSemaphore, &_acquiredImageIndex);

		if (acquireStatus == SwapchainStatus::Recreated)
		{
			return SwapchainStatus::NotReady;
		}
		if (acquireStatus == SwapchainStatus::Error)
		{
			auto* logger = Logger::GetLogger("renderer");
			LOG_ERROR(logger, "Image acquisition failed after swapchain recreation.");
			return SwapchainStatus::Error;
		}
	}
	else if (acquireStatus == SwapchainStatus::Error)
	{
		return SwapchainStatus::Error;
	}

	// Step 4: reset fence only after successful acquire
	vkResetFences(_device.GetHandle(), 1, &frame.inFlightFence);

	// Step 5: reset and begin command buffer
	VkCommandBuffer cmd = _commandBuffers[_currentFrameIndex];
	vkResetCommandBuffer(cmd, 0);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &beginInfo);

	return wasRecreated ? SwapchainStatus::Recreated : SwapchainStatus::Success;
}

// ---------------------------------------------------------------------------
// EndFrame
// ---------------------------------------------------------------------------

SwapchainStatus Context::EndFrame()
{
	const FrameData& frame = _frameData[_currentFrameIndex];
	VkCommandBuffer cmd = _commandBuffers[_currentFrameIndex];

	// Step 1: end command buffer
	vkEndCommandBuffer(cmd);

	// Step 2: submit
	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &frame.imageAvailableSemaphore;
	submitInfo.pWaitDstStageMask = &waitStage;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmd;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &frame.renderFinishedSemaphore;

	VkResult submitResult =
		vkQueueSubmit(_device.GetGraphicsQueue(), 1, &submitInfo, frame.inFlightFence);

	if (submitResult != VK_SUCCESS)
	{
		auto* logger = Logger::GetLogger("renderer");
		LOG_ERROR(logger, "vkQueueSubmit failed.");
		return SwapchainStatus::Error;
	}

	// Step 3: present
	SwapchainStatus presentStatus = _swapchain.Present(
		_device.GetPresentQueue(), frame.renderFinishedSemaphore, _acquiredImageIndex);

	if (presentStatus == SwapchainStatus::Recreated)
	{
		RecreateSwapchain();
	}

	// Step 4: advance frame index
	_currentFrameIndex = (_currentFrameIndex + 1) % _config.maxFramesInFlight;

	return presentStatus;
}

// ---------------------------------------------------------------------------
// Observers
// ---------------------------------------------------------------------------

VkCommandBuffer Context::GetCurrentCommandBuffer() const noexcept
{
	if (_commandBuffers.empty())
	{
		return VK_NULL_HANDLE;
	}
	return _commandBuffers[_currentFrameIndex];
}

VkImageView Context::GetCurrentImageView() const noexcept
{
	auto views = _swapchain.GetImageViews();
	if (_acquiredImageIndex >= views.size())
	{
		return VK_NULL_HANDLE;
	}
	return views[_acquiredImageIndex];
}

VkImage Context::GetCurrentImage() const noexcept
{
	auto images = _swapchain.GetImages();
	if (_acquiredImageIndex >= images.size())
	{
		return VK_NULL_HANDLE;
	}
	return images[_acquiredImageIndex];
}

VkFormat Context::GetSwapchainFormat() const noexcept
{
	return _swapchain.GetFormat();
}

VkExtent2D Context::GetSwapchainExtent() const noexcept
{
	return _swapchain.GetExtent();
}

const Device& Context::GetDevice() const noexcept
{
	return _device;
}

const Instance& Context::GetInstance() const noexcept
{
	return _instance;
}

const Swapchain& Context::GetSwapchain() const noexcept
{
	return _swapchain;
}

VkCommandPool Context::GetGraphicsCommandPool() const noexcept
{
	return _commandPool;
}

uint32_t Context::GetCurrentFrameIndex() const noexcept
{
	return _currentFrameIndex;
}

uint32_t Context::GetMaxFramesInFlight() const noexcept
{
	return _config.maxFramesInFlight;
}

bool Context::IsReady() const noexcept
{
	return _isReady;
}

} // namespace virasa
