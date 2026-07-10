#pragma once
#include <cstdint>
#include <functional>

// UUID (Universally Unique Identifier) class generates a unique 64-bit identifier for entities and components in the ECS system. 
// It can be constructed randomly or from an existing ID, and it can be used as a key in unordered maps.
// This class is for managing unique identifiers in the engine, ensuring that each entity or component can be uniquely identified across the system.
// This allows for consistent serialization, deserialization, and management of entities in the ECS framework.

namespace Engine
{
    class UUID
    {
    public:
        // Generates a mathematically random, unique 64-bit ID
        UUID();

        // Constructs a UUID from an existing 64-bit ID (used when loading a saved scene)
        UUID(uint64_t uuid);

		// Default copy constructor
        UUID(const UUID&) = default;

        // Implicitly converts the UUID to a standard uint64_t when needed
        operator uint64_t() const { return m_UUID; }

    private:
        uint64_t m_UUID;
    };
}

// Inject a hashing function into the standard library so std::unordered_map can use UUID as a key
namespace std
{
    template<>
    struct hash<Engine::UUID>
    {
        std::size_t operator()(const Engine::UUID& uuid) const
        {
            return hash<uint64_t>()((uint64_t)uuid);
        }
    };
}