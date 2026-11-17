using Ignite;
using System;

namespace PlatformerDemo;
public class Game : Entity
{
    public override void OnCreate()
    {
        // Called when entity created
        Console.WriteLine("Hello From C#!");
    }

    public override void OnDestroy()
    {
        // Called when entity destroyed
    }

    public override void OnUpdate(float deltaTime)
    {
    }

    public override void OnFixedUpdate()
    {
        // Physics 1/60 loop
    }
}
