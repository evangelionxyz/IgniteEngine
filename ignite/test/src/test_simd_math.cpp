// Copyright (c) 2026 Evangelion Manuhutu

#include <gtest/gtest.h>
#include "ignite/math/frustum.hpp"
#include "ignite/math/aabb.hpp"
#include <glm/gtc/matrix_transform.hpp>

using namespace ignite;

TEST(SIMDMathTest, FrustumAABBVisibility)
{
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 10.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 viewProj = proj * view;

    Frustum frustum(viewProj);

    // AABB at origin should be visible inside frustum
    AABB centerAABB(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(2.0f));
    EXPECT_TRUE(frustum.IsAABBVisible(centerAABB));

    // AABB behind the near plane (behind camera)
    AABB behindAABB(glm::vec3(0.0f, 0.0f, 15.0f), glm::vec3(1.0f));
    EXPECT_FALSE(frustum.IsAABBVisible(behindAABB));

    // AABB far outside to the left
    AABB leftOutsideAABB(glm::vec3(-100.0f, 0.0f, 0.0f), glm::vec3(1.0f));
    EXPECT_FALSE(frustum.IsAABBVisible(leftOutsideAABB));

    // AABB far outside past far plane
    AABB farOutsideAABB(glm::vec3(0.0f, 0.0f, -200.0f), glm::vec3(1.0f));
    EXPECT_FALSE(frustum.IsAABBVisible(farOutsideAABB));
}

TEST(SIMDMathTest, FrustumSphereAndPointVisibility)
{
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 50.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    Frustum frustum(proj * view);

    EXPECT_TRUE(frustum.IsPointVisible(glm::vec3(0.0f, 0.0f, 0.0f)));
    EXPECT_FALSE(frustum.IsPointVisible(glm::vec3(0.0f, 0.0f, 10.0f)));

    EXPECT_TRUE(frustum.IsSphereVisible(glm::vec3(0.0f, 0.0f, 0.0f), 1.0f));
    EXPECT_FALSE(frustum.IsSphereVisible(glm::vec3(50.0f, 50.0f, 50.0f), 1.0f));
}

TEST(SIMDMathTest, FrustumBatchAABBVisibility)
{
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 50.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    Frustum frustum(proj * view);

    AABB boxes[3] = {
        AABB(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f)),
        AABB(glm::vec3(-100.0f, 0.0f, 0.0f), glm::vec3(1.0f)),
        AABB(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(1.0f))
    };
    uint8_t results[3] = { 0 };
    frustum.IsAABBVisibleBatch(boxes, 3, results);

    EXPECT_EQ(results[0], 1);
    EXPECT_EQ(results[1], 0);
    EXPECT_EQ(results[2], 1);
}

TEST(SIMDMathTest, AABBTransform)
{
    AABB box(glm::vec3(0.0f), glm::vec3(2.0f)); // min: (-1,-1,-1), max: (1,1,1)

    // Translation matrix
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 20.0f, 30.0f));
    AABB transformed = box.Transform(translation);

    EXPECT_NEAR(transformed.min.x, 9.0f, 1e-4f);
    EXPECT_NEAR(transformed.min.y, 19.0f, 1e-4f);
    EXPECT_NEAR(transformed.min.z, 29.0f, 1e-4f);

    EXPECT_NEAR(transformed.max.x, 11.0f, 1e-4f);
    EXPECT_NEAR(transformed.max.y, 21.0f, 1e-4f);
    EXPECT_NEAR(transformed.max.z, 31.0f, 1e-4f);
}

TEST(SIMDMathTest, AABBRayIntersection)
{
    AABB box(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(2.0f)); // min: (-1,-1,-1), max: (1,1,1)

    glm::vec3 rayOrigin(0.0f, 0.0f, -5.0f);
    glm::vec3 rayDir(0.0f, 0.0f, 1.0f);

    float t = 0.0f;
    bool hit = box.IntersectRay(rayOrigin, rayDir, t);

    EXPECT_TRUE(hit);
    EXPECT_NEAR(t, 4.0f, 1e-4f);

    // Ray pointing away
    glm::vec3 rayDirAway(0.0f, 0.0f, -1.0f);
    bool hitAway = box.IntersectRay(rayOrigin, rayDirAway, t);
    EXPECT_FALSE(hitAway);
}
