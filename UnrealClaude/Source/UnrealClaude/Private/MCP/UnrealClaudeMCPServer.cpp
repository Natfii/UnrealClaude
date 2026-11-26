// Copyright Natali Caggiano. All Rights Reserved.

#include "UnrealClaudeMCPServer.h"
#include "MCPToolRegistry.h"
#include "MCPResponseFormatter.h"
#include "UnrealClaudeModule.h"
#include "UnrealClaudeConstants.h"
#include "ProjectContext.h"
#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/Guid.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

FUnrealClaudeMCPServer::FUnrealClaudeMCPServer()
	: bIsRunning(false)
	, ServerPort(UnrealClaudeConstants::MCPServer::DefaultPort)
{
	// Generate a fresh bearer token for every editor session.
	// Prefer platform secure RNG (UE exposes GetSecureRandomBytes on supported
	// targets). Fall back to mixed GUID + high-res cycle counter entropy.
	// Token is never committed and is deleted from disk on Stop().
	uint8 RandomBytes[32];
	FMemory::Memzero(RandomBytes, sizeof(RandomBytes));
	FPlatformMisc::GetSecureRandomBytes(RandomBytes, sizeof(RandomBytes));

	bool bAllZero = true;
	for (uint8 Byte : RandomBytes)
	{
		if (Byte != 0)
		{
			bAllZero = false;
			break;
		}
	}

	if (!bAllZero)
	{
		AuthToken.Empty(64);
		for (uint8 Byte : RandomBytes)
		{
			AuthToken += FString::Printf(TEXT("%02x"), Byte);
		}
	}
	else
	{
		AuthToken = FGuid::NewGuid().ToString(EGuidFormats::Digits)
			+ FGuid::NewGuid().ToString(EGuidFormats::Digits)
			+ FString::Printf(TEXT("%016llx"), static_cast<unsigned long long>(FPlatformTime::Cycles64()));
	}
	ToolRegistry = MakeShared<FMCPToolRegistry>();
}

FUnrealClaudeMCPServer::~FUnrealClaudeMCPServer()
{
	Stop();
}

bool FUnrealClaudeMCPServer::Start(uint32 Port)
{
	if (bIsRunning)
	{
		UE_LOG(LogUnrealClaude, Warning, TEXT("MCP Server is already running on port %d"), ServerPort);
		return true;
	}

	ServerPort = Port;

	FHttpServerModule& HttpServerModule = FHttpServerModule::Get();

	HttpRouter = HttpServerModule.GetHttpRouter(ServerPort);
	if (!HttpRouter.IsValid())
	{
		UE_LOG(LogUnrealClaude, Error, TEXT("Failed to get HTTP router for port %d"), ServerPort);
		return false;
	}

	SetupRoutes();

	HttpServerModule.StartAllListeners();

	bIsRunning = true;

	if (ToolRegistry.IsValid())
	{
		ToolRegistry->StartTaskQueue();
	}

	// FHttpServerModule binds to all interfaces on the chosen port. Auth is
	// required on every route, CORS is localhost-only, and the token is
	// session-scoped + deleted on Stop. Prefer firewalling the port if the
	// host is multi-user. Binding exclusively to 127.0.0.1 is not exposed by
	// the stock HTTPServer module API used here.
	UE_LOG(LogUnrealClaude, Log, TEXT("MCP Server started on port %d (auth required; treat as localhost-only)"), ServerPort);
	UE_LOG(LogUnrealClaude, Log, TEXT("  GET  /mcp/tools      - List available tools"));
	UE_LOG(LogUnrealClaude, Log, TEXT("  POST /mcp/tool/{name} - Execute a tool"));
	UE_LOG(LogUnrealClaude, Log, TEXT("  GET  /mcp/status     - Server status"));

	return true;
}

void FUnrealClaudeMCPServer::Stop()
{
	if (!bIsRunning)
	{
		return;
	}

	// Stop the async task queue first
	if (ToolRegistry.IsValid())
	{
		ToolRegistry->StopTaskQueue();
	}

	if (HttpRouter.IsValid())
	{
		if (ListToolsHandle.IsValid())
		{
			HttpRouter->UnbindRoute(ListToolsHandle);
		}
		if (ExecuteToolHandle.IsValid())
		{
			HttpRouter->UnbindRoute(ExecuteToolHandle);
		}
		if (StatusHandle.IsValid())
		{
			HttpRouter->UnbindRoute(StatusHandle);
		}
		if (ProjectContextHandle.IsValid())
		{
			HttpRouter->UnbindRoute(ProjectContextHandle);
		}
	}

	bIsRunning = false;

	// Delete the runtime MCP config so the session token is not left on disk.
	const FString MCPConfigPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealClaude"), TEXT("mcp-config.json"));
	if (IFileManager::Get().FileExists(*MCPConfigPath))
	{
		IFileManager::Get().Delete(*MCPConfigPath);
		UE_LOG(LogUnrealClaude, Log, TEXT("Deleted runtime MCP config: %s"), *MCPConfigPath);
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("MCP Server stopped"));
}

