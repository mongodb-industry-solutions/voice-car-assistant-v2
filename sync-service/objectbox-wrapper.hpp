#include "objectbox.hpp"
#include "objectbox-sync.hpp"

// Provide definitions for functions that should be inline but aren't in headers
namespace obx {
namespace internal {

inline void checkErrOrThrow(obx_err err) {
    if (err != OBX_SUCCESS) throwLastError(err);
}

inline void checkPtrOrThrow(const void* ptr, const char* contextPrefix) {
    if (ptr == nullptr) throwLastError(obx_last_error_code(), contextPrefix);
}

inline void checkIdOrThrow(uint64_t id, const char* contextPrefix) {
    if (id == 0) throwLastError(obx_last_error_code(), contextPrefix);
}

inline bool checkSuccessOrThrow(obx_err err) {
    if (err == OBX_NO_SUCCESS) return false;
    checkErrOrThrow(err);
    return true;
}

inline void throwIllegalStateException(const char* msg, const char* contextPrefix) {
    std::string fullMsg = msg ? msg : "Illegal state";
    if (contextPrefix) fullMsg += std::string(": ") + contextPrefix;
    throw std::runtime_error(fullMsg);
}

inline void throwIllegalArgumentException(const char* msg, const char* contextPrefix) {
    std::string fullMsg = msg ? msg : "Illegal argument";
    if (contextPrefix) fullMsg += std::string(": ") + contextPrefix;
    throw std::invalid_argument(fullMsg);
}

} // namespace internal

// Store::cPtr() implementation
inline OBX_store* Store::cPtr() const {
    OBX_store* store = cStore_.load();
    if (store == nullptr) throw ShuttingDownException("Store is already closed");
    return store;
}

} // namespace obx
