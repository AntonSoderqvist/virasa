#include "virasa/editor/io/GltfLoader.h"

#include <vulkan/vulkan.h>

#include <quill/LogMacros.h>
#include <quill/Logger.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <span>
#include <string>
#include <tiny_gltf.h>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "virasa/core/Logger.h"
#include "virasa/ecs/ComponentAccess.h"
#include "virasa/ecs/Components.h"
#include "virasa/editor/io/ImageLoader.h"
#include "virasa/math/Transform.h"
#include "virasa/renderer/Types.h"
#include "virasa/renderer/geometry/Tangents.h"
#include "virasa/renderer/material/Visual.h"

namespace virasa
{
namespace editor::io
{
namespace
{
using TexturePrimitiveKey = std::pair<int, int>;

struct TexturePrimitiveKeyHash
{
	std::size_t operator()(const TexturePrimitiveKey& key) const noexcept
	{
		const std::size_t firstHash = std::hash<int>{}(key.first);
		const std::size_t secondHash = std::hash<int>{}(key.second);
		return firstHash ^ (secondHash + 0x9e3779b9u + (firstHash << 6u) + (firstHash >> 2u));
	}
};

[[nodiscard]] quill::Logger* GetEditorLogger()
{
	return virasa::Logger::GetLogger("editor.io");
}

void RollbackEntities(virasa::ecs::World& world, std::vector<virasa::ecs::Entity>& createdEntities)
{
	for (auto it = createdEntities.rbegin(); it != createdEntities.rend(); ++it)
	{
		if (world.IsValid(*it))
		{
			world.DestroyEntity(*it);
		}
	}

	createdEntities.clear();
}

[[nodiscard]] bool ReadFileBytes(const std::string& path, std::vector<unsigned char>& outBytes)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file.is_open())
	{
		return false;
	}

	const std::streamsize fileSize = file.tellg();
	if (fileSize < 0)
	{
		outBytes.clear();
		return true;
	}

	outBytes.resize(static_cast<std::size_t>(fileSize));
	file.seekg(0, std::ios::beg);
	if (fileSize > 0)
	{
		file.read(reinterpret_cast<char*>(outBytes.data()), fileSize);
	}

