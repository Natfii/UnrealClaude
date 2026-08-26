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
	UWorld* World = nullptr;
	if (auto Error = ValidateEditorContext(World))
	{
		return Error.GetValue();
	}

	FString ActorName;
	TOptional<FMCPToolResult> ParamError;
	if (!ExtractActorName(Params, TEXT("actor_name"), ActorName, ParamError))
	{
		return ParamError.GetValue();
	}

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

	AActor* Actor = FindActorByNameOrLabel(World, ActorName);
	if (!Actor)
	{
		return ActorNotFoundError(ActorName);
	}

	FString ErrorMessage;
	if (!SetPropertyFromJson(Actor, PropertyPath, Value, ErrorMessage))
	{
		return FMCPToolResult::Error(ErrorMessage);
	}

	Actor->MarkPackageDirty();
	MarkWorldDirty(World);

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

		OutProperty = OutObject->GetClass()->FindPropertyByName(FName(*PartName));

		if (!OutProperty)
		{
			// On actors, allow component names as path segments (e.g. "MeshComp.StaticMesh")
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
				OutError = FString::Printf(TEXT("Property not found: '%s' on %s."), *PartName, *OutObject->GetClass()->GetName());

				TArray<FString> AvailableProps;
				for (TFieldIterator<FProperty> It(OutObject->GetClass()); It; ++It)
				{
					if (AvailableProps.Num() >= 20) break;
					AvailableProps.Add(It->GetName());
				}
				if (AvailableProps.Num() > 0)
				{
					OutError += FString::Printf(TEXT(" Available: %s"), *FString::Join(AvailableProps, TEXT(", ")));
				}

				if (AActor* AsActor = Cast<AActor>(OutObject))
				{
					TArray<FString> CompNames;
					for (UActorComponent* Comp : AsActor->GetComponents())
					{
						if (Comp && CompNames.Num() < 15)
						{
							CompNames.Add(Comp->GetName());
						}
					}
					if (CompNames.Num() > 0)
					{
						OutError += FString::Printf(TEXT(". Components: %s"), *FString::Join(CompNames, TEXT(", ")));
					}
				}

				return false;
			}
			continue;
		}

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
				OutError = FString::Printf(TEXT("Cannot set component '%s' directly. Use a property path like '%s.PropertyName'."), *PartName, *Comp->GetName());
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
		// Fallback: coerce string "42.5" → double
		FString StrVal;
		if (Value->TryGetString(StrVal) && StrVal.IsNumeric())
		{
			NumProp->SetFloatingPointPropertyValue(ValuePtr, FCString::Atod(*StrVal));
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
		// Fallback: coerce string "42" → int64
		FString StrVal;
		if (Value->TryGetString(StrVal) && StrVal.IsNumeric())
		{
			NumProp->SetIntPropertyValue(ValuePtr, FCString::Atoi64(*StrVal));
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
	// Accepts both uppercase (R,G,B,A) and lowercase (r,g,b,a) field names.
	if (bIsFColor)
	{
		FColor Color;
		double R = 0, G = 0, B = 0, A = 255;
		if (!(*ObjVal)->TryGetNumberField(TEXT("R"), R))
			(*ObjVal)->TryGetNumberField(TEXT("r"), R);
		if (!(*ObjVal)->TryGetNumberField(TEXT("G"), G))
			(*ObjVal)->TryGetNumberField(TEXT("g"), G);
		if (!(*ObjVal)->TryGetNumberField(TEXT("B"), B))
			(*ObjVal)->TryGetNumberField(TEXT("b"), B);
		if (!(*ObjVal)->TryGetNumberField(TEXT("A"), A))
		{
			if (!(*ObjVal)->TryGetNumberField(TEXT("a"), A))
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
	// Accepts both uppercase (R,G,B,A) and lowercase (r,g,b,a) field names.
	if (bIsLinearColor)
	{
		FLinearColor Color;
		if (!(*ObjVal)->TryGetNumberField(TEXT("R"), Color.R))
			(*ObjVal)->TryGetNumberField(TEXT("r"), Color.R);
		if (!(*ObjVal)->TryGetNumberField(TEXT("G"), Color.G))
			(*ObjVal)->TryGetNumberField(TEXT("g"), Color.G);
		if (!(*ObjVal)->TryGetNumberField(TEXT("B"), Color.B))
			(*ObjVal)->TryGetNumberField(TEXT("b"), Color.B);
		if (!(*ObjVal)->TryGetNumberField(TEXT("A"), Color.A))
		{
			if (!(*ObjVal)->TryGetNumberField(TEXT("a"), Color.A))
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
			TextRepresentation += FString(*Pair.Key).ToUpper() + TEXT("=");

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

	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(TargetObject);
	bool bPropertySet = false;

	if (FNumericProperty* NumProp = CastField<FNumericProperty>(Property))
	{
		bPropertySet = SetNumericPropertyValue(NumProp, ValuePtr, Value);
	}
	else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		bool BoolVal = false;
		if (Value->TryGetBool(BoolVal))
		{
			BoolProp->SetPropertyValue(ValuePtr, BoolVal);
			bPropertySet = true;
		}
		else
		{
			// Fallback: coerce string "true"/"false"/"1"/"0" → bool
			FString StrVal;
			if (Value->TryGetString(StrVal))
			{
				if (StrVal.Equals(TEXT("true"), ESearchCase::IgnoreCase) || StrVal == TEXT("1"))
				{
					BoolProp->SetPropertyValue(ValuePtr, true);
					bPropertySet = true;
				}
				else if (StrVal.Equals(TEXT("false"), ESearchCase::IgnoreCase) || StrVal == TEXT("0"))
				{
					BoolProp->SetPropertyValue(ValuePtr, false);
					bPropertySet = true;
				}
			}
		}
	}
	else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		FString StrVal;
		if (Value->TryGetString(StrVal))
		{
			StrProp->SetPropertyValue(ValuePtr, StrVal);
			bPropertySet = true;
		}
	}
	else if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		FString StrVal;
		if (Value->TryGetString(StrVal))
		{
			NameProp->SetPropertyValue(ValuePtr, FName(*StrVal));
			bPropertySet = true;
		}
	}
	else if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
	{
		if (!SetObjectPropertyValue(ObjProp, ValuePtr, Value, OutError))
		{
			return false;
		}
		bPropertySet = true;
	}
	else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		bPropertySet = SetStructPropertyValue(StructProp, ValuePtr, Value);
		if (!bPropertySet)
		{
			OutError = FString::Printf(TEXT("Failed to set struct property '%s' (type: F%s). Supported formats: JSON object with fields, hex color string, or UE text format like \"(X=1,Y=2,Z=3)\"."),
				*PropertyPath, *StructProp->Struct->GetName());
			return false;
		}
	}

	if (!bPropertySet)
	{
		OutError = FString::Printf(TEXT("Unsupported property type '%s' for: %s"),
			*Property->GetCPPType(), *PropertyPath);
		return false;
	}

	// Notify engine systems (streaming, physics, navigation) about the property change.
	// Without this, setting object references (e.g. StaticMeshComponent.StaticMesh) via
	// raw reflection leaves the streaming manager unaware. When it later detects the
	// inconsistency during IncrementalUpdate() (while LevelRenderAssetManagersLock is held),
	// it crashes with an assertion. PostEditChangeProperty triggers the notification
	// immediately, before IncrementalUpdate runs.
	FPropertyChangedEvent PropertyChangedEvent(Property, EPropertyChangeType::ValueSet);
	TargetObject->PostEditChangeProperty(PropertyChangedEvent);

	return true;
}

bool FMCPTool_SetProperty::SetObjectPropertyValue(FObjectProperty* ObjProp, void* ValuePtr, const TSharedPtr<FJsonValue>& Value, FString& OutError)
{
	FString ObjectPath;
	if (!Value->TryGetString(ObjectPath))
	{
		OutError = TEXT("Object property value must be a string asset path (e.g. \"/Game/Meshes/SM_Rock\")");
		return false;
	}

	// Empty or "None" clears the reference
	if (ObjectPath.IsEmpty() || ObjectPath.Equals(TEXT("None"), ESearchCase::IgnoreCase))
	{
		ObjProp->SetObjectPropertyValue(ValuePtr, nullptr);
		return true;
	}

	UObject* ReferencedObject = LoadObject<UObject>(nullptr, *ObjectPath);
	if (!ReferencedObject)
	{
		OutError = FString::Printf(TEXT("Failed to load object: %s"), *ObjectPath);
		return false;
	}

	if (!ReferencedObject->IsA(ObjProp->PropertyClass))
	{
		OutError = FString::Printf(TEXT("Object type mismatch. Expected %s, got %s"),
			*ObjProp->PropertyClass->GetName(), *ReferencedObject->GetClass()->GetName());
		return false;
	}

	ObjProp->SetObjectPropertyValue(ValuePtr, ReferencedObject);
	return true;
}
