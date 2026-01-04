// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

/**
 * Utility class for common JSON operations
 * Reduces code duplication across the plugin
 */
class FJsonUtils
{
public:
	/**
	 * Serialize a JSON object to a string
	 * @param JsonObject - The JSON object to serialize
	 * @param bPrettyPrint - Whether to format the output with indentation
	 * @return The serialized JSON string, or empty string on failure
	 */
	static FString Stringify(const TSharedPtr<FJsonObject>& JsonObject, bool bPrettyPrint = false);

	/**
	 * Serialize a JSON object reference to a string
	 */
	static FString Stringify(const TSharedRef<FJsonObject>& JsonObject, bool bPrettyPrint = false);

	/**
	 * Parse a JSON string into a JSON object
	 * @param JsonString - The JSON string to parse
	 * @return The parsed JSON object, or nullptr on failure
	 */
	static TSharedPtr<FJsonObject> Parse(const FString& JsonString);

	/**
	 * Create a success response JSON object
	 * @param Message - The success message
	 * @param Data - Optional additional data object
	 * @return The response JSON object
	 */
	static TSharedPtr<FJsonObject> CreateSuccessResponse(const FString& Message, TSharedPtr<FJsonObject> Data = nullptr);

	/**
	 * Create an error response JSON object
	 * @param ErrorMessage - The error message
	 * @return The response JSON object
	 */
	static TSharedPtr<FJsonObject> CreateErrorResponse(const FString& ErrorMessage);

	/**
	 * Safely get a string field from a JSON object
	 * @param JsonObject - The JSON object
	 * @param FieldName - The field name to retrieve
	 * @param OutValue - Output string value
	 * @return true if the field exists and is a string
	 */
	static bool GetStringField(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, FString& OutValue);
	static bool GetStringField(const TSharedRef<FJsonObject>& JsonObject, const FString& FieldName, FString& OutValue);

	/**
	 * Safely get a number field from a JSON object
	 * @param JsonObject - The JSON object
	 * @param FieldName - The field name to retrieve
	 * @param OutValue - Output numeric value
	 * @return true if the field exists and is a number
	 */
	static bool GetNumberField(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, double& OutValue);
	static bool GetNumberField(const TSharedRef<FJsonObject>& JsonObject, const FString& FieldName, double& OutValue);

	/**
	 * Safely get a boolean field from a JSON object
	 * @param JsonObject - The JSON object
	 * @param FieldName - The field name to retrieve
	 * @param OutValue - Output boolean value
	 * @return true if the field exists and is a boolean
	 */
	static bool GetBoolField(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, bool& OutValue);
	static bool GetBoolField(const TSharedRef<FJsonObject>& JsonObject, const FString& FieldName, bool& OutValue);

	/**
	 * Safely get an array field from a JSON object
	 * @param JsonObject - The JSON object
	 * @param FieldName - The field name to retrieve
	 * @param OutArray - Output array
	 * @return true if the field exists and is an array
	 */
	static bool GetArrayField(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<TSharedPtr<FJsonValue>>& OutArray);
	static bool GetArrayField(const TSharedRef<FJsonObject>& JsonObject, const FString& FieldName, TArray<TSharedPtr<FJsonValue>>& OutArray);

	/**
	 * Create a JSON array from an array of strings
	 * @param Strings - The string array
	 * @return Array of JSON values
	 */
	static TArray<TSharedPtr<FJsonValue>> StringArrayToJson(const TArray<FString>& Strings);

	/**
	 * Convert a JSON array to an array of strings
	 * @param JsonArray - The JSON array
	 * @return Array of strings
	 */
	static TArray<FString> JsonArrayToStrings(const TArray<TSharedPtr<FJsonValue>>& JsonArray);

	// ===== Geometry Conversion Helpers =====

	/**
	 * Convert a FVector to a JSON object with x, y, z fields
	 * @param Vec - The vector to convert
	 * @return JSON object representation
	 */
	static TSharedPtr<FJsonObject> VectorToJson(const FVector& Vec);

	/**
	 * Convert a FRotator to a JSON object with pitch, yaw, roll fields
	 * @param Rot - The rotator to convert
	 * @return JSON object representation
	 */
	static TSharedPtr<FJsonObject> RotatorToJson(const FRotator& Rot);

	/**
	 * Convert a FVector (used as scale) to a JSON object with x, y, z fields
	 * @param Scale - The scale vector to convert
	 * @return JSON object representation
	 */
	static TSharedPtr<FJsonObject> ScaleToJson(const FVector& Scale);

	/**
	 * Parse a JSON object to FVector
	 * @param JsonObject - The JSON object with x, y, z fields
	 * @param OutVec - Output vector
	 * @return true if successfully parsed at least one component
	 */
	static bool JsonToVector(const TSharedPtr<FJsonObject>& JsonObject, FVector& OutVec);

	/**
	 * Parse a JSON object to FRotator
	 * @param JsonObject - The JSON object with pitch, yaw, roll fields
	 * @param OutRot - Output rotator
	 * @return true if successfully parsed at least one component
	 */
	static bool JsonToRotator(const TSharedPtr<FJsonObject>& JsonObject, FRotator& OutRot);

	/**
	 * Parse a JSON object to FVector for scale (with default of 1.0)
	 * @param JsonObject - The JSON object with x, y, z fields
	 * @param OutScale - Output scale vector (defaults to 1,1,1)
	 * @return true if successfully parsed at least one component
	 */
	static bool JsonToScale(const TSharedPtr<FJsonObject>& JsonObject, FVector& OutScale);
};
