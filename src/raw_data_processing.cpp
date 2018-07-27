#include <tekari/raw_data_processing.h>
#include <tekari/stop_watch.h>

#define REAL float
#define VOID void
#include <triangle.h>

#include <limits>
#include <tbb/parallel_for.h>

TEKARI_NAMESPACE_BEGIN

#define MAX_SAMPLING_DISTANCE 0.05f
#define CORRECTION_FACTOR 1e-5

void compute_normals(
    const Matrix3Xu& F,
    const Matrix2Xf& V2D,
    const MatrixXXf& H,
    const MatrixXXf& LH,
    Matrix4XXf& N,
    Matrix4XXf& LN
);

// inline void compute_triangle_normal(
//     const Matrix2Xf& V2D,
//     const VectorXf& H,
//     Matrix4XXf::Row& N,
//     size_t i0,
//     size_t i1,
//     size_t i2
// );

void compute_normalized_heights(
    const RawMeasurement& raw_measurement,
    PointsStats& points_stats,
    MatrixXXf& H,
    MatrixXXf& LH
);

void triangulate_data(
    Matrix3Xu& F,
    Matrix2Xf& V2D
);

void compute_path_segments(
    VectorXu& path_segments,
    const Matrix2Xf& V2D
);

void recompute_data(
    const RawMeasurement& raw_measurement,
    PointsStats& points_stats,
    VectorXu& path_segments,
    Matrix3Xu& F,
    Matrix2Xf& V2D,
    MatrixXXf& H, MatrixXXf& LH,
    Matrix4XXf& N, Matrix4XXf& LN
)
{
    compute_min_max_intensities(points_stats, raw_measurement);
    compute_normalized_heights(raw_measurement, points_stats, H, LH);
    triangulate_data(F, V2D);
    compute_path_segments(path_segments, V2D);
    compute_normals(F, V2D, H, LH, N, LN);
    update_points_stats(points_stats, raw_measurement, V2D, H);
}

void compute_normals(
    const Matrix3Xu& F,
    const Matrix2Xf& V2D,
    const MatrixXXf& H,
    const MatrixXXf& LH,
    Matrix4XXf& N,
    Matrix4XXf& LN
)
{
    START_PROFILING("Computing normals");
    N.assign (H.n_rows(),  H.n_cols(),  Vector4f(0));
    LN.assign(LH.n_rows(), LH.n_cols(), Vector4f(0));

    tbb::parallel_for(tbb::blocked_range<uint32_t>(0u, (uint32_t)N.size(), 1),
        [&](const tbb::blocked_range<uint32_t>& range) {
            for (uint32_t j = range.begin(); j != range.end(); ++j) {
                for (Index f = 0; f < F.size(); ++f)
                {
                    Vector3f fn = Vector3f(0.0f);
                    Vector3f lfn = Vector3f(0.0f);
                    for (int i = 0; i < 3; ++i) {
                        Vector3f v0 = get_3d_point(V2D, H[j], F[f][i]),
                                 v1 = get_3d_point(V2D, H[j], F[f][(i+1)%3]),
                                 v2 = get_3d_point(V2D, H[j], F[f][(i+2)%3]),
                                 d0 = v1-v0,
                                 d1 = v2-v0;

                        if (i == 0) {
                            fn = enoki::normalize(enoki::cross(d0, d1));
                        }
                        float angle = fast_acos(enoki::dot(d0, d1) / std::sqrt(enoki::squared_norm(d0) * enoki::squared_norm(d1)));
                        N[j][F[f][i]] += enoki::concat(fn*angle, 0.0f);

                        // same for log mesh

                        v0 = get_3d_point(V2D, LH[j], F[f][i]);
                        v1 = get_3d_point(V2D, LH[j], F[f][(i+1)%3]);
                        v2 = get_3d_point(V2D, LH[j], F[f][(i+2)%3]);
                        d0 = v1-v0;
                        d1 = v2-v0;

                        if (i == 0) {
                            lfn = enoki::normalize(enoki::cross(d0, d1));
                        }
                        angle = fast_acos(enoki::dot(d0, d1) / std::sqrt(enoki::squared_norm(d0) * enoki::squared_norm(d1)));
                        LN[j][F[f][i]] += enoki::concat(lfn*angle, 0.0f);
                    }

                }

                tbb::parallel_for(tbb::blocked_range<uint32_t>(0u, (uint32_t)N.n_cols(), GRAIN_SIZE),
                    [&](const tbb::blocked_range<uint32_t>& range) {
                        for (uint32_t i = range.begin(); i != range.end(); ++i) {
                            N[j][i] = enoki::normalize(N[j][i]);
                            LN[j][i] = enoki::normalize(LN[j][i]);
                        }
                    }
                );
            }
        }
    );
    END_PROFILING();
}

