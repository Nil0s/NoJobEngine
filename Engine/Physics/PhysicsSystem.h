#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace JPH
{
    class PhysicsSystem;
    class TempAllocatorImpl;
    class JobSystemThreadPool;
}

namespace NoJob
{
    class Scene;

    class PhysicsSystem
    {
    public:
        PhysicsSystem();
        ~PhysicsSystem();

        PhysicsSystem(const PhysicsSystem&) = delete;
        PhysicsSystem& operator=(const PhysicsSystem&) = delete;

        void Start(Scene& scene);
        void Stop();
        void Update(float deltaTime);

        bool IsRunning() const { return m_Running; }

    private:
        struct Implementation;
        std::unique_ptr<Implementation> m_Impl;
        Scene* m_Scene = nullptr;
        bool m_Running = false;

        void CreateBodies();
        void SyncTransformsFromPhysics();
    };
}
