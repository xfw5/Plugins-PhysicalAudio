// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

/**
 * 
 */
class PHYSICALAUDIO_API PhysicalUtils
{
public:

	static USceneComponent* FindFirstParentOfClass(const USceneComponent* SearchRoot, TSubclassOf<USceneComponent> ComponentClass);

#if WITH_EDITORONLY_DATA
	static void DumpMsg(UObject* WorldContextObject, const FVector& MsgLocation, const FString& Msg, const FColor& Color, int MsgLogLevel)
	{
		UKismetSystemLibrary::DrawDebugString(WorldContextObject, MsgLocation, Msg, nullptr, Color, 3.f);

		if (MsgLogLevel < 2)
		{
			UE_LOG(LogTemp, Log, TEXT("%s"), *Msg);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("%s"), *Msg);
		}
	}
#endif
};
