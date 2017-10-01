#include "pch.h"

void* operator new(size_t size, POOL_TYPE type, ULONG tag) {
    return ExAllocatePoolWithTag(type, size, tag);
}
void operator delete(void* p, size_t) {
    ExFreePool(p);
}
