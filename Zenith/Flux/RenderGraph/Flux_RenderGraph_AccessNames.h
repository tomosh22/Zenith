#pragma once

#include "Flux/Flux_Enums.h"

// ============================================================================
// ResourceAccess -> diagnostic name.
//
// Both Flux_RenderGraph.cpp (validation / pass-order dumps) and
// Flux_RenderGraph_Compilation.cpp (barrier + producer-order diagnostics) log
// access modes, so each carried its own file-static copy of this table --
// deliberately, "to avoid introducing a shared internal header for a 10-line
// function". Two verbatim copies of an enum table is precisely the drift the
// duplicate gate exists to catch: adding a ResourceAccess value to
// Flux_Enums.h and updating only one copy makes the other silently print
// "???". One table, one place to update.
// ============================================================================
inline const char* Flux_AccessToString(ResourceAccess eAccess)
{
	switch (eAccess)
	{
		case RESOURCE_ACCESS_UNDEFINED:           return "UNDEFINED";
		case RESOURCE_ACCESS_READ_SRV:            return "READ_SRV";
		case RESOURCE_ACCESS_READ_DEPTH:          return "READ_DEPTH";
		case RESOURCE_ACCESS_WRITE_RTV:           return "WRITE_RTV";
		case RESOURCE_ACCESS_WRITE_DSV:           return "WRITE_DSV";
		case RESOURCE_ACCESS_WRITE_UAV:           return "WRITE_UAV";
		case RESOURCE_ACCESS_READWRITE_UAV:       return "READWRITE_UAV";
		case RESOURCE_ACCESS_READ_INDIRECT_ARG:   return "READ_INDIRECT_ARG";
		case RESOURCE_ACCESS_READ_BUFFER_SRV:     return "READ_BUFFER_SRV";
		case RESOURCE_ACCESS_READ_VERTEX_BUFFER:  return "READ_VERTEX_BUFFER";
		case RESOURCE_ACCESS_HOST_TRANSFER_WRITE: return "HOST_TRANSFER_WRITE";
	}
	return "???";
}
