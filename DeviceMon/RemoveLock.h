#pragma once

struct AutoRemoveLock {
public:
    AutoRemoveLock(IO_REMOVE_LOCK* lock, ULONG tag = DRIVER_TAG) : _lock(lock), _tag(tag) {
        _acquired = IoAcquireRemoveLock(lock, (PVOID)tag) == STATUS_SUCCESS;
    }

    operator bool() const {
        return _acquired;
    }

    IO_REMOVE_LOCK* Detach() {
        _acquired = false;
        return _lock;
    }

    ~AutoRemoveLock() {
        if (_acquired) {
            IoReleaseRemoveLock(_lock, (PVOID)_tag);
        }
    }

private:
    ULONG _tag;
    IO_REMOVE_LOCK* _lock;
    bool _acquired;
};
