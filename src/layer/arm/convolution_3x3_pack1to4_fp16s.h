// Copyright 2021 Tencent
// SPDX-License-Identifier: BSD-3-Clause

static void conv3x3s1_pack1to4_fp16sa_neon(const Mat& bottom_blob, Mat& top_blob, const Mat& kernel, const Mat& _bias, const Option& opt)
{
    int inch = bottom_blob.c;
    int outw = top_blob.w;
    int outh = top_blob.h;
    int outch = top_blob.c;

    const __fp16* bias = _bias;

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int p = 0; p < outch; p++)
    {
        Mat out0 = top_blob.channel(p);

        float16x4_t _bias0 = bias ? vld1_f16(bias + p * 4) : vdup_n_f16((__fp16)0.f);
        out0.fill(_bias0);

        const __fp16* k0 = kernel.channel(p);

        int q = 0;
        for (; q < inch; q++)
        {
            __fp16* outptr0 = out0;

            const Mat img0 = bottom_blob.channel(q);

            const __fp16* r0 = img0.row<const __fp16>(0);
            const __fp16* r1 = img0.row<const __fp16>(1);
            const __fp16* r2 = img0.row<const __fp16>(2);

            float16x4_t _k00 = vld1_f16(k0);
            float16x4_t _k01 = vld1_f16(k0 + 4);
            float16x4_t _k02 = vld1_f16(k0 + 8);
            float16x4_t _k10 = vld1_f16(k0 + 12);
            float16x4_t _k11 = vld1_f16(k0 + 16);
            float16x4_t _k12 = vld1_f16(k0 + 20);
            float16x4_t _k20 = vld1_f16(k0 + 24);
            float16x4_t _k21 = vld1_f16(k0 + 28);
            float16x4_t _k22 = vld1_f16(k0 + 32);

            int i = 0;
            for (; i < outh; i++)
            {
                int j = 0;
                for (; j + 7 < outw; j += 8)
                {
                    asm volatile(
                        "prfm   pldl1keep, [%0, #256]       \n"
                        "ld1    {v24.4h, v25.4h, v26.4h, v27.4h}, [%0], #32 \n" // sum0 sum1 sum2 sum3

                        "prfm   pldl1keep, [%0, #256]       \n"
                        "ld1    {v28.4h, v29.4h, v30.4h, v31.4h}, [%0] \n" // sum4 sum5 sum6 sum7

                        "sub    %0, %0, #32                 \n"

                        "prfm   pldl1keep, [%1, #128]       \n"
                        "ld1    {v0.8h}, [%1], #16          \n" // r0
                        "ld1    {v1.4h}, [%1]               \n"

                        "fmla   v24.4h, %8.4h, v0.h[0]      \n"
                        "fmla   v25.4h, %8.4h, v0.h[1]      \n"
                        "fmla   v26.4h, %8.4h, v0.h[2]      \n"
                        "fmla   v27.4h, %8.4h, v0.h[3]      \n"
                        "fmla   v28.4h, %8.4h, v0.h[4]      \n"
                        "fmla   v29.4h, %8.4h, v0.h[5]      \n"
                        "fmla   v30.4h, %8.4h, v0.h[6]      \n"
                        "fmla   v31.4h, %8.4h, v0.h[7]      \n"

                        "fmla   v24.4h, %9.4h, v0.h[1]      \n"
                        "fmla   v25.4h, %9.4h, v0.h[2]      \n"
                        "fmla   v26.4h, %9.4h, v0.h[3]      \n"
                        "fmla   v27.4h, %9.4h, v0.h[4]      \n"
                        "fmla   v28.4h, %9.4h, v0.h[5]      \n"
                        "fmla   v29.4h, %9.4h, v0.h[6]      \n"
                        "fmla   v30.4h, %9.4h, v0.h[7]      \n"
                        "fmla   v31.4h, %9.4h, v1.h[0]      \n"

                        "fmla   v24.4h, %10.4h, v0.h[2]     \n"
                        "fmla   v25.4h, %10.4h, v0.h[3]     \n"
                        "fmla   v26.4h, %10.4h, v0.h[4]     \n"
                        "fmla   v27.4h, %10.4h, v0.h[5]     \n"
                        "fmla   v28.4h, %10.4h, v0.h[6]     \n"
                        "fmla   v29.4h, %10.4h, v0.h[7]     \n"
                        "fmla   v30.4h, %10.4h, v1.h[0]     \n"
                        "fmla   v31.4h, %10.4h, v1.h[1]     \n"

                        "prfm   pldl1keep, [%2, #128]       \n"
                        "ld1    {v2.8h}, [%2], #16          \n" // r1
                        "ld1    {v3.4h}, [%2]               \n"

                        "fmla   v24.4h, %11.4h, v2.h[0]     \n"
                        "fmla   v25.4h, %11.4h, v2.h[1]     \n"
                        "fmla   v26.4h, %11.4h, v2.h[2]     \n"
                        "fmla   v27.4h, %11.4h, v2.h[3]     \n"
                        "fmla   v28.4h, %11.4h, v2.h[4]     \n"
                        "fmla   v29.4h, %11.4h, v2.h[5]     \n"
                        "fmla   v30.4h, %11.4h, v2.h[6]     \n"
                        "fmla   v31.4h, %11.4h, v2.h[7]     \n"

                        "fmla   v24.4h, %12.4h, v2.h[1]     \n"
                        "fmla   v25.4h, %12.4h, v2.h[2]     \n"
                        "fmla   v26.4h, %12.4h, v2.h[3]     \n"
                        "fmla   v27.4h, %12.4h, v2.h[4]     \n"
                        "fmla   v28.4h, %12.4h, v2.h[5]     \n"
                        "fmla   v29.4h, %12.4h, v2.h[6]     \n"
                        "fmla   v30.4h, %12.4h, v2.h[7]     \n"
                        "fmla   v31.4h, %12.4h, v3.h[0]     \n"

                        "fmla   v24.4h, %13.4h, v2.h[2]     \n"
                        "fmla   v25.4h, %13.4h, v2.h[3]     \n"
                        "fmla   v26.4h, %13.4h, v2.h[4]     \n"
                        "fmla   v27.4h, %13.4h, v2.h[5]     \n"
                        "fmla   v28.4h, %13.4h, v2.h[6]     \n"
                        "fmla   v29.4h, %13.4h, v2.h[7]     \n"
                        "fmla   v30.4h, %13.4h, v3.h[0]     \n"
                        "fmla   v31.4h, %13.4h, v3.h[1]     \n"

                        "prfm   pldl1keep, [%3, #128]       \n"
                        "ld1    {v4.8h}, [%3], #16          \n" // r2
                        "ld1    {v5.4h}, [%3]               \n"

                        "fmla   v24.4h, %14.4h, v4.h[0]     \n"
                        "fmla   v25.4h, %14.4h, v4.h[1]     \n"
                        "fmla   v26.4h, %14.4h, v4.h[2]     \n"
                        "fmla   v27.4h, %14.4h, v4.h[3]     \n"
                        "fmla   v28.4h, %14.4h, v4.h[4]     \n"
                        "fmla   v29.4h, %14.4h, v4.h[5]     \n"
                        "fmla   v30.4h, %14.4h, v4.h[6]     \n"
                        "fmla   v31.4h, %14.4h, v4.h[7]     \n"

                        "fmla   v24.4h, %15.4h, v4.h[1]     \n"
                        "fmla   v25.4h, %15.4h, v4.h[2]     \n"
                        "fmla   v26.4h, %15.4h, v4.h[3]     \n"
                        "fmla   v27.4h, %15.4h, v4.h[4]     \n"
                        "fmla   v28.4h, %15.4h, v4.h[5]     \n"
                        "fmla   v29.4h, %15.4h, v4.h[6]     \n"
                        "fmla   v30.4h, %15.4h, v4.h[7]     \n"
                        "fmla   v31.4h, %15.4h, v5.h[0]     \n"

                        "fmla   v24.4h, %16.4h, v4.h[2]     \n"
                        "fmla   v25.4h, %16.4h, v4.h[3]     \n"
                        "fmla   v26.4h, %16.4h, v4.h[4]     \n"
                        "fmla   v27.4h, %16.4h, v4.h[5]     \n"
                        "fmla   v28.4h, %16.4h, v4.h[6]     \n"
                        "fmla   v29.4h, %16.4h, v4.h[7]     \n"
                        "fmla   v30.4h, %16.4h, v5.h[0]     \n"
                        "fmla   v31.4h, %16.4h, v5.h[1]     \n"

                        "st1    {v24.4h, v25.4h, v26.4h, v27.4h}, [%0], #32 \n"
                        "st1    {v28.4h, v29.4h, v30.4h, v31.4h}, [%0], #32 \n"

                        : "=r"(outptr0), // %0
                        "=r"(r0),      // %1
                        "=r"(r1),      // %2
                        "=r"(r2)       // %3
                        : "0"(outptr0),
                        "1"(r0),
                        "2"(r1),
                        "3"(r2),
                        "w"(_k00), // %8
                        "w"(_k01), // %9
                        "w"(_k02), // %10
                        "w"(_k10), // %11
                        "w"(_k11), // %12
                        "w"(_k12), // %13
                        "w"(_k20), // %14
                        "w"(_k21), // %15
                        "w"(_k22)  // %16
                        : "cc", "memory", "v0", "v1", "v2", "v3", "v4", "v5", "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31");
                }
                for (; j + 3 < outw; j += 4)
                {
                    asm volatile(
                        "prfm   pldl1keep, [%0, #256]       \n"
                        "ld1    {v28.4h, v29.4h, v30.4h, v31.4h}, [%0] \n" // sum0 sum1 sum2 sum3

                        "prfm   pldl1keep, [%1, #128]       \n"
                        "ld1    {v0.8h}, [%1]               \n" // r0

                        "fmla   v28.4h, %8.4h, v0.h[0]      \n"
                        "fmla   v29.4h, %8.4h, v0.h[1]      \n"
                        "fmla   v30.4h, %8.4h, v0.h[2]      \n"
                        "fmla   v31.4h, %8.4h, v0.h[3]      \n"

                        "fmla   v28.4h, %9.4h, v0.h[1]      \n"
                        "fmla   v29.4h, %9.4h, v0.h[2]      \n"
                        "fmla   v30.4h, %9.4h, v0.h[3]      \n"
                        "fmla   v31.4h, %9.4h, v0.h[4]      \n"

                        "fmla   v28.4h, %10.4h, v0.h[2]     \n"
                        "fmla   v29.4h, %10.4h, v0.h[3]     \n"
                        "fmla   v30.4h, %10.4h, v0.h[4]     \n"
                        "fmla   v31.4h, %10.4h, v0.h[5]     \n"

                        "prfm   pldl1keep, [%2, #128]       \n"
                        "ld1    {v1.8h}, [%2]               \n" // r1

                        "fmla   v28.4h, %11.4h, v1.h[0]     \n"
                        "fmla   v29.4h, %11.4h, v1.h[1]     \n"
                        "fmla   v30.4h, %11.4h, v1.h[2]     \n"
                        "fmla   v31.4h, %11.4h, v1.h[3]     \n"

                        "fmla   v28.4h, %12.4h, v1.h[1]     \n"
                        "fmla   v29.4h, %12.4h, v1.h[2]     \n"
                        "fmla   v30.4h, %12.4h, v1.h[3]     \n"
                        "fmla   v31.4h, %12.4h, v1.h[4]     \n"

                        "fmla   v28.4h, %13.4h, v1.h[2]     \n"
                        "fmla   v29.4h, %13.4h, v1.h[3]     \n"
                        "fmla   v30.4h, %13.4h, v1.h[4]     \n"
                        "fmla   v31.4h, %13.4h, v1.h[5]     \n"

                        "prfm   pldl1keep, [%3, #128]       \n"
                        "ld1    {v2.8h}, [%3]               \n" // r2

                        "fmla   v28.4h, %14.4h, v2.h[0]     \n"
                        "fmla   v29.4h, %14.4h, v2.h[1]     \n"
                        "fmla   v30.4h, %14.4h, v2.h[2]     \n"
                        "fmla   v31.4h, %14.4h, v2.h[3]     \n"

                        "fmla   v28.4h, %15.4h, v2.h[1]     \n"
                        "fmla   v29.4h, %15.4h, v2.h[2]     \n"
                        "fmla   v30.4h, %15.4h, v2.h[3]     \n"
                        "fmla   v31.4h, %15.4h, v2.h[4]     \n"

                        "fmla   v28.4h, %16.4h, v2.h[2]     \n"
                        "fmla   v29.4h, %16.4h, v2.h[3]     \n"
                        "fmla   v30.4h, %16.4h, v2.h[4]     \n"
                        "fmla   v31.4h, %16.4h, v2.h[5]     \n"

                        "add    %1, %1, #8                  \n"
                        "add    %2, %2, #8                  \n"
                        "add    %3, %3, #8                  \n"

                        "st1    {v28.4h, v29.4h, v30.4h, v31.4h}, [%0], #32 \n"

                        : "=r"(outptr0), // %0
                        "=r"(r0),      // %1
                        "=r"(r1),      // %2
                        "=r"(r2)       // %3
                        : "0"(outptr0),
                        "1"(r0),
                        "2"(r1),
                        "3"(r2),
                        "w"(_k00), // %8
                        "w"(_k01), // %9
                        "w"(_k02), // %10
                        "w"(_k10), // %11
                        "w"(_k11), // %12
                        "w"(_k12), // %13
                        "w"(_k20), // %14
                        "w"(_k21), // %15
                        "w"(_k22)  // %16
                        : "cc", "memory", "v0", "v1", "v2", "v28", "v29", "v30", "v31");
                }
                for (; j + 1 < outw; j += 2)
                {
                    asm volatile(
                        "prfm   pldl1keep, [%0, #128]       \n"
                        "ld1    {v30.4h, v31.4h}, [%0]      \n" // sum0 sum1

                        "prfm   pldl1keep, [%1, #64]        \n"
                        "ld1    {v0.4h}, [%1]               \n" // r0

                        "fmla   v30.4h, %8.4h, v0.h[0]      \n"
                        "fmla   v31.4h, %8.4h, v0.h[1]      \n"
                        "fmla   v30.4h, %9.4h, v0.h[1]      \n"
                        "fmla   v31.4h, %9.4h, v0.h[2]      \n"
                        "fmla   v30.4h, %10.4h, v0.h[2]     \n"
                        "fmla   v31.4h, %10.4h, v0.h[3]     \n"

                        "prfm   pldl1keep, [%2, #64]        \n"
                        "ld1    {v1.4h}, [%2]               \n" // r1

                        "fmla   v30.4h, %11.4h, v1.h[0]     \n"
                        "fmla   v31.4h, %11.4h, v1.h[1]     \n"
                        "fmla   v30.4h, %12.4h, v1.h[1]     \n"
                        "fmla   v31.4h, %12.4h, v1.h[2]     \n"
                        "fmla   v30.4h, %13.4h, v1.h[2]     \n"
                        "fmla   v31.4h, %13.4h, v1.h[3]     \n"

                        "prfm   pldl1keep, [%3, #64]        \n"
                        "ld1    {v2.4h}, [%3]               \n" // r2

                        "fmla   v30.4h, %14.4h, v2.h[0]     \n"
                        "fmla   v31.4h, %14.4h, v2.h[1]     \n"
                        "fmla   v30.4h, %15.4h, v2.h[1]     \n"
                        "fmla   v31.4h, %15.4h, v2.h[2]     \n"
                        "fmla   v30.4h, %16.4h, v2.h[2]     \n"
                        "fmla   v31.4h, %16.4h, v2.h[3]     \n"

                        "add    %1, %1, #4                  \n"
                        "add    %2, %2, #4                  \n"
                        "add    %3, %3, #4                  \n"

                        "st1    {v30.4h, v31.4h}, [%0], #16 \n"

                        : "=r"(outptr0), // %0
                        "=r"(r0),      // %1
                        "=r"(r1),      // %2
                        "=r"(r2)       // %3
                        : "0"(outptr0),
                        "1"(r0),
                        "2"(r1),
                        "3"(r2),
                        "w"(_k00), // %8
                        "w"(_k01), // %9
                        "w"(_k02), // %10
                        "w"(_k10), // %11
                        "w"(_k11), // %12
                        "w"(_k12), // %13
                        "w"(_k20), // %14
                        "w"(_k21), // %15
                        "w"(_k22)  // %16
                        : "cc", "memory", "v0", "v1", "v2", "v30", "v31");
                }
                for (; j < outw; j++)
                {
                    asm volatile(
                        "prfm   pldl1keep, [%0, #64]        \n"
                        "ld1    {v30.4h}, [%0]              \n" // sum0

                        "prfm   pldl1keep, [%1, #64]        \n"
                        "ld1    {v0.4h}, [%1]               \n" // r0

                        "fmla   v30.4h, %8.4h, v0.h[0]      \n"
                        "fmla   v30.4h, %9.4h, v0.h[1]      \n"
                        "fmla   v30.4h, %10.4h, v0.h[2]     \n"

                        "prfm   pldl1keep, [%2, #64]        \n"
                        "ld1    {v1.4h}, [%2]               \n" // r1

                        "fmla   v30.4h, %11.4h, v1.h[0]     \n"
                        "fmla   v30.4h, %12.4h, v1.h[1]     \n"
                        "fmla   v30.4h, %13.4h, v1.h[2]     \n"

                        "prfm   pldl1keep, [%3, #64]        \n"
                        "ld1    {v2.4h}, [%3]               \n" // r2

                        "fmla   v30.4h, %14.4h, v2.h[0]     \n"
                        "fmla   v30.4h, %15.4h, v2.h[1]     \n"
                        "fmla   v30.4h, %16.4h, v2.h[2]     \n"

                        "add    %1, %1, #2                  \n"
                        "add    %2, %2, #2                  \n"
                        "add    %3, %3, #2                  \n"

                        "st1    {v30.4h}, [%0], #8          \n"

                        : "=r"(outptr0), // %0
                        "=r"(r0),      // %1
                        "=r"(r1),      // %2
                        "=r"(r2)       // %3
                        : "0"(outptr0),
                        "1"(r0),
                        "2"(r1),
                        "3"(r2),
                        "w"(_k00), // %8
                        "w"(_k01), // %9
                        "w"(_k02), // %10
                        "w"(_k10), // %11
                        "w"(_k11), // %12
                        "w"(_k12), // %13
                        "w"(_k20), // %14
                        "w"(_k21), // %15
                        "w"(_k22)  // %16
                        : "cc", "memory", "v0", "v1", "v2", "v30");
                }

                r0 += 2;
                r1 += 2;
                r2 += 2;
            }

            k0 += 9 * 4;
        }
    }
}

