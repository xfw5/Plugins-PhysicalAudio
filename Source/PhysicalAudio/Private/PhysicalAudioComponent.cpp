// Fill out your copyright notice in the Description page of Project Settings.

#include "PhysicalAudio.h"
#include "PhysicalAudioComponent.h"
#include "PhysicalUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"


FTrackedBone::FTrackedBone()
	: Controller()
	, BoneName("Invalid")
	, SoundCueLoop()
	, SoundCueMedium()
	, SoundCueHigh()
	, ThresholdLoop(0.2f)
	, ThresholdMedium(3.0f)
	, ThresholdHigh(5.0f)
	, RetriggerDelay()
	, TrackingSpace(ETrackedBoneSpace::Relative)
	, VelocityTrackingType(ETrackedBoneVelocityType::Rotational)
	, LoopInstance()
	, Delta()
	, TimeSinceLastTrigger()
	, bTriggeredLoopLayer()
	, InterpolatedVolume()
	, TorqueCurrent()
	, TorqueFinal()
	, OldRotation()
	, NewRotation()
{
}

#if WITH_EDITORONLY_DATA
ETrackedBoneEvent FTrackedBone::Update(UPhysicalAudioComponent* Owner, USkeletalMeshComponent* Mesh, float DeltaTime, float TimeDilation, bool bIgnoreDilation, float InterpSpeed, float VolumeMultiplier, bool DebugOn)
#else
ETrackedBoneEvent FTrackedBone::Update(USkeletalMeshComponent* Mesh, float DeltaTime, float TimeDilation, bool bIgnoreDilation, float InterpSpeed, float VolumeMultiplier)
#endif
{
	// Measure time since last sound cue trigger
	TimeSinceLastTrigger += DeltaTime;

	// Poll current/previous location from actor's Bone
	// HACK: Use "Custom" velocity tracking type to allow BP to specify custom Transform to track
	if (VelocityTrackingType != ETrackedBoneVelocityType::Custom)
	{
		if (Mesh && Mesh->GetBoneIndex(BoneName) != INDEX_NONE)
		{
			GetCurrentDeltaFromMesh(Mesh);
		}
		else
		{
			return ETrackedBoneEvent::None;
		}
	}

	// Calculate delta time
	float NewDeltaTime = FMath::Min(DeltaTime, 1.f / 45.f);
	if (bIgnoreDilation)
	{
		float InverseDilation = 1.0f / TimeDilation;
		NewDeltaTime = InverseDilation * NewDeltaTime;
	}

	// Linear/angular velocity tracking
	if (VelocityTrackingType == ETrackedBoneVelocityType::Rotational)
	{
		// Calculate and interpolate to new torque
		TorqueCurrent = (NewRotation - OldRotation) / NewDeltaTime;

		FQuat RotDelta = TorqueCurrent - TorqueFinal;

		TorqueFinal += RotDelta * NewDeltaTime * InterpSpeed;

		Delta = RotDelta.Size();
	}
	else if (VelocityTrackingType == ETrackedBoneVelocityType::Linear)
	{
		// Calculate and interpolate to new velocity
		ForceCurrent = (NewPosition - OldPosition) / NewDeltaTime;

		FVector PosDelta = ForceCurrent - ForceFinal;

		ForceFinal += PosDelta * NewDeltaTime * InterpSpeed;

		Delta = PosDelta.Size();
	}
	else if (VelocityTrackingType == ETrackedBoneVelocityType::Custom)
	{
		// Calculate and interpolate to new torque
		TorqueCurrent = (NewRotation - OldRotation) / NewDeltaTime;
		FQuat RotDelta = TorqueCurrent - TorqueFinal;
		TorqueFinal += RotDelta * NewDeltaTime * InterpSpeed;

		PreviousTriggerQuat = RotDelta;

		// Calculate and interpolate to new velocity
		ForceCurrent = (NewPosition - OldPosition) / NewDeltaTime;
		FVector PosDelta = ForceCurrent - ForceFinal;
		ForceFinal += PosDelta * NewDeltaTime * InterpSpeed;

		Delta = PosDelta.Size() + RotDelta.Size();

		// Examine this delta against the previous delta (POSITION ONLY), to see if it's an abrupt change in direction
		FVector CurrentTriggerVector = NewPosition - OldPosition;
		CurrentTriggerVector.Normalize();

		float DotFromLastTrigger = FVector::DotProduct(CurrentTriggerVector, PreviousTriggerVector);

		if (DotFromLastTrigger <= 0.0f && CurrentTriggerVector.Size() >= ThresholdMedium / 2.0f)
		{
			bDirectionChangedSinceLastTrigger = true;
		}
		else
		{
			bDirectionChangedSinceLastTrigger = false;
		}

		PreviousTriggerVector = CurrentTriggerVector;
	}

#if WITH_EDITORONLY_DATA
	if (DebugOn && Delta > 0.f)
	{
		FString Msg = FString::Printf(TEXT("Delta: %f bDirectionChangedSinceLastTrigger: %s TimeSinceLastTrigger: %f"), Delta, bDirectionChangedSinceLastTrigger ? TEXT("True") : TEXT("False"), TimeSinceLastTrigger);
		PhysicalUtils::DumpMsg(Owner, Mesh == nullptr ? FVector::ZeroVector : Mesh->GetSocketLocation(BoneName), Msg, Delta > 0.f ? FColor::Red : FColor::White, Delta > 0.f ? 2 : 0);
	}
#endif

	// Trigger events based on velocity delta
	if (Delta > ThresholdLoop)
	{
		if (!LoopInstance && SoundCueLoop)
		{
			return SendEvent(ETrackedBoneEvent::SlowThresholdStart);
		}
	}
	else if (FMath::IsNearlyEqual(InterpolatedVolume, 0.0f))
	{
		if (LoopInstance)
		{
			return SendEvent(ETrackedBoneEvent::SlowThresholdStop);
		}
	}

	if (TimeSinceLastTrigger >= RetriggerDelay || (bDirectionChangedSinceLastTrigger && TrackingSpace == ETrackedBoneSpace::World))
	{
		if (Delta >= ThresholdMedium && Delta < ThresholdHigh && SoundCueMedium)
		{
			return SendEvent(ETrackedBoneEvent::MediumThreshold);
		}
		else if (Delta >= ThresholdHigh && SoundCueHigh)
		{
			return SendEvent(ETrackedBoneEvent::FastThreshold);
		}
	}

	// Fade in/out and pitch up/down loop layer based on normalized movement delta
	if (LoopInstance)
	{
		float Volume = GetRangeMappedDelta(ThresholdLoop, ThresholdHigh);

		InterpolatedVolume += (Volume - InterpolatedVolume) * NewDeltaTime * VolumeInterpolatedSpeed;

		Controller->OnLoopSoundModulated.Broadcast(LoopInstance, InterpolatedVolume);

		LoopInstance->SetVolumeMultiplier(InterpolatedVolume * VolumeMultiplier);
	}

	return ETrackedBoneEvent::None;
}

