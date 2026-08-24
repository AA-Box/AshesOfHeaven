// Copyright Epic Games, Inc. All Rights Reserved.

#include "Performance/AHShaderPipelineSettings.h"

const UAHShaderPipelineSettings* UAHShaderPipelineSettings::Get()
{
	return GetDefault<UAHShaderPipelineSettings>();
}
