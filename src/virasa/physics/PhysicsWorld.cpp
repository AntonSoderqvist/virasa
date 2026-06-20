#include "virasa/physics/PhysicsWorld.h"

#include "virasa/ecs/World.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include "glm/geometric.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace virasa::physics
{

namespace
{

namespace Layers
{
	static constexpr JPH::ObjectLayer NonMoving = 0;
	static constexpr JPH::ObjectLayer Moving = 1;
	static constexpr JPH::ObjectLayer Count = 2;
}

namespace BroadPhaseLayers
{
	static constexpr JPH::BroadPhaseLayer NonMoving(0);
	static constexpr JPH::BroadPhaseLayer Moving(1);
	static constexpr JPH::uint Count = 2;
}

struct EntityHash
{
	size_t operator()(virasa::ecs::Entity entity) const noexcept
	{
		const uint64_t packed =
			(static_cast<uint64_t>(entity.index) << 32u) | entity.generation;
		return std::hash<uint64_t>{}(packed);
	}
};

struct BodyIDHash
{
	size_t operator()(JPH::BodyID bodyId) const noexcept
	{
		return std::hash<JPH::uint32>{}(bodyId.GetIndexAndSequenceNumber());
	}
};

struct CollisionMeshGeometry
{
	std::vector<virasa::math::Vec3> vertices;
	std::vector<uint32_t> indices;
};

class OptionalIgnoreBodyFilter final : public JPH::BodyFilter
{
public:
	explicit OptionalIgnoreBodyFilter(JPH::BodyID ignoredBody) noexcept
		: _ignoredBody(ignoredBody)
	{
	}

	bool ShouldCollide(const JPH::BodyID& bodyId) const override
	{
		return _ignoredBody.IsInvalid() || bodyId != _ignoredBody;
	}

private:
	JPH::BodyID _ignoredBody;
};

class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter
{
public:
	bool ShouldCollide(JPH::ObjectLayer object1, JPH::ObjectLayer object2) const override
	{
		switch (object1)
		{
		case Layers::NonMoving:
			return object2 == Layers::Moving;
		case Layers::Moving:
			return true;
		default:
			assert(false);
			return false;
		}
	}
};

class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
{
public:
	BPLayerInterfaceImpl()
	{
		_objectToBroadPhase[Layers::NonMoving] = BroadPhaseLayers::NonMoving;
		_objectToBroadPhase[Layers::Moving] = BroadPhaseLayers::Moving;
	}

	JPH::uint GetNumBroadPhaseLayers() const override
	{
		return BroadPhaseLayers::Count;
	}

	JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
	{
		assert(layer < Layers::Count);
		return _objectToBroadPhase[layer];
	}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
	{
		switch (static_cast<JPH::BroadPhaseLayer::Type>(layer))
		{
		case static_cast<JPH::BroadPhaseLayer::Type>(BroadPhaseLayers::NonMoving):
			return "NonMoving";
		case static_cast<JPH::BroadPhaseLayer::Type>(BroadPhaseLayers::Moving):
			return "Moving";
		default:
			assert(false);
			return "Invalid";
		}
	}
#endif

private:
	JPH::BroadPhaseLayer _objectToBroadPhase[Layers::Count];
};

class ObjectVsBroadPhaseLayerFilterImpl final
	: public JPH::ObjectVsBroadPhaseLayerFilter
{
public:
	bool ShouldCollide(
		JPH::ObjectLayer layer,
		JPH::BroadPhaseLayer broadPhaseLayer) const override
	{
		switch (layer)
		{
		case Layers::NonMoving:
			return broadPhaseLayer == BroadPhaseLayers::Moving;
		case Layers::Moving:
			return true;
		default:
			assert(false);
			return false;
		}
	}
};

void InitializeJoltOnce()
{
	static std::once_flag initFlag;
	std::call_once(initFlag, []()
	{
		JPH::RegisterDefaultAllocator();
		static std::unique_ptr<JPH::Factory> factory;
		if (JPH::Factory::sInstance == nullptr)
		{
			factory = std::make_unique<JPH::Factory>();
			JPH::Factory::sInstance = factory.get();
		}
		JPH::RegisterTypes();
	});
}

JPH::Vec3 ToJoltVec3(const virasa::math::Vec3& value)
{
	return JPH::Vec3(value.x, value.y, value.z);
}

JPH::RVec3 ToJoltRVec3(const virasa::math::Vec3& value)
{
	return JPH::RVec3(value.x, value.y, value.z);
}

JPH::Quat ToJoltQuat(const virasa::math::Quat& value)
{
	return JPH::Quat(value.x, value.y, value.z, value.w);
}

virasa::math::Vec3 FromJoltRVec3(const JPH::RVec3& value)
{
	return virasa::math::Vec3(
		static_cast<float>(value.GetX()),
		static_cast<float>(value.GetY()),
		static_cast<float>(value.GetZ()));
}

virasa::math::Vec3 FromJoltVec3(const JPH::Vec3& value)
{
	return virasa::math::Vec3(value.GetX(), value.GetY(), value.GetZ());
}

virasa::math::Quat FromJoltQuat(const JPH::Quat& value)
{
	return virasa::math::Quat(value.GetW(), value.GetX(), value.GetY(), value.GetZ());
}

bool NormalizeDirection(
	const virasa::math::Vec3& direction,
	virasa::math::Vec3& outNormalizedDirection)
{
	const float length = glm::length(direction);
	if (length <= 0.0f)
		return false;

	outNormalizedDirection = direction / length;
	return true;
}

JPH::EMotionType ToJoltMotionType(virasa::physics::BodyType bodyType)
{
	switch (bodyType)
	{
	case virasa::physics::BodyType::Static:
		return JPH::EMotionType::Static;
	case virasa::physics::BodyType::Kinematic:
		return JPH::EMotionType::Kinematic;
	case virasa::physics::BodyType::Dynamic:
		return JPH::EMotionType::Dynamic;
	default:
		assert(false);
		return JPH::EMotionType::Dynamic;
	}
}

JPH::ObjectLayer ToJoltObjectLayer(virasa::physics::BodyType bodyType)
{
	return bodyType == virasa::physics::BodyType::Static
		? Layers::NonMoving
		: Layers::Moving;
}

bool HasOffset(const virasa::math::Vec3& offset)
{
	return offset.x != 0.0f || offset.y != 0.0f || offset.z != 0.0f;
}

bool HasUnitScale(const virasa::math::Vec3& scale)
{
	return scale.x == 1.0f && scale.y == 1.0f && scale.z == 1.0f;
}

JPH::ShapeRefC CreateShape(
	const virasa::physics::ColliderComponent& collider,
	const virasa::math::Vec3& scale,
	const std::unordered_map<uint32_t, CollisionMeshGeometry>& collisionMeshes)
{
	JPH::ShapeRefC shape;
	switch (collider.shape)
	{
	case virasa::physics::ColliderShape::Box:
		shape = new JPH::BoxShape(ToJoltVec3(collider.halfExtents));
		break;
	case virasa::physics::ColliderShape::Sphere:
		shape = new JPH::SphereShape(collider.radius);
		break;
	case virasa::physics::ColliderShape::Capsule:
	{
		JPH::ShapeRefC capsule = new JPH::CapsuleShape(collider.halfHeight, collider.radius);
		const JPH::Quat yToZ =
			JPH::Quat::sRotation(JPH::Vec3::sAxisX(), 0.5f * JPH::JPH_PI);
		shape = new JPH::RotatedTranslatedShape(
			JPH::Vec3::sZero(),
			yToZ,
			capsule);
		break;
	}
	case virasa::physics::ColliderShape::Mesh:
	{
		const auto meshIter = collisionMeshes.find(collider.meshId);
		if (meshIter == collisionMeshes.end())
			return {};

		const CollisionMeshGeometry& geometry = meshIter->second;
		JPH::VertexList vertices;
		vertices.reserve(geometry.vertices.size());
		for (const virasa::math::Vec3& vertex : geometry.vertices)
			vertices.push_back(JPH::Float3(vertex.x, vertex.y, vertex.z));

		JPH::IndexedTriangleList triangles;
		triangles.reserve(geometry.indices.size() / 3u);
		for (size_t i = 0; i + 2u < geometry.indices.size(); i += 3u)
		{
			triangles.push_back(JPH::IndexedTriangle(
				geometry.indices[i],
				geometry.indices[i + 1u],
				geometry.indices[i + 2u],
				0u));
		}

		JPH::MeshShapeSettings settings(std::move(vertices), std::move(triangles));
		shape = settings.Create().Get();
		break;
	}
	default:
		assert(false);
		shape = new JPH::BoxShape(ToJoltVec3(collider.halfExtents));
		break;
	}

	if (!HasUnitScale(scale))
		shape = new JPH::ScaledShape(shape, ToJoltVec3(scale));

	if (HasOffset(collider.offset))
	{
		shape = new JPH::RotatedTranslatedShape(
			ToJoltVec3(collider.offset),
			JPH::Quat::sIdentity(),
			shape);
	}

	return shape;
}

JPH::uint ThreadCount()
{
	const uint32_t hardwareThreads = std::thread::hardware_concurrency();
	if (hardwareThreads <= 1u)
		return 1u;
	return static_cast<JPH::uint>(hardwareThreads - 1u);
}

JPH::BodyID BodyToIgnore(
	const std::unordered_map<virasa::ecs::Entity, JPH::BodyID, EntityHash>& bodies,
	virasa::ecs::Entity ignore)
{
	auto iter = bodies.find(ignore);
	return iter != bodies.end() ? iter->second : JPH::BodyID();
}

virasa::ecs::Entity EntityForBody(
	const std::unordered_map<JPH::BodyID, virasa::ecs::Entity, BodyIDHash>& entitiesByBody,
	JPH::BodyID bodyId)
{
	auto iter = entitiesByBody.find(bodyId);
	return iter != entitiesByBody.end()
		? iter->second
		: virasa::ecs::Entity::Invalid();
}

} // namespace

struct PhysicsWorld::Impl
{
	explicit Impl(const virasa::physics::PhysicsConfig& config)
		: tempAllocator(10u * 1024u * 1024u)
		, jobSystem(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, ThreadCount())
	{
		const JPH::uint maxBodies = static_cast<JPH::uint>(config.maxBodies);
		constexpr JPH::uint numBodyMutexes = 0;
		const JPH::uint maxBodyPairs = static_cast<JPH::uint>(config.maxBodyPairs);
		const JPH::uint maxContactConstraints =
			static_cast<JPH::uint>(config.maxContactConstraints);

		physicsSystem.Init(
			maxBodies,
			numBodyMutexes,
			maxBodyPairs,
			maxContactConstraints,
			broadPhaseLayerInterface,
			objectVsBroadPhaseLayerFilter,
			objectLayerPairFilter);
		physicsSystem.SetGravity(ToJoltVec3(config.gravity));
	}

	~Impl()
	{
		JPH::BodyInterface& bodyInterface = physicsSystem.GetBodyInterface();
		for (const auto& entry : bodies)
		{
			bodyInterface.RemoveBody(entry.second);
			bodyInterface.DestroyBody(entry.second);
		}
		bodies.clear();
		entitiesByBody.clear();
	}

	BPLayerInterfaceImpl broadPhaseLayerInterface;
	ObjectVsBroadPhaseLayerFilterImpl objectVsBroadPhaseLayerFilter;
	ObjectLayerPairFilterImpl objectLayerPairFilter;
	JPH::PhysicsSystem physicsSystem;
	JPH::TempAllocatorImpl tempAllocator;
	JPH::JobSystemThreadPool jobSystem;
	std::unordered_map<virasa::ecs::Entity, JPH::BodyID, EntityHash> bodies;
	std::unordered_map<JPH::BodyID, virasa::ecs::Entity, BodyIDHash> entitiesByBody;
	std::unordered_map<uint32_t, CollisionMeshGeometry> collisionMeshes;
};

PhysicsWorld::PhysicsWorld(const virasa::physics::PhysicsConfig& config)
{
	InitializeJoltOnce();
	_impl = std::make_unique<Impl>(config);
}

PhysicsWorld::~PhysicsWorld() noexcept = default;

void PhysicsWorld::AddBody(
	virasa::ecs::Entity entity,
	const virasa::physics::RigidBodyComponent& body,
	const virasa::physics::ColliderComponent& collider,
	const virasa::math::Transform& transform)
{
	assert(entity != virasa::ecs::Entity::Invalid());
	assert(!HasBody(entity));

	const virasa::math::Vec3 shapeScale = transform.scale * collider.scaleFactor;
	JPH::ShapeRefC shape = CreateShape(collider, shapeScale, _impl->collisionMeshes);
	if (shape.GetPtr() == nullptr)
		return;

	JPH::BodyCreationSettings settings(
		shape,
		ToJoltRVec3(transform.translation),
		ToJoltQuat(transform.rotation),
		ToJoltMotionType(body.bodyType),
		ToJoltObjectLayer(body.bodyType));
	settings.mLinearDamping = body.linearDamping;
	settings.mAngularDamping = body.angularDamping;
	settings.mFriction = body.friction;
	settings.mRestitution = body.restitution;
	settings.mGravityFactor = body.gravityFactor;

	if (body.bodyType == virasa::physics::BodyType::Dynamic)
	{
		settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
		settings.mMassPropertiesOverride.mMass = body.mass;
	}

	const JPH::EActivation activation =
		body.bodyType == virasa::physics::BodyType::Dynamic
			? JPH::EActivation::Activate
			: JPH::EActivation::DontActivate;

	JPH::BodyInterface& bodyInterface = _impl->physicsSystem.GetBodyInterface();
	JPH::BodyID bodyId = bodyInterface.CreateAndAddBody(settings, activation);
	assert(!bodyId.IsInvalid());
	_impl->bodies.emplace(entity, bodyId);
	_impl->entitiesByBody.emplace(bodyId, entity);
}

void PhysicsWorld::RegisterCollisionMesh(
	uint32_t meshId,
	std::vector<virasa::math::Vec3> vertices,
	std::vector<uint32_t> indices)
{
	_impl->collisionMeshes[meshId] = CollisionMeshGeometry{
		std::move(vertices),
		std::move(indices)};
}

void PhysicsWorld::RemoveBody(virasa::ecs::Entity entity)
{
	auto iter = _impl->bodies.find(entity);
	if (iter == _impl->bodies.end())
		return;

	JPH::BodyInterface& bodyInterface = _impl->physicsSystem.GetBodyInterface();
	bodyInterface.RemoveBody(iter->second);
	bodyInterface.DestroyBody(iter->second);
	_impl->entitiesByBody.erase(iter->second);
	_impl->bodies.erase(iter);
}

bool PhysicsWorld::HasBody(virasa::ecs::Entity entity) const noexcept
{
	return _impl->bodies.find(entity) != _impl->bodies.end();
}

size_t PhysicsWorld::BodyCount() const noexcept
{
	return _impl->bodies.size();
}

void PhysicsWorld::SetBodyTransform(
	virasa::ecs::Entity entity,
	const virasa::math::Transform& transform)
{
	auto iter = _impl->bodies.find(entity);
	if (iter == _impl->bodies.end())
		return;

	JPH::BodyInterface& bodyInterface = _impl->physicsSystem.GetBodyInterface();
	bodyInterface.SetPositionAndRotation(
		iter->second,
		ToJoltRVec3(transform.translation),
		ToJoltQuat(transform.rotation),
		JPH::EActivation::Activate);
}

virasa::math::Transform PhysicsWorld::GetBodyTransform(virasa::ecs::Entity entity) const
{
	auto iter = _impl->bodies.find(entity);
	if (iter == _impl->bodies.end())
		return virasa::math::Transform::Identity();

	JPH::RVec3 position;
	JPH::Quat rotation;
	const JPH::BodyInterface& bodyInterface = _impl->physicsSystem.GetBodyInterface();
	bodyInterface.GetPositionAndRotation(iter->second, position, rotation);

	virasa::math::Transform transform;
	transform.translation = FromJoltRVec3(position);
	transform.rotation = FromJoltQuat(rotation);
	transform.scale = virasa::math::Vec3(1.0f, 1.0f, 1.0f);
	return transform;
}

void PhysicsWorld::Step(float deltaTime)
{
	if (deltaTime <= 0.0f)
		return;

	_impl->physicsSystem.Update(deltaTime, 1, &_impl->tempAllocator, &_impl->jobSystem);
}

void PhysicsWorld::SyncToWorld(virasa::ecs::World& world) const
{
	virasa::ecs::TransformSystem& transforms = world.Transforms();
	for (const auto& entry : _impl->bodies)
	{
		if (!transforms.Has(entry.first))
			continue;

		virasa::math::Transform local = transforms.GetLocal(entry.first);
		const virasa::math::Transform bodyTransform = GetBodyTransform(entry.first);
		local.translation = bodyTransform.translation;
		local.rotation = bodyTransform.rotation;
		transforms.SetLocal(entry.first, local);
	}
}

virasa::physics::CastHit PhysicsWorld::RayCast(
	const virasa::math::Vec3& origin,
	const virasa::math::Vec3& direction,
	float maxDistance,
	virasa::ecs::Entity ignore) const
{
	virasa::math::Vec3 normalizedDirection;
	if (maxDistance <= 0.0f || !NormalizeDirection(direction, normalizedDirection))
		return virasa::physics::CastHit{};

	const JPH::RRayCast ray(
		ToJoltRVec3(origin),
		maxDistance * ToJoltVec3(normalizedDirection));
	const OptionalIgnoreBodyFilter bodyFilter(BodyToIgnore(_impl->bodies, ignore));

	JPH::RayCastResult hit;
	if (!_impl->physicsSystem.GetNarrowPhaseQuery().CastRay(ray, hit, {}, {}, bodyFilter))
		return virasa::physics::CastHit{};

	virasa::physics::CastHit result;
	result.hit = true;
	result.entity = EntityForBody(_impl->entitiesByBody, hit.mBodyID);
	result.fraction = hit.mFraction;
	result.point = FromJoltRVec3(ray.GetPointOnRay(hit.mFraction));

	JPH::BodyLockRead lock(_impl->physicsSystem.GetBodyLockInterface(), hit.mBodyID);
	if (lock.Succeeded())
	{
		result.normal = FromJoltVec3(
			lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, ToJoltRVec3(result.point)));
	}

	return result;
}