	return true;
}

[[nodiscard]] bool HasUnsupportedFeatures(const tinygltf::Model& model)
{
	if (!model.animations.empty() || !model.skins.empty())
	{
		return true;
	}

	for (const tinygltf::Mesh& mesh : model.meshes)
	{
		for (const tinygltf::Primitive& primitive : mesh.primitives)
		{
			if (primitive.mode != TINYGLTF_MODE_TRIANGLES)
			{
				return true;
			}

			if (primitive.targets.size() > 0)
			{
				return true;
			}
		}
	}

	for (const std::string& extension : model.extensionsUsed)
	{
		if (extension != "KHR_texture_basisu")
		{
			return true;
		}
	}

	for (const tinygltf::Node& node : model.nodes)
	{
		if (node.camera >= 0)
		{
			QUILL_LOG_INFO(
				GetEditorLogger(), "glTF node references camera {}; ignored", node.camera);
		}
	}

	if (!model.cameras.empty())
	{
		QUILL_LOG_INFO(
			GetEditorLogger(), "glTF contains {} camera(s); ignored", model.cameras.size());
	}

	if (!model.lights.empty())
	{
		QUILL_LOG_INFO(
			GetEditorLogger(), "glTF contains {} light(s); ignored", model.lights.size());
	}

	return false;
}

[[nodiscard]] VkFilter MapMagFilter(int magFilter)
{
	switch (magFilter)
	{
		case TINYGLTF_TEXTURE_FILTER_NEAREST:
			return VK_FILTER_NEAREST;
		case TINYGLTF_TEXTURE_FILTER_LINEAR:
		default:
			return VK_FILTER_LINEAR;
	}
}

[[nodiscard]] VkFilter MapMinFilter(int minFilter)
{
	switch (minFilter)
	{
		case TINYGLTF_TEXTURE_FILTER_NEAREST:
		case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
		case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
			return VK_FILTER_NEAREST;
		case TINYGLTF_TEXTURE_FILTER_LINEAR:
		case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
		case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
		default:
			return VK_FILTER_LINEAR;
	}
}

[[nodiscard]] VkSamplerMipmapMode MapMipmapMode(int minFilter)
{
	switch (minFilter)
	{
		case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
		case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
			return VK_SAMPLER_MIPMAP_MODE_NEAREST;
		case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
		case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
			return VK_SAMPLER_MIPMAP_MODE_LINEAR;
		case TINYGLTF_TEXTURE_FILTER_NEAREST:
		case TINYGLTF_TEXTURE_FILTER_LINEAR:
		default:
			return VK_SAMPLER_MIPMAP_MODE_LINEAR;
	}
}

[[nodiscard]] VkSamplerAddressMode MapWrapMode(int wrapMode)
{
	switch (wrapMode)
	{
		case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
			return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
			return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		case TINYGLTF_TEXTURE_WRAP_REPEAT:
		default:
			return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	}
}

[[nodiscard]] bool IsSrgbTextureRole(const tinygltf::Model& model, int textureIndex) noexcept
{
	for (const tinygltf::Material& material : model.materials)
	{
		if (material.pbrMetallicRoughness.baseColorTexture.index == textureIndex)
		{
			return true;
		}

		if (material.emissiveTexture.index == textureIndex)
		{
			return true;
		}
	}

	return false;
}

[[nodiscard]] std::span<const std::byte> GetImageBytesFromBufferView(
	const tinygltf::Model& model, const tinygltf::Image& image)
{
	if (image.bufferView < 0 || image.bufferView >= static_cast<int>(model.bufferViews.size()))
	{
		return {};
	}

	const tinygltf::BufferView& bufferView =
		model.bufferViews[static_cast<std::size_t>(image.bufferView)];
	if (bufferView.buffer < 0 || bufferView.buffer >= static_cast<int>(model.buffers.size()))
	{
		return {};
	}

	const tinygltf::Buffer& buffer = model.buffers[static_cast<std::size_t>(bufferView.buffer)];
	const std::size_t offset = bufferView.byteOffset;
	const std::size_t size = bufferView.byteLength;
	if (offset + size > buffer.data.size())
	{
		return {};
	}

	return std::span<const std::byte>(
		reinterpret_cast<const std::byte*>(buffer.data.data() + offset), size);
}

[[nodiscard]] virasa::math::Transform NodeToTransform(const tinygltf::Node& node)
{
	virasa::math::Transform transform;

	if (node.matrix.size() == 16)
	{
		glm::mat4 matrix(1.0f);
		for (int column = 0; column < 4; ++column)
		{
			for (int row = 0; row < 4; ++row)
			{
				matrix[column][row] = static_cast<float>(
					node.matrix[static_cast<std::size_t>(column * 4 + row)]);
			}
		}

		glm::vec3 skew(0.0f);
		glm::vec4 perspective(0.0f);
		glm::vec3 scale(1.0f);
		glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
		glm::vec3 translation(0.0f);
		glm::decompose(matrix, scale, rotation, translation, skew, perspective);
		rotation = glm::conjugate(rotation);

		transform.translation = translation;
		transform.rotation = rotation;
		transform.scale = scale;
		return transform;
	}

	if (node.translation.size() == 3)
	{
		transform.translation = virasa::math::Vec3(static_cast<float>(node.translation[0]),
			static_cast<float>(node.translation[1]),
			static_cast<float>(node.translation[2]));
	}

	if (node.rotation.size() == 4)
	{
		transform.rotation = virasa::math::Quat(static_cast<float>(node.rotation[3]),
			static_cast<float>(node.rotation[0]),
			static_cast<float>(node.rotation[1]),
			static_cast<float>(node.rotation[2]));
	}

	if (node.scale.size() == 3)
	{
		transform.scale = virasa::math::Vec3(static_cast<float>(node.scale[0]),
			static_cast<float>(node.scale[1]),
			static_cast<float>(node.scale[2]));
	}

	return transform;
}

template <typename T>
[[nodiscard]] const T* GetAccessorData(
	const tinygltf::Model& model, const tinygltf::Accessor& accessor)
{
	if (accessor.bufferView < 0 ||
		accessor.bufferView >= static_cast<int>(model.bufferViews.size()))
	{
		return nullptr;
	}

	const tinygltf::BufferView& bufferView =
		model.bufferViews[static_cast<std::size_t>(accessor.bufferView)];
	if (bufferView.buffer < 0 || bufferView.buffer >= static_cast<int>(model.buffers.size()))
	{
		return nullptr;
	}

	const tinygltf::Buffer& buffer = model.buffers[static_cast<std::size_t>(bufferView.buffer)];
	const std::size_t byteOffset = bufferView.byteOffset + accessor.byteOffset;
	if (byteOffset >= buffer.data.size())
	{
		return nullptr;
	}

	return reinterpret_cast<const T*>(buffer.data.data() + byteOffset);
}

[[nodiscard]] std::size_t GetAccessorStride(
	const tinygltf::Model& model, const tinygltf::Accessor& accessor)
{
	if (accessor.bufferView < 0 ||
		accessor.bufferView >= static_cast<int>(model.bufferViews.size()))
	{
		return 0;
	}

	return accessor.ByteStride(model.bufferViews[static_cast<std::size_t>(accessor.bufferView)]);
}

[[nodiscard]] virasa::MeshData BuildMeshData(
	const tinygltf::Model& model, const tinygltf::Primitive& primitive)
{
	virasa::MeshData meshData;

	const auto positionIt = primitive.attributes.find("POSITION");
	const auto normalIt = primitive.attributes.find("NORMAL");
	const auto uvIt = primitive.attributes.find("TEXCOORD_0");
	const auto tangentIt = primitive.attributes.find("TANGENT");

	if (positionIt == primitive.attributes.end())
	{
		QUILL_LOG_ERROR(GetEditorLogger(),
			"BuildMeshData: primitive missing POSITION attribute");
		return meshData;
	}
	if (normalIt == primitive.attributes.end())
	{
		QUILL_LOG_ERROR(GetEditorLogger(),
			"BuildMeshData: primitive missing NORMAL attribute");
		return meshData;
	}
	const tinygltf::Accessor& positionAccessor =
		model.accessors[static_cast<std::size_t>(positionIt->second)];
	const tinygltf::Accessor& normalAccessor =
		model.accessors[static_cast<std::size_t>(normalIt->second)];
	const tinygltf::Accessor* uvAccessor =
		uvIt != primitive.attributes.end()
			? &model.accessors[static_cast<std::size_t>(uvIt->second)]
			: nullptr;
	const tinygltf::Accessor* tangentAccessor = nullptr;
	if (tangentIt != primitive.attributes.end())
	{
		tangentAccessor = &model.accessors[static_cast<std::size_t>(tangentIt->second)];
	}

	const std::size_t vertexCount = positionAccessor.count;
	meshData.vertices.resize(vertexCount);

	const unsigned char* positionBytes = reinterpret_cast<const unsigned char*>(
		GetAccessorData<std::byte>(model, positionAccessor));
	const unsigned char* normalBytes = reinterpret_cast<const unsigned char*>(
		GetAccessorData<std::byte>(model, normalAccessor));
	const unsigned char* uvBytes = uvAccessor != nullptr
		? reinterpret_cast<const unsigned char*>(
			  GetAccessorData<std::byte>(model, *uvAccessor))
		: nullptr;
	const unsigned char* tangentBytes =
		tangentAccessor != nullptr ? reinterpret_cast<const unsigned char*>(
							     GetAccessorData<std::byte>(model, *tangentAccessor))
						   : nullptr;

	const std::size_t positionStride = GetAccessorStride(model, positionAccessor);
	const std::size_t normalStride = GetAccessorStride(model, normalAccessor);
	const std::size_t uvStride =
		uvAccessor != nullptr ? GetAccessorStride(model, *uvAccessor) : 0;
	const std::size_t tangentStride =
		tangentAccessor != nullptr ? GetAccessorStride(model, *tangentAccessor) : 0;

	for (std::size_t i = 0; i < vertexCount; ++i)
	{
		const float* position =
			reinterpret_cast<const float*>(positionBytes + i * positionStride);
		const float* normal = reinterpret_cast<const float*>(normalBytes + i * normalStride);

		meshData.vertices[i].position =
			virasa::math::Vec3(position[0], position[1], position[2]);
		meshData.vertices[i].normal = virasa::math::Vec3(normal[0], normal[1], normal[2]);
		if (uvBytes != nullptr)
		{
			const float* uv = reinterpret_cast<const float*>(uvBytes + i * uvStride);
			meshData.vertices[i].uv = virasa::math::Vec2(uv[0], uv[1]);
		}
		else
		{
			meshData.vertices[i].uv = virasa::math::Vec2(0.0f, 0.0f);
		}

		if (tangentBytes != nullptr)
		{
			const float* tangent =
				reinterpret_cast<const float*>(tangentBytes + i * tangentStride);
			meshData.vertices[i].tangent =
				virasa::math::Vec4(tangent[0], tangent[1], tangent[2], tangent[3]);
		}
	}

	if (primitive.indices < 0)
	{
		meshData.indices.resize(vertexCount);
		for (std::size_t i = 0; i < vertexCount; ++i)
		{
			meshData.indices[i] = static_cast<uint32_t>(i);
		}
	}
	else
	{
		const tinygltf::Accessor& indexAccessor =
			model.accessors[static_cast<std::size_t>(primitive.indices)];
		meshData.indices.reserve(indexAccessor.count);

		const unsigned char* indexBytes = reinterpret_cast<const unsigned char*>(
			GetAccessorData<std::byte>(model, indexAccessor));
		const std::size_t indexStride = GetAccessorStride(model, indexAccessor);

		for (std::size_t i = 0; i < indexAccessor.count; ++i)
		{
			const unsigned char* element = indexBytes + i * indexStride;
			switch (indexAccessor.componentType)
			{
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
					meshData.indices.push_back(
						*reinterpret_cast<const uint8_t*>(element));
					break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
					meshData.indices.push_back(
						*reinterpret_cast<const uint16_t*>(element));
					break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
					meshData.indices.push_back(
						*reinterpret_cast<const uint32_t*>(element));
					break;
				default:
					break;
			}
		}
	}

	if (tangentAccessor == nullptr && !meshData.vertices.empty() && !meshData.indices.empty())
	{
		virasa::GenerateTangents(meshData);
	}

	return meshData;
}

[[nodiscard]] virasa::AlphaMode MapAlphaMode(const std::string& alphaMode)
{
	if (alphaMode == "MASK")
	{
		return virasa::AlphaMode::Mask;
	}

	if (alphaMode == "BLEND")
	{
		return virasa::AlphaMode::Blend;
	}

	return virasa::AlphaMode::Opaque;
}
} // namespace

