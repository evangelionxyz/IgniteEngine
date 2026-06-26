#define MAX_BONES 100

struct Skeleton
{
    float4x4 boneTransforms[MAX_BONES];
};
