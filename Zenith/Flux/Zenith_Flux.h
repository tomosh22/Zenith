#pragma once

enum CommandType {
	COMMANDTYPE_GRAPHICS,
	COMMANDTYPE_COMPUTE,
	COMMANDTYPE_COPY,
	COMMANDTYPE_PRESENT,
	COMMANDTYPE_MAX
};

class Zenith_Flux
{
public:
	static void Initialise();
};