virasa::editor::io::GltfLoadResult LoadGlb(const std::string& path, virasa::ecs::World& world,
	virasa::renderer::scene::SceneRenderer& scene_renderer)
{
	std::vector<unsigned char> fileBytes;
	if (!ReadFileBytes(path, fileBytes))
	{
		return {virasa::ecs::Entity::Invalid(), GltfLoadError::FileNotFound};
	}

	tinygltf::TinyGLTF loader;
	// We disable tinygltf's built-in image decode (TINYGLTF_NO_STB_IMAGE) and
	// do our own via ImageLoader in Step 3, but tinygltf still demands a
	// LoadImageData callback. Register a no-op so the parse succeeds; the
	// raw image bytes are recovered from buffer views afterwards.
	loader.SetImageLoader(
		[](tinygltf::Image*, const int, std::string*, std::string*,
			int, int, const unsigned char*, int, void*) -> bool { return true; },
		nullptr);
	tinygltf::Model model;
	std::string warnings;
	std::string errors;
	const bool loaded = loader.LoadBinaryFromMemory(&model,
		&errors,
		&warnings,
		fileBytes.data(),
		static_cast<unsigned int>(fileBytes.size()));
	if (!warnings.empty())
	{
		QUILL_LOG_INFO(GetEditorLogger(), "tinygltf warning: {}", warnings);
	}
	if (!loaded)
	{
		QUILL_LOG_ERROR(GetEditorLogger(), "tinygltf parse failed: {}", errors);
		return {virasa::ecs::Entity::Invalid(), GltfLoadError::ParseFailed};
	}

	if (HasUnsupportedFeatures(model))
	{
		QUILL_LOG_ERROR(GetEditorLogger(), "glTF uses unsupported feature(s)");
		return {virasa::ecs::Entity::Invalid(), GltfLoadError::UnsupportedFeature};
	}

	std::vector<virasa::editor::io::DecodedImage> decodedImages;
	decodedImages.reserve(model.images.size());
	for (std::size_t imageIndex = 0; imageIndex < model.images.size(); ++imageIndex)
	{
		const tinygltf::Image& image = model.images[imageIndex];
		virasa::editor::io::DecodedImage decoded;

		if (image.bufferView >= 0)
		{
			decoded = virasa::editor::io::LoadFromMemory(
				GetImageBytesFromBufferView(model, image));
		}
		else if (!image.uri.empty())
		{
			decoded = virasa::editor::io::LoadFromFile(image.uri);
		}
		else if (!image.image.empty())
		{
			decoded = virasa::editor::io::LoadFromMemory(std::span<const std::byte>(
				reinterpret_cast<const std::byte*>(image.image.data()),
				image.image.size()));
		}

		if (decoded.error != virasa::editor::io::ImageLoadError::None)
		{
			QUILL_LOG_ERROR(
				GetEditorLogger(), "Texture decode failed for image {}", imageIndex);
			return {virasa::ecs::Entity::Invalid(), GltfLoadError::TextureDecodeFailed};
		}

		decodedImages.push_back(std::move(decoded));
	}

	std::unordered_map<int, uint32_t> textureSlots;
	for (std::size_t textureIndex = 0; textureIndex < model.textures.size(); ++textureIndex)
	{
		const tinygltf::Texture& texture = model.textures[textureIndex];
		if (texture.source < 0 || texture.source >= static_cast<int>(decodedImages.size()))
		{
			QUILL_LOG_ERROR(GetEditorLogger(),
				"Texture {} references invalid image source",
				textureIndex);
			return {virasa::ecs::Entity::Invalid(), GltfLoadError::TextureDecodeFailed};
		}

		virasa::SamplerConfig samplerConfig;
		if (texture.sampler >= 0 && texture.sampler < static_cast<int>(model.samplers.size()))
		{
			const tinygltf::Sampler& sampler =
				model.samplers[static_cast<std::size_t>(texture.sampler)];
			samplerConfig.magFilter = MapMagFilter(sampler.magFilter);
			samplerConfig.minFilter = MapMinFilter(sampler.minFilter);
			samplerConfig.mipmapMode = MapMipmapMode(sampler.minFilter);
			samplerConfig.addressModeU = MapWrapMode(sampler.wrapS);
			samplerConfig.addressModeV = MapWrapMode(sampler.wrapT);
			samplerConfig.addressModeW = samplerConfig.addressModeU;
		}

		const virasa::editor::io::DecodedImage& decoded =
			decodedImages[static_cast<std::size_t>(texture.source)];
		virasa::TextureUpload upload;
		upload.pixels =
			std::span<const std::byte>(decoded.pixels.data(), decoded.pixels.size());
		upload.width = decoded.width;
		upload.height = decoded.height;
		upload.format = IsSrgbTextureRole(model, static_cast<int>(textureIndex))
					    ? VK_FORMAT_R8G8B8A8_SRGB
					    : VK_FORMAT_R8G8B8A8_UNORM;
		upload.sampler = samplerConfig;

		uint32_t slot = 0;
		const virasa::RegisterError registerError =
			scene_renderer.RegisterTexture(upload, slot);
		if (registerError != virasa::RegisterError::None)
		{
			QUILL_LOG_ERROR(GetEditorLogger(),
				"Texture registration failed for texture {} with RegisterError {}",
				textureIndex,
				static_cast<int>(registerError));
			return {virasa::ecs::Entity::Invalid(), GltfLoadError::TextureRegistrationFailed};
		}

		textureSlots.emplace(static_cast<int>(textureIndex), slot);
	}

	std::unordered_map<int, uint32_t> materialIds;
	for (std::size_t materialIndex = 0; materialIndex < model.materials.size(); ++materialIndex)
	{
		const tinygltf::Material& material = model.materials[materialIndex];
		virasa::VisualMaterial visualMaterial;
		visualMaterial.factors.baseColorFactor = virasa::math::Vec4(
			static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[0]),
			static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[1]),
			static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[2]),
			static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[3]));
		visualMaterial.factors.metallicFactor =
			static_cast<float>(material.pbrMetallicRoughness.metallicFactor);
		visualMaterial.factors.roughnessFactor =
			static_cast<float>(material.pbrMetallicRoughness.roughnessFactor);
		visualMaterial.factors.emissiveFactor =
			virasa::math::Vec3(static_cast<float>(material.emissiveFactor[0]),
				static_cast<float>(material.emissiveFactor[1]),
				static_cast<float>(material.emissiveFactor[2]));
		visualMaterial.factors.normalScale = static_cast<float>(material.normalTexture.scale);
		visualMaterial.factors.occlusionStrength =
			static_cast<float>(material.occlusionTexture.strength);
		visualMaterial.factors.alphaCutoff = static_cast<float>(material.alphaCutoff);
		visualMaterial.alphaMode = MapAlphaMode(material.alphaMode);
		visualMaterial.doubleSided = material.doubleSided;

		if (material.pbrMetallicRoughness.baseColorTexture.index >= 0)
		{
			visualMaterial.baseColorTexture =
				textureSlots[material.pbrMetallicRoughness.baseColorTexture.index];
		}
		if (material.normalTexture.index >= 0)
		{
			visualMaterial.normalTexture = textureSlots[material.normalTexture.index];
		}
		if (material.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0)
		{
			visualMaterial.metallicRoughnessTexture =
				textureSlots[material.pbrMetallicRoughness.metallicRoughnessTexture.index];
		}
		if (material.occlusionTexture.index >= 0)
		{
			visualMaterial.occlusionTexture = textureSlots[material.occlusionTexture.index];
		}
		if (material.emissiveTexture.index >= 0)
		{
			visualMaterial.emissiveTexture = textureSlots[material.emissiveTexture.index];
		}

		uint32_t materialId = 0;
		const virasa::RegisterError registerError =
			scene_renderer.RegisterMaterial(visualMaterial, materialId);
		if (registerError != virasa::RegisterError::None)
		{
			QUILL_LOG_ERROR(GetEditorLogger(),
				"Material registration failed for material {} with RegisterError {}",
				materialIndex,
				static_cast<int>(registerError));
			return {
				virasa::ecs::Entity::Invalid(), GltfLoadError::MaterialRegistrationFailed};
		}

		materialIds.emplace(static_cast<int>(materialIndex), materialId);
	}

	std::unordered_map<TexturePrimitiveKey, uint32_t, TexturePrimitiveKeyHash> meshIds;
	for (std::size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex)
	{
		const tinygltf::Mesh& mesh = model.meshes[meshIndex];
		for (std::size_t primitiveIndex = 0; primitiveIndex < mesh.primitives.size();
			++primitiveIndex)
		{
			const tinygltf::Primitive& primitive = mesh.primitives[primitiveIndex];
			virasa::MeshData meshData = BuildMeshData(model, primitive);

			uint32_t meshId = 0;
			const virasa::RegisterError registerError =
				scene_renderer.RegisterMesh(meshData, meshId);
			if (registerError != virasa::RegisterError::None)
			{
				QUILL_LOG_ERROR(GetEditorLogger(),
					"Mesh registration failed for mesh {}, primitive {} with "
					"RegisterError {}",
					meshIndex,
					primitiveIndex,
					static_cast<int>(registerError));
				return {virasa::ecs::Entity::Invalid(),
					GltfLoadError::MeshRegistrationFailed};
			}

			meshIds.emplace(TexturePrimitiveKey(static_cast<int>(meshIndex),
						    static_cast<int>(primitiveIndex)),
				meshId);
		}
	}

	std::vector<virasa::ecs::Entity> createdEntities;
	const virasa::ecs::Entity root = world.CreateEntity("gltf_axis_bake");
	createdEntities.push_back(root);

	virasa::math::Transform rootTransform;
	rootTransform.rotation =
		glm::angleAxis(glm::radians(90.0f), virasa::math::Vec3(1.0f, 0.0f, 0.0f));
	world.Transforms().Add(root, rootTransform);

	std::vector<virasa::ecs::Entity> nodeEntities(
		model.nodes.size(), virasa::ecs::Entity::Invalid());
	for (std::size_t nodeIndex = 0; nodeIndex < model.nodes.size(); ++nodeIndex)
	{
		const tinygltf::Node& node = model.nodes[nodeIndex];
		std::string nodeName =
			node.name.empty() ? std::string("node_") + std::to_string(nodeIndex) : node.name;

		nodeEntities[nodeIndex] = world.CreateEntity(nodeName);
		createdEntities.push_back(nodeEntities[nodeIndex]);
		world.Transforms().Add(nodeEntities[nodeIndex], NodeToTransform(node));
	}

	const int defaultSceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
	std::vector<int> topLevelNodes;
	if (!model.scenes.empty() && defaultSceneIndex < static_cast<int>(model.scenes.size()))
	{
		topLevelNodes = model.scenes[static_cast<std::size_t>(defaultSceneIndex)].nodes;
	}
	else
	{
		for (std::size_t nodeIndex = 0; nodeIndex < model.nodes.size(); ++nodeIndex)
		{
			topLevelNodes.push_back(static_cast<int>(nodeIndex));
		}
	}

	for (std::size_t nodeIndex = 0; nodeIndex < model.nodes.size(); ++nodeIndex)
	{
		const tinygltf::Node& node = model.nodes[nodeIndex];
		for (int childIndex : node.children)
		{
			world.SetParent(nodeEntities[static_cast<std::size_t>(childIndex)],
				nodeEntities[nodeIndex]);
		}
	}

	for (int topLevelNode : topLevelNodes)
	{
		world.SetParent(nodeEntities[static_cast<std::size_t>(topLevelNode)], root);
	}

	for (std::size_t nodeIndex = 0; nodeIndex < model.nodes.size(); ++nodeIndex)
	{
		const tinygltf::Node& node = model.nodes[nodeIndex];
		if (node.mesh < 0 || node.mesh >= static_cast<int>(model.meshes.size()))
		{
			continue;
		}

		const tinygltf::Mesh& mesh = model.meshes[static_cast<std::size_t>(node.mesh)];
		for (std::size_t primitiveIndex = 0; primitiveIndex < mesh.primitives.size();
			++primitiveIndex)
		{
			const tinygltf::Primitive& primitive = mesh.primitives[primitiveIndex];
			const virasa::ecs::Entity primitiveEntity = world.CreateEntity("mesh_primitive");
			createdEntities.push_back(primitiveEntity);
			world.Transforms().Add(primitiveEntity, virasa::math::Transform::Identity());
			virasa::ecs::AddMesh(world, primitiveEntity,
				virasa::ecs::MeshComponent{
					.meshId = meshIds[TexturePrimitiveKey(static_cast<int>(node.mesh),
						static_cast<int>(primitiveIndex))]});
			virasa::ecs::AddVisual(world, primitiveEntity,
				virasa::ecs::VisualComponent{
					.materialId = primitive.material >= 0
								  ? materialIds[primitive.material]
								  : 0u});
			world.SetParent(primitiveEntity, nodeEntities[nodeIndex]);
		}
	}

	return {root, GltfLoadError::None};
}
} // namespace editor::io
} // namespace virasa