float FTrackedBone::GetRangeMappedDelta(float Left, float Right)
{
	FVector2D RangeFrom(Left, Right);
	FVector2D RangeTo(0.0f, 1.0f);
	return FMath::GetMappedRangeValueClamped(RangeFrom, RangeTo, Delta);
}

void FTrackedBone::GetCurrentDeltaFromMesh(USkeletalMeshComponent* Mesh)
{
	int32 BoneIndex = Mesh->GetBoneIndex(BoneName);
	FTransform Transform = Mesh->GetBoneTransform(BoneIndex);
	FTransform RelTransform = Mesh->GetComponentTransform().GetRelativeTransform(Transform);

	// Store previous information
	OldRotation = NewRotation;
	OldPosition = NewPosition;

	// Switch coordinate spaces for tracking current information
	switch (TrackingSpace)
	{
	case ETrackedBoneSpace::Relative:
		NewRotation = RelTransform.GetRotation();
		NewPosition = RelTransform.GetLocation();
		break;
	case ETrackedBoneSpace::World:
		NewRotation = Transform.GetRotation();
		NewPosition = Transform.GetLocation();
		break;
	}
}

void FTrackedBone::GetCurrentDeltaFromTransform(FTransform const& Transform)
{
	// Store previous information
	OldRotation = NewRotation;
	OldPosition = NewPosition;

	// Update to new information
	NewRotation = Transform.GetRotation();
	NewPosition = Transform.GetLocation();
}

