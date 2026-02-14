// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UEdGraph;
class UEdGraphNode;
class UAnimSequence;
class UBlendSpace;
class UBlendSpace1D;

/**
 * AnimAssetNodeFactory - Factory for creating animation asset player nodes
 *
 * Responsibilities:
 * - Creating SequencePlayer nodes
 * - Creating BlendSpacePlayer nodes (2D and 1D)
 * - Connecting animation nodes to output poses
 * - Clearing state graphs
 */
class FAnimAssetNodeFactory
{
public:
	/**
	 * Create an animation sequence player node
	 * @param StateGraph State graph to add node to
	 * @param AnimSequence Animation sequence asset
	 * @param Position Node position
	 * @param OutNodeId Generated node ID
	 * @param OutError Error message if failed
	 * @return Created node or nullptr
	 */
	static UEdGraphNode* CreateAnimSequenceNode(
		UEdGraph* StateGraph,
		UAnimSequence* AnimSequence,
		FVector2D Position,
		FString& OutNodeId,
		FString& OutError
	);

	/**
	 * Create a BlendSpace player node (2D)
	 * @param StateGraph State graph to add node to
	 * @param BlendSpace BlendSpace asset
	 * @param Position Node position
	 * @param OutNodeId Generated node ID
	 * @param OutError Error message if failed
	 * @return Created node or nullptr
	 */
	static UEdGraphNode* CreateBlendSpaceNode(
		UEdGraph* StateGraph,
		UBlendSpace* BlendSpace,
		FVector2D Position,
		FString& OutNodeId,
		FString& OutError
	);

	/**
	 * Create a BlendSpace1D player node
	 * @param StateGraph State graph to add node to
	 * @param BlendSpace BlendSpace1D asset
	 * @param Position Node position
	 * @param OutNodeId Generated node ID
	 * @param OutError Error message if failed
	 * @return Created node or nullptr
	 */
	static UEdGraphNode* CreateBlendSpace1DNode(
		UEdGraph* StateGraph,
		UBlendSpace1D* BlendSpace,
		FVector2D Position,
		FString& OutNodeId,
		FString& OutError
	);

	/**
	 * Connect animation node to state output pose
	 * @param StateGraph State graph
	 * @param AnimNodeId Animation node ID
	 * @param OutError Error message if failed
	 * @return True if successful
	 */
	static bool ConnectToOutputPose(
		UEdGraph* StateGraph,
		const FString& AnimNodeId,
		FString& OutError
	);

	/**
	 * Clear all nodes from state graph (except result node)
	 * @param StateGraph State graph to clear
	 * @param OutError Error message if failed
	 * @return True if successful
	 */
	static bool ClearStateGraph(UEdGraph* StateGraph, FString& OutError);
};
