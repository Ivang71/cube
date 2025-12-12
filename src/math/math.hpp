#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <utility>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

namespace cube::math {

using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;
using Mat4 = glm::mat4;

inline Mat4 mul(const Mat4& a, const Mat4& b) { return a * b; }

inline Mat4 perspective_vk(float fovy_rad, float aspect, float z_near, float z_far) {
    Mat4 m = glm::perspectiveRH_ZO(fovy_rad, aspect, z_near, z_far);
    m[1][1] *= -1.0f;
    return m;
}

inline Mat4 look_at(const Vec3& eye, const Vec3& center, const Vec3& up) {
    return glm::lookAtRH(eye, center, up);
}

struct Aabb {
    Vec3 min{0.0f};
    Vec3 max{0.0f};
};

inline bool intersects(const Aabb& a, const Aabb& b) {
    return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
           (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
           (a.min.z <= b.max.z && a.max.z >= b.min.z);
}

struct Ray {
    Vec3 origin{0.0f};
    Vec3 dir{0.0f, 0.0f, -1.0f};
};

inline bool ray_aabb(const Ray& r, const Aabb& b, float& t_hit) {
    float tmin = 0.0f;
    float tmax = 1e30f;
    for (int i = 0; i < 3; ++i) {
        const float o = r.origin[i];
        const float d = r.dir[i];
        if (glm::abs(d) < 1e-8f) {
            if (o < b.min[i] || o > b.max[i]) return false;
            continue;
        }
        const float inv_d = 1.0f / d;
        float t0 = (b.min[i] - o) * inv_d;
        float t1 = (b.max[i] - o) * inv_d;
        if (t0 > t1) std::swap(t0, t1);
        tmin = glm::max(tmin, t0);
        tmax = glm::min(tmax, t1);
        if (tmin > tmax) return false;
    }
    t_hit = tmin;
    return true;
}

struct Frustum {
    Vec4 planes[6]{};
};

inline Vec4 normalize_plane(const Vec4& p) {
    const float len = glm::length(Vec3(p));
    if (len <= 0.0f) return p;
    return p / len;
}

inline Frustum extract_frustum(const Mat4& clip) {
    const Vec4 r0{clip[0][0], clip[1][0], clip[2][0], clip[3][0]};
    const Vec4 r1{clip[0][1], clip[1][1], clip[2][1], clip[3][1]};
    const Vec4 r2{clip[0][2], clip[1][2], clip[2][2], clip[3][2]};
    const Vec4 r3{clip[0][3], clip[1][3], clip[2][3], clip[3][3]};

    Frustum f{};
    f.planes[0] = normalize_plane(r3 + r0);
    f.planes[1] = normalize_plane(r3 - r0);
    f.planes[2] = normalize_plane(r3 + r1);
    f.planes[3] = normalize_plane(r3 - r1);
    f.planes[4] = normalize_plane(r3 + r2);
    f.planes[5] = normalize_plane(r3 - r2);
    return f;
}

inline bool intersects(const Frustum& f, const Aabb& b) {
    for (const Vec4& p : f.planes) {
        const Vec3 n{p.x, p.y, p.z};
        Vec3 v{
            (n.x >= 0.0f) ? b.max.x : b.min.x,
            (n.y >= 0.0f) ? b.max.y : b.min.y,
            (n.z >= 0.0f) ? b.max.z : b.min.z,
        };
        if (glm::dot(n, v) + p.w < 0.0f) return false;
    }
    return true;
}

} // namespace cube::math


