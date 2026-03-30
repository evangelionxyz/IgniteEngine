using Ignite;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;

namespace TestProject;

public class Player : Entity
{
    public Transform? tr;
    public Rigidbody2D? rb;

    public Random rng = new Random();

    public Entity? camera;
    public Entity? circle;
    
    public float red = 0.0f;
    public float green = 0.0f;
    public float blue = 0.0f;
    public float spawnInterval = 1.0f;
    public float spawnTimer = 1.0f;
    public int spawnCount = 100;

    private List<Entity> _pendingDeleteEntity = new();

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
            // rb.LinearVelocity = new Vector2(rb.LinearVelocity.X + deltaTime * 10.0f, rb.LinearVelocity.Y);
            if (Input.IsKeyPressed(KeyCode.Space))
            {
                rb.ApplyForce(new Vector2(0.0f, 80.0f), new Vector2(0.0f), true);
            }
            
            float speed = 2.0f;
            float dir = 0.0f;
            if (Input.IsKeyPressed(KeyCode.A))
                dir = -1.0f;
            else if (Input.IsKeyPressed(KeyCode.D))
                dir = 1.0f;

            rb.LinearVelocity = new Vector2(rb.LinearVelocity.X + (dir * speed), rb.LinearVelocity.Y);
        }

        {
            if (circle != null)
            {
                if (camera != null)
                {
                    camera.Translation = new Vector3(circle.Translation.X, circle.Translation.Y, camera.Translation.Z);
                }

                if (spawnTimer <= 0.0f)
                {
                    Entity e = Instantiate(this.circle);
                    if (e != null && e.ID != 0)
                        _pendingDeleteEntity.Add(e);

                    var rigidbody = e!.GetComponent<Rigidbody2D>();
                    rigidbody.ApplyForce(new Vector2(0.0f, 1500.0f), new Vector2(0.0f), true);

                    var circle = e.GetComponent<Circle2D>();

                    float r = rng.NextSingle();
                    float g = rng.NextSingle();
                    float b = rng.NextSingle();
                    var c = new Vector4(r, g, b, 1.0f);
                    if (circle != null)
                        circle.Color = c;

                    spawnTimer = spawnInterval;
                }

                if (_pendingDeleteEntity.Count >= spawnCount)
                {
                    Entity e = _pendingDeleteEntity[0];
                    _pendingDeleteEntity.RemoveAt(0);
                    if (e != null && e.ID != 0)
                        Destroy(e);
                }
            }

            spawnTimer -= deltaTime;
        }
    }
}