// inline void compute_triangle_normal(
//     const Matrix2Xf& V2D,
//     const MatrixXXf::Row& H,
//     Matrix4Xf& N,
//     size_t i0,
//     size_t i1,
//     size_t i2
// )
// {
//     using namespace enoki;

//     const Vector3f e01 = normalize(get_3d_point(V2D, H, i1) - get_3d_point(V2D, H, i0));
//     const Vector3f e12 = normalize(get_3d_point(V2D, H, i2) - get_3d_point(V2D, H, i1));
//     const Vector3f e20 = normalize(get_3d_point(V2D, H, i0) - get_3d_point(V2D, H, i2));

//     Vector3f face_normal = normalize(cross(e12, -e01));

//     Vector3f weights = Vector3f( dot(e01, -e20),
//                                  dot(e12, -e01),
//                                  dot(e20, -e12));
//     weights = acos(max(-1.0f, (min(1.0f, weights)))); 

//     N[i0] += weights[0] * concat(face_normal, 0.0f);
//     N[i1] += weights[1] * concat(face_normal, 0.0f);
//     N[i2] += weights[2] * concat(face_normal, 0.0f);
// }

void compute_normalized_heights(
    const RawMeasurement& raw_measurement,
    PointsStats& points_stats,
    MatrixXXf& H, MatrixXXf& LH
)
{
    START_PROFILING("Computing normalized heights");

    Index n_intensities = raw_measurement.n_wave_lengths() + 1;
    Index n_sample_points = raw_measurement.n_sample_points();

    // compute overall min/max intensity
    float min_intensity = std::numeric_limits<float>::max();
    float max_intensity = std::numeric_limits<float>::min();
    for (size_t j = 0; j < n_intensities; ++j)
    {
        min_intensity = std::min(min_intensity, points_stats.min_intensity(j));
        max_intensity = std::max(max_intensity, points_stats.max_intensity(j));
    }
    float correction_factor = (min_intensity <= 0.0f ? -min_intensity + CORRECTION_FACTOR : 0.0f);

    float min_log_intensity = log(min_intensity + correction_factor);
    float max_log_intensity = log(max_intensity + correction_factor);

    H.resize(n_sample_points, n_intensities);
    LH.resize(n_sample_points, n_intensities);
    for (size_t j = 0; j < n_intensities; ++j)
    {
        // normalize intensities
        tbb::parallel_for(tbb::blocked_range<uint32_t>(0, (uint32_t)n_sample_points, GRAIN_SIZE),
            [&](const tbb::blocked_range<uint32_t>& range)
        {
            for (uint32_t i = range.begin(); i < range.end(); ++i)
            {
                H[j][i] = (raw_measurement[i].intensity(j) - min_intensity) / (max_intensity - min_intensity);
                LH[j][i] = (log(raw_measurement[i].intensity(j) + correction_factor) - min_log_intensity) / (max_log_intensity - min_log_intensity);
            }
        }
        );
    }
    END_PROFILING();
}

void triangulate_data(Matrix3Xu& F, Matrix2Xf& V2D)
{
    START_PROFILING("Triangulating data");

    struct triangulateio in, out;
    memset(&in, 0, sizeof(struct triangulateio));
    memset(&out, 0, sizeof(struct triangulateio));

    in.pointlist = (float*)V2D.data();
    in.numberofpoints = V2D.size();

    // for(int i = 0; i < V2D.size(); ++i)
    // {
    //     cout << V2D[i] << endl;
    // }

    char cmds[4] = {'z', 'Q', 'N', '\0'};
    triangulate(cmds, &in, &out, NULL);

    F.resize(out.numberoftriangles);
    memcpy(F.data(), out.trianglelist, out.numberoftriangles * 3 * sizeof(int));

    free(out.trianglelist);

    END_PROFILING();
}

void compute_path_segments(VectorXu& path_segments, const Matrix2Xf& V2D)
{
    START_PROFILING("Computing path segments");
    path_segments.clear();
    // path segments must always contain the first point
    path_segments.push_back(0);
    for (Index i = 1; i < V2D.size(); ++i)
    {
        // if two last points are too far appart, a new path segments begins
        const Vector2f& current = V2D[i];
        const Vector2f& prev = V2D[i - 1];
        if (enoki::squared_norm(prev - current) > MAX_SAMPLING_DISTANCE)
        {
            path_segments.push_back(i);
        }
    }
    // path segments must always contain the last point
    path_segments.push_back(V2D.size());
    END_PROFILING();
}


TEKARI_NAMESPACE_END