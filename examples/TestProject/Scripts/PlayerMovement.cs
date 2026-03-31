using Ignite;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;

namespace TestProject;

public class PlayerMovement : Entity
{
    public Transform? tr;
    public Rigidbody2D? rb;

    public Entity? camera;
    
    public override void OnCreate()
    {
        rb = GetComponent<Rigidbody2D>();
        if (rb == null)
        {
            Console.WriteLine("Failed to get rigidbody2d");
        }
    }

    public override void OnUpdate(float deltaTime)
    {
        if (rb == null)
            return;

        {
            if (Input.IsKeyPressed(KeyCode.Space))
            {
                rb.ApplyForce(new Vector2(0.0f, 120.0f), new Vector2(0.0f), true);
            }
            
            float speed = 2.0f;
            float dir = 0.0f;
            if (Input.IsKeyPressed(KeyCode.A))
                dir = -1.0f;
            else if (Input.IsKeyPressed(KeyCode.D))
                dir = 1.0f;

            rb.LinearVelocity = new Vector2(rb.LinearVelocity.X + (dir * speed), rb.LinearVelocity.Y);
        }
    }
}