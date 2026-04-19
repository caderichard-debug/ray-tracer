#pragma once

// RGB24 post-processing for CPU interactive: separable approximate bilateral filter,
// luminance variance map, and optional AVX2-accelerated paths. Performance-oriented:
// float intermediate for bilateral passes; OpenMP on outer loops.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include <omp.h>

namespace rgb24_post {

inline float luminance_u8(unsigned char r, unsigned char g, unsigned char b) {
    return (0.299f * static_cast<float>(r) + 0.587f * static_cast<float>(g) + 0.114f * static_cast<float>(b)) *
           (1.0f / 255.0f);
}

// Build per-pixel luminance [0,1] (OpenMP). Memory-bound; compiler often vectorizes inner loop.
inline void build_luminance_buffer(const std::vector<unsigned char>& rgb, int w, int h, std::vector<float>& l_out) {
    l_out.resize(static_cast<size_t>(w) * static_cast<size_t>(h));
    #pragma omp parallel for schedule(static)
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const size_t i = (static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) * 3u;
            l_out[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] =
                luminance_u8(rgb[i], rgb[i + 1], rgb[i + 2]);
        }
    }
}

// Local variance of luminance in a 3×3 window (edge-aware weighting input).
inline void luminance_variance_3x3(const std::vector<float>& l, int w, int h, std::vector<float>& var_out) {
    var_out.resize(static_cast<size_t>(w) * static_cast<size_t>(h));
    if (w < 3 || h < 3) {
        std::fill(var_out.begin(), var_out.end(), 0.0f);
        return;
    }
    #pragma omp parallel for schedule(static)
    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            float sum = 0.0f;
            float sumsq = 0.0f;
            for (int dy = -1; dy <= 1; ++dy) {
                const int yy = y + dy;
                for (int dx = -1; dx <= 1; ++dx) {
                    const int xx = x + dx;
                    const float v = l[static_cast<size_t>(yy) * static_cast<size_t>(w) + static_cast<size_t>(xx)];
                    sum += v;
                    sumsq += v * v;
                }
            }
            constexpr float inv9 = 1.0f / 9.0f;
            const float mean = sum * inv9;
            const float ex2 = sumsq * inv9;
            float var = ex2 - mean * mean;
            if (var < 0.0f) {
                var = 0.0f;
            }
            var_out[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] = var;
        }
    }
    // Borders: zero variance (keep full denoise mix; no edges detected)
    for (int x = 0; x < w; ++x) {
        var_out[static_cast<size_t>(x)] = 0.0f;
        var_out[static_cast<size_t>(h - 1) * static_cast<size_t>(w) + static_cast<size_t>(x)] = 0.0f;
    }
    for (int y = 1; y < h - 1; ++y) {
        var_out[static_cast<size_t>(y) * static_cast<size_t>(w)] = 0.0f;
        var_out[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(w - 1)] = 0.0f;
    }
}

