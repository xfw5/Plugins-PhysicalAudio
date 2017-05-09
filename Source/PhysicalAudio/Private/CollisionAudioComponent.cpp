// Fill out your copyright notice in the Description page of Project Settings.

#include "PhysicalAudio.h"
#include "CollisionAudioComponent.h"
#include "Kismet/DataTableFunctionLibrary.h"
#include "Components/PrimitiveComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"


// Sets default values for this component's properties
UCollisionAudioComponent::UCollisionAudioComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	bCanPlay = false;
	bCanEverPlay = true;
	bFirstHit = true;
	bIsHeavyHit = false;
	bDisableDeltaThreshold = false;

	LastInvalidHitGameTime = 0.f;
	LastTriggerGameTime = 0.f;
	LastTriggerTransform = FTransform();

	TriggerLocationDeltaThreshold = 25.f;
	TriggerRotationDeltaThreshold = 90.f;
}


// Called when the game starts
void UCollisionAudioComponent::BeginPlay()
{
	Super::BeginPlay();

	Initialize();
}


void UCollisionAudioComponent::Initialize()
{
	if (UDataTableFunctionLibrary::Generic_GetDataTableRowFromName(DataTableAsset, ImpactNameRef, &ImpactAudioData))
	{
		BindCollisionEvent();
	}
}

void UCollisionAudioComponent::BindCollisionEvent()
{
	AActor* Owner = GetOwner();
	if (Owner)
	{
		TArray<UPrimitiveComponent*> CollisionComponents;
		Owner->GetComponents(CollisionComponents, false);

		bool bHasCollisionComponent = false;
		for (UPrimitiveComponent* PrimitiveComponent : CollisionComponents)
		{
			if (PrimitiveComponent->ComponentHasTag(CollisionComponentTag))
			{
				bHasCollisionComponent = true;
				PrimitiveComponent->OnComponentHit.AddUniqueDynamic(this, &UCollisionAudioComponent::OnTaggedComponentHit);
			}
		}

		if (!bHasCollisionComponent)
		{
			Owner->OnActorHit.AddUniqueDynamic(this, &UCollisionAudioComponent::OnActorHit);
		}
	}
}

void UCollisionAudioComponent::OnImpactHandle(FVector NormalImpulse, const FHitResult& Hit)
{
	if (DetectValidHit(NormalImpulse))
	{
		ImpulseMagnitude = UKismetMathLibrary::MapRangeClamped(NormalImpulse.Size(), ImpactAudioData.ImpactMagnitudeThresholdMin, ImpactAudioData.ImpactMagnitudeThresholdMax, 0.f, 1.f);
		PlayImpactSound(Hit.Location);

		UpdateLastTriggerStatus(GetOwner()->GetTransform());

		bFirstHit = false;

#if WITH_EDITORONLY_DATA
		if (bDebugImpactMsg)
		{
			FString Msg = FString::Printf(TEXT("Collision Impulse Magnitude : %s(%d)"), *NormalImpulse.ToString(), NormalImpulse.Size());
			UKismetSystemLibrary::DrawDebugString(this, Hit.Location, Msg, nullptr, FColor::White, 3.f);
		}
#endif
	}
}

void UCollisionAudioComponent::OnActorHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit)
{
	OnImpactHandle(NormalImpulse, Hit);
}

void UCollisionAudioComponent::OnTaggedComponentHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	OnImpactHandle(NormalImpulse, Hit);
}

bool UCollisionAudioComponent::DetectValidHit(FVector Impulse)
{
	ImpulseMagnitude = Impulse.Size();

	if (bCanPlay && bCanEverPlay &&
		IsTriggerDeltaThreshold() &&
		IsRetriggerCooldown() &&
		IsImpulseAllow(ImpulseMagnitude))
	{
		return true;
	}

	LastInvalidHitGameTime = UKismetSystemLibrary::GetGameTimeInSeconds(this);
	return false;
}

void UCollisionAudioComponent::PlayImpactSound(FVector Location)
{
	USoundBase* Sound;
	float Volume = 1.f;
	float Pitch = 1.f;

	if (ImpulseMagnitude >= 1)
	{
		Sound = ImpactAudioData.SoundHeavy;
	}
	else
	{
		Sound = ImpactAudioData.SoundDefault;
		Volume = UKismetMathLibrary::MapRangeClamped(ImpulseMagnitude, 0.f, 1.f, ImpactAudioData.VolumeModulationMin, ImpactAudioData.VolumeModulationMax);
		Pitch = UKismetMathLibrary::MapRangeClamped(ImpulseMagnitude, 0.f, 1.f, ImpactAudioData.PitchModulationMin, ImpactAudioData.PitchModulationMax);
	}

	UGameplayStatics::PlaySoundAtLocation(this, Sound, Location, Volume, Pitch);
	if (OnPlayCollisionSound.IsBound())
	{
		OnPlayCollisionSound.Broadcast(this, Sound);
	}
}

bool UCollisionAudioComponent::IsTriggerDeltaThreshold()
{
	if (bDisableDeltaThreshold || bFirstHit) return true;

	return !UKismetMathLibrary::NearlyEqual_TransformTransform(LastTriggerTransform, GetOwner()->GetTransform(), TriggerLocationDeltaThreshold, TriggerRotationDeltaThreshold, 0.0001f);
}

void UCollisionAudioComponent::SetImpactNameRef(FName InNameRef)
{
	ImpactNameRef = InNameRef;

	Initialize();
}

void UCollisionAudioComponent::SetCanPlay(bool CanPlay)
{
	bCanPlay = CanPlay;

	LastTriggerGameTime = UKismetSystemLibrary::GetGameTimeInSeconds(this);
	bFirstHit = true;
}

void UCollisionAudioComponent::SetCanEverPlay(bool CanEverPlay)
{
	bCanEverPlay = CanEverPlay;

	if (bCanEverPlay)
	{
		bFirstHit = true;
	}
}