void FTrackedBone::ResetLoop()
{
	if (LoopInstance)
	{
		LoopInstance->FadeOut(0.5f, 0.0f);
		LoopInstance = nullptr;
		InterpolatedVolume = 0.0f;
	}
}

ETrackedBoneEvent FTrackedBone::SendEvent(ETrackedBoneEvent Event)
{
	switch (Event)
	{
	case ETrackedBoneEvent::SlowThresholdStart:
	case ETrackedBoneEvent::SlowThresholdStop:
		break;

	default:
		TimeSinceLastTrigger = 0.0f;
		break;
	}

	return Event;
}

FPhysicalAudioData::FPhysicalAudioData()
	: TrackedBones()
{
}

// Sets default values for this component's properties
UPhysicalAudioComponent::UPhysicalAudioComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;

	bVisible = false; // We don't draw anything (and probably should just be an ActorComponent)
	bUseAttachParentBound = true; // Avoid CalcBounds() when transform changes.
	bNeverNeedsRenderUpdate = true;

	bShouldIgnoreDilation = false;
	bShouldAttachOneShots = false;
	bCanPlay = false;
	InterpSpeed = 50;
	VolumeMultiplier = 1.0f;
}


// Called when the game starts
void UPhysicalAudioComponent::BeginPlay()
{
	Super::BeginPlay();

	// Locate parented skeletal mesh component, for tracking Bones and listening for broken constraints
	Mesh = Cast<USkeletalMeshComponent>(PhysicalUtils::FindFirstParentOfClass(this, USkeletalMeshComponent::StaticClass()));
	if (Mesh == nullptr)
	{
		bIsSkeletalMesh = false;
		Mesh = Cast<UStaticMeshComponent>(PhysicalUtils::FindFirstParentOfClass(this, UStaticMeshComponent::StaticClass()));
	}
	else
	{
		bIsSkeletalMesh = true;
	}

	// We can detach from parent now, since we've cached the mesh reference
	DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);

	// Fill-out data from table based on Name Ref
	ResetDataFromTable();
}


// Called every frame
void UPhysicalAudioComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Warning: we dynamically enable/disable our tick based on bCanPlay (see SetCanPlay)
	// We must be parented to a skeletal mesh component for Bone information to be read
	if (bCanPlay && Mesh)
	{
		// Tick all the tracked Bone objects for this component
		for (auto &VTS : TrackedBones)
		{
			float TimeDilation = UGameplayStatics::GetGlobalTimeDilation(GetWorld());

			if (VTS.VelocityTrackingType == ETrackedBoneVelocityType::Custom)
			{
				if (OnCustomTrackedTick.IsBound())
				{
					OnCustomTrackedTick.Broadcast(VTS);
				}
			}

#if WITH_EDITORONLY_DATA
			ETrackedBoneEvent Result = VTS.Update(this, bIsSkeletalMesh ? Cast<USkeletalMeshComponent>(Mesh) : nullptr, DeltaTime, TimeDilation, bShouldIgnoreDilation, InterpSpeed, VolumeMultiplier, bEnableDebug);
#else
			ETrackedBoneEvent Result = VTS.Update(bIsSkeletalMesh? Cast<USkeletalMeshComponent>(Mesh): nullptr, DeltaTime, TimeDilation, bShouldIgnoreDilation, InterpSpeed, VolumeMultiplier);
#endif
			// Receive events thrown by tracked Bones
			switch (Result)
			{
			case ETrackedBoneEvent::SlowThresholdStart:
			{
				VTS.ResetLoop();
				VTS.LoopInstance = PlaySoundFromBone(VTS, VTS.SoundCueLoop, 0.0f, true);

				if (OnLoopSoundTriggered.IsBound())
				{
					OnLoopSoundTriggered.Broadcast(VTS.LoopInstance);
				}
			} break;

			case ETrackedBoneEvent::SlowThresholdStop:
				if (VTS.LoopInstance)
				{
					VTS.LoopInstance->Stop();
					VTS.LoopInstance->DestroyComponent();
					VTS.LoopInstance = nullptr;
				}
				break;

			case ETrackedBoneEvent::MediumThreshold:
			{
				// Determine whether or not we need to create an audio component for this one-shot
				bool bUseAudioComponent = false;
				if (OnMediumSoundTriggered.IsBound() || bShouldAttachOneShots)
				{
					bUseAudioComponent = true;
				}

				// Trigger sound to play, modulate volume based on intensity of movement delta
				float Volume = VTS.GetRangeMappedDelta(VTS.ThresholdMedium, VTS.ThresholdHigh);
				UAudioComponent* MediumSound = PlaySoundFromBone(VTS, VTS.SoundCueMedium, Volume, bUseAudioComponent);

				if (bUseAudioComponent && MediumSound)
				{
					OnMediumSoundTriggered.Broadcast(MediumSound, Volume);
				}
			} break;

			case ETrackedBoneEvent::FastThreshold:
			{
				// Determine whether or not we need to create an audio component for this one-shot
				bool bUseAudioComponent = false;
				if (OnHeavySoundTriggered.IsBound() || bShouldAttachOneShots)
				{
					bUseAudioComponent = true;
				}

				// Trigger sound to play, modulate volume based on intensity of movement delta
				UAudioComponent* HeavySound = PlaySoundFromBone(VTS, VTS.SoundCueHigh, 1.0f, bUseAudioComponent);

				if (bUseAudioComponent && HeavySound)
				{
					OnHeavySoundTriggered.Broadcast(HeavySound);
				}
			} break;
			}
		}
	}
}

