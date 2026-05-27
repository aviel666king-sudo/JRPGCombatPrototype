#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CombatTypes.h"   // FCombatStats
#include "CharacterArmorDataAsset.generated.h"

class UTexture2D;
class USkeletalMesh;

/**
 * UCharacterArmorDataAsset
 *
 * One armor slot per character. Per the pitch:
 *   "single piece defines visual. Pure programmable shell — properties come
 *    from override-protocol chips that can be extracted and reassigned."
 *
 * So the armor itself is mostly cosmetic + a small stat baseline. Its real
 * power is what chips it currently holds via "override protocols" (future
 * pass — for now we just track the base stat delta and the visual mesh).
 */
UCLASS(BlueprintType)
class JRPGCOMBAT_API UCharacterArmorDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Armor|Identity")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Armor|Identity",
              meta = (MultiLine = "true"))
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Armor|Identity")
    TObjectPtr<UTexture2D> Icon;

    /** Body visual replacement. Optional — character keeps its default mesh
     *  if this is null. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Armor|Identity")
    TObjectPtr<USkeletalMesh> VisualMesh;

    /** Base stat delta. Override-protocol chips will stack on top of this. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Armor|Stats")
    FCombatStats StatDelta;
};
