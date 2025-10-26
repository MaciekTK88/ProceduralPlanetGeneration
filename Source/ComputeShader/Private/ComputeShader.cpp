// Copyright Epic Games, Inc. All Rights Reserved.

#include "ComputeShader.h"

#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "RHI.h"
#include "GlobalShader.h"
#include "RHICommandList.h"
#include "RenderGraphBuilder.h"
#include "RenderTargetPool.h"
#include "Runtime/Core/Public/Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FComputeShader"

void FComputeShader::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	const FString ShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("PPG"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/ComputeShaderShaders"), ShaderDir);

#if WITH_EDITOR
	GAllowActorScriptExecutionInEditor = true;
#endif
}

void FComputeShader::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FComputeShader, ComputeShader)