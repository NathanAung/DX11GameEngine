#include "Engine/PhysicsManager.h"
#include <thread>

using namespace JPH;

namespace Engine {

bool PhysicsManager::Initialize() {
    // Register allocator and factory/types
    RegisterDefaultAllocator();
    Factory::sInstance = new Factory();
    RegisterTypes();

    // Temp allocator ~10 MB
    const uint32_t tempSize = 10 * 1024 * 1024;
    m_tempAllocator = new TempAllocatorImpl(tempSize);

    // Job system: use hardware_concurrency - 1 worker threads (minimum 1)
    const uint32_t workerThreads = std::max(1u, std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() - 1 : 1u);
    m_jobSystem = new JobSystemThreadPool(cMaxPhysicsJobs, cMaxPhysicsBarriers, workerThreads);

    // Layer helpers
    m_bpLayerInterface   = new BPLayerInterfaceImpl();
    m_objVsBpLayerFilter = new ObjectVsBroadPhaseLayerFilterImpl();
    m_objLayerPairFilter = new ObjectLayerPairFilterImpl();

    // Physics system and settings
    m_physicsSystem = new PhysicsSystem();

    // World limits / capacity (tune later as needed)
    const uint32_t maxBodies         = 1024;
    const uint32_t numBodyMutexes    = 1024;
    const uint32_t maxBodyPairs      = 1024;
    const uint32_t maxContactConstraints = 1024;

    // No activation listener for now
    BodyActivationListener* activationListener = nullptr;

    // Settings (default is fine initially)
    PhysicsSettings settings;

    m_physicsSystem->Init(
        maxBodies,
        numBodyMutexes,
        maxBodyPairs,
        maxContactConstraints,
        *m_bpLayerInterface,
        *m_objVsBpLayerFilter,
        *m_objLayerPairFilter
    );

    // Optionally set gravity (default is (0, -9.81f, 0))
    m_physicsSystem->SetGravity(Vec3(0.0f, -9.81f, 0.0f));

    // Activation listener can be set if needed later
    m_physicsSystem->SetBodyActivationListener(activationListener);

    return true;
}

void PhysicsManager::Shutdown() {
    // Destroy physics system and helpers in reverse order
    if (m_physicsSystem)      { delete m_physicsSystem;      m_physicsSystem = nullptr; }
    if (m_objLayerPairFilter) { delete m_objLayerPairFilter; m_objLayerPairFilter = nullptr; }
    if (m_objVsBpLayerFilter) { delete m_objVsBpLayerFilter; m_objVsBpLayerFilter = nullptr; }
    if (m_bpLayerInterface)   { delete m_bpLayerInterface;   m_bpLayerInterface = nullptr; }
    if (m_jobSystem)          { delete m_jobSystem;          m_jobSystem = nullptr; }
    if (m_tempAllocator)      { delete m_tempAllocator;      m_tempAllocator = nullptr; }

    // Unregister factory/types
    if (Factory::sInstance) {
        delete Factory::sInstance;
        Factory::sInstance = nullptr;
    }
}

void PhysicsManager::Update(float deltaTime) {
    if (!m_physicsSystem) return;

    // One step per frame for now; sub-steps/accumulator later if needed
    const int collisionSteps = 1;
    m_physicsSystem->Update(deltaTime, collisionSteps, m_tempAllocator, m_jobSystem);
}

JPH::BodyInterface& PhysicsManager::GetBodyInterface() {
    return m_physicsSystem->GetBodyInterface();
}

}