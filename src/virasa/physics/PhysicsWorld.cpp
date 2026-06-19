#include "virasa/physics/PhysicsWorld.h"

#include "virasa/ecs/World.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

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

virasa::math::Quat FromJoltQuat(const JPH::Quat& value)
{
	return virasa::math::Quat(value.GetW(), value.GetX(), value.GetY(), value.GetZ());
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

JPH::ShapeRefC CreateShape(const virasa::physics::ColliderComponent& collider)
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
	default:
		assert(false);
		shape = new JPH::BoxShape(ToJoltVec3(collider.halfExtents));
		break;
	}

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
	}

	BPLayerInterfaceImpl broadPhaseLayerInterface;
	ObjectVsBroadPhaseLayerFilterImpl objectVsBroadPhaseLayerFilter;
	ObjectLayerPairFilterImpl objectLayerPairFilter;
	JPH::PhysicsSystem physicsSystem;
	JPH::TempAllocatorImpl tempAllocator;
	JPH::JobSystemThreadPool jobSystem;
	std::unordered_map<virasa::ecs::Entity, JPH::BodyID, EntityHash> bodies;
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

	JPH::BodyCreationSettings settings(
		CreateShape(collider),
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
}

void PhysicsWorld::RemoveBody(virasa::ecs::Entity entity)
{
	auto iter = _impl->bodies.find(entity);
	if (iter == _impl->bodies.end())
		return;

	JPH::BodyInterface& bodyInterface = _impl->physicsSystem.GetBodyInterface();
	bodyInterface.RemoveBody(iter->second);
	bodyInterface.DestroyBody(iter->second);
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

} // namespace virasa::physics
