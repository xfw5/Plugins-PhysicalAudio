// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "Kismet/KismetSystemLibrary.h"
#include "CollisionAudioComponent.generated.h"

class UAudioComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayCollisionSound, UCollisionAudioComponent*, CollisionAudioComponent, USoundBase*, Sound);

USTRUCT(BlueprintType)
struct FCollisionAudioImpactData : public FTableRowBase
{
	GENERATED_USTRUCT_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AudioImpactData")
	float RetriggerCooldown;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AudioImpactData")
	float ImpactMagnitudeThresholdMin;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AudioImpactData")
	float ImpactMagnitudeThresholdMax;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AudioImpactData")
	USoundBase* SoundDefault;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AudioImpactData")
	USoundBase* SoundHeavy;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AudioImpactData")
	float PitchModulationMin;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AudioImpactData")
	float PitchModulationMax;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AudioImpactData")
	float VolumeModulationMin;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AudioImpactData")
	float VolumeModulationMax;

	FCollisionAudioImpactData()
		: RetriggerCooldown()
		, ImpactMagnitudeThresholdMin()
		, ImpactMagnitudeThresholdMax()
		, SoundDefault()
		, SoundHeavy()
		, PitchModulationMin()
		, PitchModulationMax()
		, VolumeModulationMin()
		, VolumeModulationMax()
	{
	}
};

/*
* Audio base on physical collision. 
*/
UCLASS( ClassGroup=(PhysicalAudio), meta=(BlueprintSpawnableComponent) )
class PHYSICALAUDIO_API UCollisionAudioComponent : public UActorComponent
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Collision Audio")
	uint32 bDisableDeltaThreshold : 1;

	/* Collision impact table. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Collision Audio")
	UDataTable* DataTableAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Collision Audio")
	FName ImpactNameRef;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, category = "Collision Audio")
	FName CollisionComponentTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Collision Audio")
	float TriggerLocationDeltaThreshold;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Collision Audio")
	float TriggerRotationDeltaThreshold;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, category = "Collision Audio")
	uint32 bIsHeavyHit : 1;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, category = "Collision Audio")
	uint32 bFirstHit : 1;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, category = "Collision Audio")
	FTransform LastTriggerTransform;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, category = "Collision Audio")
	float LastTriggerGameTime;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, category = "Collision Audio")
	float LastInvalidHitGameTime;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, category = "Collision Audio")
	uint32 bCanPlay : 1;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, category = "Collision Audio")
	uint32 bCanEverPlay : 1;

	UPROPERTY(Transient)
	float ImpulseMagnitude;

	UPROPERTY(Transient)
	FCollisionAudioImpactData ImpactAudioData;

#if WITH_EDITORONLY_DATA
	/* Edit Only: Display collision impact msg. */
	UPROPERTY(EditAnywhere, category = "Collision Audio")
	uint32 bDebugImpactMsg : 1;
#endif

public:	
	// Sets default values for this component's properties
	UCollisionAudioComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, category = "Components|CollisionAudio")
	void Initialize();

	UFUNCTION(BlueprintCallable, category = "Components|CollisionAudio")
	void BindCollisionEvent();

	UFUNCTION(BlueprintCallable, category = "Components|CollisionAudio")
	void OnImpactHandle(FVector NormalImpulse, const FHitResult& Hit);
	
	UFUNCTION(BlueprintCallable, category = "Components|CollisionAudio")
	void OnActorHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit);
	
	UFUNCTION(BlueprintCallable, category = "Components|CollisionAudio")
	void OnTaggedComponentHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	bool DetectValidHit(FVector Impulse);
	void PlayImpactSound(FVector Location);

	FORCEINLINE bool IsRetriggerCooldown() { return UKismetSystemLibrary::GetGameTimeInSeconds(this) - LastTriggerGameTime >= ImpactAudioData.RetriggerCooldown; }
	FORCEINLINE bool IsImpulseAllow(float QueryImpulse) { return QueryImpulse > ImpactAudioData.ImpactMagnitudeThresholdMin; }
	FORCEINLINE void UpdateLastTriggerStatus(FTransform InLastTransform) { LastTriggerTransform = InLastTransform; LastTriggerGameTime = UKismetSystemLibrary::GetGameTimeInSeconds(this); }
	bool IsTriggerDeltaThreshold();

public:	

	UFUNCTION(BlueprintCallable, category = "Components|CollisionAudio")
	void SetImpactNameRef(FName InNameRef);

	UFUNCTION(BlueprintCallable, category = "Components|CollisionAudio")
	void SetCanPlay(bool CanPlay);

	UFUNCTION(BlueprintCallable, category = "Components|CollisionAudio")
	void SetCanEverPlay(bool CanEverPlay);

	UPROPERTY(BlueprintAssignable, Category = "Components|CollisionAudio")
	FOnPlayCollisionSound OnPlayCollisionSound;
};
