#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CraftingMaterialDataAsset.generated.h"

class UTexture2D;

/**
 * UCraftingMaterialDataAsset
 *
 * One kind of upgrade material (e.g. "Metal Scraps"). Enemies drop quantities
 * of these; the camp upgrade screen spends them (alongside gold) to raise a
 * weapon's tier or a chip's level.
 *
 * Authored as a Data Asset in the Content Browser. For first-pass testing a
 * single shared material (Metal Scraps) is enough; more can be added later and
 * referenced by upgrade-cost tables without code changes.
 */
UCLASS(BlueprintType)
class JRPGCOMBAT_API UCraftingMaterialDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material",
              meta = (MultiLine = "true"))
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material")
    TObjectPtr<UTexture2D> Icon;
};
