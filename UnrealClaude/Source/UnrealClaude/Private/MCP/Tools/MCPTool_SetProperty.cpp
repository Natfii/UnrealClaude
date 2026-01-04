// Copyright Your Name. All Rights Reserved.

#include "MCPTool_SetProperty.h"
#include "MCP/MCPParamValidator.h"
#include "UnrealClaudeModule.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include "UObject/PropertyAccessUtil.h"

FMCPToolResult FMCPTool_SetProperty::Execute(const TSharedRef<FJsonObject>& Params)
{
	// Validate editor context using base class
	UWorld* World = nullptr;
	if (auto Error = ValidateEditorContext(World))
	{
		return Error.GetValue();
	}

	// Get parameters
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: actor_name"));
	}

	// Validate actor name
	FString ValidationError;
	if (!FMCPParamValidator::ValidateActorName(ActorName, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	FString PropertyPath;
	if (!Params->TryGetStringField(TEXT("property"), PropertyPath))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: property"));
	}

	// Validate property path
	if (!FMCPParamValidator::ValidatePropertyPath(PropertyPath, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	const TSharedPtr<FJsonValue>* ValuePtr = nullptr;
	if (!Params->HasField(TEXT("value")))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: value"));
	}
	TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));

	// Find the actor using base class helper
	AActor* Actor = FindActorByNameOrLabel(World, ActorName);
	if (!Actor)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	// Set the property
	FString ErrorMessage;
	if (!SetPropertyFromJson(Actor, PropertyPath, Value, ErrorMessage))
	{
		return FMCPToolResult::Error(ErrorMessage);
	}

	// Mark dirty using base class helper
	Actor->MarkPackageDirty();
	MarkWorldDirty(World);

	// Build result
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("actor"), Actor->GetName());
	ResultData->SetStringField(TEXT("property"), PropertyPath);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Set property '%s' on actor '%s'"), *PropertyPath, *Actor->GetName()),
		ResultData
	);
}

bool FMCPTool_SetProperty::NavigateToProperty(
	UObject* StartObject,
	const TArray<FString>& PathParts,
	UObject*& OutObject,
	FProperty*& OutProperty,
	FString& OutError)
{
	OutObject = StartObject;
	OutProperty = nullptr;

	for (int32 i = 0; i < PathParts.Num(); ++i)
	{
		const FString& PartName = PathParts[i];
		const bool bIsLastPart = (i == PathParts.Num() - 1);

		// Try to find the property
		OutProperty = OutObject->GetClass()->FindPropertyByName(FName(*PartName));

		if (!OutProperty)
		{
			// Try finding as component on actors
			if (!TryNavigateToComponent(OutObject, PartName, bIsLastPart, OutError))
			{
				if (!OutError.IsEmpty())
				{
					return false;
				}
			}
			else
			{
				OutProperty = nullptr;
				continue;
			}

			if (bIsLastPart)
			{
				OutError = FString::Printf(TEXT("Property not found: %s on %s"), *PartName, *OutObject->GetClass()->GetName());
				return false;
			}
			continue;
		}

		// If not the last part, navigate into nested object
		if (!bIsLastPart)
		{
			if (!NavigateIntoNestedObject(OutObject, OutProperty, PartName, OutError))
			{
				return false;
			}
			OutProperty = nullptr;
		}
	}

	return OutProperty != nullptr;
}

bool FMCPTool_SetProperty::TryNavigateToComponent(
	UObject*& CurrentObject,
	const FString& PartName,
	bool bIsLastPart,
	FString& OutError)
{
	AActor* Actor = Cast<AActor>(CurrentObject);
	if (!Actor)
	{
		return false;
	}

	for (UActorComponent* Comp : Actor->GetComponents())
	{
		if (Comp && Comp->GetName().Contains(PartName))
		{
			if (bIsLastPart)
			{
				OutError = FString::Printf(TEXT("Cannot set component as value: %s"), *PartName);
				return false;
			}
			CurrentObject = Comp;
			return true;
		}
	}
	return false;
}

bool FMCPTool_SetProperty::NavigateIntoNestedObject(
	UObject*& CurrentObject,
	FProperty* Property,
	const FString& PartName,
	FString& OutError)
{
	FObjectProperty* ObjProp = CastField<FObjectProperty>(Property);
	if (!ObjProp)
	{
		OutError = FString::Printf(TEXT("Cannot navigate into non-object property: %s"), *PartName);
		return false;
	}

	UObject* NestedObj = ObjProp->GetObjectPropertyValue(ObjProp->ContainerPtrToValuePtr<void>(CurrentObject));
	if (!NestedObj)
	{
		OutError = FString::Printf(TEXT("Nested object is null: %s"), *PartName);
		return false;
	}

	CurrentObject = NestedObj;
	return true;
}