static void conv3x3s2_pack1to4_fp16sa_neon(const Mat& bottom_blob, Mat& top_blob, const Mat& kernel, const Mat& _bias, const Option& opt)
{
    int w = bottom_blob.w;
    int inch = bottom_blob.c;
    int outw = top_blob.w;
    int outh = top_blob.h;
    int outch = top_blob.c;

    const int tailstep = w - 2 * outw + w;

    const __fp16* bias = _bias;

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int p = 0; p < outch; p++)
    {
        Mat out0 = top_blob.channel(p);

        float16x4_t _bias0 = bias ? vld1_f16(bias + p * 4) : vdup_n_f16((__fp16)0.f);
        out0.fill(_bias0);

        const __fp16* k0 = kernel.channel(p);

        int q = 0;
        for (; q < inch; q++)
        {
            __fp16* outptr0 = out0;

            const Mat img0 = bottom_blob.channel(q);

            const __fp16* r0 = img0.row<const __fp16>(0);
            const __fp16* r1 = img0.row<const __fp16>(1);
            const __fp16* r2 = img0.row<const __fp16>(2);

            float16x4_t _k00 = vld1_f16(k0);
            float16x4_t _k01 = vld1_f16(k0 + 4);
            float16x4_t _k02 = vld1_f16(k0 + 8);
            float16x4_t _k10 = vld1_f16(k0 + 12);
            float16x4_t _k11 = vld1_f16(k0 + 16);
            float16x4_t _k12 = vld1_f16(k0 + 20);
            float16x4_t _k20 = vld1_f16(k0 + 24);
            float16x4_t _k21 = vld1_f16(k0 + 28);
            float16x4_t _k22 = vld1_f16(k0 + 32);

            int i = 0;
            for (; i < outh; i++)
            {
                int j = 0;
                for (; j + 3 < outw; j += 4)
                {
                    asm volatile(
                        "prfm   pldl1keep, [%0, #256]       \n"
                        "ld1    {v28.4h, v29.4h, v30.4h, v31.4h}, [%0] \n" // sum0 sum1 sum2 sum3

                        "prfm   pldl1keep, [%1, #128]       \n"
                        "ld1    {v0.8h}, [%1], #16          \n" // r0
                        "ld1    {v1.h}[0], [%1]             \n"

                        "fmla   v28.4h, %8.4h, v0.h[0]      \n"
                        "fmla   v29.4h, %8.4h, v0.h[2]      \n"
                        "fmla   v30.4h, %8.4h, v0.h[4]      \n"
                        "fmla   v31.4h, %8.4h, v0.h[6]      \n"

                        "fmla   v28.4h, %9.4h, v0.h[1]      \n"
                        "fmla   v29.4h, %9.4h, v0.h[3]      \n"
                        "fmla   v30.4h, %9.4h, v0.h[5]      \n"
                        "fmla   v31.4h, %9.4h, v0.h[7]      \n"

                        "fmla   v28.4h, %10.4h, v0.h[2]     \n"
                        "fmla   v29.4h, %10.4h, v0.h[4]     \n"
                        "fmla   v30.4h, %10.4h, v0.h[6]     \n"
                        "fmla   v31.4h, %10.4h, v1.h[0]     \n"

                        "prfm   pldl1keep, [%2, #128]       \n"
                        "ld1    {v2.8h}, [%2], #16          \n" // r1
                        "ld1    {v3.h}[0], [%2]             \n"

                        "fmla   v28.4h, %11.4h, v2.h[0]     \n"
                        "fmla   v29.4h, %11.4h, v2.h[2]     \n"
                        "fmla   v30.4h, %11.4h, v2.h[4]     \n"
                        "fmla   v31.4h, %11.4h, v2.h[6]     \n"

                        "fmla   v28.4h, %12.4h, v2.h[1]     \n"
                        "fmla   v29.4h, %12.4h, v2.h[3]     \n"
                        "fmla   v30.4h, %12.4h, v2.h[5]     \n"
                        "fmla   v31.4h, %12.4h, v2.h[7]     \n"

                        "fmla   v28.4h, %13.4h, v2.h[2]     \n"
                        "fmla   v29.4h, %13.4h, v2.h[4]     \n"
                        "fmla   v30.4h, %13.4h, v2.h[6]     \n"
                        "fmla   v31.4h, %13.4h, v3.h[0]     \n"

                        "prfm   pldl1keep, [%3, #128]       \n"
                        "ld1    {v4.8h}, [%3], #16          \n" // r2
                        "ld1    {v5.h}[0], [%3]             \n"

                        "fmla   v28.4h, %14.4h, v4.h[0]     \n"
                        "fmla   v29.4h, %14.4h, v4.h[2]     \n"
                        "fmla   v30.4h, %14.4h, v4.h[4]     \n"
                        "fmla   v31.4h, %14.4h, v4.h[6]     \n"

                        "fmla   v28.4h, %15.4h, v4.h[1]     \n"
                        "fmla   v29.4h, %15.4h, v4.h[3]     \n"
                        "fmla   v30.4h, %15.4h, v4.h[5]     \n"
                        "fmla   v31.4h, %15.4h, v4.h[7]     \n"

                        "fmla   v28.4h, %16.4h, v4.h[2]     \n"
                        "fmla   v29.4h, %16.4h, v4.h[4]     \n"
                        "fmla   v30.4h, %16.4h, v4.h[6]     \n"
                        "fmla   v31.4h, %16.4h, v5.h[0]     \n"

                        "st1    {v28.4h, v29.4h, v30.4h, v31.4h}, [%0], #32 \n"

                        : "=r"(outptr0), // %0
                        "=r"(r0),      // %1
                        "=r"(r1),      // %2
                        "=r"(r2)       // %3
                        : "0"(outptr0),
                        "1"(r0),
                        "2"(r1),
                        "3"(r2),
                        "w"(_k00), // %8
                        "w"(_k01), // %9
                        "w"(_k02), // %10
                        "w"(_k10), // %11
                        "w"(_k11), // %12
                        "w"(_k12), // %13
                        "w"(_k20), // %14
                        "w"(_k21), // %15
                        "w"(_k22)  // %16
                        : "cc", "memory", "v0", "v1", "v2", "v3", "v4", "v5", "v28", "v29", "v30", "v31");
                }
                for (; j + 1 < outw; j += 2)
                {
                    asm volatile(
                        "prfm   pldl1keep, [%0, #128]       \n"
                        "ld1    {v30.4h, v31.4h}, [%0]      \n" // sum0 sum1

                        "prfm   pldl1keep, [%1, #64]        \n"
                        "ld1    {v0.4h}, [%1], #8           \n" // r0
                        "ld1    {v1.h}[0], [%1]             \n"

                        "fmla   v30.4h, %8.4h, v0.h[0]      \n"
                        "fmla   v31.4h, %8.4h, v0.h[2]      \n"
                        "fmla   v30.4h, %9.4h, v0.h[1]      \n"
                        "fmla   v31.4h, %9.4h, v0.h[3]      \n"
                        "fmla   v30.4h, %10.4h, v0.h[2]     \n"
                        "fmla   v31.4h, %10.4h, v1.h[0]     \n"

                        "prfm   pldl1keep, [%2, #64]        \n"
                        "ld1    {v2.4h}, [%2], #8           \n" // r1
                        "ld1    {v3.h}[0], [%2]             \n"

                        "fmla   v30.4h, %11.4h, v2.h[0]     \n"
                        "fmla   v31.4h, %11.4h, v2.h[2]     \n"
                        "fmla   v30.4h, %12.4h, v2.h[1]     \n"
                        "fmla   v31.4h, %12.4h, v2.h[3]     \n"
                        "fmla   v30.4h, %13.4h, v2.h[2]     \n"
                        "fmla   v31.4h, %13.4h, v3.h[0]     \n"

                        "prfm   pldl1keep, [%3, #64]        \n"
                        "ld1    {v4.4h}, [%3], #8           \n" // r2
                        "ld1    {v5.h}[0], [%3]             \n"

                        "fmla   v30.4h, %14.4h, v4.h[0]     \n"
                        "fmla   v31.4h, %14.4h, v4.h[2]     \n"
                        "fmla   v30.4h, %15.4h, v4.h[1]     \n"
                        "fmla   v31.4h, %15.4h, v4.h[3]     \n"
                        "fmla   v30.4h, %16.4h, v4.h[2]     \n"
                        "fmla   v31.4h, %16.4h, v5.h[0]     \n"

                        "st1    {v30.4h, v31.4h}, [%0], #16 \n"

                        : "=r"(outptr0), // %0
                        "=r"(r0),      // %1
                        "=r"(r1),      // %2
                        "=r"(r2)       // %3
                        : "0"(outptr0),
                        "1"(r0),
                        "2"(r1),
                        "3"(r2),
                        "w"(_k00), // %8
                        "w"(_k01), // %9
                        "w"(_k02), // %10
                        "w"(_k10), // %11
                        "w"(_k11), // %12
                        "w"(_k12), // %13
                        "w"(_k20), // %14
                        "w"(_k21), // %15
                        "w"(_k22)  // %16
                        : "cc", "memory", "v0", "v1", "v2", "v3", "v4", "v5", "v30", "v31");
                }
                for (; j < outw; j++)
                {
                    asm volatile(
                        "prfm   pldl1keep, [%0, #64]        \n"
                        "ld1    {v30.4h}, [%0]              \n" // sum0

                        "prfm   pldl1keep, [%1, #64]        \n"
                        "ld1    {v0.4h}, [%1]               \n" // r0

                        "fmla   v30.4h, %8.4h, v0.h[0]      \n"
                        "fmla   v30.4h, %9.4h, v0.h[1]      \n"
                        "fmla   v30.4h, %10.4h, v0.h[2]     \n"

                        "prfm   pldl1keep, [%2, #64]        \n"
                        "ld1    {v1.4h}, [%2]               \n" // r1

                        "fmla   v30.4h, %11.4h, v1.h[0]     \n"
                        "fmla   v30.4h, %12.4h, v1.h[1]     \n"
                        "fmla   v30.4h, %13.4h, v1.h[2]     \n"

                        "prfm   pldl1keep, [%3, #64]        \n"
                        "ld1    {v2.4h}, [%3]               \n" // r2

                        "fmla   v30.4h, %14.4h, v2.h[0]     \n"
                        "fmla   v30.4h, %15.4h, v2.h[1]     \n"
                        "fmla   v30.4h, %16.4h, v2.h[2]     \n"

                        "add    %1, %1, #4                  \n"
                        "add    %2, %2, #4                  \n"
                        "add    %3, %3, #4                  \n"

                        "st1    {v30.4h}, [%0], #8          \n"

                        : "=r"(outptr0), // %0
                        "=r"(r0),      // %1
                        "=r"(r1),      // %2
                        "=r"(r2)       // %3
                        : "0"(outptr0),
                        "1"(r0),
                        "2"(r1),
                        "3"(r2),
                        "w"(_k00), // %8
                        "w"(_k01), // %9
                        "w"(_k02), // %10
                        "w"(_k10), // %11
                        "w"(_k11), // %12
                        "w"(_k12), // %13
                        "w"(_k20), // %14
                        "w"(_k21), // %15
                        "w"(_k22)  // %16
                        : "cc", "memory", "v0", "v1", "v2", "v30");
                }

                r0 += tailstep;
                r1 += tailstep;
                r2 += tailstep;
            }

            k0 += 9 * 4;
        }
    }
}
