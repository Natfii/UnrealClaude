// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../MCPToolRegistry.h"

/**
 * MCP Tool: Set a property on an actor
 */
class FMCPTool_SetProperty : public IMCPTool
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("set_property");
		Info.Description = TEXT("Set a property value on an actor by name");
		Info.Parameters = {
			FMCPToolParameter(TEXT("actor_name"), TEXT("string"), TEXT("The name of the actor to modify"), true),
			FMCPToolParameter(TEXT("property"), TEXT("string"), TEXT("The property path to set (e.g., 'RelativeLocation', 'LightComponent.Intensity')"), true),
			FMCPToolParameter(TEXT("value"), TEXT("any"), TEXT("The value to set (type depends on property)"), true)
		};
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	/** Helper to find an actor by name */
	AActor* FindActorByName(UWorld* World, const FString& Name);

	/** Helper to set a property value from JSON */
	bool SetPropertyFromJson(UObject* Object, const FString& PropertyPath, const TSharedPtr<FJsonValue>& Value, FString& OutError);
};
