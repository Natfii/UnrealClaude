// Copyright Natali Caggiano. All Rights Reserved.

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

	// Extract and validate actor name using base class helper
	FString ActorName;
	TOptional<FMCPToolResult> ParamError;
	if (!ExtractActorName(Params, TEXT("actor_name"), ActorName, ParamError))
	{
		return ParamError.GetValue();
	}

	// Extract and validate property path using base class helpers
	FString PropertyPath;
	if (!ExtractRequiredString(Params, TEXT("property"), PropertyPath, ParamError))
	{
		return ParamError.GetValue();
	}
	if (!ValidatePropertyPathParam(PropertyPath, ParamError))
	{
		return ParamError.GetValue();
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
		return ActorNotFoundError(ActorName);
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
	// Use both pointer comparison and name-based matching for robustness.
	// TBaseStructure<T>::Get() can fail to match at runtime in some module configurations,
	// so we fall back to the struct's reflected name (UE drops the 'F' prefix).
	const FName StructName = StructProp->Struct->GetFName();

	const bool bIsFColor = (StructProp->Struct == TBaseStructure<FColor>::Get())
		|| (StructName == FName("Color"));
	const bool bIsLinearColor = (StructProp->Struct == TBaseStructure<FLinearColor>::Get())
		|| (StructName == FName("LinearColor"));
	const bool bIsVector = (StructProp->Struct == TBaseStructure<FVector>::Get())
		|| (StructName == FName("Vector"));
	const bool bIsRotator = (StructProp->Struct == TBaseStructure<FRotator>::Get())
		|| (StructName == FName("Rotator"));

	// Check for hex string format first (works for both FColor and FLinearColor)
	FString StringValue;
	if (Value->TryGetString(StringValue))
	{
		FString HexString = StringValue;
		if (HexString.StartsWith(TEXT("#")))
		{
			HexString = HexString.RightChop(1);
		}

		if (HexString.Len() == 6 || HexString.Len() == 8)
		{
			FColor ParsedColor = FColor::FromHex(HexString);

			if (bIsFColor)
			{
				*reinterpret_cast<FColor*>(ValuePtr) = ParsedColor;
				return true;
			}

			if (bIsLinearColor)
			{
				*reinterpret_cast<FLinearColor*>(ValuePtr) = FLinearColor(ParsedColor);
				return true;
			}
		}

		// Generic string fallback: try UE's built-in ImportText for any struct type.
		// Handles formats like "(R=255,G=0,B=0,A=255)" or "(X=1.0,Y=2.0,Z=3.0)".
		const TCHAR* ImportResult = StructProp->ImportText_Direct(*StringValue, ValuePtr, nullptr, 0);
		if (ImportResult != nullptr)
		{
			return true;
		}
	}

	// Try object format
	const TSharedPtr<FJsonObject>* ObjVal;
	if (!Value->TryGetObject(ObjVal))
	{
		return false;
	}

	if (bIsVector)
	{
		FVector Vec;
		(*ObjVal)->TryGetNumberField(TEXT("x"), Vec.X);
		(*ObjVal)->TryGetNumberField(TEXT("y"), Vec.Y);
		(*ObjVal)->TryGetNumberField(TEXT("z"), Vec.Z);
		*reinterpret_cast<FVector*>(ValuePtr) = Vec;
		return true;
	}

	if (bIsRotator)
	{
		FRotator Rot;
		(*ObjVal)->TryGetNumberField(TEXT("pitch"), Rot.Pitch);
		(*ObjVal)->TryGetNumberField(TEXT("yaw"), Rot.Yaw);
		(*ObjVal)->TryGetNumberField(TEXT("roll"), Rot.Roll);
		*reinterpret_cast<FRotator*>(ValuePtr) = Rot;
		return true;
	}

	// FColor - uses uint8 values (0-255). Parse via double to handle JSON number types robustly.
	if (bIsFColor)
	{
		FColor Color;
		double R = 0, G = 0, B = 0, A = 255;
		(*ObjVal)->TryGetNumberField(TEXT("r"), R);
		(*ObjVal)->TryGetNumberField(TEXT("g"), G);
		(*ObjVal)->TryGetNumberField(TEXT("b"), B);
		if (!(*ObjVal)->TryGetNumberField(TEXT("a"), A))
		{
			A = 255.0;
		}
		Color.R = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(R), 0, 255));
		Color.G = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(G), 0, 255));
		Color.B = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(B), 0, 255));
		Color.A = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(A), 0, 255));
		*reinterpret_cast<FColor*>(ValuePtr) = Color;
		return true;
	}

	// FLinearColor - uses float values (0.0-1.0)
	if (bIsLinearColor)
	{
		FLinearColor Color;
		(*ObjVal)->TryGetNumberField(TEXT("r"), Color.R);
		(*ObjVal)->TryGetNumberField(TEXT("g"), Color.G);
		(*ObjVal)->TryGetNumberField(TEXT("b"), Color.B);
		if (!(*ObjVal)->TryGetNumberField(TEXT("a"), Color.A))
		{
			Color.A = 1.0f;
		}
		// Auto-normalize: if any color component > 1.5, assume 0-255 range
		if (Color.R > 1.5f || Color.G > 1.5f || Color.B > 1.5f)
		{
			Color.R /= 255.0f;
			Color.G /= 255.0f;
			Color.B /= 255.0f;
			if (Color.A > 1.5f) Color.A /= 255.0f;
		}
		*reinterpret_cast<FLinearColor*>(ValuePtr) = Color;
		return true;
	}

	// Generic fallback: convert JSON object to UE text format and use ImportText.
	// Builds "(Key=Value,Key=Value,...)" from the JSON fields.
	{
		FString TextRepresentation = TEXT("(");
		bool bFirst = true;
		for (const auto& Pair : (*ObjVal)->Values)
		{
			if (!bFirst) TextRepresentation += TEXT(",");
			TextRepresentation += Pair.Key.ToUpper() + TEXT("=");

			double NumVal;
			FString StrVal;
			if (Pair.Value->TryGetNumber(NumVal))
			{
				TextRepresentation += FString::SanitizeFloat(NumVal);
			}
			else if (Pair.Value->TryGetString(StrVal))
			{
				TextRepresentation += StrVal;
			}
			bFirst = false;
		}
		TextRepresentation += TEXT(")");

		const TCHAR* ImportResult = StructProp->ImportText_Direct(*TextRepresentation, ValuePtr, nullptr, 0);
		if (ImportResult != nullptr)
		{
			return true;
		}
	}

	return false;
}


