// Copyright Natali Caggiano. All Rights Reserved.

#include "AnimAssetNodeFactory.h"
#include "AnimGraphEditor.h"
#include "AnimNodePinUtils.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_StateResult.h"
#include "AnimGraphNode_TransitionResult.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"

UEdGraphNode* FAnimAssetNodeFactory::CreateAnimSequenceNode(
	UEdGraph* StateGraph,
	UAnimSequence* AnimSequence,
	FVector2D Position,
	FString& OutNodeId,
	FString& OutError)
{
	if (!StateGraph)
	{
		OutError = TEXT("Invalid state graph");
		return nullptr;
	}

	if (!AnimSequence)
	{
		OutError = TEXT("Invalid animation sequence");
		return nullptr;
	}

	// Create sequence player node
	FGraphNodeCreator<UAnimGraphNode_SequencePlayer> NodeCreator(*StateGraph);
	UAnimGraphNode_SequencePlayer* SeqNode = NodeCreator.CreateNode();

	if (!SeqNode)
	{
		OutError = TEXT("Failed to create sequence player node");
		return nullptr;
	}

	SeqNode->NodePosX = static_cast<int32>(Position.X);
	SeqNode->NodePosY = static_cast<int32>(Position.Y);

	// Set the animation sequence
	SeqNode->Node.SetSequence(AnimSequence);

	NodeCreator.Finalize();

	// Generate ID
	OutNodeId = FAnimGraphEditor::GenerateAnimNodeId(TEXT("Anim"), AnimSequence->GetName(), StateGraph);
	FAnimGraphEditor::SetNodeId(SeqNode, OutNodeId);

	StateGraph->Modify();

	return SeqNode;
}

UEdGraphNode* FAnimAssetNodeFactory::CreateBlendSpaceNode(
	UEdGraph* StateGraph,
	UBlendSpace* BlendSpace,
	FVector2D Position,
	FString& OutNodeId,
	FString& OutError)
{
	if (!StateGraph)
	{
		OutError = TEXT("Invalid state graph");
		return nullptr;
	}

	if (!BlendSpace)
	{
		OutError = TEXT("Invalid BlendSpace");
		return nullptr;
	}

	// Create BlendSpace player node
	FGraphNodeCreator<UAnimGraphNode_BlendSpacePlayer> NodeCreator(*StateGraph);
	UAnimGraphNode_BlendSpacePlayer* BSNode = NodeCreator.CreateNode();

	if (!BSNode)
	{
		OutError = TEXT("Failed to create BlendSpace player node");
		return nullptr;
	}

	BSNode->NodePosX = static_cast<int32>(Position.X);
	BSNode->NodePosY = static_cast<int32>(Position.Y);

	// Set the BlendSpace
	BSNode->Node.SetBlendSpace(BlendSpace);

	NodeCreator.Finalize();

	// Generate ID
	OutNodeId = FAnimGraphEditor::GenerateAnimNodeId(TEXT("BlendSpace"), BlendSpace->GetName(), StateGraph);
	FAnimGraphEditor::SetNodeId(BSNode, OutNodeId);

	StateGraph->Modify();

	return BSNode;
}

UEdGraphNode* FAnimAssetNodeFactory::CreateBlendSpace1DNode(
	UEdGraph* StateGraph,
	UBlendSpace1D* BlendSpace,
	FVector2D Position,
	FString& OutNodeId,
	FString& OutError)
{
	if (!StateGraph)
	{
		OutError = TEXT("Invalid state graph");
		return nullptr;
	}

	if (!BlendSpace)
	{
		OutError = TEXT("Invalid BlendSpace1D");
		return nullptr;
	}

	// BlendSpace1D uses the same player node
	FGraphNodeCreator<UAnimGraphNode_BlendSpacePlayer> NodeCreator(*StateGraph);
	UAnimGraphNode_BlendSpacePlayer* BSNode = NodeCreator.CreateNode();

	if (!BSNode)
	{
		OutError = TEXT("Failed to create BlendSpace1D player node");
		return nullptr;
	}

	BSNode->NodePosX = static_cast<int32>(Position.X);
	BSNode->NodePosY = static_cast<int32>(Position.Y);

	// Set the BlendSpace
	BSNode->Node.SetBlendSpace(BlendSpace);

	NodeCreator.Finalize();

	// Generate ID
	OutNodeId = FAnimGraphEditor::GenerateAnimNodeId(TEXT("BlendSpace1D"), BlendSpace->GetName(), StateGraph);
	FAnimGraphEditor::SetNodeId(BSNode, OutNodeId);

	StateGraph->Modify();

	return BSNode;
}

bool FAnimAssetNodeFactory::ConnectToOutputPose(
	UEdGraph* StateGraph,
	const FString& AnimNodeId,
	FString& OutError)
{
	if (!StateGraph)
	{
		OutError = TEXT("Invalid state graph");
		return false;
	}

	// Find anim node
	UEdGraphNode* AnimNode = FAnimGraphEditor::FindNodeById(StateGraph, AnimNodeId);
	if (!AnimNode)
	{
		OutError = FString::Printf(TEXT("Animation node not found: %s"), *AnimNodeId);
		return false;
	}

	// Find pose output pin on anim node
	auto PoseOutputConfig = FPinSearchConfig::Output({
		FName("Pose"),
		FName("Output"),
		FName("Output Pose")
	}).WithCategory(UEdGraphSchema_K2::PC_Struct).WithNameContains(TEXT("Pose"));

	UEdGraphPin* PosePin = FAnimNodePinUtils::FindPinWithFallbacks(AnimNode, PoseOutputConfig, &OutError);
	if (!PosePin)
	{
		return false;
	}

	// Find result node
	UEdGraphNode* ResultNode = FAnimNodePinUtils::FindResultNode(StateGraph);
	if (!ResultNode)
	{
		OutError = TEXT("State result node not found");
		return false;
	}

	// Find result input pin
	auto ResultConfig = FPinSearchConfig::Input({
		FName("Result"),
		FName("Pose"),
		FName("Output Pose"),
		FName("InPose")
	}).AcceptAny();

	UEdGraphPin* ResultPin = FAnimNodePinUtils::FindPinWithFallbacks(ResultNode, ResultConfig, &OutError);
	if (!ResultPin)
	{
		return false;
	}

	// Make connection
	PosePin->MakeLinkTo(ResultPin);
	StateGraph->Modify();

	return true;
}

bool FAnimAssetNodeFactory::ClearStateGraph(UEdGraph* StateGraph, FString& OutError)
{
	if (!StateGraph)
	{
		OutError = TEXT("Invalid state graph");
		return false;
	}

	// Collect nodes to remove (exclude result node)
	TArray<UEdGraphNode*> NodesToRemove;
	for (UEdGraphNode* Node : StateGraph->Nodes)
	{
		// Keep result nodes
		if (Node->IsA<UAnimGraphNode_StateResult>() ||
			Node->IsA<UAnimGraphNode_TransitionResult>())
		{
			continue;
		}

		NodesToRemove.Add(Node);
	}

	// Remove nodes
	for (UEdGraphNode* Node : NodesToRemove)
	{
		Node->BreakAllNodeLinks();
		StateGraph->RemoveNode(Node);
	}

	StateGraph->Modify();

	return true;
}