void FUnrealClaudeMCPServer::SetupRoutes()
{
	if (!HttpRouter.IsValid())
	{
		return;
	}

	ListToolsHandle = HttpRouter->BindRoute(
		FHttpPath(TEXT("/mcp/tools")),
		EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateRaw(this, &FUnrealClaudeMCPServer::HandleListTools)
	);

	ExecuteToolHandle = HttpRouter->BindRoute(
		FHttpPath(TEXT("/mcp/tool")),
		EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateRaw(this, &FUnrealClaudeMCPServer::HandleExecuteTool)
	);

	StatusHandle = HttpRouter->BindRoute(
		FHttpPath(TEXT("/mcp/status")),
		EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateRaw(this, &FUnrealClaudeMCPServer::HandleStatus)
	);

	ProjectContextHandle = HttpRouter->BindRoute(
		FHttpPath(TEXT("/mcp/project_context")),
		EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateRaw(this, &FUnrealClaudeMCPServer::HandleProjectContext)
	);
}

bool FUnrealClaudeMCPServer::HandleListTools(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	if (!IsAuthorized(Request))
	{
		OnComplete(CreateErrorResponse(TEXT("Unauthorized"), EHttpServerResponseCodes::Unauthorized));
		return true;
	}

	TSharedPtr<FJsonObject> ResponseJson = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> ToolsArray;

	if (ToolRegistry.IsValid())
	{
		TArray<FMCPToolInfo> Tools = ToolRegistry->GetAllTools();
		for (const FMCPToolInfo& Tool : Tools)
		{
			TSharedPtr<FJsonObject> ToolJson = MakeShared<FJsonObject>();
			ToolJson->SetStringField(TEXT("name"), Tool.Name);
			ToolJson->SetStringField(TEXT("description"), Tool.Description);

			TArray<TSharedPtr<FJsonValue>> ParamsArray;
			for (const FMCPToolParameter& Param : Tool.Parameters)
			{
				TSharedPtr<FJsonObject> ParamJson = MakeShared<FJsonObject>();
				ParamJson->SetStringField(TEXT("name"), Param.Name);
				ParamJson->SetStringField(TEXT("type"), Param.Type);
				ParamJson->SetStringField(TEXT("description"), Param.Description);
				ParamJson->SetBoolField(TEXT("required"), Param.bRequired);
				if (!Param.DefaultValue.IsEmpty())
				{
					ParamJson->SetStringField(TEXT("default"), Param.DefaultValue);
				}
				ParamsArray.Add(MakeShared<FJsonValueObject>(ParamJson));
			}
			ToolJson->SetArrayField(TEXT("parameters"), ParamsArray);

			TSharedPtr<FJsonObject> AnnotationsJson = MakeShared<FJsonObject>();
			AnnotationsJson->SetBoolField(TEXT("readOnlyHint"), Tool.Annotations.bReadOnlyHint);
			AnnotationsJson->SetBoolField(TEXT("destructiveHint"), Tool.Annotations.bDestructiveHint);
			AnnotationsJson->SetBoolField(TEXT("idempotentHint"), Tool.Annotations.bIdempotentHint);
			AnnotationsJson->SetBoolField(TEXT("openWorldHint"), Tool.Annotations.bOpenWorldHint);
			ToolJson->SetObjectField(TEXT("annotations"), AnnotationsJson);

			ToolsArray.Add(MakeShared<FJsonValueObject>(ToolJson));
		}
	}

	ResponseJson->SetArrayField(TEXT("tools"), ToolsArray);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);

	OnComplete(CreateJsonResponse(JsonString));
	return true;
}

