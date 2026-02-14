// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Set a property on an actor
 */
class FMCPTool_SetProperty : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("set_property");
		Info.Description = TEXT(
			"Set any property value on an actor, including component properties.\n\n"
			"This is a powerful tool for modifying actor settings that aren't covered by other tools. "
			"Use dot notation to access nested properties and components.\n\n"
			"Property path examples:\n"
			"- 'bHidden' - Actor visibility\n"
			"- 'Tags' - Actor tags array\n"
			"- 'LightComponent.Intensity' - Light intensity\n"
			"- 'LightComponent.LightColor' - Light color {R, G, B, A}\n"
			"- 'StaticMeshComponent.RelativeScale3D' - Mesh scale\n"
			"- 'RootComponent.RelativeLocation' - Root position\n\n"
			"Value types: strings, numbers, booleans, objects (FVector, FRotator, FLinearColor), arrays.\n\n"
			"Returns: Confirmation of property change."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("actor_name"), TEXT("string"), TEXT("The name of the actor to modify"), true),
			FMCPToolParameter(TEXT("property"), TEXT("string"), TEXT("The property path to set (e.g., 'RelativeLocation', 'LightComponent.Intensity')"), true),
			FMCPToolParameter(TEXT("value"), TEXT("any"), TEXT("The value to set (type depends on property)"), true)
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	/** Navigate through a property path to find the target object and property */
	bool NavigateToProperty(
		UObject* StartObject,
		const TArray<FString>& PathParts,
		UObject*& OutObject,
		FProperty*& OutProperty,
		FString& OutError);

	/** Try to navigate into a component on an actor */
	bool TryNavigateToComponent(
		UObject*& CurrentObject,
		const FString& PartName,
		bool bIsLastPart,
		FString& OutError);

	/** Navigate into a nested object property */
	bool NavigateIntoNestedObject(
		UObject*& CurrentObject,
		FProperty* Property,
		const FString& PartName,
		FString& OutError);

	/** Set a numeric property value from JSON */
	bool SetNumericPropertyValue(FNumericProperty* NumProp, void* ValuePtr, const TSharedPtr<FJsonValue>& Value);

	/** Set a struct property value from JSON (FVector, FRotator, FLinearColor) */
	bool SetStructPropertyValue(FStructProperty* StructProp, void* ValuePtr, const TSharedPtr<FJsonValue>& Value);

	/** Helper to set a property value from JSON */
	bool SetPropertyFromJson(UObject* Object, const FString& PropertyPath, const TSharedPtr<FJsonValue>& Value, FString& OutError);
};