virasa::physics::CastHit PhysicsWorld::SphereCast(
	const virasa::math::Vec3& origin,
	float radius,
	const virasa::math::Vec3& direction,
	float maxDistance,
	virasa::ecs::Entity ignore) const
{
	virasa::math::Vec3 normalizedDirection;
	if (maxDistance <= 0.0f || !NormalizeDirection(direction, normalizedDirection))
		return virasa::physics::CastHit{};

	JPH::ShapeRefC sphere = new JPH::SphereShape(radius);
	const JPH::RShapeCast shapeCast(
		sphere,
		JPH::Vec3::sOne(),
		JPH::RMat44::sTranslation(ToJoltRVec3(origin)),
		maxDistance * ToJoltVec3(normalizedDirection));
	const OptionalIgnoreBodyFilter bodyFilter(BodyToIgnore(_impl->bodies, ignore));

	JPH::ShapeCastSettings settings;
	JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
	_impl->physicsSystem.GetNarrowPhaseQuery().CastShape(
		shapeCast,
		settings,
		JPH::RVec3::sZero(),
		collector,
		{},
		{},
		bodyFilter);

	if (!collector.HadHit())
		return virasa::physics::CastHit{};

	const JPH::ShapeCastResult& hit = collector.mHit;
	virasa::physics::CastHit result;
	result.hit = true;
	result.entity = EntityForBody(_impl->entitiesByBody, hit.mBodyID2);
	result.fraction = hit.mFraction;
	result.point = FromJoltVec3(hit.mContactPointOn2);

	if (hit.mPenetrationAxis.LengthSq() > 0.0f)
		result.normal = FromJoltVec3(-hit.mPenetrationAxis.Normalized());
	else
		result.normal = -normalizedDirection;

	return result;
}

