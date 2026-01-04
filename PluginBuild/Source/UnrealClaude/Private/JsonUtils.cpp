// Copyright Your Name. All Rights Reserved.

#include "JsonUtils.h"

FString FJsonUtils::Stringify(const TSharedPtr<FJsonObject>& JsonObject, bool bPrettyPrint)
{
	if (!JsonObject.IsValid())
	{
		return FString();
	}

	FString OutputString;
	if (bPrettyPrint)
	{
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
		FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	}
	else
	{
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutputString);
		FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	}

	return OutputString;
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

TSharedPtr<FJsonObject> FJsonUtils::VectorToJson(const FVector& Vec)
{
	TSharedPtr<FJsonObject> JsonObj = MakeShared<FJsonObject>();
	JsonObj->SetNumberField(TEXT("x"), Vec.X);
	JsonObj->SetNumberField(TEXT("y"), Vec.Y);
	JsonObj->SetNumberField(TEXT("z"), Vec.Z);
	return JsonObj;
}

TSharedPtr<FJsonObject> FJsonUtils::RotatorToJson(const FRotator& Rot)
{
	TSharedPtr<FJsonObject> JsonObj = MakeShared<FJsonObject>();
	JsonObj->SetNumberField(TEXT("pitch"), Rot.Pitch);
	JsonObj->SetNumberField(TEXT("yaw"), Rot.Yaw);
	JsonObj->SetNumberField(TEXT("roll"), Rot.Roll);
	return JsonObj;
}

TSharedPtr<FJsonObject> FJsonUtils::ScaleToJson(const FVector& Scale)
{
	// Same as VectorToJson, but semantically different
	return VectorToJson(Scale);
}

bool FJsonUtils::JsonToVector(const TSharedPtr<FJsonObject>& JsonObject, FVector& OutVec)
{
	if (!JsonObject.IsValid())
	{
		return false;
	}

	OutVec = FVector::ZeroVector;
	bool bParsedAny = false;

	double Val;
	if (JsonObject->TryGetNumberField(TEXT("x"), Val))
	{
		OutVec.X = Val;
		bParsedAny = true;
	}
	if (JsonObject->TryGetNumberField(TEXT("y"), Val))
	{
		OutVec.Y = Val;
		bParsedAny = true;
	}
	if (JsonObject->TryGetNumberField(TEXT("z"), Val))
	{
		OutVec.Z = Val;
		bParsedAny = true;
	}

	return bParsedAny;
}

bool FJsonUtils::JsonToRotator(const TSharedPtr<FJsonObject>& JsonObject, FRotator& OutRot)
{
	if (!JsonObject.IsValid())
	{
		return false;
	}

	OutRot = FRotator::ZeroRotator;
	bool bParsedAny = false;

	double Val;
	if (JsonObject->TryGetNumberField(TEXT("pitch"), Val))
	{
		OutRot.Pitch = Val;
		bParsedAny = true;
	}
	if (JsonObject->TryGetNumberField(TEXT("yaw"), Val))
	{
		OutRot.Yaw = Val;
		bParsedAny = true;
	}
	if (JsonObject->TryGetNumberField(TEXT("roll"), Val))
	{
		OutRot.Roll = Val;
		bParsedAny = true;
	}

	return bParsedAny;
}

bool FJsonUtils::JsonToScale(const TSharedPtr<FJsonObject>& JsonObject, FVector& OutScale)
{
	if (!JsonObject.IsValid())
	{
		return false;
	}

	OutScale = FVector::OneVector; // Default scale is 1,1,1
	bool bParsedAny = false;

	double Val;
	if (JsonObject->TryGetNumberField(TEXT("x"), Val))
	{
		OutScale.X = Val;
		bParsedAny = true;
	}
	if (JsonObject->TryGetNumberField(TEXT("y"), Val))
	{
		OutScale.Y = Val;
		bParsedAny = true;
	}
	if (JsonObject->TryGetNumberField(TEXT("z"), Val))
	{
		OutScale.Z = Val;
		bParsedAny = true;
	}

	return bParsedAny;
}
