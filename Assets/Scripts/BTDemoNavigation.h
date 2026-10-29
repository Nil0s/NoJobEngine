/** @file BTDemoNavigation.h @brief Self-contained navigation for the Behaviour Tree example. */
#pragma once
#include "Engine/Scene/ScriptableEntity.h"
#include "Engine/AI/Navigation/NavigationSystem.h"
#include <memory>
namespace NoJob {
/** @brief Bake the example floor on Play, including in exported games.
 * @details Reuses editor navigation when available; otherwise owns navigation until Stop.
 * The original testplane_100x100.obj supplies the bake geometry.
 */
class BTDemoNavigation final : public Script {
public:
    /** @brief Bake the scene geometry before the first navigation update. */
    void OnCreate() override;
    /** @brief Unlink owned navigation before releasing its storage. */
    void OnDestroy() override;
private:
    std::unique_ptr<NavigationSystem> navigation_; ///< Used only when the host provides no navigation.
};
}