/**
 * Set a property value on an object using Unreal's reflection system.
 *
 * This function traverses a dot-separated property path (e.g., "Component.Transform.Location")
 * and sets the final property to the provided JSON value. It supports:
 *
 * - Numeric types (int32, float, double, etc.)
 * - Boolean properties
 * - String and Name properties
 * - Struct properties (FVector, FRotator, FLinearColor, etc.)
 *
 * Property path navigation:
 * 1. Parse path into components (e.g., "Component.Location" -> ["Component", "Location"])
 * 2. For each component, find the property on the current object
 * 3. If property is an object reference, dereference and continue
 * 4. Set the final property value using appropriate type handler
 *
 * Security: Property paths are validated by ValidatePropertyPath() before calling this.
 *
 * @param Object - The root object to start navigation from
 * @param PropertyPath - Dot-separated path to the property (e.g., "Transform.Location.X")
 * @param Value - JSON value to set (type must be compatible with property type)
 * @param OutError - Error message if operation fails
 * @return true if property was successfully set
 */
bool FMCPTool_SetProperty::SetPropertyFromJson(UObject* Object, const FString& PropertyPath, const TSharedPtr<FJsonValue>& Value, FString& OutError)
{
	if (!Object || !Value.IsValid())
	{
		OutError = TEXT("Invalid object or value");
		return false;
	}

	// Parse property path into components for traversal
	// Example: "StaticMeshComponent.RelativeLocation.X" -> ["StaticMeshComponent", "RelativeLocation", "X"]
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
		OutError = FString::Printf(TEXT("Failed to set struct property '%s' (type: F%s). Supported formats: JSON object with fields, hex color string, or UE text format like \"(X=1,Y=2,Z=3)\"."),
			*PropertyPath, *StructProp->Struct->GetName());
		return false;
	}

	OutError = FString::Printf(TEXT("Unsupported property type '%s' for: %s"),
		*Property->GetCPPType(), *PropertyPath);
	return false;
}
