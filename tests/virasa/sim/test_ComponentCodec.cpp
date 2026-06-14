#include <gtest/gtest.h>

#include "virasa/sim/ComponentCodec.h"

#include "virasa/ecs/Components.h"
#include "virasa/math/Transform.h"
#include "virasa/sim/GameplayComponents.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

using namespace virasa::sim;

namespace
{

class TestCodec final : public ComponentCodec
{
public:
	explicit TestCodec(std::string_view name)
		: _name(name)
	{
	}

	[[nodiscard]] std::string_view ComponentName() const noexcept override
	{
		return _name;
	}

	[[nodiscard]] size_t ElementSize() const noexcept override
	{
		return sizeof(uint32_t);
	}

	[[nodiscard]] nlohmann::json ToJson(
		const void* component,
		const AssetCatalog& catalog) const override
	{
		(void)catalog;
		return nlohmann::json{{"value", *static_cast<const uint32_t*>(component)}};
	}

	bool FromJson(
		const nlohmann::json& json,
		const AssetCatalog& catalog,
		void* dst) const override
	{
		(void)catalog;
		if (!json.contains("value") || !json["value"].is_number_unsigned())
		{
			*static_cast<uint32_t*>(dst) = 0u;
			return false;
		}

		*static_cast<uint32_t*>(dst) = json["value"].get<uint32_t>();
		return true;
	}

private:
	std::string _name;
};

const ComponentCodec& RequireCodec(
	const ComponentCodecRegistry& registry,
	std::string_view name)
{
	const ComponentCodec* codec = registry.Find(name);
	EXPECT_NE(codec, nullptr);
	return *codec;
}

void ExpectVec3Eq(const virasa::math::Vec3& actual, const virasa::math::Vec3& expected)
{
	EXPECT_FLOAT_EQ(actual.x, expected.x);
	EXPECT_FLOAT_EQ(actual.y, expected.y);
	EXPECT_FLOAT_EQ(actual.z, expected.z);
}

void ExpectQuatEq(const virasa::math::Quat& actual, const virasa::math::Quat& expected)
{
	EXPECT_FLOAT_EQ(actual.w, expected.w);
	EXPECT_FLOAT_EQ(actual.x, expected.x);
	EXPECT_FLOAT_EQ(actual.y, expected.y);
	EXPECT_FLOAT_EQ(actual.z, expected.z);
}

void ExpectJsonVec3Eq(const nlohmann::json& json, const virasa::math::Vec3& expected)
{
	ASSERT_TRUE(json.is_array());
	ASSERT_EQ(json.size(), 3u);
	EXPECT_FLOAT_EQ(json[0].get<float>(), expected.x);
	EXPECT_FLOAT_EQ(json[1].get<float>(), expected.y);
	EXPECT_FLOAT_EQ(json[2].get<float>(), expected.z);
}

void ExpectJsonQuatEq(const nlohmann::json& json, const virasa::math::Quat& expected)
{
	ASSERT_TRUE(json.is_array());
	ASSERT_EQ(json.size(), 4u);
	EXPECT_FLOAT_EQ(json[0].get<float>(), expected.w);
	EXPECT_FLOAT_EQ(json[1].get<float>(), expected.x);
	EXPECT_FLOAT_EQ(json[2].get<float>(), expected.y);
	EXPECT_FLOAT_EQ(json[3].get<float>(), expected.z);
}

} // namespace