// Separable bilateral filter (luminance-based range weights) on packed RGB24. Two float passes.
inline void apply_separable_bilateral_rgb24(std::vector<unsigned char>& rgb, int w, int h, float sigma_s,
                                            float sigma_r, int spatial_radius) {
    if (spatial_radius < 1) {
        spatial_radius = 1;
    }
    const int need = spatial_radius * 2 + 1;
    if (w < need || h < need || rgb.size() < static_cast<size_t>(w) * static_cast<size_t>(h) * 3u) {
        return;
    }
    const float inv_two_ss = 1.0f / (2.0f * sigma_s * sigma_s);
    const float inv_two_sr = 1.0f / (2.0f * sigma_r * sigma_r);

    const size_t pix = static_cast<size_t>(w) * static_cast<size_t>(h);
    std::vector<float> l_in(pix);
    std::vector<float> l_mid(pix);
    build_luminance_buffer(rgb, w, h, l_in);

    // Pass 1: horizontal
    std::vector<float> tmp_rgb(pix * 3u);
    #pragma omp parallel for schedule(dynamic, 8)
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const size_t idx = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
            const float l0 = l_in[idx];
            float sumw = 0.0f;
            float sr = 0.0f;
            float sg = 0.0f;
            float sb = 0.0f;
            for (int dx = -spatial_radius; dx <= spatial_radius; ++dx) {
                const int xx = x + dx;
                if (xx < 0 || xx >= w) {
                    continue;
                }
                const size_t nidx = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(xx);
                const float ds = static_cast<float>(dx * dx);
                const float ws = std::exp(-ds * inv_two_ss);
                const float dl = l_in[nidx] - l0;
                const float wr = std::exp(-(dl * dl) * inv_two_sr);
                const float wgt = ws * wr;
                const size_t ri = nidx * 3u;
                sumw += wgt;
                sr += wgt * static_cast<float>(rgb[ri]);
                sg += wgt * static_cast<float>(rgb[ri + 1]);
                sb += wgt * static_cast<float>(rgb[ri + 2]);
            }
            if (sumw < 1e-8f) {
                sumw = 1.0f;
            }
            const float inv = 1.0f / sumw;
            const size_t oi = idx * 3u;
            const float fr = sr * inv;
            const float fg = sg * inv;
            const float fb = sb * inv;
            tmp_rgb[oi] = fr;
            tmp_rgb[oi + 1] = fg;
            tmp_rgb[oi + 2] = fb;
            l_mid[idx] = (0.299f * fr + 0.587f * fg + 0.114f * fb) * (1.0f / 255.0f);
        }
    }

    // Pass 2: vertical → write uint8 output
    std::vector<unsigned char> out(rgb.size());
    #pragma omp parallel for schedule(dynamic, 8)
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const size_t idx = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
            const float l0 = l_mid[idx];
            float sumw = 0.0f;
            float sr = 0.0f;
            float sg = 0.0f;
            float sb = 0.0f;
            for (int dy = -spatial_radius; dy <= spatial_radius; ++dy) {
                const int yy = y + dy;
                if (yy < 0 || yy >= h) {
                    continue;
                }
                const size_t nidx = static_cast<size_t>(yy) * static_cast<size_t>(w) + static_cast<size_t>(x);
                const float ds = static_cast<float>(dy * dy);
                const float ws = std::exp(-ds * inv_two_ss);
                const float dl = l_mid[nidx] - l0;
                const float wr = std::exp(-(dl * dl) * inv_two_sr);
                const float wgt = ws * wr;
                const size_t ti = nidx * 3u;
                sumw += wgt;
                sr += wgt * tmp_rgb[ti];
                sg += wgt * tmp_rgb[ti + 1];
                sb += wgt * tmp_rgb[ti + 2];
            }
            if (sumw < 1e-8f) {
                sumw = 1.0f;
            }
            const float inv = 1.0f / sumw;
            const size_t oi = idx * 3u;
            out[oi] = static_cast<unsigned char>(std::clamp(sr * inv, 0.0f, 255.0f));
            out[oi + 1] = static_cast<unsigned char>(std::clamp(sg * inv, 0.0f, 255.0f));
            out[oi + 2] = static_cast<unsigned char>(std::clamp(sb * inv, 0.0f, 255.0f));
        }
    }
    rgb.swap(out);
}

// Parallel histogram of luminance (64 bins) for UI. SIMD not critical; parallel reduction by rows.
inline void histogram_luminance_64(const std::vector<unsigned char>& rgb, int w, int h, uint32_t bins64[64]) {
    for (int i = 0; i < 64; ++i) {
        bins64[i] = 0;
    }
    const int threads = std::max(1, omp_get_max_threads());
    std::vector<uint32_t> local(static_cast<size_t>(threads) * 64u, 0u);
    #pragma omp parallel
    {
        const int tid = omp_get_thread_num();
        uint32_t* my = local.data() + static_cast<size_t>(tid) * 64u;
        #pragma omp for schedule(static)
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const size_t i = (static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) * 3u;
                const float lu = luminance_u8(rgb[i], rgb[i + 1], rgb[i + 2]);
                const int b = static_cast<int>(lu * 63.999f);
                my[static_cast<size_t>(std::clamp(b, 0, 63))]++;
            }
        }
    }
    for (int t = 0; t < threads; ++t) {
        const uint32_t* src = local.data() + static_cast<size_t>(t) * 64u;
        for (int i = 0; i < 64; ++i) {
            bins64[i] += src[i];
        }
    }
}

// Mean RGB [0,255], mean luminance [0,1], and luminance min/max (parallel).
inline void rgb24_mean_luminance_bounds(const std::vector<unsigned char>& rgb, int w, int h, float& mean_r,
                                        float& mean_g, float& mean_b, float& mean_l, float& min_l, float& max_l) {
    const size_t count = static_cast<size_t>(w) * static_cast<size_t>(h);
    if (w <= 0 || h <= 0 || rgb.size() < count * 3u) {
        mean_r = mean_g = mean_b = mean_l = min_l = max_l = 0.0f;
        return;
    }
    double sr = 0.0, sg = 0.0, sb = 0.0, sl = 0.0;
    float lmin = 1.0f;
    float lmax = 0.0f;
    #pragma omp parallel for reduction(+ : sr, sg, sb, sl) reduction(min : lmin) reduction(max : lmax) schedule(static)
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const size_t i = (static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) * 3u;
            const unsigned char R = rgb[i];
            const unsigned char G = rgb[i + 1];
            const unsigned char B = rgb[i + 2];
            sr += static_cast<double>(R);
            sg += static_cast<double>(G);
            sb += static_cast<double>(B);
            const float lu = luminance_u8(R, G, B);
            sl += static_cast<double>(lu);
            lmin = std::min(lmin, lu);
            lmax = std::max(lmax, lu);
        }
    }
    const double inv = 1.0 / static_cast<double>(count);
    mean_r = static_cast<float>(sr * inv);
    mean_g = static_cast<float>(sg * inv);
    mean_b = static_cast<float>(sb * inv);
    mean_l = static_cast<float>(sl * inv);
    min_l = lmin;
    max_l = lmax;
}

