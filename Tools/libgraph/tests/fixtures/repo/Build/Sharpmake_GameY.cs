using Sharpmake;
using System;

[Sharpmake.Generate]
public class GameYGameProject : GameProject
{
	public override string GameName => "GameY";
}

[Sharpmake.Generate]
public class GameYGameSolution : GameSolution
{
	public override string GameName => "GameY";
}