TEST(ComponentCodec, test_component_codec_translates_component_bytes_to_json)
{
	static_assert(std::has_virtual_destructor_v<ComponentCodec>);
	static_assert(std::is_abstract_v<ComponentCodec>);
	static_assert(!std::is_copy_constructible_v<ComponentCodec>);
	static_assert(!std::is_copy_assignable_v<ComponentCodec>);
	static_assert(!std::is_move_constructible_v<ComponentCodec>);
	static_assert(!std::is_move_assignable_v<ComponentCodec>);
	static_assert(std::is_constructible_v<TestCodec, std::string_view>);

	ComponentCodecRegistry registry;
	RegisterBuiltinComponentCodecs(registry);
	const ComponentCodec& codec = RequireCodec(registry, "Transform");
	const AssetCatalog catalog;

	virasa::math::Transform transform;
	transform.translation = virasa::math::Vec3(1.0f, 2.0f, 3.0f);
	transform.rotation = virasa::math::Quat(0.5f, 0.25f, 0.75f, 1.0f);
	transform.scale = virasa::math::Vec3(4.0f, 5.0f, 6.0f);

	EXPECT_EQ(codec.ComponentName(), std::string_view("Transform"));
	EXPECT_EQ(codec.ElementSize(), sizeof(virasa::math::Transform));

	const nlohmann::json json = codec.ToJson(&transform, catalog);
	ExpectJsonVec3Eq(json["translation"], transform.translation);
	ExpectJsonQuatEq(json["rotation"], transform.rotation);
	ExpectJsonVec3Eq(json["scale"], transform.scale);

	virasa::math::Transform decoded;
	ASSERT_TRUE(codec.FromJson(json, catalog, &decoded));
	ExpectVec3Eq(decoded.translation, transform.translation);
	ExpectQuatEq(decoded.rotation, transform.rotation);
	ExpectVec3Eq(decoded.scale, transform.scale);

	const nlohmann::json unreadable = nlohmann::json::array();
	decoded.translation = virasa::math::Vec3(9.0f, 9.0f, 9.0f);
	EXPECT_FALSE(codec.FromJson(unreadable, catalog, &decoded));
	ExpectVec3Eq(decoded.translation, virasa::math::Transform{}.translation);
	ExpectQuatEq(decoded.rotation, virasa::math::Transform{}.rotation);
	ExpectVec3Eq(decoded.scale, virasa::math::Transform{}.scale);
}

TEST(ComponentCodec, test_component_codec_registry_maps_names_to_codecs)
{
	static_assert(std::is_final_v<ComponentCodecRegistry>);
	static_assert(std::is_default_constructible_v<ComponentCodecRegistry>);
	static_assert(!std::is_copy_constructible_v<ComponentCodecRegistry>);
	static_assert(!std::is_copy_assignable_v<ComponentCodecRegistry>);
	static_assert(std::is_move_constructible_v<ComponentCodecRegistry>);
	static_assert(std::is_move_assignable_v<ComponentCodecRegistry>);
	static_assert(noexcept(std::declval<const ComponentCodecRegistry&>().Size()));

	ComponentCodecRegistry registry;

	EXPECT_EQ(registry.Size(), 0u);
	EXPECT_FALSE(registry.Contains("Alpha"));
	EXPECT_EQ(registry.Find("Alpha"), nullptr);
	EXPECT_TRUE(registry.Names().empty());

	auto alpha = std::make_unique<TestCodec>("Alpha");
	const ComponentCodec* alphaPtr = alpha.get();
	registry.Register(std::move(alpha));

	EXPECT_EQ(registry.Size(), 1u);
	EXPECT_TRUE(registry.Contains("Alpha"));
	EXPECT_FALSE(registry.Contains("alpha"));
	EXPECT_EQ(registry.Find("Alpha"), alphaPtr);
	EXPECT_EQ(registry.Find("alpha"), nullptr);

	registry.Register(std::make_unique<TestCodec>("Beta"));

	const std::vector<std::string> expectedNames = {"Alpha", "Beta"};
	EXPECT_EQ(registry.Size(), 2u);
	EXPECT_EQ(registry.Names(), expectedNames);

	std::vector<std::string> names = registry.Names();
	names.clear();
	EXPECT_EQ(registry.Names(), expectedNames);

	ComponentCodecRegistry moved = std::move(registry);
	EXPECT_EQ(moved.Size(), 2u);
	EXPECT_EQ(moved.Find("Alpha"), alphaPtr);
	EXPECT_TRUE(moved.Contains("Beta"));
}

