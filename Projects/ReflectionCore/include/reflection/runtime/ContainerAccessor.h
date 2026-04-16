#pragma once

#include <cstddef>
#include <vector>

namespace reflection
{
    struct ContainerAccessor
    {
        std::size_t(*GetSize)(const void* container) = nullptr;
        void (*Clear)(void* container) = nullptr;
        void (*ForEach)(void* container, void (*callback)(void* element, void* key, void* user_data), void* user_data) = nullptr;

        void (*Add)(void* container, const void* value_ptr) = nullptr;
        void (*Remove)(void* container, const void* value_ptr) = nullptr;
        void* (*GetElement)(void* container, std::size_t index) = nullptr;

        std::vector<void*>(*GetElements)(void* container) = nullptr;
        void (*Resize)(void* container, std::size_t size) = nullptr;

        void (*AddPair)(void* container) = nullptr;
        void* (*GetValue)(void* container, const void* key_ptr) = nullptr;
        void* (*AddAndGetElement)(void* container, const void* key_ptr) = nullptr;
    };
}