// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HttpServerModule.h"
#include "IHttpRouter.h"

class FMCPToolRegistry;

/**
 * MCP HTTP Server for editor control
 * Provides REST API endpoints for Claude to interact with the Unreal Editor
 */
class UNREALCLAUDE_API FUnrealClaudeMCPServer
{
public:
	FUnrealClaudeMCPServer();
	~FUnrealClaudeMCPServer();

	/** Start the MCP server on the specified port */
	bool Start(uint32 Port = 3000);

	/** Stop the MCP server */
	void Stop();

	/** Check if server is running */
	bool IsRunning() const { return bIsRunning; }

	/** Get the server port */
	uint32 GetPort() const { return ServerPort; }

	/** Get the tool registry */
	TSharedPtr<FMCPToolRegistry> GetToolRegistry() const { return ToolRegistry; }

private:
	/** Setup HTTP routes */
	void SetupRoutes();

	/** Handle GET /mcp/tools - List all available tools */
	bool HandleListTools(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

	/** Handle POST /mcp/tool/{name} - Execute a tool */
	bool HandleExecuteTool(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

	/** Handle GET /mcp/status - Get server status */
	bool HandleStatus(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

	/** Helper to create JSON response */
	TUniquePtr<FHttpServerResponse> CreateJsonResponse(const FString& JsonContent, EHttpServerResponseCodes Code = EHttpServerResponseCodes::Ok);

	/** Helper to create error response */
	TUniquePtr<FHttpServerResponse> CreateErrorResponse(const FString& Message, EHttpServerResponseCodes Code = EHttpServerResponseCodes::BadRequest);

private:
	/** HTTP router handle */
	TSharedPtr<IHttpRouter> HttpRouter;

	/** Route handles for cleanup */
	FHttpRouteHandle ListToolsHandle;
	FHttpRouteHandle ExecuteToolHandle;
	FHttpRouteHandle StatusHandle;

	/** Tool registry */
	TSharedPtr<FMCPToolRegistry> ToolRegistry;

	/** Server state */
	bool bIsRunning;
	uint32 ServerPort;
};
