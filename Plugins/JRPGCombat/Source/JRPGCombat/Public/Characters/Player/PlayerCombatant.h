#pragma once

#include "CoreMinimal.h"
#include "Characters/Base/CombatantBase.h"
#include "PlayerCombatant.generated.h"

/**
 * APlayerCombatant
 *
 * Structural base for all player-controlled characters.
 * Sets Team = Player and provides a future extension point for
 * player-specific mechanics (parry windows, UI hooks, input handling).
 *
 * Contains no character-specific logic. Concrete characters
 * (e.g. ACombatantFencer) inherit from this, not from ACombatantBase.
 *
 *   ACombatantBase
 *     → APlayerCombatant
 *         → ACombatantFencer
 *         → (future characters)
 */
UCLASS(BlueprintType, Blueprintable)
class JRPGCOMBAT_API APlayerCombatant : public ACombatantBase
{
    GENERATED_BODY()

public:

    APlayerCombatant()
    {
        Team = ECombatTeam::Player;
    }
};