void UPhysicalAudioComponent::SetCanPlay(bool CanPlay)
{
	if (CanPlay == bCanPlay)
		return;

	bCanPlay = CanPlay;

	if (!bCanPlay)
	{
		for (auto &VTS : TrackedBones)
		{
			VTS.ResetLoop();
		}

		PrimaryComponentTick.SetTickFunctionEnable(false);
	}
	else
	{
		for (auto &VTS : TrackedBones)
		{
			VTS.ResetLoop();

			if (VTS.VelocityTrackingType != ETrackedBoneVelocityType::Custom)
			{
				USkeletalMeshComponent* SktMesh = Cast<USkeletalMeshComponent>(Mesh);
				if (SktMesh && SktMesh->GetBoneIndex(VTS.BoneName) != INDEX_NONE)
				{
					VTS.GetCurrentDeltaFromMesh(SktMesh);
				}
			}
		}

		if (!PrimaryComponentTick.IsTickFunctionEnabled())
		{
			PrimaryComponentTick.SetTickFunctionEnable(true);
		}
	}
}

void UPhysicalAudioComponent::SetVolumeMultiplier(float Multiplier)
{
	VolumeMultiplier = Multiplier;
}

void UPhysicalAudioComponent::SetCustomTrackedTransform(int32 Index, FTransform const& Transform)
{
	if (Index >= 0 && Index < TrackedBones.Num())
	{
		TrackedBones[Index].GetCurrentDeltaFromTransform(Transform);
	}
}

UAudioComponent* UPhysicalAudioComponent::PlaySoundFromBone(FTrackedBone const& VTS, USoundBase* Sound, float Volume, bool UseAttachedAudioComponent /*= false*/)
{
	if (UseAttachedAudioComponent)
	{
		return UGameplayStatics::SpawnSoundAttached(
			Sound,
			Mesh,
			VTS.BoneName,
			FVector(0.0f, 0.0f, 0.0f),
			EAttachLocation::SnapToTarget,
			true,
			Volume * VolumeMultiplier
		);
	}
	else if (Mesh)
	{
		FVector Location = Mesh->GetSocketLocation(VTS.BoneName);

		UGameplayStatics::PlaySoundAtLocation(this, Sound, Location);

		return nullptr;
	}

	return nullptr;
}

void UPhysicalAudioComponent::ResetDataFromTable()
{
	if (DataTableAsset)
	{
		static const FString ContextStr(TEXT("GetPhysicalAudioData"));
		FPhysicalAudioData const* Data = DataTableAsset->FindRow<FPhysicalAudioData>(DataNameRef, ContextStr);

		if (Data)
		{
			TrackedBones = Data->TrackedBones;

			for (auto& Bone : TrackedBones)
			{
				Bone.Controller = this;

				if (!bIsSkeletalMesh) Bone.VelocityTrackingType = ETrackedBoneVelocityType::Custom;
			}
		}
	}
}