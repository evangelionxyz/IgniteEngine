using System;
using Ignite;

namespace TestProject;

class Circle : Entity
{
    public Transform? tr;
    public Circle2D? circle;
    public CircleCollider2D? cc;
    public Random rng = new Random();
    public float timer = 0.0f;

    public override void OnCreate()
    {
        tr = GetComponent<Transform>();
        Console.WriteLine("TEST");

        circle = GetComponent<Circle2D>();
        if (circle == null)
            Console.WriteLine("Failed to get circle component");
        else
            circle.Color = new Vector4(1.0f, 0.0f, 1.0f, 1.0f);

        cc = GetComponent<CircleCollider2D>();
        if (cc == null)
            Console.WriteLine("Failed to get circle collider component");
        else
            cc.Radius = 0.5f;
    }

    public override void OnUpdate(float deltaTime)
    {
        if (tr != null)
        {
            const float radius = 2.5f;
            const float speed = 1.0f;
            timer += deltaTime * speed;

            float x = (float)Math.Cos(timer) * radius;
            float y = (float)Math.Sin(timer) * radius;

            tr.Translation = new Vector3(x, y, tr.Translation.Z);

            // generate a random color each frame and apply to circle and sprite if available
            float r = rng.NextSingle();
            float g = rng.NextSingle();
            float b = rng.NextSingle();
            var c = new Vector4(r, g, b, 1.0f);
            if (circle != null)
                circle.Color = c;
        }
    }
}
