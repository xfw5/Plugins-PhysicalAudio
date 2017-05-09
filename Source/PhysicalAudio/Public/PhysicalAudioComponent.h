// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "Components/SceneComponent.h"
#include "PhysicalAudioComponent.generated.h"

class UAudioComponent;
class UDataTable;
class UPhysicalAudioComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLoopSoundTriggered, UAudioComponent*, Sound);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLoopSoundModulated, UAudioComponent*, Sound, float, Intensity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMediumSoundTriggered, UAudioComponent*, Sound, float, Intensity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHeavySoundTriggered, UAudioComponent*, Sound);

UENUM()
enum class ETrackedBoneEvent : uint8
{
	None,
	SlowThresholdStart,
	SlowThresholdStop,
	MediumThreshold,
	FastThreshold
};

UENUM(BlueprintType)
enum class ETrackedBoneSpace : uint8
{
	Relative,
	World
};

UENUM(BlueprintType)
enum class ETrackedBoneVelocityType : uint8
{
	Linear,
	Rotational,
	Custom
};

USTRUCT(BlueprintType)
struct FTrackedBone
{
	GENERATED_USTRUCT_BODY()

	FTrackedBone();

#if WITH_EDITORONLY_DATA
	ETrackedBoneEvent Update(UPhysicalAudioComponent* Owner, USkeletalMeshComponent* Mesh, float DeltaTime, float TimeDilation, bool bIgnoreDilation, float InterpSpeed, float VolumeMultiplier, bool DebugOn);
#else
	ETrackedBoneEvent Update(USkeletalMeshComponent* Mesh, float DeltaTime, float TimeDilation, bool bIgnoreDilation, float InterpSpeed, float VolumeMultiplier);
#endif

	float GetRangeMappedDelta(float Left, float Right);
	void GetCurrentDeltaFromMesh(USkeletalMeshComponent* Mesh);
	void GetCurrentDeltaFromTransform(FTransform const& Transform);
	void ResetLoop();

	UPhysicalAudioComponent* Controller;

	// Name of the Bone to track
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName BoneName;

	// Sound cues to be triggered when velocity
	// deltas are discovered
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USoundBase* SoundCueLoop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USoundBase* SoundCueMedium;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USoundBase* SoundCueHigh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ThresholdLoop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ThresholdMedium;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ThresholdHigh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float VolumeInterpolatedSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RetriggerDelay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETrackedBoneSpace TrackingSpace;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETrackedBoneVelocityType VelocityTrackingType;

	UPROPERTY()
	UAudioComponent* LoopInstance;

	UPROPERTY(BlueprintReadOnly)
	float Delta;

private:

	ETrackedBoneEvent SendEvent(ETrackedBoneEvent Event);

	float TimeSinceLastTrigger;
	bool bTriggeredLoopLayer;
	float InterpolatedVolume;
	bool bDirectionChangedSinceLastTrigger;

	// Data required for rotational velocity tracking
	FQuat TorqueCurrent;
	FQuat TorqueFinal;
	FQuat OldRotation;
	FQuat NewRotation;
	FQuat PreviousTriggerQuat;

	// Data required for linear velocity tracking
	FVector ForceCurrent;
	FVector ForceFinal;
	FVector OldPosition;
	FVector NewPosition;
	FVector PreviousTriggerVector;
};

USTRUCT(BlueprintType)
struct FPhysicalAudioData : public FTableRowBase
{
	GENERATED_USTRUCT_BODY()

	FPhysicalAudioData();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FTrackedBone> TrackedBones;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCustomTrackedTick, const FTrackedBone&, TrackedBone);

/*
* Audio base on physical bone Velocity(Linear and Rotational) delta.
* HACK: For static mesh use custom mode and update tracking transform in tick function. 
*/
UCLASS(ClassGroup = (PhysicalAudio), meta = (BlueprintSpawnableComponent))
class PHYSICALAUDIO_API UPhysicalAudioComponent : public USceneComponent
{
	GENERATED_BODY()

protected:
	/* Data table row's name configured in @DataTableAsset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName DataNameRef;

	/* Physical audio data table. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UDataTable* DataTableAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint32 bShouldIgnoreDilation : 1;

	/* How fast tracked velocity changed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float InterpSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint32 bShouldAttachOneShots : 1;

	/* Indicate whether physical audio is simulate in skeletal mesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	uint32 bIsSkeletalMesh : 1;

	UPROPERTY(BlueprintReadOnly)
	TArray<FTrackedBone> TrackedBones;

	UPROPERTY(BlueprintReadOnly)
	class UMeshComponent* Mesh;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, category = "Level")
	uint32 bEnableDebug : 1;
#endif

public:
	// Sets default values for this component's properties
	UPhysicalAudioComponent();

	// Called when the game starts
	virtual void BeginPlay() override;

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Components|PhysicalAudio")
	void SetCanPlay(bool CanPlay);

	UFUNCTION(BlueprintCallable, Category = "Components|PhysicalAudio")
	void SetVolumeMultiplier(float Multiplier);

	UFUNCTION(BlueprintCallable, Category = "Components|PhysicalAudio")
	void SetCustomTrackedTransform(int32 Index, FTransform const& Transform);

	UPROPERTY(BlueprintAssignable, Category = "Physics Audio")
	FOnCustomTrackedTick OnCustomTrackedTick;

	UPROPERTY(BlueprintAssignable, Category = "Physics Audio")
	FOnLoopSoundTriggered OnLoopSoundTriggered;

	UPROPERTY(BlueprintAssignable, Category = "Physics Audio")
	FOnLoopSoundModulated OnLoopSoundModulated;

	UPROPERTY(BlueprintAssignable, Category = "Physics Audio")
	FOnMediumSoundTriggered OnMediumSoundTriggered;

	UPROPERTY(BlueprintAssignable, Category = "Physics Audio")
	FOnHeavySoundTriggered OnHeavySoundTriggered;

private:

	UAudioComponent* PlaySoundFromBone(FTrackedBone const& VTS, USoundBase* Sound, float Volume, bool UseAttachedAudioComponent = false);

	void ResetDataFromTable();

	bool bCanPlay;

	float VolumeMultiplier;
};

