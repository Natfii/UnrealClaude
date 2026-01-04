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
	// Validate we're in editor
	if (!GEditor)
	{
		return FMCPToolResult::Error(TEXT("Editor not available"));
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return FMCPToolResult::Error(TEXT("No active world"));
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

	// Find the actor
	AActor* Actor = FindActorByName(World, ActorName);
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

	// Mark dirty
	Actor->MarkPackageDirty();
	World->MarkPackageDirty();

	// Build result
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("actor"), Actor->GetName());
	ResultData->SetStringField(TEXT("property"), PropertyPath);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Set property '%s' on actor '%s'"), *PropertyPath, *Actor->GetName()),
		ResultData
	);
}

AActor* FMCPTool_SetProperty::FindActorByName(UWorld* World, const FString& Name)
{
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && (Actor->GetName() == Name || Actor->GetActorLabel() == Name))
		{
			return Actor;
		}
	}
	return nullptr;
}

bool FMCPTool_SetProperty::SetPropertyFromJson(UObject* Object, const FString& PropertyPath, const TSharedPtr<FJsonValue>& Value, FString& OutError)
{
	if (!Object || !Value.IsValid())
	{
		OutError = TEXT("Invalid object or value");
		return false;
	}

	// Handle nested property paths (e.g., "LightComponent.Intensity")
	TArray<FString> PathParts;
	PropertyPath.ParseIntoArray(PathParts, TEXT("."), true);

	UObject* CurrentObject = Object;
	FProperty* Property = nullptr;

	// Navigate to the final object/property
	for (int32 i = 0; i < PathParts.Num(); ++i)
	{
		const FString& PartName = PathParts[i];

		// Try to find the property
		Property = CurrentObject->GetClass()->FindPropertyByName(FName(*PartName));

		if (!Property)
		{
			// Try finding as component
			AActor* Actor = Cast<AActor>(CurrentObject);
			if (Actor)
			{
				for (UActorComponent* Comp : Actor->GetComponents())
				{
					if (Comp && Comp->GetName().Contains(PartName))
					{
						if (i == PathParts.Num() - 1)
						{
							// This is the final part but it's a component, can't set it
							OutError = FString::Printf(TEXT("Cannot set component as value: %s"), *PartName);
							return false;
						}
						CurrentObject = Comp;
						Property = nullptr;
						break;
					}
				}
			}

			if (!Property && i == PathParts.Num() - 1)
			{
				OutError = FString::Printf(TEXT("Property not found: %s on %s"), *PartName, *CurrentObject->GetClass()->GetName());
				return false;
			}

			continue;
		}

		// If not the last part, navigate into nested object
		if (i < PathParts.Num() - 1)
		{
			FObjectProperty* ObjProp = CastField<FObjectProperty>(Property);
			if (ObjProp)
			{
				UObject* NestedObj = ObjProp->GetObjectPropertyValue(ObjProp->ContainerPtrToValuePtr<void>(CurrentObject));
				if (NestedObj)
				{
					CurrentObject = NestedObj;
					Property = nullptr;
				}
				else
				{
					OutError = FString::Printf(TEXT("Nested object is null: %s"), *PartName);
					return false;
				}
			}
			else
			{
				OutError = FString::Printf(TEXT("Cannot navigate into non-object property: %s"), *PartName);
				return false;
			}
		}
	}

	if (!Property)
	{
		OutError = FString::Printf(TEXT("Property not found: %s"), *PropertyPath);
		return false;
	}

	// Get property address
	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(CurrentObject);

	// Set value based on property type
	if (FNumericProperty* NumProp = CastField<FNumericProperty>(Property))
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
	}
	else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		bool BoolVal = false;
		if (Value->TryGetBool(BoolVal))
		{
			BoolProp->SetPropertyValue(ValuePtr, BoolVal);
			return true;
		}
	}
	else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		FString StrVal;
		if (Value->TryGetString(StrVal))
		{
			StrProp->SetPropertyValue(ValuePtr, StrVal);
			return true;
		}
	}
	else if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		FString StrVal;
		if (Value->TryGetString(StrVal))
		{
			NameProp->SetPropertyValue(ValuePtr, FName(*StrVal));
			return true;
		}
	}
	else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		// Handle common struct types
		if (StructProp->Struct == TBaseStructure<FVector>::Get())
		{
			const TSharedPtr<FJsonObject>* ObjVal;
			if (Value->TryGetObject(ObjVal))
			{
				FVector Vec;
				(*ObjVal)->TryGetNumberField(TEXT("x"), Vec.X);
				(*ObjVal)->TryGetNumberField(TEXT("y"), Vec.Y);
				(*ObjVal)->TryGetNumberField(TEXT("z"), Vec.Z);
				*reinterpret_cast<FVector*>(ValuePtr) = Vec;
				return true;
			}
		}
		else if (StructProp->Struct == TBaseStructure<FRotator>::Get())
		{
			const TSharedPtr<FJsonObject>* ObjVal;
			if (Value->TryGetObject(ObjVal))
			{
				FRotator Rot;
				(*ObjVal)->TryGetNumberField(TEXT("pitch"), Rot.Pitch);
				(*ObjVal)->TryGetNumberField(TEXT("yaw"), Rot.Yaw);
				(*ObjVal)->TryGetNumberField(TEXT("roll"), Rot.Roll);
				*reinterpret_cast<FRotator*>(ValuePtr) = Rot;
				return true;
			}
		}
		else if (StructProp->Struct == TBaseStructure<FLinearColor>::Get())
		{
			const TSharedPtr<FJsonObject>* ObjVal;
			if (Value->TryGetObject(ObjVal))
			{
				FLinearColor Color;
				(*ObjVal)->TryGetNumberField(TEXT("r"), Color.R);
				(*ObjVal)->TryGetNumberField(TEXT("g"), Color.G);
				(*ObjVal)->TryGetNumberField(TEXT("b"), Color.B);
				(*ObjVal)->TryGetNumberField(TEXT("a"), Color.A);
				*reinterpret_cast<FLinearColor*>(ValuePtr) = Color;
				return true;
			}
		}
	}

	OutError = FString::Printf(TEXT("Unsupported property type for: %s"), *PropertyPath);
	return false;
}