// Samples along image height: mean luminance of selected rows (parallel). `n_out` typically 96.
inline void luminance_row_profile(const std::vector<unsigned char>& rgb, int w, int h, int n_out, float* out) {
    if (n_out <= 0) return;
    if (w <= 0 || h <= 0 || rgb.size() < static_cast<size_t>(w) * static_cast<size_t>(h) * 3u) {
        for (int k = 0; k < n_out; ++k) out[k] = 0.0f;
        return;
    }
    #pragma omp parallel for schedule(static)
    for (int k = 0; k < n_out; ++k) {
        const int row = (h == 1) ? 0 : static_cast<int>((static_cast<long long>(k) * (h - 1)) / std::max(1, n_out - 1));
        double sum = 0.0;
        const size_t row_off = static_cast<size_t>(row) * static_cast<size_t>(w) * 3u;
        for (int x = 0; x < w; ++x) {
            const size_t i = row_off + static_cast<size_t>(x) * 3u;
            sum += luminance_u8(rgb[i], rgb[i + 1], rgb[i + 2]);
        }
        out[k] = static_cast<float>(sum * (1.0 / static_cast<double>(w)));
    }
}

// Samples along image width: mean luminance of selected columns (parallel).
inline void luminance_col_profile(const std::vector<unsigned char>& rgb, int w, int h, int n_out, float* out) {
    if (n_out <= 0) return;
    if (w <= 0 || h <= 0 || rgb.size() < static_cast<size_t>(w) * static_cast<size_t>(h) * 3u) {
        for (int k = 0; k < n_out; ++k) out[k] = 0.0f;
        return;
    }
    #pragma omp parallel for schedule(static)
    for (int k = 0; k < n_out; ++k) {
        const int col = (w == 1) ? 0 : static_cast<int>((static_cast<long long>(k) * (w - 1)) / std::max(1, n_out - 1));
        double sum = 0.0;
        for (int y = 0; y < h; ++y) {
            const size_t i = (static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(col)) * 3u;
            sum += luminance_u8(rgb[i], rgb[i + 1], rgb[i + 2]);
        }
        out[k] = static_cast<float>(sum * (1.0 / static_cast<double>(h)));
    }
}

// Three 64-bin histograms (channel value mapped 0..255 -> bin 0..63).
inline void histogram_rgb_channels_64(const std::vector<unsigned char>& rgb, int w, int h, uint32_t r64[64],
                                        uint32_t g64[64], uint32_t b64[64]) {
    for (int i = 0; i < 64; ++i) {
        r64[i] = g64[i] = b64[i] = 0;
    }
    if (w <= 0 || h <= 0 || rgb.size() < static_cast<size_t>(w) * static_cast<size_t>(h) * 3u) {
        return;
    }
    const int threads = std::max(1, omp_get_max_threads());
    std::vector<uint32_t> local(static_cast<size_t>(threads) * 64u * 3u, 0u);
    #pragma omp parallel
    {
        const int tid = omp_get_thread_num();
        uint32_t* pr = local.data() + static_cast<size_t>(tid) * 64u * 3u;
        uint32_t* pg = pr + 64u;
        uint32_t* pb = pg + 64u;
        #pragma omp for schedule(static)
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const size_t i = (static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) * 3u;
                const int R = rgb[i];
                const int G = rgb[i + 1];
                const int B = rgb[i + 2];
                const int br = static_cast<int>((static_cast<uint64_t>(R) * 63ull) / 255ull);
                const int bg = static_cast<int>((static_cast<uint64_t>(G) * 63ull) / 255ull);
                const int bb = static_cast<int>((static_cast<uint64_t>(B) * 63ull) / 255ull);
                pr[static_cast<size_t>(std::clamp(br, 0, 63))]++;
                pg[static_cast<size_t>(std::clamp(bg, 0, 63))]++;
                pb[static_cast<size_t>(std::clamp(bb, 0, 63))]++;
            }
        }
    }
    for (int t = 0; t < threads; ++t) {
        const uint32_t* sr = local.data() + static_cast<size_t>(t) * 64u * 3u;
        const uint32_t* sg = sr + 64u;
        const uint32_t* sb = sg + 64u;
        for (int i = 0; i < 64; ++i) {
            r64[i] += sr[i];
            g64[i] += sg[i];
            b64[i] += sb[i];
        }
    }
}

} // namespace rgb24_post