void PhysicsWorld::AddForce(
	virasa::ecs::Entity entity,
	const virasa::math::Vec3& force)
{
	auto iter = _impl->bodies.find(entity);
	if (iter == _impl->bodies.end())
		return;

	_impl->physicsSystem.GetBodyInterface().AddForce(iter->second, ToJoltVec3(force));
}

void PhysicsWorld::AddForceAtPoint(
	virasa::ecs::Entity entity,
	const virasa::math::Vec3& force,
	const virasa::math::Vec3& worldPoint)
{
	auto iter = _impl->bodies.find(entity);
	if (iter == _impl->bodies.end())
		return;

	_impl->physicsSystem.GetBodyInterface().AddForce(
		iter->second,
		ToJoltVec3(force),
		ToJoltRVec3(worldPoint));
}

void PhysicsWorld::AddTorque(
	virasa::ecs::Entity entity,
	const virasa::math::Vec3& torque)
{
	auto iter = _impl->bodies.find(entity);
	if (iter == _impl->bodies.end())
		return;

	_impl->physicsSystem.GetBodyInterface().AddTorque(iter->second, ToJoltVec3(torque));
}

virasa::math::Vec3 PhysicsWorld::GetLinearVelocity(virasa::ecs::Entity entity) const
{
	auto iter = _impl->bodies.find(entity);
	if (iter == _impl->bodies.end())
		return virasa::math::Vec3(0.0f, 0.0f, 0.0f);

	return FromJoltVec3(_impl->physicsSystem.GetBodyInterface().GetLinearVelocity(iter->second));
}

virasa::math::Vec3 PhysicsWorld::GetPointVelocity(
	virasa::ecs::Entity entity,
	const virasa::math::Vec3& worldPoint) const
{
	auto iter = _impl->bodies.find(entity);
	if (iter == _impl->bodies.end())
		return virasa::math::Vec3(0.0f, 0.0f, 0.0f);

	return FromJoltVec3(
		_impl->physicsSystem.GetBodyInterface().GetPointVelocity(
			iter->second,
			ToJoltRVec3(worldPoint)));
}

} // namespace virasa::physics
