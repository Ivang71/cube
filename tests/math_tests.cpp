#include "math/math.hpp"

#include <cstdio>
#include <cstdlib>
#include <cmath>

static bool nearf(float a, float b, float eps = 1e-5f) { return std::fabs(a - b) <= eps; }

static bool mat_near(const cube::math::Mat4& a, const cube::math::Mat4& b, float eps = 1e-5f) {
    for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r) if (!nearf(a[c][r], b[c][r], eps)) return false;
    return true;
}

static int fail(int code, const char* what) {
    std::fprintf(stderr, "cube_tests: FAIL(%d): %s\n", code, what);
    return code;
}

int run_memory_tests();

int main() {
    using namespace cube::math;

    {
        UniversalCoord a{1, 0, 0, -1, 0, 0};
        UniversalCoord b{0, 0, 0, 2, 0, 0};
        UniversalCoord c = a + b;
        if (c.sx != 1 || c.mx != 1) return fail(11, "UniversalCoord add (1km-1m)+(2m)=(1km+1m)");
    }

    {
        UniversalCoord a{0, 0, 0, 999, 0, 0};
        UniversalCoord b{0, 0, 0, 100, 0, 0};
        UniversalCoord c = a + b;
        if (c.sx != 1 || c.mx != 99) return fail(12, "UniversalCoord overflow 999m+100m=1km+99m");
    }

    {
        UniversalCoord a{0, 0, 0, 1, 0, 0};
        UniversalCoord b{0, 0, 0, 100, 0, 0};
        UniversalCoord c = a - b;
        if (c.sx != -1 || c.mx != 901) return fail(13, "UniversalCoord underflow 1m-100m=-1km+901m");
    }

    {
        const std::int64_t scales_km[] = {1, 100, 10000, 1000000};
        for (std::int64_t s : scales_km) {
            UniversalCoord base{s, 0, 0, 0, 0, 0};
            UniversalCoord o1 = base + UniversalCoord{0, 0, 0, 1, 0, 0};
            const double d = base.distance(o1);
            if (std::fabs(d - 1.0) > 0.0) return fail(14, "UniversalCoord distance precision (base vs base+1m)");

            const Vec3 rel = o1.to_relative(base);
            if (!nearf(rel.x, 1.0f, 0.0f) || !nearf(rel.y, 0.0f, 0.0f) || !nearf(rel.z, 0.0f, 0.0f))
                return fail(15, "UniversalCoord to_relative precision (base vs base+1m)");

            UniversalCoord dcoord = (base + UniversalCoord{0, 0, 0, 2, 0, 0}) - o1;
            if (dcoord.sx != 0 || dcoord.mx != 1) return fail(16, "UniversalCoord subtraction stays small for nearby");
        }
    }

    {
        Mat4 t = glm::translate(Mat4(1.0f), Vec3(1.0f, 2.0f, 3.0f));
        Mat4 i(1.0f);
        if (!mat_near(mul(t, i), t)) return fail(1, "mat4 multiply (T*I)");
        if (!mat_near(mul(i, t), t)) return fail(2, "mat4 multiply (I*T)");
    }

    {
        const float fov = glm::radians(90.0f);
        const float aspect = 16.0f / 9.0f;
        const float zn = 0.1f;
        const float zf = 100.0f;
        const float f = 1.0f / std::tan(fov * 0.5f);

        Mat4 expected(0.0f);
        expected[0][0] = f / aspect;
        expected[1][1] = -f;
        expected[2][2] = zf / (zn - zf);
        expected[2][3] = -1.0f;
        expected[3][2] = (zf * zn) / (zn - zf);

        Mat4 got = perspective_vk(fov, aspect, zn, zf);
        if (!mat_near(got, expected, 1e-4f)) return fail(3, "perspective_vk known matrix");
    }

    {
        Vec3 eye{0.0f, 0.0f, 2.0f};
        Vec3 center{0.0f, 0.0f, 0.0f};
        Vec3 up{0.0f, 1.0f, 0.0f};

        Mat4 expected(1.0f);
        expected[3][2] = -2.0f;

        Mat4 got = look_at(eye, center, up);
        if (!mat_near(got, expected, 1e-5f)) return fail(4, "look_at known matrix");
    }

    {
        Aabb a{{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}};
        Aabb b{{0.5f, 0.5f, 0.5f}, {2.0f, 2.0f, 2.0f}};
        Aabb c{{2.1f, 0.0f, 0.0f}, {3.0f, 1.0f, 1.0f}};
        if (!intersects(a, b)) return fail(5, "AABB-AABB intersection (overlap)");
        if (intersects(a, c)) return fail(6, "AABB-AABB intersection (separated)");
    }

    {
        Aabb box{{-1.0f, -1.0f, -5.0f}, {1.0f, 1.0f, -3.0f}};
        Ray r{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}};
        float t = 0.0f;
        if (!ray_aabb(r, box, t)) return fail(7, "ray_aabb hit");
        if (!nearf(t, 3.0f, 1e-5f)) return fail(8, "ray_aabb t");
    }

    {
        Mat4 proj = glm::perspectiveRH_ZO(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
        Mat4 view(1.0f);
        Frustum f = extract_frustum(proj * view);
        Aabb inside{{-1.0f, -1.0f, -5.0f}, {1.0f, 1.0f, -3.0f}};
        Aabb outside{{100.0f, 0.0f, -5.0f}, {101.0f, 1.0f, -4.0f}};
        if (!intersects(f, inside)) return fail(9, "frustum vs AABB (inside)");
        if (intersects(f, outside)) return fail(10, "frustum vs AABB (outside)");
    }

    if (int r = run_memory_tests(); r != 0) return r;

    std::puts("cube_tests: OK");
    return 0;
}