TEST(ComponentCodec, test_register_builtin_component_codecs_covers_serializable_components)
{
	ComponentCodecRegistry registry;
	RegisterBuiltinComponentCodecs(registry);
	const AssetCatalog catalog;

	const std::vector<std::string> expectedNames = {
		"Transform",
		"Mesh",
		"Visual",
		"DirectionalLight",
		"PointLight",
		"SpotLight",
		"Camera",
		"Highlight",
		"Spin",
	};

	EXPECT_EQ(registry.Size(), expectedNames.size());
	EXPECT_EQ(registry.Names(), expectedNames);
	for (const std::string& name : expectedNames)
	{
		EXPECT_TRUE(registry.Contains(name));
		EXPECT_NE(registry.Find(name), nullptr);
	}
	EXPECT_FALSE(registry.Contains("Name"));

	const ComponentCodec& directional = RequireCodec(registry, "DirectionalLight");
	virasa::ecs::DirectionalLightComponent directionalLight;
	directionalLight.direction = virasa::math::Vec3(0.0f, 1.0f, 0.0f);
	directionalLight.color = virasa::math::Vec3(0.25f, 0.5f, 0.75f);
	directionalLight.intensity = 3.5f;
	directionalLight.castsShadows = false;
	virasa::ecs::DirectionalLightComponent decodedDirectional;
	ASSERT_TRUE(directional.FromJson(
		directional.ToJson(&directionalLight, catalog),
		catalog,
		&decodedDirectional));
	ExpectVec3Eq(decodedDirectional.direction, directionalLight.direction);
	ExpectVec3Eq(decodedDirectional.color, directionalLight.color);
	EXPECT_FLOAT_EQ(decodedDirectional.intensity, directionalLight.intensity);
	EXPECT_EQ(decodedDirectional.castsShadows, directionalLight.castsShadows);

	const ComponentCodec& point = RequireCodec(registry, "PointLight");
	virasa::ecs::PointLightComponent pointLight;
	pointLight.color = virasa::math::Vec3(0.4f, 0.5f, 0.6f);
	pointLight.intensity = 8.0f;
	pointLight.range = 40.0f;
	virasa::ecs::PointLightComponent decodedPoint;
	ASSERT_TRUE(point.FromJson(
		point.ToJson(&pointLight, catalog),
		catalog,
		&decodedPoint));
	ExpectVec3Eq(decodedPoint.color, pointLight.color);
	EXPECT_FLOAT_EQ(decodedPoint.intensity, pointLight.intensity);
	EXPECT_FLOAT_EQ(decodedPoint.range, pointLight.range);

	const ComponentCodec& spot = RequireCodec(registry, "SpotLight");
	virasa::ecs::SpotLightComponent spotLight;
	spotLight.color = virasa::math::Vec3(0.7f, 0.8f, 0.9f);
	spotLight.intensity = 6.0f;
	spotLight.range = 25.0f;
	spotLight.innerConeCos = 0.8f;
	spotLight.outerConeCos = 0.6f;
	spotLight.castsShadows = false;
	virasa::ecs::SpotLightComponent decodedSpot;
	ASSERT_TRUE(spot.FromJson(spot.ToJson(&spotLight, catalog), catalog, &decodedSpot));
	ExpectVec3Eq(decodedSpot.color, spotLight.color);
	EXPECT_FLOAT_EQ(decodedSpot.intensity, spotLight.intensity);
	EXPECT_FLOAT_EQ(decodedSpot.range, spotLight.range);
	EXPECT_FLOAT_EQ(decodedSpot.innerConeCos, spotLight.innerConeCos);
	EXPECT_FLOAT_EQ(decodedSpot.outerConeCos, spotLight.outerConeCos);
	EXPECT_EQ(decodedSpot.castsShadows, spotLight.castsShadows);

	const ComponentCodec& camera = RequireCodec(registry, "Camera");
	virasa::ecs::CameraComponent cameraComponent;
	cameraComponent.domain = virasa::CameraDomain::Editor;
	cameraComponent.fovY = 1.25f;
	cameraComponent.aspect = 1.777f;
	cameraComponent.nearPlane = 0.25f;
	cameraComponent.farPlane = 250.0f;
	virasa::ecs::CameraComponent decodedCamera;
	const nlohmann::json cameraJson = camera.ToJson(&cameraComponent, catalog);
	EXPECT_EQ(cameraJson["domain"].get<uint32_t>(), 1u);
	ASSERT_TRUE(camera.FromJson(cameraJson, catalog, &decodedCamera));
	EXPECT_EQ(decodedCamera.domain, cameraComponent.domain);
	EXPECT_FLOAT_EQ(decodedCamera.fovY, cameraComponent.fovY);
	EXPECT_FLOAT_EQ(decodedCamera.aspect, cameraComponent.aspect);
	EXPECT_FLOAT_EQ(decodedCamera.nearPlane, cameraComponent.nearPlane);
	EXPECT_FLOAT_EQ(decodedCamera.farPlane, cameraComponent.farPlane);

	const ComponentCodec& highlight = RequireCodec(registry, "Highlight");
	virasa::ecs::HighlightComponent highlightComponent;
	highlightComponent.color = virasa::math::Vec3(1.0f, 0.5f, 0.25f);
	highlightComponent.intensity = 2.25f;
	highlightComponent.priority = 4;
	virasa::ecs::HighlightComponent decodedHighlight;
	ASSERT_TRUE(highlight.FromJson(
		highlight.ToJson(&highlightComponent, catalog),
		catalog,
		&decodedHighlight));
	ExpectVec3Eq(decodedHighlight.color, highlightComponent.color);
	EXPECT_FLOAT_EQ(decodedHighlight.intensity, highlightComponent.intensity);
	EXPECT_EQ(decodedHighlight.priority, highlightComponent.priority);

	const ComponentCodec& spin = RequireCodec(registry, "Spin");
	virasa::sim::SpinComponent spinComponent;
	spinComponent.angularVelocity = virasa::math::Vec3(0.1f, 0.2f, 0.3f);
	virasa::sim::SpinComponent decodedSpin;
	ASSERT_TRUE(spin.FromJson(
		spin.ToJson(&spinComponent, catalog),
		catalog,
		&decodedSpin));
	ExpectVec3Eq(decodedSpin.angularVelocity, spinComponent.angularVelocity);

	virasa::ecs::HighlightComponent missingOptional;
	missingOptional.color = virasa::math::Vec3(9.0f, 9.0f, 9.0f);
	ASSERT_TRUE(highlight.FromJson(
		nlohmann::json{{"intensity", 7.0f}},
		catalog,
		&missingOptional));
	ExpectVec3Eq(missingOptional.color, virasa::ecs::HighlightComponent{}.color);
	EXPECT_FLOAT_EQ(missingOptional.intensity, 7.0f);
	EXPECT_EQ(missingOptional.priority, virasa::ecs::HighlightComponent{}.priority);
}