bool FUnrealClaudeMCPServer::HandleExecuteTool(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	if (!IsAuthorized(Request))
	{
		OnComplete(CreateErrorResponse(TEXT("Unauthorized"), EHttpServerResponseCodes::Unauthorized));
		return true;
	}

	FString RelativePath = Request.RelativePath.GetPath();

	FString ToolName;
	if (RelativePath.StartsWith(TEXT("/mcp/tool/")))
	{
		ToolName = RelativePath.RightChop(10); // Remove "/mcp/tool/"
	}
	else if (RelativePath.StartsWith(TEXT("/")))
	{
		ToolName = RelativePath.RightChop(1);
	}
	else
	{
		ToolName = RelativePath;
	}

	if (ToolName.IsEmpty())
	{
		OnComplete(CreateErrorResponse(TEXT("Tool name not specified. Use POST /mcp/tool/{toolname}"), EHttpServerResponseCodes::BadRequest));
		return true;
	}

	TSharedPtr<FJsonObject> ParamsJson;
	if (Request.Body.Num() > UnrealClaudeConstants::MCPServer::MaxRequestBodySize)
	{
		UE_LOG(LogUnrealClaude, Warning, TEXT("Request body too large: %d bytes (max %d)"), Request.Body.Num(), UnrealClaudeConstants::MCPServer::MaxRequestBodySize);
		OnComplete(CreateErrorResponse(TEXT("Request body too large"), EHttpServerResponseCodes::BadRequest));
		return true;
	}
	if (Request.Body.Num() > 0)
	{
		// Ensure null-termination for safe string conversion
		TArray<uint8> NullTerminatedBody = Request.Body;
		NullTerminatedBody.Add(0);
		FString BodyString = UTF8_TO_TCHAR(reinterpret_cast<const char*>(NullTerminatedBody.GetData()));

		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BodyString);
		if (!FJsonSerializer::Deserialize(Reader, ParamsJson) || !ParamsJson.IsValid())
		{
			// Do not echo arbitrary request bodies into the editor log. Requests may
			// contain secrets or large attacker-controlled payloads.
			UE_LOG(LogUnrealClaude, Warning, TEXT("Failed to parse JSON body (%d bytes)"), Request.Body.Num());
			OnComplete(CreateErrorResponse(TEXT("Invalid JSON body"), EHttpServerResponseCodes::BadRequest));
			return true;
		}
	}
	else
	{
		ParamsJson = MakeShared<FJsonObject>();
	}

	if (!ToolRegistry.IsValid())
	{
		OnComplete(CreateErrorResponse(TEXT("Tool registry not initialized"), EHttpServerResponseCodes::ServerError));
		return true;
	}

	FMCPToolResult Result = ToolRegistry->ExecuteTool(ToolName, ParamsJson.ToSharedRef());

	TSharedPtr<FJsonObject> ResponseJson = UnrealClaude::MCP::BuildToolResultJson(Result);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);

	if (static_cast<int64>(JsonString.Len()) * sizeof(TCHAR) > UnrealClaudeConstants::MCPServer::MaxResponseSize)
	{
		UE_LOG(LogUnrealClaude, Warning, TEXT("MCP response too large: %d characters (%lld bytes, max %d bytes)"),
			JsonString.Len(), static_cast<int64>(JsonString.Len()) * sizeof(TCHAR), UnrealClaudeConstants::MCPServer::MaxResponseSize);
		OnComplete(CreateErrorResponse(TEXT("MCP response exceeded the maximum response size"), EHttpServerResponseCodes::InternalServerError));
		return true;
	}

	EHttpServerResponseCodes Code = Result.bSuccess ? EHttpServerResponseCodes::Ok : EHttpServerResponseCodes::BadRequest;
	OnComplete(CreateJsonResponse(JsonString, Code));
	return true;
}

bool FUnrealClaudeMCPServer::HandleStatus(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	if (!IsAuthorized(Request))
	{
		OnComplete(CreateErrorResponse(TEXT("Unauthorized"), EHttpServerResponseCodes::Unauthorized));
		return true;
	}

	TSharedPtr<FJsonObject> ResponseJson = MakeShared<FJsonObject>();

	ResponseJson->SetStringField(TEXT("status"), TEXT("running"));
	ResponseJson->SetNumberField(TEXT("port"), ServerPort);
	ResponseJson->SetStringField(TEXT("version"), TEXT("1.5.1"));
	ResponseJson->SetNumberField(TEXT("toolCount"), ToolRegistry.IsValid() ? ToolRegistry->GetAllTools().Num() : 0);

	if (ToolRegistry.IsValid())
	{
		TArray<TSharedPtr<FJsonValue>> ToolsArray;
		for (const FMCPToolInfo& ToolInfo : ToolRegistry->GetAllTools())
		{
			TSharedPtr<FJsonObject> ToolObj = MakeShared<FJsonObject>();
			ToolObj->SetStringField(TEXT("name"), ToolInfo.Name);
			ToolObj->SetStringField(TEXT("description"), ToolInfo.Description);
			ToolsArray.Add(MakeShared<FJsonValueObject>(ToolObj));
		}
		ResponseJson->SetArrayField(TEXT("tools"), ToolsArray);
	}

	ResponseJson->SetStringField(TEXT("projectName"), FApp::GetProjectName());
	ResponseJson->SetStringField(TEXT("engineVersion"), FEngineVersion::Current().ToString());

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);

	OnComplete(CreateJsonResponse(JsonString));
	return true;
}

