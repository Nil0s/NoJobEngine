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
#include <Jolt/Physics/PhysicsSystem.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <thread>

namespace NoJob
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
            static constexpr unsigned Count = 2;
        }

        class ObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter
        {
        public:
            bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override
            {
                if (a == Layers::NonMoving)
                    return b == Layers::Moving;
                if (a == Layers::Moving)
                    return true;
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

        class ObjectVsBroadPhaseLayerFilter final
            : public JPH::ObjectVsBroadPhaseLayerFilter
        {
        public:
            bool ShouldCollide(
                JPH::ObjectLayer layer,
                JPH::BroadPhaseLayer broadPhaseLayer) const override
            {
                if (layer == Layers::NonMoving)
                    return broadPhaseLayer == BroadPhaseLayers::Moving;
                if (layer == Layers::Moving)
                    return true;
                return false;
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
    }

    struct PhysicsSystem::Implementation
    {
        BroadPhaseLayerInterface BroadPhaseInterface;
        ObjectVsBroadPhaseLayerFilter ObjectVsBroadPhaseFilter;
        ObjectLayerPairFilter ObjectPairFilter;

        std::unique_ptr<JPH::TempAllocatorImpl> TempAllocator;
        std::unique_ptr<JPH::JobSystemThreadPool> JobSystem;
        std::unique_ptr<JPH::PhysicsSystem> World;

        std::unordered_map<std::uint64_t, JPH::BodyID> Bodies;
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
            1024,
            0,
            1024,
            1024,
            m_Impl->BroadPhaseInterface,
            m_Impl->ObjectVsBroadPhaseFilter,
            m_Impl->ObjectPairFilter);

        m_Impl->World->SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));
        m_Running = true;
        CreateBodies();
        m_Impl->World->OptimizeBroadPhase();
    }

    void PhysicsSystem::Stop()
    {
        if (!m_Impl)
            return;

        m_Impl->Bodies.clear();
        m_Impl->World.reset();
        m_Impl->JobSystem.reset();
        m_Impl->TempAllocator.reset();
        m_Scene = nullptr;
        m_Running = false;
    }

    void PhysicsSystem::CreateBodies()
    {
        if (!m_Running || !m_Scene || !m_Impl->World)
            return;

        auto& bodyInterface = m_Impl->World->GetBodyInterface();

        for (Entity entity : m_Scene->GetEntities())
        {
            if (!entity.HasComponent<RigidbodyComponent>() ||
                !entity.HasComponent<BoxColliderComponent>())
                continue;

            const auto& rigidbody =
                entity.GetComponent<RigidbodyComponent>();
            const auto& collider =
                entity.GetComponent<BoxColliderComponent>();

            glm::vec3 position;
            glm::quat rotation;
            glm::vec3 scale;
            DecomposeWorld(
                m_Scene->GetWorldTransform(entity),
                position,
                rotation,
                scale);

            const glm::vec3 fullSize =
                glm::abs(collider.Size * scale);
            const glm::vec3 halfExtent =
                glm::max(fullSize * 0.5f, glm::vec3(0.01f));

            JPH::BoxShapeSettings shapeSettings(
                JPH::Vec3(halfExtent.x, halfExtent.y, halfExtent.z));
            const auto shapeResult = shapeSettings.Create();
            if (shapeResult.HasError())
                continue;

            const JPH::EMotionType motionType =
                ToJoltMotion(rigidbody.Type);
            const JPH::ObjectLayer layer =
                rigidbody.Type == RigidbodyType::Static
                    ? Layers::NonMoving
                    : Layers::Moving;

            JPH::BodyCreationSettings settings(
                shapeResult.Get(),
                JPH::RVec3(position.x, position.y, position.z),
                ToJoltQuat(rotation),
                motionType,
                layer);

            settings.mGravityFactor =
                rigidbody.UseGravity ? 1.0f : 0.0f;

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
                m_Impl->Bodies.emplace(
                    entity.GetComponent<IDComponent>().ID,
                    bodyID);
        }
    }

    void PhysicsSystem::Update(float deltaTime)
    {
        if (!m_Running || !m_Scene || !m_Impl->World || deltaTime <= 0.0f)
            return;

        // Avoid a huge physics jump after debugging / dragging the window.
        const float step = std::min(deltaTime, 1.0f / 15.0f);

        m_Impl->World->Update(
            step,
            1,
            m_Impl->TempAllocator.get(),
            m_Impl->JobSystem.get());

        SyncTransformsFromPhysics();
    }

    void PhysicsSystem::SyncTransformsFromPhysics()
    {
        auto& bodyInterface = m_Impl->World->GetBodyInterface();

        for (Entity entity : m_Scene->GetEntities())
        {
            const auto bodyIt = m_Impl->Bodies.find(
                entity.GetComponent<IDComponent>().ID);
            if (bodyIt == m_Impl->Bodies.end() ||
                !entity.HasComponent<RigidbodyComponent>())
                continue;

            const auto& rigidbody =
                entity.GetComponent<RigidbodyComponent>();

            if (rigidbody.Type == RigidbodyType::Static)
                continue;

            JPH::RVec3 position;
            JPH::Quat rotation;
            bodyInterface.GetPositionAndRotation(
                bodyIt->second,
                position,
                rotation);

            glm::vec3 oldPosition;
            glm::quat oldRotation;
            glm::vec3 worldScale;
            DecomposeWorld(
                m_Scene->GetWorldTransform(entity),
                oldPosition,
                oldRotation,
                worldScale);

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
}
