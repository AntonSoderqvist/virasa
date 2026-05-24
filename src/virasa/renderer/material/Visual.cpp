#include "virasa/renderer/material/Visual.h"

#include <cstring>
#include <utility>

#include "quill/LogMacros.h"
#include "virasa/core/Logger.h"
#include "vulkan/vulkan.h"

namespace virasa
{

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

VisualMaterialGPU VisualMaterialTable::BuildGPURecord(const VisualMaterial& material)
{
	VisualMaterialGPU record{};
	record.factors = material.factors;
	record.factors.alphaModeBits = static_cast<uint32_t>(material.alphaMode);
	// Padding inside factors is already zero from default construction
	record.factors._pad0 = 0.0f;
	record.factors._pad1 = 0.0f;
	record.factors._pad2 = 0.0f;
	record.baseColorTexture = material.baseColorTexture;
	record.normalTexture = material.normalTexture;
	record.metallicRoughnessTexture = material.metallicRoughnessTexture;
	record.occlusionTexture = material.occlusionTexture;
	record.emissiveTexture = material.emissiveTexture;
	record._pad3 = 0u;
	record._pad4 = 0u;
	record._pad5 = 0u;
	return record;
}

// ---------------------------------------------------------------------------
// Teardown
// ---------------------------------------------------------------------------

void VisualMaterialTable::Teardown()
{
	_mapped = nullptr;
	_bufferAddress = 0;
	_maxMaterials = 0;
	_rasterStates.clear();
	_freeSlots.clear();
	// Buffer's own destructor / move handles Vulkan resource release.
	// We replace it with a default-constructed (empty) Buffer.
	_buffer = Buffer{};
}

// ---------------------------------------------------------------------------
// Special members
// ---------------------------------------------------------------------------

VisualMaterialTable::VisualMaterialTable(VisualMaterialTable&& other) noexcept
    : _buffer(std::move(other._buffer)), _mapped(other._mapped),
	_bufferAddress(other._bufferAddress), _maxMaterials(other._maxMaterials),
	_rasterStates(std::move(other._rasterStates)), _freeSlots(std::move(other._freeSlots))
{
	other._mapped = nullptr;
	other._bufferAddress = 0;
	other._maxMaterials = 0;
}

VisualMaterialTable& VisualMaterialTable::operator=(VisualMaterialTable&& other) noexcept
{
	if (this != &other)
	{
		Teardown();
		_buffer = std::move(other._buffer);
		_mapped = other._mapped;
		_bufferAddress = other._bufferAddress;
		_maxMaterials = other._maxMaterials;
		_rasterStates = std::move(other._rasterStates);
		_freeSlots = std::move(other._freeSlots);
		other._mapped = nullptr;
		other._bufferAddress = 0;
		other._maxMaterials = 0;
	}
	return *this;
}

VisualMaterialTable::~VisualMaterialTable()
{
	// _buffer's destructor handles Vulkan cleanup via its own RAII.
	// We just clear our scalar state.
	_mapped = nullptr;
	_bufferAddress = 0;
	_maxMaterials = 0;
	_rasterStates.clear();
	_freeSlots.clear();
}

// ---------------------------------------------------------------------------
// Initialize
// ---------------------------------------------------------------------------

RenderError VisualMaterialTable::Initialize(const Device& device, uint32_t max_materials)
{
	// If already initialized, tear down first.
	if (_buffer.IsInitialized())
	{
		Teardown();
	}

	// Step 1: create material buffer.
	BufferConfig config{};
	config.size = static_cast<VkDeviceSize>(max_materials) * sizeof(VisualMaterialGPU);
	config.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	config.memoryProperties =
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	RenderError err = _buffer.Initialize(device, config);
	if (err != RenderError::None)
	{
		return err;
	}

	// Step 2: map material buffer.
	_mapped = _buffer.Map();
	if (_mapped == nullptr)
	{
		auto* logger = Logger::GetLogger("renderer");
		LOG_ERROR(logger, "VisualMaterialTable::Initialize: failed to map material buffer");
		Teardown();
		return RenderError::MemoryMapFailed;
	}

	// Step 3: size raster-state vector.
	_rasterStates.resize(max_materials);

	// Step 4: initialize free-slot list in ascending order.
	_freeSlots.reserve(max_materials);
	for (uint32_t i = 0; i < max_materials; ++i)
	{
		_freeSlots.push_back(i);
	}

	_maxMaterials = max_materials;
	_bufferAddress = _buffer.GetDeviceAddress(device);

	return RenderError::None;
}

// ---------------------------------------------------------------------------
// Allocate
// ---------------------------------------------------------------------------

uint32_t VisualMaterialTable::Allocate(const VisualMaterial& material)
{
	if (_freeSlots.empty())
	{
		auto* logger = Logger::GetLogger("renderer");
		LOG_ERROR(logger, "VisualMaterialTable::Allocate: no free material slots");
		return UINT32_MAX;
	}

	// Pop from the back (stack behaviour).
	const uint32_t id = _freeSlots.back();
	_freeSlots.pop_back();

	// Build and write GPU record.
	VisualMaterialGPU record = BuildGPURecord(material);
	std::memcpy(
		static_cast<uint8_t*>(_mapped) + static_cast<size_t>(id) * sizeof(VisualMaterialGPU),
		&record,
		sizeof(VisualMaterialGPU));

	// Write raster state.
	_rasterStates[id] = VisualMaterialRasterState{material.alphaMode, material.doubleSided};

	return id;
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

RenderError VisualMaterialTable::Update(uint32_t id, const VisualMaterial& material)
{
	// Build and write GPU record.
	VisualMaterialGPU record = BuildGPURecord(material);
	std::memcpy(
		static_cast<uint8_t*>(_mapped) + static_cast<size_t>(id) * sizeof(VisualMaterialGPU),
		&record,
		sizeof(VisualMaterialGPU));

	// Overwrite raster state.
	_rasterStates[id] = VisualMaterialRasterState{material.alphaMode, material.doubleSided};

	return RenderError::None;
}

// ---------------------------------------------------------------------------
// Free
// ---------------------------------------------------------------------------

void VisualMaterialTable::Free(uint32_t id)
{
	_freeSlots.push_back(id);
}

// ---------------------------------------------------------------------------
// Observers
// ---------------------------------------------------------------------------

VisualMaterialRasterState VisualMaterialTable::GetRasterState(uint32_t id) const noexcept
{
	return _rasterStates[id];
}

VkDeviceAddress VisualMaterialTable::GetBufferAddress() const noexcept
{
	return _bufferAddress;
}

uint32_t VisualMaterialTable::GetMaxMaterials() const noexcept
{
	return _maxMaterials;
}

bool VisualMaterialTable::IsInitialized() const noexcept
{
	return _bufferAddress != 0;
}

} // namespace virasa
