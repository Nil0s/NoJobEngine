#define GLM_ENABLE_EXPERIMENTAL

#include "Engine/Physics/PhysicsSystem.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Scene.h"

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <thread>
#include <mutex>
#include <iostream>

namespace NoJob
{
    namespace
    {
        class LineOfSightBodyFilter final : public JPH::BodyFilter
        {
        public:
            LineOfSightBodyFilter(
                JPH::BodyID observerBody,
                JPH::BodyID targetBody)
                : m_ObserverBody(observerBody),
                m_TargetBody(targetBody)
            {
            }

            bool ShouldCollide(
                const JPH::BodyID& bodyID) const override
            {
                return bodyID != m_ObserverBody &&
                    bodyID != m_TargetBody;
            }

            bool ShouldCollideLocked(
                const JPH::Body& body) const override
            {
                return !body.IsSensor();
            }

        private:
            JPH::BodyID m_ObserverBody;
            JPH::BodyID m_TargetBody;
        };
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
            static constexpr unsigned Count = 2;
        }

        class ObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter
        {
        public:
            bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override
            {
                if (a == Layers::NonMoving) return b == Layers::Moving;
                if (a == Layers::Moving) return true;
                return false;
            }
        };

        class BroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface
        {
        public:
            BroadPhaseLayerInterface()
            {
                m_Map[Layers::NonMoving] = BroadPhaseLayers::NonMoving;
                m_Map[Layers::Moving] = BroadPhaseLayers::Moving;
            }

            unsigned GetNumBroadPhaseLayers() const override
            {
                return BroadPhaseLayers::Count;
            }

            JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
            {
                JPH_ASSERT(layer < Layers::Count);
                return m_Map[layer];
            }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
            const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
            {
                if (layer == BroadPhaseLayers::NonMoving) return "NON_MOVING";
                if (layer == BroadPhaseLayers::Moving) return "MOVING";
                return "UNKNOWN";
            }
#endif
        private:
            JPH::BroadPhaseLayer m_Map[Layers::Count];
        };

        class ObjectVsBroadPhaseLayerFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
        {
        public:
            bool ShouldCollide(
                JPH::ObjectLayer layer,
                JPH::BroadPhaseLayer broadPhaseLayer) const override
            {
                if (layer == Layers::NonMoving)
                    return broadPhaseLayer == BroadPhaseLayers::Moving;
                return layer == Layers::Moving;
            }
        };

        JPH::EMotionType ToJoltMotion(RigidbodyType type)
        {
            switch (type)
            {
            case RigidbodyType::Static: return JPH::EMotionType::Static;
            case RigidbodyType::Kinematic: return JPH::EMotionType::Kinematic;
            case RigidbodyType::Dynamic: return JPH::EMotionType::Dynamic;
            }
            return JPH::EMotionType::Dynamic;
        }

        JPH::Quat ToJoltQuat(const glm::quat& q)
        {
            return JPH::Quat(q.x, q.y, q.z, q.w);
        }

