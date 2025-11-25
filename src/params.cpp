#include "params.h"

#include <string.h>

namespace oi {

ParameterManager& ParameterManager::instance() {
    static ParameterManager instance;
    return instance;
}

ParameterManager::ParameterManager() = default;

bool ParameterManager::registerParameter(ParameterBase* parameter) {
    if (parameter == nullptr) {
        return false;
    }
    if (count_ >= MAX_PARAMS) {
        parameter->setFlag(ParamFlag::Error);
        return false;
    }

    for (size_t i = 0; i < count_; ++i) {
        const bool duplicateId = registry_[i]->getID() == parameter->getID();
        const bool duplicateName = (registry_[i]->getName() != nullptr && parameter->getName() != nullptr &&
                                    strcmp(registry_[i]->getName(), parameter->getName()) == 0);
        if (duplicateId || duplicateName) {
            registry_[i]->setFlag(ParamFlag::Error);
            parameter->setFlag(ParamFlag::Error);
        }
    }

    registry_[count_++] = parameter;
    return true;
}

ParameterBase* ParameterManager::getByID(uint16_t id) const {
    for (size_t i = 0; i < count_; ++i) {
        if (registry_[i]->getID() == id) {
            return registry_[i];
        }
    }
    return nullptr;
}

ParameterBase* ParameterManager::getByName(const char* name) const {
    if (name == nullptr) {
        return nullptr;
    }
    for (size_t i = 0; i < count_; ++i) {
        const char* candidate = registry_[i]->getName();
        if (candidate != nullptr && strcmp(candidate, name) == 0) {
            return registry_[i];
        }
    }
    return nullptr;
}

void ParameterManager::checkTimeouts(uint32_t nowMs) {
    for (size_t i = 0; i < count_; ++i) {
        registry_[i]->checkTimeout(nowMs);
    }
}

} // namespace oi

