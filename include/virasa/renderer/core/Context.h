#ifndef VIRASA_RENDERER_CORE_CONTEXT_H
#define VIRASA_RENDERER_CORE_CONTEXT_H

#include <cstdint>
#include <vector>

#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Device.h"
#include "virasa/renderer/core/Instance.h"
#include "virasa/renderer/core/Surface.h"
#include "virasa/renderer/core/Swapchain.h"
#include "virasa/renderer/resources/Image.h"
#include "vulkan/vulkan.h"

namespace virasa
{

namespace window
{
class Platform;
}

/**
 * @brief Owns the full core Vulkan stack and per-frame resources.
 *
 * Context is the top-level renderer object. It owns an Instance, Surface,
 * Device, and Swapchain, plus per-frame command buffers and synchronization
 * objects. It is final, non-copyable, and movable.
 *
 * Callers must check IsReady() before entering the BeginFrame/EndFrame loop.
 * The Platform passed to Initialize must outlive any ready Context initialized
 * against it.
 */
class Context final
{
	public:
	/**
	 * @brief Default-constructs a Context that owns nothing and is not ready.
	 */
	Context() = default;

	/**
	 * @brief Destructor. Performs the same teardown as Shutdown().
	 */
	~Context();

	Context(const Context&) = delete;
	Context& operator=(const Context&) = delete;

	/**
	 * @brief Move constructor. Transfers ownership of the entire stack.
	 * @param other The source Context. After the move, other is not ready.
	 */
	Context(Context&& other) noexcept;

	/**
	 * @brief Move assignment. Shuts down this Context if ready, then takes ownership.
	 * @param other The source Context. After the move, other is not ready.
	 * @return Reference to this.
	 */
	Context& operator=(Context&& other) noexcept;

	/**
	 * @brief Builds the full Vulkan stack and prepares per-frame resources.
	 * @param platform An initialized Platform. Must outlive this Context while ready.
	 * @param config Renderer configuration.
	 * @return RenderError::None on success, or the first error encountered.
	 */
	[[nodiscard]] RenderError Initialize(
		virasa::window::Platform& platform, const RendererConfig& config);

	/**
	 * @brief Tears down all resources and returns the Context to the not-ready state.
	 */
	void Shutdown();

	/**
	 * @brief Prepares the renderer to record commands for one frame.
	 * @return SwapchainStatus indicating how the caller should proceed.
	 */
	[[nodiscard]] SwapchainStatus BeginFrame();

	/**
	 * @brief Finishes recording, submits, and presents the current frame.
	 * @return SwapchainStatus from the present step.
	 */
	[[nodiscard]] SwapchainStatus EndFrame();

	/**
	 * @brief Returns the command buffer for the current frame-in-flight index.
	 * @return VkCommandBuffer in the recording state between BeginFrame and EndFrame.
	 */
	[[nodiscard]] VkCommandBuffer GetCurrentCommandBuffer() const noexcept;

	/**
	 * @brief Returns the image view of the most recently acquired swapchain image.
	 * @return VkImageView valid between a successful BeginFrame and EndFrame.
	 */
	[[nodiscard]] VkImageView GetCurrentImageView() const noexcept;

	/**
	 * @brief Returns the VkImage of the most recently acquired swapchain image.
	 * @return VkImage valid between a successful BeginFrame and EndFrame.
	 */
	[[nodiscard]] VkImage GetCurrentImage() const noexcept;

	/**
	 * @brief Returns the swapchain image format.
	 * @return VkFormat reflecting the most recent swapchain creation or recreation.
	 */
	[[nodiscard]] VkFormat GetSwapchainFormat() const noexcept;

	/**
	 * @brief Returns the current swapchain extent.
	 * @return VkExtent2D reflecting the most recent swapchain creation or recreation.
	 */
	[[nodiscard]] VkExtent2D GetSwapchainExtent() const noexcept;

	/**
	 * @brief Returns a const reference to the owned Device.
	 * @return Reference valid while the Context is ready.
	 */
	[[nodiscard]] const Device& GetDevice() const noexcept;

	/**
	 * @brief Returns a const reference to the owned Instance.
	 * @return Reference valid while the Context is ready.
	 */
	[[nodiscard]] const Instance& GetInstance() const noexcept;

	/**
	 * @brief Returns a const reference to the owned Swapchain.
	 * @return Reference valid while the Context is ready.
	 */
	[[nodiscard]] const Swapchain& GetSwapchain() const noexcept;

	/**
	 * @brief Returns the VkImageView of the owned depth image.
	 * @return VkImageView valid until the next swapchain recreation, Shutdown, or destruction.
	 */
	[[nodiscard]] VkImageView GetDepthImageView() const noexcept;

	/**
	 * @brief Returns the VkFormat used for the depth image.
	 * @return VkFormat equal to config.depthFormat supplied to Initialize.
	 */
	[[nodiscard]] VkFormat GetDepthFormat() const noexcept;

	/**
	 * @brief Returns the graphics command pool used for per-frame command buffers.
	 * @return VkCommandPool valid while the Context is ready.
	 */
	[[nodiscard]] VkCommandPool GetGraphicsCommandPool() const noexcept;

	/**
	 * @brief Returns the transfer command pool intended for staging/upload command buffers.
	 * @return VkCommandPool valid while the Context is ready.
	 */
	[[nodiscard]] VkCommandPool GetTransferCommandPool() const noexcept;

	/**
	 * @brief Returns the current frame-in-flight index.
	 * @return Index in [0, GetMaxFramesInFlight()). Returns 0 on a non-ready Context.
	 */
	[[nodiscard]] uint32_t GetCurrentFrameIndex() const noexcept;

	/**
	 * @brief Returns the number of frames-in-flight from the RendererConfig.
	 * @return maxFramesInFlight from the config supplied to Initialize.
	 */
	[[nodiscard]] uint32_t GetMaxFramesInFlight() const noexcept;

	/**
	 * @brief Returns true if Initialize completed successfully and no Shutdown has occurred.
	 * @return true if the Context is ready to render.
	 */
	[[nodiscard]] bool IsReady() const noexcept;

	private:
	struct FrameData
	{
		VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
		VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
		VkFence inFlightFence = VK_NULL_HANDLE;
	};

	void RecreateSwapchain();
	void DestroyPerFrameResources();

	virasa::window::Platform* _platform = nullptr;
	RendererConfig _config{};

	Instance _instance{};
	Surface _surface{};
	Device _device{};
	Swapchain _swapchain{};

	Image _depthImage{}; ///< Depth attachment shared across frames-in-flight.
	VkCommandPool _commandPool = VK_NULL_HANDLE;
	VkCommandPool _transferCommandPool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> _commandBuffers{};
	std::vector<FrameData> _frameData{};

	uint32_t _currentFrameIndex = 0;
	uint32_t _acquiredImageIndex = 0;
	bool _isReady = false;
};

} // namespace virasa

#endif // VIRASA_RENDERER_CORE_CONTEXT_H
