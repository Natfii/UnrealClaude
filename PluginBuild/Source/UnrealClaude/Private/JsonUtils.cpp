// Copyright Your Name. All Rights Reserved.

#include "JsonUtils.h"
#include "UnrealClaudeUtils.h"

FString FJsonUtils::Stringify(const TSharedPtr<FJsonObject>& JsonObject, bool bPrettyPrint)
{
	if (!JsonObject.IsValid())
	{
		return FString();
	}
	return Stringify(JsonObject.ToSharedRef(), bPrettyPrint);
}

FString FJsonUtils::Stringify(const TSharedRef<FJsonObject>& JsonObject, bool bPrettyPrint)
{
	FString OutputString;
	if (bPrettyPrint)
	{
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
		FJsonSerializer::Serialize(JsonObject, Writer);
	}
	else
	{
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutputString);
		FJsonSerializer::Serialize(JsonObject, Writer);
	}
	return OutputString;
}

TSharedPtr<FJsonObject> FJsonUtils::Parse(const FString& JsonString)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (FJsonSerializer::Deserialize(Reader, JsonObject))
	{
		return JsonObject;
	}

	return nullptr;
}

TSharedPtr<FJsonObject> FJsonUtils::CreateSuccessResponse(const FString& Message, TSharedPtr<FJsonObject> Data)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("message"), Message);

	if (Data.IsValid())
	{
		Response->SetObjectField(TEXT("data"), Data);
	}

	return Response;
}

TSharedPtr<FJsonObject> FJsonUtils::CreateErrorResponse(const FString& ErrorMessage)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), false);
	Response->SetStringField(TEXT("error"), ErrorMessage);

	return Response;
}

bool FJsonUtils::GetStringField(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, FString& OutValue)
{
	if (!JsonObject.IsValid())
	{
		return false;
	}

	return JsonObject->TryGetStringField(FieldName, OutValue);
}

bool FJsonUtils::GetStringField(const TSharedRef<FJsonObject>& JsonObject, const FString& FieldName, FString& OutValue)
{
	return JsonObject->TryGetStringField(FieldName, OutValue);
}

bool FJsonUtils::GetNumberField(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, double& OutValue)
{
	if (!JsonObject.IsValid())
	{
		return false;
	}

	return JsonObject->TryGetNumberField(FieldName, OutValue);
}

bool FJsonUtils::GetNumberField(const TSharedRef<FJsonObject>& JsonObject, const FString& FieldName, double& OutValue)
{
	return JsonObject->TryGetNumberField(FieldName, OutValue);
}

bool FJsonUtils::GetBoolField(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, bool& OutValue)
{
	if (!JsonObject.IsValid())
	{
		return false;
	}

	return JsonObject->TryGetBoolField(FieldName, OutValue);
}

bool FJsonUtils::GetBoolField(const TSharedRef<FJsonObject>& JsonObject, const FString& FieldName, bool& OutValue)
{
	return JsonObject->TryGetBoolField(FieldName, OutValue);
}

bool FJsonUtils::GetArrayField(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<TSharedPtr<FJsonValue>>& OutArray)
{
	if (!JsonObject.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* ArrayPtr;
	if (JsonObject->TryGetArrayField(FieldName, ArrayPtr))
	{
		OutArray = *ArrayPtr;
		return true;
	}

	return false;
}

bool FJsonUtils::GetArrayField(const TSharedRef<FJsonObject>& JsonObject, const FString& FieldName, TArray<TSharedPtr<FJsonValue>>& OutArray)
{
	const TArray<TSharedPtr<FJsonValue>>* ArrayPtr;
	if (JsonObject->TryGetArrayField(FieldName, ArrayPtr))
	{
		OutArray = *ArrayPtr;
		return true;
	}

	return false;
}

TArray<TSharedPtr<FJsonValue>> FJsonUtils::StringArrayToJson(const TArray<FString>& Strings)
{
	TArray<TSharedPtr<FJsonValue>> JsonArray;
	JsonArray.Reserve(Strings.Num());

	for (const FString& Str : Strings)
	{
		JsonArray.Add(MakeShared<FJsonValueString>(Str));
	}

	return JsonArray;
}

TArray<FString> FJsonUtils::JsonArrayToStrings(const TArray<TSharedPtr<FJsonValue>>& JsonArray)
{
	TArray<FString> Strings;
	Strings.Reserve(JsonArray.Num());

	for (const TSharedPtr<FJsonValue>& Value : JsonArray)
	{
		if (Value.IsValid())
		{
			Strings.Add(Value->AsString());
		}
	}

	return Strings;
}

// ===== Geometry Conversion Helpers =====
// Forward to UnrealClaudeJsonUtils to avoid duplication

TSharedPtr<FJsonObject> FJsonUtils::VectorToJson(const FVector& Vec)
{
	return UnrealClaudeJsonUtils::VectorToJson(Vec);
}

TSharedPtr<FJsonObject> FJsonUtils::RotatorToJson(const FRotator& Rot)
{
	return UnrealClaudeJsonUtils::RotatorToJson(Rot);
}

TSharedPtr<FJsonObject> FJsonUtils::ScaleToJson(const FVector& Scale)
{
	return UnrealClaudeJsonUtils::VectorToJson(Scale);
}

bool FJsonUtils::JsonToVector(const TSharedPtr<FJsonObject>& JsonObject, FVector& OutVec)
{
	OutVec = UnrealClaudeJsonUtils::ExtractVector(JsonObject, FVector::ZeroVector);
	return JsonObject.IsValid();
}

bool FJsonUtils::JsonToRotator(const TSharedPtr<FJsonObject>& JsonObject, FRotator& OutRot)
{
	OutRot = UnrealClaudeJsonUtils::ExtractRotator(JsonObject, FRotator::ZeroRotator);
	return JsonObject.IsValid();
}

bool FJsonUtils::JsonToScale(const TSharedPtr<FJsonObject>& JsonObject, FVector& OutScale)
{
	OutScale = UnrealClaudeJsonUtils::ExtractScale(JsonObject, FVector::OneVector);
	return JsonObject.IsValid();
}
