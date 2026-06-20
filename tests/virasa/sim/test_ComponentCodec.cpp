#include <gtest/gtest.h>

#include "virasa/sim/ComponentCodec.h"

#include "virasa/ecs/Components.h"
#include "virasa/math/Transform.h"
#include "virasa/physics/PhysicsComponents.h"
#include "virasa/sim/GameplayComponents.h"
#include "virasa/sim/VehicleComponent.h"
#include "virasa/sim/WheelComponent.h"
#include "virasa/sim/behaviors/ChaseCameraBehavior.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
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
		const SerializationContext& context) const override
	{
		(void)context;
		return nlohmann::json{{"value", *static_cast<const uint32_t*>(component)}};
	}

	bool FromJson(
		const nlohmann::json& json,
		const SerializationContext& context,
		void* dst) const override
	{
		(void)context;
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

class TestEntityResolver final : public EntityResolver
{
public:
	[[nodiscard]] int32_t StableIdForEntity(virasa::ecs::Entity entity) const override
	{
		for (const auto& binding : _bindings)
		{
			if (binding.second == entity)
			{
				return binding.first;
			}
		}

		return -1;
	}

	[[nodiscard]] virasa::ecs::Entity EntityForStableId(int32_t stableId) const override
	{
		for (const auto& binding : _bindings)
		{
			if (binding.first == stableId)
			{
				return binding.second;
			}
		}

		return virasa::ecs::Entity::Invalid();
	}

	void Bind(int32_t stableId, virasa::ecs::Entity entity)
	{
		_bindings.emplace_back(stableId, entity);
	}

private:
	std::vector<std::pair<int32_t, virasa::ecs::Entity>> _bindings;
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

void ExpectEntityEq(virasa::ecs::Entity actual, virasa::ecs::Entity expected)
{
	EXPECT_EQ(actual.index, expected.index);
	EXPECT_EQ(actual.generation, expected.generation);
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
	TestEntityResolver resolver;
	const SerializationContext context(catalog, resolver);

	virasa::math::Transform transform;
	transform.translation = virasa::math::Vec3(1.0f, 2.0f, 3.0f);
	transform.rotation = virasa::math::Quat(0.5f, 0.25f, 0.75f, 1.0f);
	transform.scale = virasa::math::Vec3(4.0f, 5.0f, 6.0f);

	EXPECT_EQ(codec.ComponentName(), std::string_view("Transform"));
	EXPECT_EQ(codec.ElementSize(), sizeof(virasa::math::Transform));

	const nlohmann::json json = codec.ToJson(&transform, context);
	ExpectJsonVec3Eq(json["translation"], transform.translation);
	ExpectJsonQuatEq(json["rotation"], transform.rotation);
	ExpectJsonVec3Eq(json["scale"], transform.scale);

	virasa::math::Transform decoded;
	ASSERT_TRUE(codec.FromJson(json, context, &decoded));
	ExpectVec3Eq(decoded.translation, transform.translation);
	ExpectQuatEq(decoded.rotation, transform.rotation);
	ExpectVec3Eq(decoded.scale, transform.scale);

	const nlohmann::json unreadable = nlohmann::json::array();
	decoded.translation = virasa::math::Vec3(9.0f, 9.0f, 9.0f);
	EXPECT_FALSE(codec.FromJson(unreadable, context, &decoded));
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

TEST(ComponentCodec, serialization_context_bundles_catalog_and_entity_resolver)
{
	static_assert(!std::is_default_constructible_v<SerializationContext>);
	static_assert(!std::is_copy_constructible_v<SerializationContext>);
	static_assert(!std::is_copy_assignable_v<SerializationContext>);
	static_assert(!std::is_move_constructible_v<SerializationContext>);
	static_assert(!std::is_move_assignable_v<SerializationContext>);

	const AssetCatalog catalog;
	const TestEntityResolver resolver;
	const SerializationContext context(catalog, resolver);

	EXPECT_EQ(&context.catalog, &catalog);
	EXPECT_EQ(&context.entities, static_cast<const EntityResolver*>(&resolver));
}

TEST(ComponentCodec, component_codec_entity_fields_round_trip_through_resolver)
{
	ComponentCodecRegistry registry;
	RegisterBuiltinComponentCodecs(registry);
	const ComponentCodec& vehicleCodec = RequireCodec(registry, "Vehicle");

	const AssetCatalog catalog;
	TestEntityResolver resolver;
	const virasa::ecs::Entity frontRight{20u, 4u};
	const virasa::ecs::Entity backRight{22u, 6u};
	resolver.Bind(201, frontRight);
	resolver.Bind(203, backRight);
	const SerializationContext context(catalog, resolver);

	virasa::sim::VehicleComponent vehicle;
	vehicle.wheels.frontRight = frontRight;
	vehicle.wheels.frontLeft = virasa::ecs::Entity::Invalid();
	vehicle.wheels.backRight = backRight;
	vehicle.wheels.backLeft = virasa::ecs::Entity::Invalid();

	const nlohmann::json json = vehicleCodec.ToJson(&vehicle, context);
	ASSERT_TRUE(json["wheels"].is_object());
	EXPECT_EQ(json["wheels"]["frontRight"].get<int32_t>(), 201);
	EXPECT_TRUE(json["wheels"]["frontLeft"].is_null());
	EXPECT_EQ(json["wheels"]["backRight"].get<int32_t>(), 203);
	EXPECT_TRUE(json["wheels"]["backLeft"].is_null());

	virasa::sim::VehicleComponent decoded;
	ASSERT_TRUE(vehicleCodec.FromJson(
		nlohmann::json{
			{"wheels",
				nlohmann::json{
					{"frontRight", 201},
					{"frontLeft", nullptr},
					{"backRight", 203},
				}},
		},
		context,
		&decoded));
	ExpectEntityEq(decoded.wheels.frontRight, frontRight);
	ExpectEntityEq(decoded.wheels.frontLeft, virasa::ecs::Entity::Invalid());
	ExpectEntityEq(decoded.wheels.backRight, backRight);
	ExpectEntityEq(decoded.wheels.backLeft, virasa::ecs::Entity::Invalid());
}

TEST(ComponentCodec, test_register_builtin_component_codecs_covers_serializable_components)
{
	ComponentCodecRegistry registry;
	RegisterBuiltinComponentCodecs(registry);
	const AssetCatalog catalog;
	TestEntityResolver resolver;
	const SerializationContext context(catalog, resolver);

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
		"RigidBody",
		"Collider",
		"Wheel",
		"Vehicle",
		"ChaseCamera",
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
		directional.ToJson(&directionalLight, context),
		context,
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
		point.ToJson(&pointLight, context),
		context,
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
	ASSERT_TRUE(spot.FromJson(spot.ToJson(&spotLight, context), context, &decodedSpot));
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
	const nlohmann::json cameraJson = camera.ToJson(&cameraComponent, context);
	EXPECT_EQ(cameraJson["domain"].get<uint32_t>(), 1u);
	ASSERT_TRUE(camera.FromJson(cameraJson, context, &decodedCamera));
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
		highlight.ToJson(&highlightComponent, context),
		context,
		&decodedHighlight));
	ExpectVec3Eq(decodedHighlight.color, highlightComponent.color);
	EXPECT_FLOAT_EQ(decodedHighlight.intensity, highlightComponent.intensity);
	EXPECT_EQ(decodedHighlight.priority, highlightComponent.priority);

	const ComponentCodec& spin = RequireCodec(registry, "Spin");
	virasa::sim::SpinComponent spinComponent;
	spinComponent.angularVelocity = virasa::math::Vec3(0.1f, 0.2f, 0.3f);
	virasa::sim::SpinComponent decodedSpin;
	ASSERT_TRUE(spin.FromJson(
		spin.ToJson(&spinComponent, context),
		context,
		&decodedSpin));
	ExpectVec3Eq(decodedSpin.angularVelocity, spinComponent.angularVelocity);

	const ComponentCodec& rigidBody = RequireCodec(registry, "RigidBody");
	virasa::physics::RigidBodyComponent rigidBodyComponent;
	rigidBodyComponent.bodyType = virasa::physics::BodyType::Kinematic;
	rigidBodyComponent.mass = 12.5f;
	rigidBodyComponent.linearDamping = 0.15f;
	rigidBodyComponent.angularDamping = 0.25f;
	rigidBodyComponent.friction = 0.65f;
	rigidBodyComponent.restitution = 0.35f;
	rigidBodyComponent.gravityFactor = 0.5f;
	virasa::physics::RigidBodyComponent decodedRigidBody;
	const nlohmann::json rigidBodyJson = rigidBody.ToJson(&rigidBodyComponent, context);
	EXPECT_EQ(rigidBody.ComponentName(), std::string_view("RigidBody"));
	EXPECT_EQ(rigidBody.ElementSize(), sizeof(virasa::physics::RigidBodyComponent));
	EXPECT_EQ(rigidBodyJson["bodyType"].get<uint32_t>(), 1u);
	ASSERT_TRUE(rigidBody.FromJson(rigidBodyJson, context, &decodedRigidBody));
	EXPECT_EQ(decodedRigidBody.bodyType, rigidBodyComponent.bodyType);
	EXPECT_FLOAT_EQ(decodedRigidBody.mass, rigidBodyComponent.mass);
	EXPECT_FLOAT_EQ(decodedRigidBody.linearDamping, rigidBodyComponent.linearDamping);
	EXPECT_FLOAT_EQ(decodedRigidBody.angularDamping, rigidBodyComponent.angularDamping);
	EXPECT_FLOAT_EQ(decodedRigidBody.friction, rigidBodyComponent.friction);
	EXPECT_FLOAT_EQ(decodedRigidBody.restitution, rigidBodyComponent.restitution);
	EXPECT_FLOAT_EQ(decodedRigidBody.gravityFactor, rigidBodyComponent.gravityFactor);

	const ComponentCodec& collider = RequireCodec(registry, "Collider");
	virasa::physics::ColliderComponent colliderComponent;
	colliderComponent.shape = virasa::physics::ColliderShape::Mesh;
	colliderComponent.halfExtents = virasa::math::Vec3(1.0f, 2.0f, 3.0f);
	colliderComponent.radius = 0.75f;
	colliderComponent.halfHeight = 2.5f;
	colliderComponent.offset = virasa::math::Vec3(-1.0f, 0.5f, 4.0f);
	colliderComponent.scaleFactor = virasa::math::Vec3(1.25f, 0.75f, 2.0f);
	colliderComponent.meshId = 42u;
	virasa::physics::ColliderComponent decodedCollider;
	const nlohmann::json colliderJson = collider.ToJson(&colliderComponent, context);
	EXPECT_EQ(collider.ComponentName(), std::string_view("Collider"));
	EXPECT_EQ(collider.ElementSize(), sizeof(virasa::physics::ColliderComponent));
	EXPECT_EQ(colliderJson["shape"].get<uint32_t>(), 3u);
	ExpectJsonVec3Eq(colliderJson["halfExtents"], colliderComponent.halfExtents);
	EXPECT_FLOAT_EQ(colliderJson["radius"].get<float>(), colliderComponent.radius);
	EXPECT_FLOAT_EQ(colliderJson["halfHeight"].get<float>(), colliderComponent.halfHeight);
	ExpectJsonVec3Eq(colliderJson["offset"], colliderComponent.offset);
	ExpectJsonVec3Eq(colliderJson["scaleFactor"], colliderComponent.scaleFactor);
	ASSERT_TRUE(colliderJson.contains("meshId"));
	EXPECT_EQ(colliderJson["meshId"].get<uint32_t>(), colliderComponent.meshId);
	ASSERT_TRUE(collider.FromJson(colliderJson, context, &decodedCollider));
	EXPECT_EQ(decodedCollider.shape, colliderComponent.shape);
	ExpectVec3Eq(decodedCollider.halfExtents, colliderComponent.halfExtents);
	EXPECT_FLOAT_EQ(decodedCollider.radius, colliderComponent.radius);
	EXPECT_FLOAT_EQ(decodedCollider.halfHeight, colliderComponent.halfHeight);
	ExpectVec3Eq(decodedCollider.offset, colliderComponent.offset);
	ExpectVec3Eq(decodedCollider.scaleFactor, colliderComponent.scaleFactor);
	EXPECT_EQ(decodedCollider.meshId, colliderComponent.meshId);

	nlohmann::json colliderJsonWithoutScale = colliderJson;
	colliderJsonWithoutScale.erase("scaleFactor");
	ASSERT_TRUE(collider.FromJson(colliderJsonWithoutScale, context, &decodedCollider));
	ExpectVec3Eq(decodedCollider.scaleFactor, virasa::physics::ColliderComponent{}.scaleFactor);

	const nlohmann::json colliderJsonWithoutMeshId{
		{"shape", static_cast<uint32_t>(virasa::physics::ColliderShape::Mesh)},
		{"halfExtents", nlohmann::json::array({1.0f, 2.0f, 3.0f})},
		{"radius", 0.75f},
		{"halfHeight", 2.5f},
		{"offset", nlohmann::json::array({-1.0f, 0.5f, 4.0f})},
		{"scaleFactor", nlohmann::json::array({1.25f, 0.75f, 2.0f})},
	};
	ASSERT_TRUE(collider.FromJson(colliderJsonWithoutMeshId, context, &decodedCollider));
	EXPECT_EQ(decodedCollider.shape, virasa::physics::ColliderShape::Mesh);
	ExpectVec3Eq(decodedCollider.halfExtents, colliderComponent.halfExtents);
	EXPECT_FLOAT_EQ(decodedCollider.radius, colliderComponent.radius);
	EXPECT_FLOAT_EQ(decodedCollider.halfHeight, colliderComponent.halfHeight);
	ExpectVec3Eq(decodedCollider.offset, colliderComponent.offset);
	ExpectVec3Eq(decodedCollider.scaleFactor, colliderComponent.scaleFactor);
	EXPECT_EQ(decodedCollider.meshId, virasa::physics::ColliderComponent{}.meshId);

	const ComponentCodec& wheel = RequireCodec(registry, "Wheel");
	virasa::sim::WheelComponent wheelComponent;
	wheelComponent.steered = true;
	wheelComponent.driven = false;
	wheelComponent.radiusFactor = 1.35f;
	virasa::sim::WheelComponent decodedWheel;
	const nlohmann::json wheelJson = wheel.ToJson(&wheelComponent, context);
	EXPECT_EQ(wheel.ComponentName(), std::string_view("Wheel"));
	EXPECT_EQ(wheel.ElementSize(), sizeof(virasa::sim::WheelComponent));
	EXPECT_EQ(wheelJson["steered"].get<bool>(), wheelComponent.steered);
	EXPECT_EQ(wheelJson["driven"].get<bool>(), wheelComponent.driven);
	EXPECT_FLOAT_EQ(wheelJson["radiusFactor"].get<float>(), wheelComponent.radiusFactor);
	ASSERT_TRUE(wheel.FromJson(wheelJson, context, &decodedWheel));
	EXPECT_EQ(decodedWheel.steered, wheelComponent.steered);
	EXPECT_EQ(decodedWheel.driven, wheelComponent.driven);
	EXPECT_FLOAT_EQ(decodedWheel.radiusFactor, wheelComponent.radiusFactor);

	const ComponentCodec& vehicle = RequireCodec(registry, "Vehicle");
	const virasa::ecs::Entity frontRight{10u, 1u};
	const virasa::ecs::Entity frontLeft{11u, 2u};
	const virasa::ecs::Entity backRight{12u, 3u};
	resolver.Bind(101, frontRight);
	resolver.Bind(102, frontLeft);
	resolver.Bind(103, backRight);
	virasa::sim::VehicleComponent vehicleComponent;
	vehicleComponent.wheels.frontRight = frontRight;
	vehicleComponent.wheels.frontLeft = frontLeft;
	vehicleComponent.wheels.backRight = backRight;
	vehicleComponent.wheels.backLeft = virasa::ecs::Entity::Invalid();
	vehicleComponent.suspensionRestLength = 0.75f;
	vehicleComponent.suspensionStiffness = 42000.0f;
	vehicleComponent.suspensionDamping = 5100.0f;
	vehicleComponent.maxSteerAngle = 0.67f;
	vehicleComponent.enginePower = 21000.0f;
	vehicleComponent.brakeForce = 13000.0f;
	vehicleComponent.tireGrip = 9500.0f;
	vehicleComponent.throttleAction = static_cast<virasa::input::ActionId>(11);
	vehicleComponent.steerAction = static_cast<virasa::input::ActionId>(12);
	vehicleComponent.brakeAction = static_cast<virasa::input::ActionId>(13);
	vehicleComponent.handbrakeAction = static_cast<virasa::input::ActionId>(14);
	virasa::sim::VehicleComponent decodedVehicle;
	const nlohmann::json vehicleJson = vehicle.ToJson(&vehicleComponent, context);
	EXPECT_EQ(vehicle.ComponentName(), std::string_view("Vehicle"));
	EXPECT_EQ(vehicle.ElementSize(), sizeof(virasa::sim::VehicleComponent));
	ASSERT_TRUE(vehicleJson["wheels"].is_object());
	EXPECT_EQ(vehicleJson["wheels"]["frontRight"].get<int32_t>(), 101);
	EXPECT_EQ(vehicleJson["wheels"]["frontLeft"].get<int32_t>(), 102);
	EXPECT_EQ(vehicleJson["wheels"]["backRight"].get<int32_t>(), 103);
	EXPECT_TRUE(vehicleJson["wheels"]["backLeft"].is_null());
	ASSERT_TRUE(vehicle.FromJson(vehicleJson, context, &decodedVehicle));
	ExpectEntityEq(decodedVehicle.wheels.frontRight, vehicleComponent.wheels.frontRight);
	ExpectEntityEq(decodedVehicle.wheels.frontLeft, vehicleComponent.wheels.frontLeft);
	ExpectEntityEq(decodedVehicle.wheels.backRight, vehicleComponent.wheels.backRight);
	ExpectEntityEq(decodedVehicle.wheels.backLeft, virasa::ecs::Entity::Invalid());
	EXPECT_FLOAT_EQ(decodedVehicle.suspensionRestLength, vehicleComponent.suspensionRestLength);
	EXPECT_FLOAT_EQ(decodedVehicle.suspensionStiffness, vehicleComponent.suspensionStiffness);
	EXPECT_FLOAT_EQ(decodedVehicle.suspensionDamping, vehicleComponent.suspensionDamping);
	EXPECT_FLOAT_EQ(decodedVehicle.maxSteerAngle, vehicleComponent.maxSteerAngle);
	EXPECT_FLOAT_EQ(decodedVehicle.enginePower, vehicleComponent.enginePower);
	EXPECT_FLOAT_EQ(decodedVehicle.brakeForce, vehicleComponent.brakeForce);
	EXPECT_FLOAT_EQ(decodedVehicle.tireGrip, vehicleComponent.tireGrip);
	EXPECT_EQ(decodedVehicle.throttleAction, vehicleComponent.throttleAction);
	EXPECT_EQ(decodedVehicle.steerAction, vehicleComponent.steerAction);
	EXPECT_EQ(decodedVehicle.brakeAction, vehicleComponent.brakeAction);
	EXPECT_EQ(decodedVehicle.handbrakeAction, vehicleComponent.handbrakeAction);

	const ComponentCodec& chaseCamera = RequireCodec(registry, "ChaseCamera");
	virasa::sim::behaviors::ChaseCameraComponent chaseCameraComponent;
	chaseCameraComponent.distance = 12.0f;
	chaseCameraComponent.height = 4.5f;
	chaseCameraComponent.lookAtHeight = 1.8f;
	chaseCameraComponent.positionSmoothing = 5.25f;
	virasa::sim::behaviors::ChaseCameraComponent decodedChaseCamera;
	const nlohmann::json chaseCameraJson = chaseCamera.ToJson(&chaseCameraComponent, context);
	EXPECT_EQ(chaseCamera.ComponentName(), std::string_view("ChaseCamera"));
	EXPECT_EQ(
		chaseCamera.ElementSize(),
		sizeof(virasa::sim::behaviors::ChaseCameraComponent));
	ASSERT_TRUE(chaseCamera.FromJson(chaseCameraJson, context, &decodedChaseCamera));
	EXPECT_FLOAT_EQ(decodedChaseCamera.distance, chaseCameraComponent.distance);
	EXPECT_FLOAT_EQ(decodedChaseCamera.height, chaseCameraComponent.height);
	EXPECT_FLOAT_EQ(decodedChaseCamera.lookAtHeight, chaseCameraComponent.lookAtHeight);
	EXPECT_FLOAT_EQ(
		decodedChaseCamera.positionSmoothing,
		chaseCameraComponent.positionSmoothing);

	virasa::ecs::HighlightComponent missingOptional;
	missingOptional.color = virasa::math::Vec3(9.0f, 9.0f, 9.0f);
	ASSERT_TRUE(highlight.FromJson(
		nlohmann::json{{"intensity", 7.0f}},
		context,
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
	const TestEntityResolver resolver;
	const SerializationContext context(catalog, resolver);

	virasa::ecs::MeshComponent mesh;
	mesh.meshId = 42u;
	const nlohmann::json meshJson = meshCodec.ToJson(&mesh, context);
	ASSERT_TRUE(meshJson.contains("meshId"));
	EXPECT_EQ(meshJson["meshId"], "builtin:cube");

	virasa::ecs::MeshComponent decodedMesh;
	ASSERT_TRUE(meshCodec.FromJson(meshJson, context, &decodedMesh));
	EXPECT_EQ(decodedMesh.meshId, 42u);

	virasa::ecs::VisualComponent visual;
	visual.materialId = 9u;
	const nlohmann::json visualJson = visualCodec.ToJson(&visual, context);
	ASSERT_TRUE(visualJson.contains("materialId"));
	EXPECT_EQ(visualJson["materialId"], "materials:default");

	virasa::ecs::VisualComponent decodedVisual;
	ASSERT_TRUE(visualCodec.FromJson(visualJson, context, &decodedVisual));
	EXPECT_EQ(decodedVisual.materialId, 9u);

	mesh.meshId = 77u;
	const nlohmann::json unresolvedMeshJson = meshCodec.ToJson(&mesh, context);
	EXPECT_TRUE(unresolvedMeshJson["meshId"].is_null());
	ASSERT_TRUE(meshCodec.FromJson(
		nlohmann::json{{"meshId", "missing:mesh"}},
		context,
		&decodedMesh));
	EXPECT_EQ(decodedMesh.meshId, kInvalidAssetId);
	ASSERT_TRUE(meshCodec.FromJson(
		nlohmann::json{{"meshId", nullptr}},
		context,
		&decodedMesh));
	EXPECT_EQ(decodedMesh.meshId, kInvalidAssetId);
	ASSERT_TRUE(meshCodec.FromJson(nlohmann::json::object(), context, &decodedMesh));
	EXPECT_EQ(decodedMesh.meshId, kInvalidAssetId);

	visual.materialId = kInvalidAssetId;
	const nlohmann::json unresolvedVisualJson = visualCodec.ToJson(&visual, context);
	EXPECT_TRUE(unresolvedVisualJson["materialId"].is_null());
	ASSERT_TRUE(visualCodec.FromJson(
		nlohmann::json{{"materialId", "missing:material"}},
		context,
		&decodedVisual));
	EXPECT_EQ(decodedVisual.materialId, kInvalidAssetId);
	ASSERT_TRUE(visualCodec.FromJson(
		nlohmann::json{{"materialId", nullptr}},
		context,
		&decodedVisual));
	EXPECT_EQ(decodedVisual.materialId, kInvalidAssetId);
	ASSERT_TRUE(visualCodec.FromJson(nlohmann::json::object(), context, &decodedVisual));
	EXPECT_EQ(decodedVisual.materialId, kInvalidAssetId);
}
