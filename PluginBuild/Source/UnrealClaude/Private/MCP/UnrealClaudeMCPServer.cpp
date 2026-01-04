// Copyright Your Name. All Rights Reserved.

#include "UnrealClaudeMCPServer.h"
#include "MCPToolRegistry.h"
#include "UnrealClaudeModule.h"
#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FUnrealClaudeMCPServer::FUnrealClaudeMCPServer()
	: bIsRunning(false)
	, ServerPort(3000)
{
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

	// Get or start the HTTP server module
	FHttpServerModule& HttpServerModule = FHttpServerModule::Get();

	// Start listening on the specified port
	HttpRouter = HttpServerModule.GetHttpRouter(ServerPort);
	if (!HttpRouter.IsValid())
	{
		UE_LOG(LogUnrealClaude, Error, TEXT("Failed to get HTTP router for port %d"), ServerPort);
		return false;
	}

	// Setup routes
	SetupRoutes();

	// Start the listeners
	HttpServerModule.StartAllListeners();

	bIsRunning = true;
	UE_LOG(LogUnrealClaude, Log, TEXT("MCP Server started on http://localhost:%d"), ServerPort);
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

	// Unbind routes
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
	}

	bIsRunning = false;
	UE_LOG(LogUnrealClaude, Log, TEXT("MCP Server stopped"));
}

void FUnrealClaudeMCPServer::SetupRoutes()
{
	if (!HttpRouter.IsValid())
	{
		return;
	}

	// GET /mcp/tools - List all available tools
	ListToolsHandle = HttpRouter->BindRoute(
		FHttpPath(TEXT("/mcp/tools")),
		EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateRaw(this, &FUnrealClaudeMCPServer::HandleListTools)
	);

	// POST /mcp/tool/* - Execute a tool (wildcard path)
	ExecuteToolHandle = HttpRouter->BindRoute(
		FHttpPath(TEXT("/mcp/tool")),
		EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateRaw(this, &FUnrealClaudeMCPServer::HandleExecuteTool)
	);

	// GET /mcp/status - Server status
	StatusHandle = HttpRouter->BindRoute(
		FHttpPath(TEXT("/mcp/status")),
		EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateRaw(this, &FUnrealClaudeMCPServer::HandleStatus)
	);
}

bool FUnrealClaudeMCPServer::HandleListTools(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
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

			// Add parameters schema
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
	// Extract tool name from path: /mcp/tool/{name}
	FString RelativePath = Request.RelativePath.GetPath();

	// Parse tool name from path
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

	// Parse JSON body for parameters
	TSharedPtr<FJsonObject> ParamsJson;
	if (Request.Body.Num() > 0)
	{
		FString BodyString = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Request.Body.GetData())));
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BodyString);
		if (!FJsonSerializer::Deserialize(Reader, ParamsJson) || !ParamsJson.IsValid())
		{
			OnComplete(CreateErrorResponse(TEXT("Invalid JSON body"), EHttpServerResponseCodes::BadRequest));
			return true;
		}
	}
	else
	{
		ParamsJson = MakeShared<FJsonObject>();
	}

	// Execute tool
	if (!ToolRegistry.IsValid())
	{
		OnComplete(CreateErrorResponse(TEXT("Tool registry not initialized"), EHttpServerResponseCodes::ServerError));
		return true;
	}

	FMCPToolResult Result = ToolRegistry->ExecuteTool(ToolName, ParamsJson.ToSharedRef());

	// Build response
	TSharedPtr<FJsonObject> ResponseJson = MakeShared<FJsonObject>();
	ResponseJson->SetBoolField(TEXT("success"), Result.bSuccess);
	ResponseJson->SetStringField(TEXT("message"), Result.Message);

	if (Result.Data.IsValid())
	{
		ResponseJson->SetObjectField(TEXT("data"), Result.Data);
	}

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);

	EHttpServerResponseCodes Code = Result.bSuccess ? EHttpServerResponseCodes::Ok : EHttpServerResponseCodes::BadRequest;
	OnComplete(CreateJsonResponse(JsonString, Code));
	return true;
}

bool FUnrealClaudeMCPServer::HandleStatus(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	TSharedPtr<FJsonObject> ResponseJson = MakeShared<FJsonObject>();

	ResponseJson->SetStringField(TEXT("status"), TEXT("running"));
	ResponseJson->SetNumberField(TEXT("port"), ServerPort);
	ResponseJson->SetStringField(TEXT("version"), TEXT("1.0.0"));
	ResponseJson->SetNumberField(TEXT("toolCount"), ToolRegistry.IsValid() ? ToolRegistry->GetAllTools().Num() : 0);

	// Add project info
	ResponseJson->SetStringField(TEXT("projectName"), FApp::GetProjectName());
	ResponseJson->SetStringField(TEXT("engineVersion"), FEngineVersion::Current().ToString());

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);

	OnComplete(CreateJsonResponse(JsonString));
	return true;
}

TUniquePtr<FHttpServerResponse> FUnrealClaudeMCPServer::CreateJsonResponse(const FString& JsonContent, EHttpServerResponseCodes Code)
{
	TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(JsonContent, TEXT("application/json"));
	Response->Code = Code;

	// Add CORS headers for local development
	Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), { TEXT("*") });
	Response->Headers.Add(TEXT("Access-Control-Allow-Methods"), { TEXT("GET, POST, OPTIONS") });
	Response->Headers.Add(TEXT("Access-Control-Allow-Headers"), { TEXT("Content-Type") });

	return Response;
}

TUniquePtr<FHttpServerResponse> FUnrealClaudeMCPServer::CreateErrorResponse(const FString& Message, EHttpServerResponseCodes Code)
{
	TSharedPtr<FJsonObject> ErrorJson = MakeShared<FJsonObject>();
	ErrorJson->SetBoolField(TEXT("success"), false);
	ErrorJson->SetStringField(TEXT("error"), Message);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(ErrorJson.ToSharedRef(), Writer);

	return CreateJsonResponse(JsonString, Code);
}
