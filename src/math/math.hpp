#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <cstdint>
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

struct UniversalCoord {
    std::int64_t sx = 0, sy = 0, sz = 0;
    std::int32_t mx = 0, my = 0, mz = 0;

    static constexpr std::int64_t sector_m = 1000;

    constexpr UniversalCoord() = default;
    constexpr UniversalCoord(std::int64_t sx_, std::int64_t sy_, std::int64_t sz_, std::int32_t mx_, std::int32_t my_, std::int32_t mz_)
        : sx(sx_), sy(sy_), sz(sz_), mx(mx_), my(my_), mz(mz_) {
        normalize();
    }

    static constexpr UniversalCoord from_meters(std::int64_t x_m, std::int64_t y_m, std::int64_t z_m) {
        UniversalCoord u{};
        u.set_from_total_m(x_m, y_m, z_m);
        return u;
    }

    constexpr UniversalCoord operator+(const UniversalCoord& o) const {
        UniversalCoord r{sx + o.sx, sy + o.sy, sz + o.sz, mx + o.mx, my + o.my, mz + o.mz};
        return r;
    }

    constexpr UniversalCoord operator-(const UniversalCoord& o) const {
        UniversalCoord r = from_meters(total_x_m() - o.total_x_m(), total_y_m() - o.total_y_m(), total_z_m() - o.total_z_m());
        return r;
    }

    constexpr UniversalCoord& operator+=(const UniversalCoord& o) {
        sx += o.sx;
        sy += o.sy;
        sz += o.sz;
        mx += o.mx;
        my += o.my;
        mz += o.mz;
        normalize();
        return *this;
    }

    constexpr UniversalCoord& operator-=(const UniversalCoord& o) {
        *this = *this - o;
        return *this;
    }

    constexpr Vec3 to_relative(const UniversalCoord& camera) const {
        const std::int64_t dx = total_x_m() - camera.total_x_m();
        const std::int64_t dy = total_y_m() - camera.total_y_m();
        const std::int64_t dz = total_z_m() - camera.total_z_m();
        return Vec3(static_cast<float>(dx), static_cast<float>(dy), static_cast<float>(dz));
    }

    inline double distance(const UniversalCoord& o) const {
        const double dx = static_cast<double>(total_x_m() - o.total_x_m());
        const double dy = static_cast<double>(total_y_m() - o.total_y_m());
        const double dz = static_cast<double>(total_z_m() - o.total_z_m());
        return glm::sqrt(dx * dx + dy * dy + dz * dz);
    }

    constexpr std::int64_t total_x_m() const { return sx * sector_m + mx; }
    constexpr std::int64_t total_y_m() const { return sy * sector_m + my; }
    constexpr std::int64_t total_z_m() const { return sz * sector_m + mz; }

private:
    static constexpr std::int64_t floor_div(std::int64_t a, std::int64_t b) {
        std::int64_t q = a / b;
        const std::int64_t r = a % b;
        if ((r != 0) && ((r > 0) != (b > 0))) --q;
        return q;
    }

    constexpr void set_from_total_m(std::int64_t x_m, std::int64_t y_m, std::int64_t z_m) {
        set_axis_from_total_m(sx, mx, x_m);
        set_axis_from_total_m(sy, my, y_m);
        set_axis_from_total_m(sz, mz, z_m);
    }

    static constexpr void set_axis_from_total_m(std::int64_t& s, std::int32_t& m, std::int64_t t) {
        s = floor_div(t, sector_m);
        const std::int64_t mm = t - s * sector_m;
        m = static_cast<std::int32_t>(mm);
    }

    constexpr void normalize() {
        set_axis_from_total_m(sx, mx, total_x_m());
        set_axis_from_total_m(sy, my, total_y_m());
        set_axis_from_total_m(sz, mz, total_z_m());
    }
};

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


