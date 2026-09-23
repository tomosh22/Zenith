using Sharpmake;
using System;

// Mirrors Build/Sharpmake_GameInstances.generated.cs's shape: a concrete
// generated game shell writes only GameName (expression-bodied).
[Sharpmake.Generate]
public class GameXGameProject : GameProject
{
	public override string GameName => "GameX";
}

[Sharpmake.Generate]
public class GameXGameSolution : GameSolution
{
	public override string GameName => "GameX";
}