TEST(ComponentCodec, test_component_codec_asset_fields_round_trip_through_catalog)
{
	ComponentCodecRegistry registry;
	RegisterBuiltinComponentCodecs(registry);
	const ComponentCodec& meshCodec = RequireCodec(registry, "Mesh");
	const ComponentCodec& visualCodec = RequireCodec(registry, "Visual");

	AssetCatalog catalog;
	catalog.Bind(AssetKind::Mesh, "builtin:cube", 42u);
	catalog.Bind(AssetKind::Material, "materials:default", 9u);

	virasa::ecs::MeshComponent mesh;
	mesh.meshId = 42u;
	const nlohmann::json meshJson = meshCodec.ToJson(&mesh, catalog);
	ASSERT_TRUE(meshJson.contains("meshId"));
	EXPECT_EQ(meshJson["meshId"], "builtin:cube");

	virasa::ecs::MeshComponent decodedMesh;
	ASSERT_TRUE(meshCodec.FromJson(meshJson, catalog, &decodedMesh));
	EXPECT_EQ(decodedMesh.meshId, 42u);

	virasa::ecs::VisualComponent visual;
	visual.materialId = 9u;
	const nlohmann::json visualJson = visualCodec.ToJson(&visual, catalog);
	ASSERT_TRUE(visualJson.contains("materialId"));
	EXPECT_EQ(visualJson["materialId"], "materials:default");

	virasa::ecs::VisualComponent decodedVisual;
	ASSERT_TRUE(visualCodec.FromJson(visualJson, catalog, &decodedVisual));
	EXPECT_EQ(decodedVisual.materialId, 9u);

	mesh.meshId = 77u;
	const nlohmann::json unresolvedMeshJson = meshCodec.ToJson(&mesh, catalog);
	EXPECT_TRUE(unresolvedMeshJson["meshId"].is_null());
	ASSERT_TRUE(meshCodec.FromJson(
		nlohmann::json{{"meshId", "missing:mesh"}},
		catalog,
		&decodedMesh));
	EXPECT_EQ(decodedMesh.meshId, kInvalidAssetId);
	ASSERT_TRUE(meshCodec.FromJson(
		nlohmann::json{{"meshId", nullptr}},
		catalog,
		&decodedMesh));
	EXPECT_EQ(decodedMesh.meshId, kInvalidAssetId);
	ASSERT_TRUE(meshCodec.FromJson(nlohmann::json::object(), catalog, &decodedMesh));
	EXPECT_EQ(decodedMesh.meshId, kInvalidAssetId);

	visual.materialId = kInvalidAssetId;
	const nlohmann::json unresolvedVisualJson = visualCodec.ToJson(&visual, catalog);
	EXPECT_TRUE(unresolvedVisualJson["materialId"].is_null());
	ASSERT_TRUE(visualCodec.FromJson(
		nlohmann::json{{"materialId", "missing:material"}},
		catalog,
		&decodedVisual));
	EXPECT_EQ(decodedVisual.materialId, kInvalidAssetId);
	ASSERT_TRUE(visualCodec.FromJson(
		nlohmann::json{{"materialId", nullptr}},
		catalog,
		&decodedVisual));
	EXPECT_EQ(decodedVisual.materialId, kInvalidAssetId);
	ASSERT_TRUE(visualCodec.FromJson(nlohmann::json::object(), catalog, &decodedVisual));
	EXPECT_EQ(decodedVisual.materialId, kInvalidAssetId);
}