        glm::quat ToGlmQuat(const JPH::Quat& q)
        {
            return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ());
        }

        void DecomposeWorld(
            const glm::mat4& matrix,
            glm::vec3& position,
            glm::quat& rotation,
            glm::vec3& scale)
        {
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(matrix, scale, rotation, position, skew, perspective);
            rotation = glm::normalize(rotation);
        }

        float MaxAbsComponent(const glm::vec3& v)
        {
            const glm::vec3 a = glm::abs(v);
            return std::max(a.x, std::max(a.y, a.z));
        }
    }

    class TriggerContactListener final : public JPH::ContactListener
    {
    public:
        std::unordered_map<std::uint32_t, std::uint64_t>* EntityByBody = nullptr;
        std::vector<TriggerEvent>* Events = nullptr;

        void OnContactAdded(
            const JPH::Body& body1,
            const JPH::Body& body2,
            const JPH::ContactManifold&,
            JPH::ContactSettings&) override
        {
            if (!Events || !EntityByBody) return;
            if (!body1.IsSensor() && !body2.IsSensor()) return;

            const auto key1 = body1.GetID().GetIndexAndSequenceNumber();
            const auto key2 = body2.GetID().GetIndexAndSequenceNumber();
            const auto it1 = EntityByBody->find(key1);
            const auto it2 = EntityByBody->find(key2);
            if (it1 == EntityByBody->end() || it2 == EntityByBody->end())
                return;

            TriggerEvent event;
            event.EventType = TriggerEvent::Type::Enter;
            if (body1.IsSensor())
            {
                event.TriggerEntityID = it1->second;
                event.OtherEntityID = it2->second;
            }
            else
            {
                event.TriggerEntityID = it2->second;
                event.OtherEntityID = it1->second;
            }

            std::scoped_lock lock(m_Mutex);
            Events->push_back(event);
        }

    private:
        std::mutex m_Mutex;
    };

    struct PhysicsSystem::Implementation
    {
        BroadPhaseLayerInterface BroadPhaseInterface;
        ObjectVsBroadPhaseLayerFilter ObjectVsBroadPhaseFilter;
        ObjectLayerPairFilter ObjectPairFilter;

        std::unique_ptr<JPH::TempAllocatorImpl> TempAllocator;
        std::unique_ptr<JPH::JobSystemThreadPool> JobSystem;
        std::unique_ptr<JPH::PhysicsSystem> World;

        std::unordered_map<std::uint64_t, JPH::BodyID> Bodies;
        std::unordered_map<std::uint32_t, std::uint64_t> EntityByBody;
        TriggerContactListener ContactListener;
    };

    PhysicsSystem::PhysicsSystem()
        : m_Impl(std::make_unique<Implementation>())
    {
        JPH::RegisterDefaultAllocator();
        if (JPH::Factory::sInstance == nullptr)
        {
            JPH::Factory::sInstance = new JPH::Factory();
            JPH::RegisterTypes();
        }
    }

    PhysicsSystem::~PhysicsSystem()
    {
        Stop();
    }

    void PhysicsSystem::Start(Scene& scene)
    {
        Stop();
        m_Scene = &scene;

        m_Impl->TempAllocator =
            std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);

        const unsigned hardwareThreads =
            std::max(1u, std::thread::hardware_concurrency());
        const int workerThreads =
            static_cast<int>(hardwareThreads > 1 ? hardwareThreads - 1 : 1);

        m_Impl->JobSystem =
            std::make_unique<JPH::JobSystemThreadPool>(
                JPH::cMaxPhysicsJobs,
                JPH::cMaxPhysicsBarriers,
                workerThreads);

        m_Impl->World = std::make_unique<JPH::PhysicsSystem>();
        m_Impl->World->Init(
            2048, 0, 2048, 2048,
            m_Impl->BroadPhaseInterface,
            m_Impl->ObjectVsBroadPhaseFilter,
            m_Impl->ObjectPairFilter);

        m_Impl->World->SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));
        m_Impl->ContactListener.EntityByBody = &m_Impl->EntityByBody;
        m_Impl->ContactListener.Events = &m_TriggerEvents;
        m_Impl->World->SetContactListener(&m_Impl->ContactListener);
        m_Running = true;
        CreateBodies();
        m_Impl->World->OptimizeBroadPhase();
    }

    void PhysicsSystem::Stop()
    {
        if (!m_Impl) return;
        m_TriggerEvents.clear();
        m_Impl->EntityByBody.clear();
        m_Impl->Bodies.clear();
        m_Impl->World.reset();
        m_Impl->JobSystem.reset();
        m_Impl->TempAllocator.reset();
        m_Scene = nullptr;
        m_Running = false;
    }

    void PhysicsSystem::CreateBodies()
    {
        if (!m_Running || !m_Scene || !m_Impl->World) return;

        auto& bodyInterface = m_Impl->World->GetBodyInterface();

        for (Entity entity : m_Scene->GetEntities())
        {
            if (!entity.HasComponent<RigidbodyComponent>()) continue;

            const bool hasBox = entity.HasComponent<BoxColliderComponent>();
            const bool hasSphere = entity.HasComponent<SphereColliderComponent>();
            const bool hasCapsule = entity.HasComponent<CapsuleColliderComponent>();
            if (!hasBox && !hasSphere && !hasCapsule) continue;

            const auto& rigidbody = entity.GetComponent<RigidbodyComponent>();

            glm::vec3 position, scale;
            glm::quat rotation;
            DecomposeWorld(
                m_Scene->GetWorldTransform(entity),
                position, rotation, scale);

            JPH::ShapeRefC shape;
            bool isTrigger = false;
            float friction = 0.5f;
            float restitution = 0.0f;

            if (hasBox)
            {
                const auto& c = entity.GetComponent<BoxColliderComponent>();
                const glm::vec3 fullSize = glm::abs(c.Size * scale);
                const glm::vec3 halfExtent =
                    glm::max(fullSize * 0.5f, glm::vec3(0.01f));
                JPH::BoxShapeSettings s(
                    JPH::Vec3(halfExtent.x, halfExtent.y, halfExtent.z));
                auto result = s.Create();
                if (result.HasError()) continue;
                shape = result.Get();
                isTrigger = c.IsTrigger;
                friction = c.Material.Friction;
                restitution = c.Material.Bounciness;
            }
            else if (hasSphere)
            {
                const auto& c = entity.GetComponent<SphereColliderComponent>();
                const float radius =
                    std::max(0.01f, c.Radius * MaxAbsComponent(scale));
                JPH::SphereShapeSettings s(radius);
                auto result = s.Create();
                if (result.HasError()) continue;
                shape = result.Get();
                isTrigger = c.IsTrigger;
                friction = c.Material.Friction;
                restitution = c.Material.Bounciness;
            }
            else
            {
                const auto& c = entity.GetComponent<CapsuleColliderComponent>();
                const float radiusScale =
                    std::max(std::abs(scale.x), std::abs(scale.z));
                const float radius =
                    std::max(0.01f, c.Radius * radiusScale);
                const float totalHeight =
                    std::max(radius * 2.0f, c.Height * std::abs(scale.y));
                const float halfCylinder =
                    std::max(0.0f, totalHeight * 0.5f - radius);

                JPH::CapsuleShapeSettings s(halfCylinder, radius);
                auto result = s.Create();
                if (result.HasError()) continue;
                shape = result.Get();
                isTrigger = c.IsTrigger;
                friction = c.Material.Friction;
                restitution = c.Material.Bounciness;
            }

            const JPH::EMotionType motionType = ToJoltMotion(rigidbody.Type);
            const JPH::ObjectLayer layer =
                rigidbody.Type == RigidbodyType::Static
                    ? Layers::NonMoving : Layers::Moving;

            JPH::BodyCreationSettings settings(
                shape,
                JPH::RVec3(position.x, position.y, position.z),
                ToJoltQuat(rotation),
                motionType,
                layer);

            settings.mGravityFactor = rigidbody.UseGravity ? 1.0f : 0.0f;
            settings.mFriction = std::clamp(friction, 0.0f, 1.0f);
            settings.mRestitution = std::clamp(restitution, 0.0f, 1.0f);
            settings.mIsSensor = isTrigger;

            if (rigidbody.Type == RigidbodyType::Dynamic)
            {
                settings.mOverrideMassProperties =
                    JPH::EOverrideMassProperties::CalculateInertia;
                settings.mMassPropertiesOverride.mMass =
                    std::max(0.001f, rigidbody.Mass);
            }

            const JPH::EActivation activation =
                rigidbody.Type == RigidbodyType::Static
                    ? JPH::EActivation::DontActivate
                    : JPH::EActivation::Activate;

            const JPH::BodyID bodyID =
                bodyInterface.CreateAndAddBody(settings, activation);

            if (!bodyID.IsInvalid())
            {
                const std::uint64_t id =
                    entity.GetComponent<IDComponent>().ID;
                m_Impl->Bodies.emplace(id, bodyID);
                m_Impl->EntityByBody.emplace(bodyID.GetIndexAndSequenceNumber(), id);
            }
        }
    }

    void PhysicsSystem::Update(float deltaTime)
    {
        if (!m_Running || !m_Scene || !m_Impl->World || deltaTime <= 0.0f)
            return;

        m_TriggerEvents.clear();

        const float step = std::min(deltaTime, 1.0f / 15.0f);
        m_Impl->World->Update(
            step, 1,
            m_Impl->TempAllocator.get(),
            m_Impl->JobSystem.get());

        SyncTransformsFromPhysics();
    }

    void PhysicsSystem::SyncTransformsFromPhysics()
    {
        auto& bodyInterface = m_Impl->World->GetBodyInterface();

        for (Entity entity : m_Scene->GetEntities())
        {
            if (!entity.HasComponent<RigidbodyComponent>()) continue;

            const std::uint64_t id =
                entity.GetComponent<IDComponent>().ID;
            const auto bodyIt = m_Impl->Bodies.find(id);
            if (bodyIt == m_Impl->Bodies.end()) continue;

            const auto& rigidbody = entity.GetComponent<RigidbodyComponent>();
            if (rigidbody.Type == RigidbodyType::Static) continue;

            JPH::RVec3 position;
            JPH::Quat rotation;
            bodyInterface.GetPositionAndRotation(
                bodyIt->second, position, rotation);

            glm::vec3 oldPosition, worldScale;
            glm::quat oldRotation;
            DecomposeWorld(
                m_Scene->GetWorldTransform(entity),
                oldPosition, oldRotation, worldScale);

            const glm::vec3 newPosition(
                static_cast<float>(position.GetX()),
                static_cast<float>(position.GetY()),
                static_cast<float>(position.GetZ()));

            const glm::mat4 world =
                glm::translate(glm::mat4(1.0f), newPosition) *
                glm::mat4_cast(ToGlmQuat(rotation)) *
                glm::scale(glm::mat4(1.0f), worldScale);

            m_Scene->SetWorldTransform(entity, world);
        }
    }

    bool PhysicsSystem::AddForce(
        std::uint64_t entityID,
        const glm::vec3& force)
    {
        if (!m_Running || !m_Impl->World) return false;
        const auto it = m_Impl->Bodies.find(entityID);
        if (it == m_Impl->Bodies.end()) return false;

        m_Impl->World->GetBodyInterface().AddForce(
            it->second, JPH::Vec3(force.x, force.y, force.z));
        return true;
    }

    bool PhysicsSystem::AddImpulse(
        std::uint64_t entityID,
        const glm::vec3& impulse)
    {
        if (!m_Running || !m_Impl->World) return false;
        const auto it = m_Impl->Bodies.find(entityID);
        if (it == m_Impl->Bodies.end()) return false;

        m_Impl->World->GetBodyInterface().AddImpulse(
            it->second, JPH::Vec3(impulse.x, impulse.y, impulse.z));
        return true;
    }

    bool PhysicsSystem::SetLinearVelocity(
        std::uint64_t entityID,
        const glm::vec3& velocity)
    {
        if (!m_Running || !m_Impl->World) return false;
        const auto it = m_Impl->Bodies.find(entityID);
        if (it == m_Impl->Bodies.end()) return false;

        m_Impl->World->GetBodyInterface().SetLinearVelocity(
            it->second, JPH::Vec3(velocity.x, velocity.y, velocity.z));
        return true;
    }

    glm::vec3 PhysicsSystem::GetLinearVelocity(std::uint64_t entityID) const
    {
        if (!m_Running || !m_Impl->World) return {};
        const auto it = m_Impl->Bodies.find(entityID);
        if (it == m_Impl->Bodies.end()) return {};

        const JPH::Vec3 v =
            m_Impl->World->GetBodyInterface().GetLinearVelocity(it->second);
        return { v.GetX(), v.GetY(), v.GetZ() };
    }

    std::optional<RaycastHit> PhysicsSystem::Raycast(
        const glm::vec3& origin,
        const glm::vec3& direction,
        float maxDistance) const
    {
        if (!m_Running || !m_Impl->World || maxDistance <= 0.0f)
            return std::nullopt;

        const float length = glm::length(direction);
        if (length <= 0.00001f) return std::nullopt;

        const glm::vec3 dir = direction / length;
        const JPH::RRayCast ray(
            JPH::RVec3(origin.x, origin.y, origin.z),
            JPH::Vec3(
                dir.x * maxDistance,
                dir.y * maxDistance,
                dir.z * maxDistance));

        JPH::RayCastResult result;
        if (!m_Impl->World->GetNarrowPhaseQuery().CastRay(ray, result))
            return std::nullopt;

        const auto idIt = m_Impl->EntityByBody.find(result.mBodyID.GetIndexAndSequenceNumber());
        if (idIt == m_Impl->EntityByBody.end())
            return std::nullopt;

        RaycastHit hit;
        hit.EntityID = idIt->second;
        hit.Distance = result.mFraction * maxDistance;
        hit.Point = origin + dir * hit.Distance;
        return hit;
    }
    bool PhysicsSystem::HasObstacleBetween(
        const glm::vec3& origin,
        const glm::vec3& destination,
        std::uint64_t observerID,
        std::uint64_t targetID) const
    {
        if (!m_Running || !m_Impl->World)
            return false;

        const glm::vec3 delta = destination - origin;

        if (glm::dot(delta, delta) <= 0.00000001f)
            return false;

        JPH::BodyID observerBody;
        JPH::BodyID targetBody;

        const auto observerIt = m_Impl->Bodies.find(observerID);

        if (observerIt != m_Impl->Bodies.end())
            observerBody = observerIt->second;

        const auto targetIt = m_Impl->Bodies.find(targetID);

        if (targetIt != m_Impl->Bodies.end())
            targetBody = targetIt->second;

        const LineOfSightBodyFilter bodyFilter(
            observerBody,
            targetBody
        );

        const JPH::RRayCast ray(
            JPH::RVec3(origin.x, origin.y, origin.z),
            JPH::Vec3(delta.x, delta.y, delta.z)
        );

        JPH::RayCastResult hit;
        const bool blocked =
            m_Impl->World->GetNarrowPhaseQuery().CastRay(
                ray,
                hit,
                JPH::BroadPhaseLayerFilter(),
                JPH::ObjectLayerFilter(),
                bodyFilter
            );

        static bool previousBlocked = false;
        static bool firstCheck = true;

        if (firstCheck || blocked != previousBlocked)
        {
            std::cout
                << "[AI LOS] "
                << "Blocked: " << (blocked ? "YES" : "NO")
                << " | Origin: "
                << origin.x << ", "
                << origin.y << ", "
                << origin.z
                << " | Destination: "
                << destination.x << ", "
                << destination.y << ", "
                << destination.z
                << '\n';

            previousBlocked = blocked;
            firstCheck = false;
        }

        return blocked;
    }
}