bool FUnrealClaudeMCPServer::HandleProjectContext(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	if (!IsAuthorized(Request))
	{
		OnComplete(CreateErrorResponse(TEXT("Unauthorized"), EHttpServerResponseCodes::Unauthorized));
		return true;
	}

	FProjectContextManager& ContextMgr = FProjectContextManager::Get();
	ContextMgr.GetContext(); // ensure fresh context is available

	const FString ContextText = ContextMgr.FormatContextForPrompt();

	TSharedPtr<FJsonObject> ResponseJson = MakeShared<FJsonObject>();
	ResponseJson->SetStringField(TEXT("context"), ContextText);
	ResponseJson->SetStringField(TEXT("summary"), ContextMgr.GetContextSummary());
	ResponseJson->SetBoolField(TEXT("success"), true);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);

	OnComplete(CreateJsonResponse(JsonString));
	return true;
}

bool FUnrealClaudeMCPServer::IsAuthorized(const FHttpServerRequest& Request) const
{
	// HTTP headers are case-insensitive per RFC 7230. Node's fetch often sends
	// "authorization" (lowercase). Scan the map case-insensitively.
	const TArray<FString>* AuthorizationValues = nullptr;
	for (const auto& HeaderPair : Request.Headers)
	{
		if (HeaderPair.Key.Equals(TEXT("Authorization"), ESearchCase::IgnoreCase))
		{
			AuthorizationValues = &HeaderPair.Value;
			break;
		}
	}
	if (!AuthorizationValues)
	{
		return false;
	}

	const FString Expected = FString::Printf(TEXT("Bearer %s"), *AuthToken);
	bool bMatched = false;
	for (const FString& AuthorizationValue : *AuthorizationValues)
	{
		// Constant-time compare over the full expected length to avoid timing leaks
		// on prefix mismatches. Always walks Max(len(a), len(b)) characters.
		const int32 LenA = AuthorizationValue.Len();
		const int32 LenB = Expected.Len();
		const int32 MaxLen = FMath::Max(LenA, LenB);
		uint8 Diff = static_cast<uint8>(LenA != LenB ? 1 : 0);
		for (int32 i = 0; i < MaxLen; ++i)
		{
			const TCHAR CA = i < LenA ? AuthorizationValue[i] : 0;
			const TCHAR CB = i < LenB ? Expected[i] : 0;
			Diff |= static_cast<uint8>(CA != CB ? 1 : 0);
		}
		if (Diff == 0)
		{
			bMatched = true;
			// Do not break early — keep scanning remaining values with the same work.
		}
	}

	return bMatched;
}

TUniquePtr<FHttpServerResponse> FUnrealClaudeMCPServer::CreateJsonResponse(const FString& JsonContent, EHttpServerResponseCodes Code)
{
	TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(JsonContent, TEXT("application/json"));
	Response->Code = Code;

	// Add CORS headers - restricted to localhost for security
	Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), { TEXT("http://localhost") });
	Response->Headers.Add(TEXT("Access-Control-Allow-Methods"), { TEXT("GET, POST, OPTIONS") });
	Response->Headers.Add(TEXT("Access-Control-Allow-Headers"), { TEXT("Content-Type, Authorization") });

	return Response;
}

TUniquePtr<FHttpServerResponse> FUnrealClaudeMCPServer::CreateErrorResponse(const FString& Message, EHttpServerResponseCodes Code)
{
	TSharedPtr<FJsonObject> ErrorJson = UnrealClaude::MCP::BuildErrorEnvelopeJson(Message);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(ErrorJson.ToSharedRef(), Writer);

	return CreateJsonResponse(JsonString, Code);
}