bool FMCPTool_SetProperty::SetNumericPropertyValue(FNumericProperty* NumProp, void* ValuePtr, const TSharedPtr<FJsonValue>& Value)
{
	if (NumProp->IsFloatingPoint())
	{
		double DoubleVal = 0.0;
		if (Value->TryGetNumber(DoubleVal))
		{
			NumProp->SetFloatingPointPropertyValue(ValuePtr, DoubleVal);
			return true;
		}
	}
	else if (NumProp->IsInteger())
	{
		int64 IntVal = 0;
		if (Value->TryGetNumber(IntVal))
		{
			NumProp->SetIntPropertyValue(ValuePtr, IntVal);
			return true;
		}
	}
	return false;
}

bool FMCPTool_SetProperty::SetStructPropertyValue(FStructProperty* StructProp, void* ValuePtr, const TSharedPtr<FJsonValue>& Value)
{
	const TSharedPtr<FJsonObject>* ObjVal;
	if (!Value->TryGetObject(ObjVal))
	{
		return false;
	}

	if (StructProp->Struct == TBaseStructure<FVector>::Get())
	{
		FVector Vec;
		(*ObjVal)->TryGetNumberField(TEXT("x"), Vec.X);
		(*ObjVal)->TryGetNumberField(TEXT("y"), Vec.Y);
		(*ObjVal)->TryGetNumberField(TEXT("z"), Vec.Z);
		*reinterpret_cast<FVector*>(ValuePtr) = Vec;
		return true;
	}

	if (StructProp->Struct == TBaseStructure<FRotator>::Get())
	{
		FRotator Rot;
		(*ObjVal)->TryGetNumberField(TEXT("pitch"), Rot.Pitch);
		(*ObjVal)->TryGetNumberField(TEXT("yaw"), Rot.Yaw);
		(*ObjVal)->TryGetNumberField(TEXT("roll"), Rot.Roll);
		*reinterpret_cast<FRotator*>(ValuePtr) = Rot;
		return true;
	}

	if (StructProp->Struct == TBaseStructure<FLinearColor>::Get())
	{
		FLinearColor Color;
		(*ObjVal)->TryGetNumberField(TEXT("r"), Color.R);
		(*ObjVal)->TryGetNumberField(TEXT("g"), Color.G);
		(*ObjVal)->TryGetNumberField(TEXT("b"), Color.B);
		(*ObjVal)->TryGetNumberField(TEXT("a"), Color.A);
		*reinterpret_cast<FLinearColor*>(ValuePtr) = Color;
		return true;
	}

	return false;
}

bool FMCPTool_SetProperty::SetPropertyFromJson(UObject* Object, const FString& PropertyPath, const TSharedPtr<FJsonValue>& Value, FString& OutError)
{
	if (!Object || !Value.IsValid())
	{
		OutError = TEXT("Invalid object or value");
		return false;
	}

	// Parse property path and navigate to target
	TArray<FString> PathParts;
	PropertyPath.ParseIntoArray(PathParts, TEXT("."), true);

	UObject* TargetObject = nullptr;
	FProperty* Property = nullptr;

	if (!NavigateToProperty(Object, PathParts, TargetObject, Property, OutError))
	{
		if (OutError.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Property not found: %s"), *PropertyPath);
		}
		return false;
	}

	// Get property address and set value based on type
	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(TargetObject);

	// Try numeric property
	if (FNumericProperty* NumProp = CastField<FNumericProperty>(Property))
	{
		if (SetNumericPropertyValue(NumProp, ValuePtr, Value))
		{
			return true;
		}
	}
	// Try bool property
	else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		bool BoolVal = false;
		if (Value->TryGetBool(BoolVal))
		{
			BoolProp->SetPropertyValue(ValuePtr, BoolVal);
			return true;
		}
	}
	// Try string property
	else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		FString StrVal;
		if (Value->TryGetString(StrVal))
		{
			StrProp->SetPropertyValue(ValuePtr, StrVal);
			return true;
		}
	}
	// Try name property
	else if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		FString StrVal;
		if (Value->TryGetString(StrVal))
		{
			NameProp->SetPropertyValue(ValuePtr, FName(*StrVal));
			return true;
		}
	}
	// Try struct property
	else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		if (SetStructPropertyValue(StructProp, ValuePtr, Value))
		{
			return true;
		}
	}

	OutError = FString::Printf(TEXT("Unsupported property type for: %s"), *PropertyPath);
	return false;
}
