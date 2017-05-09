// Fill out your copyright notice in the Description page of Project Settings.

#include "PhysicalAudio.h"
#include "PhysicalUtils.h"

USceneComponent* PhysicalUtils::FindFirstParentOfClass(const USceneComponent* SearchRoot, TSubclassOf<USceneComponent> ComponentClass)
{
	USceneComponent* FoundParent = nullptr;

	if (SearchRoot && *ComponentClass)
	{
		USceneComponent* ParentIterator = SearchRoot->GetAttachParent();
		while (ParentIterator != nullptr)
		{
			if (ParentIterator->IsA(ComponentClass))
			{
				FoundParent = ParentIterator;
				break;
			}

			ParentIterator = ParentIterator->GetAttachParent();
		}
	}

	return FoundParent;
}
