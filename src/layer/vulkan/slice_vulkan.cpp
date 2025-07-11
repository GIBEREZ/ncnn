// Copyright 2019 Tencent
// SPDX-License-Identifier: BSD-3-Clause

#include "slice_vulkan.h"

#include "layer_shader_type.h"

namespace ncnn {

Slice_vulkan::Slice_vulkan()
{
    support_vulkan = true;

    pipeline_slice[0] = 0;
    pipeline_slice[1] = 0;
    pipeline_slice_pack4[0] = 0;
    pipeline_slice_pack4[1] = 0;
    pipeline_slice_pack1to4[0] = 0;
    pipeline_slice_pack1to4[1] = 0;
    pipeline_slice_pack8[0] = 0;
    pipeline_slice_pack8[1] = 0;
    pipeline_slice_pack1to8[0] = 0;
    pipeline_slice_pack1to8[1] = 0;
    pipeline_slice_pack4to8[0] = 0;
    pipeline_slice_pack4to8[1] = 0;
}

int Slice_vulkan::create_pipeline(const Option& opt)
{
    const Mat& shape = bottom_shapes.empty() ? Mat() : bottom_shapes[0];
    const Mat& out_shape = top_shapes.empty() ? Mat() : top_shapes[0];
    int positive_axis = axis < 0 ? shape.dims + axis : axis;

    int elempack = 1;
    if (shape.dims == 1) elempack = opt.use_shader_pack8 && shape.w % 8 == 0 ? 8 : shape.w % 4 == 0 ? 4 : 1;
    if (shape.dims == 2) elempack = opt.use_shader_pack8 && shape.h % 8 == 0 ? 8 : shape.h % 4 == 0 ? 4 : 1;
    if (shape.dims == 3 || shape.dims == 4) elempack = opt.use_shader_pack8 && shape.c % 8 == 0 ? 8 : shape.c % 4 == 0 ? 4 : 1;

    int out_elempack = 1;
    if (positive_axis == 0)
    {
        if (out_shape.dims == 1) out_elempack = opt.use_shader_pack8 && out_shape.w % 8 == 0 ? 8 : out_shape.w % 4 == 0 ? 4 : 1;
        if (out_shape.dims == 2) out_elempack = opt.use_shader_pack8 && out_shape.h % 8 == 0 ? 8 : out_shape.h % 4 == 0 ? 4 : 1;
        if (out_shape.dims == 3 || out_shape.dims == 4) out_elempack = opt.use_shader_pack8 && out_shape.c % 8 == 0 ? 8 : out_shape.c % 4 == 0 ? 4 : 1;

        for (size_t b = 1; b < top_shapes.size(); b++)
        {
            const Mat& shape1 = top_shapes[b];

            int out_elempack1 = 1;
            if (shape1.dims == 1) out_elempack1 = opt.use_shader_pack8 && shape1.w % 8 == 0 ? 8 : shape1.w % 4 == 0 ? 4 : 1;
            if (shape1.dims == 2) out_elempack1 = opt.use_shader_pack8 && shape1.h % 8 == 0 ? 8 : shape1.h % 4 == 0 ? 4 : 1;
            if (shape1.dims == 3 || shape1.dims == 4) out_elempack1 = opt.use_shader_pack8 && shape1.c % 8 == 0 ? 8 : shape1.c % 4 == 0 ? 4 : 1;

            out_elempack = std::min(out_elempack, out_elempack1);
        }
    }
    else
    {
        out_elempack = elempack;
    }

    size_t out_elemsize;
    if (opt.use_fp16_storage || opt.use_fp16_packed)
    {
        out_elemsize = out_elempack * 2u;
    }
    else
    {
        out_elemsize = out_elempack * 4u;
    }

    Mat shape_unpacked;
    if (shape.dims == 1) shape_unpacked = Mat(shape.w / out_elempack, (void*)0, out_elemsize, out_elempack);
    if (shape.dims == 2) shape_unpacked = Mat(shape.w, shape.h / out_elempack, (void*)0, out_elemsize, out_elempack);
    if (shape.dims == 3) shape_unpacked = Mat(shape.w, shape.h, shape.c / out_elempack, (void*)0, out_elemsize, out_elempack);
    if (shape.dims == 4) shape_unpacked = Mat(shape.w, shape.h, shape.d, shape.c / out_elempack, (void*)0, out_elemsize, out_elempack);

    std::vector<vk_specialization_type> specializations(1 + 12);
    specializations[0].i = axis;
    specializations[1 + 0].i = shape_unpacked.dims;
    specializations[1 + 1].i = shape_unpacked.w;
    specializations[1 + 2].i = shape_unpacked.h;
    specializations[1 + 3].i = shape_unpacked.d;
    specializations[1 + 4].i = shape_unpacked.c;
    specializations[1 + 5].i = shape_unpacked.cstep;
    specializations[1 + 6].i = 0; // TODO handle out_shape_packed for slice2
    specializations[1 + 7].i = 0;
    specializations[1 + 8].i = 0;
    specializations[1 + 9].i = 0;
    specializations[1 + 10].i = 0;
    specializations[1 + 11].i = 0;

    Mat local_size_xyz; // TODO more precise group size guessed from shape_unpacked
    if (shape_unpacked.dims == 1)
    {
        local_size_xyz.w = 64;
        local_size_xyz.h = 1;
        local_size_xyz.c = 1;
    }
    if (shape_unpacked.dims == 2)
    {
        local_size_xyz.w = 8;
        local_size_xyz.h = 8;
        local_size_xyz.c = 1;
    }
    if (shape_unpacked.dims == 3)
    {
        local_size_xyz.w = 4;
        local_size_xyz.h = 4;
        local_size_xyz.c = 4;
    }

    // pack1
    if (shape.dims == 0 || out_elempack == 1)
    {
        pipeline_slice[0] = new Pipeline(vkdev);
        pipeline_slice[0]->set_optimal_local_size_xyz(local_size_xyz);
        pipeline_slice[0]->create(LayerShaderType::slice, opt, specializations);
        pipeline_slice[1] = new Pipeline(vkdev);
        pipeline_slice[1]->set_optimal_local_size_xyz(local_size_xyz);
        pipeline_slice[1]->create(LayerShaderType::slice, opt, specializations);
    }

    // pack4
    if (shape.dims == 0 || out_elempack == 4)
    {
        pipeline_slice_pack4[0] = new Pipeline(vkdev);
        pipeline_slice_pack4[0]->set_optimal_local_size_xyz(local_size_xyz);
        pipeline_slice_pack4[0]->create(LayerShaderType::slice_pack4, opt, specializations);
        pipeline_slice_pack4[1] = new Pipeline(vkdev);
        pipeline_slice_pack4[1]->set_optimal_local_size_xyz(local_size_xyz);
        pipeline_slice_pack4[1]->create(LayerShaderType::slice_pack4, opt, specializations);
    }

    // pack1to4
    if ((positive_axis <= 0 && shape.dims == 0) || out_elempack == 1)
    {
        pipeline_slice_pack1to4[0] = new Pipeline(vkdev);
        pipeline_slice_pack1to4[0]->set_optimal_local_size_xyz(local_size_xyz);
        pipeline_slice_pack1to4[0]->create(LayerShaderType::slice_pack1to4, opt, specializations);
        pipeline_slice_pack1to4[1] = new Pipeline(vkdev);
        pipeline_slice_pack1to4[1]->set_optimal_local_size_xyz(local_size_xyz);
        pipeline_slice_pack1to4[1]->create(LayerShaderType::slice_pack1to4, opt, specializations);
    }

    // pack8
    if (opt.use_shader_pack8 && (shape.dims == 0 || out_elempack == 8))
    {
        pipeline_slice_pack8[0] = new Pipeline(vkdev);
        pipeline_slice_pack8[0]->set_optimal_local_size_xyz(local_size_xyz);
        pipeline_slice_pack8[0]->create(LayerShaderType::slice_pack8, opt, specializations);
        pipeline_slice_pack8[1] = new Pipeline(vkdev);
        pipeline_slice_pack8[1]->set_optimal_local_size_xyz(local_size_xyz);
        pipeline_slice_pack8[1]->create(LayerShaderType::slice_pack8, opt, specializations);
    }

    // pack1to8
    if (opt.use_shader_pack8 && ((positive_axis <= 0 && shape.dims == 0) || out_elempack == 1))
    {
        pipeline_slice_pack1to8[0] = new Pipeline(vkdev);
        pipeline_slice_pack1to8[0]->set_optimal_local_size_xyz(local_size_xyz);
        pipeline_slice_pack1to8[0]->create(LayerShaderType::slice_pack1to8, opt, specializations);
        pipeline_slice_pack1to8[1] = new Pipeline(vkdev);
        pipeline_slice_pack1to8[1]->set_optimal_local_size_xyz(local_size_xyz);
        pipeline_slice_pack1to8[1]->create(LayerShaderType::slice_pack1to8, opt, specializations);
    }

    // pack4to8
    if (opt.use_shader_pack8 && ((positive_axis <= 0 && shape.dims == 0) || out_elempack == 4))
    {
        pipeline_slice_pack4to8[0] = new Pipeline(vkdev);
        pipeline_slice_pack4to8[0]->set_optimal_local_size_xyz(local_size_xyz);
        pipeline_slice_pack4to8[0]->create(LayerShaderType::slice_pack4to8, opt, specializations);
        pipeline_slice_pack4to8[1] = new Pipeline(vkdev);
        pipeline_slice_pack4to8[1]->set_optimal_local_size_xyz(local_size_xyz);
        pipeline_slice_pack4to8[1]->create(LayerShaderType::slice_pack4to8, opt, specializations);
    }

    return 0;
}

int Slice_vulkan::destroy_pipeline(const Option& /*opt*/)
{
    delete pipeline_slice[0];
    delete pipeline_slice[1];
    pipeline_slice[0] = 0;
    pipeline_slice[1] = 0;

    delete pipeline_slice_pack4[0];
    delete pipeline_slice_pack4[1];
    pipeline_slice_pack4[0] = 0;
    pipeline_slice_pack4[1] = 0;

    delete pipeline_slice_pack1to4[0];
    delete pipeline_slice_pack1to4[1];
    pipeline_slice_pack1to4[0] = 0;
    pipeline_slice_pack1to4[1] = 0;

    delete pipeline_slice_pack8[0];
    delete pipeline_slice_pack8[1];
    pipeline_slice_pack8[0] = 0;
    pipeline_slice_pack8[1] = 0;

    delete pipeline_slice_pack1to8[0];
    delete pipeline_slice_pack1to8[1];
    pipeline_slice_pack1to8[0] = 0;
    pipeline_slice_pack1to8[1] = 0;

    delete pipeline_slice_pack4to8[0];
    delete pipeline_slice_pack4to8[1];
    pipeline_slice_pack4to8[0] = 0;
    pipeline_slice_pack4to8[1] = 0;

    return 0;
}

int Slice_vulkan::forward(const std::vector<VkMat>& bottom_blobs, std::vector<VkMat>& top_blobs, VkCompute& cmd, const Option& opt) const
{
    const VkMat& bottom_blob = bottom_blobs[0];
    int dims = bottom_blob.dims;
    size_t elemsize = bottom_blob.elemsize;
    int elempack = bottom_blob.elempack;
    const int* slices_ptr = slices;
    const int* indices_ptr = indices;
    int positive_axis = axis < 0 ? dims + axis : axis;

    if (dims == 1) // positive_axis == 0
    {
        // slice vector
        int w = bottom_blob.w * elempack;
        int q = 0;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            int slice;
            if (indices_ptr)
            {
                if (i == top_blobs.size() - 1)
                {
                    slice = w - q;
                }
                else
                {
                    int indice = indices_ptr[i];
                    int positive_indice = indice < 0 ? w + indice : indice;
                    slice = positive_indice - q;
                }
            }
            else
            {
                slice = slices_ptr[i];
                if (slice == -233)
                {
                    slice = static_cast<int>((w - q) / (top_blobs.size() - i));
                }
            }

            int out_elempack = opt.use_shader_pack8 && slice % 8 == 0 ? 8 : slice % 4 == 0 ? 4 : 1;
            size_t out_elemsize = elemsize / elempack * out_elempack;

            VkMat& top_blob = top_blobs[i];
            top_blob.create(slice / out_elempack, out_elemsize, out_elempack, opt.blob_vkallocator);
            if (top_blob.empty())
                return -100;

            q += slice;
        }

        int out_elempack = top_blobs[0].elempack;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            out_elempack = std::min(out_elempack, top_blobs[i].elempack);
        }

        VkMat bottom_blob_unpacked = bottom_blob;
        if (elempack > out_elempack)
        {
            vkdev->convert_packing(bottom_blob, bottom_blob_unpacked, out_elempack, cmd, opt);
        }

        int woffset = 0;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            VkMat& top_blob = top_blobs[i];

            std::vector<VkMat> bindings(2);
            bindings[0] = bottom_blob_unpacked;
            bindings[1] = top_blob;

            std::vector<vk_constant_type> constants(13);
            constants[0].i = bottom_blob_unpacked.dims;
            constants[1].i = bottom_blob_unpacked.w;
            constants[2].i = bottom_blob_unpacked.h;
            constants[3].i = bottom_blob_unpacked.d;
            constants[4].i = bottom_blob_unpacked.c;
            constants[5].i = bottom_blob_unpacked.cstep;
            constants[6].i = top_blob.dims;
            constants[7].i = top_blob.w;
            constants[8].i = top_blob.h;
            constants[9].i = top_blob.d;
            constants[10].i = top_blob.c;
            constants[11].i = top_blob.cstep;
            constants[12].i = woffset;

            const Pipeline* pipeline = 0;
            if (out_elempack == 1 && top_blob.elempack == 1)
            {
                pipeline = pipeline_slice[i % 2];
            }
            else if (out_elempack == 4 && top_blob.elempack == 4)
            {
                pipeline = pipeline_slice_pack4[i % 2];
            }
            else if (out_elempack == 1 && top_blob.elempack == 4)
            {
                pipeline = pipeline_slice_pack1to4[i % 2];
            }
            else if (out_elempack == 8 && top_blob.elempack == 8)
            {
                pipeline = pipeline_slice_pack8[i % 2];
            }
            else if (out_elempack == 1 && top_blob.elempack == 8)
            {
                pipeline = pipeline_slice_pack1to8[i % 2];
            }
            else if (out_elempack == 4 && top_blob.elempack == 8)
            {
                pipeline = pipeline_slice_pack4to8[i % 2];
            }

            cmd.record_pipeline(pipeline, bindings, constants, top_blob);

            woffset += top_blob.w * top_blob.elempack / out_elempack;
        }

        return 0;
    }

    if (dims == 2 && positive_axis == 0)
    {
        // slice image height
        int w = bottom_blob.w;
        int h = bottom_blob.h * elempack;

        int q = 0;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            int slice;
            if (indices_ptr)
            {
                if (i == top_blobs.size() - 1)
                {
                    slice = h - q;
                }
                else
                {
                    int indice = indices_ptr[i];
                    int positive_indice = indice < 0 ? h + indice : indice;
                    slice = positive_indice - q;
                }
            }
            else
            {
                slice = slices_ptr[i];
                if (slice == -233)
                {
                    slice = static_cast<int>((h - q) / (top_blobs.size() - i));
                }
            }

            int out_elempack = opt.use_shader_pack8 && slice % 8 == 0 ? 8 : slice % 4 == 0 ? 4 : 1;
            size_t out_elemsize = elemsize / elempack * out_elempack;

            VkMat& top_blob = top_blobs[i];
            top_blob.create(w, slice / out_elempack, out_elemsize, out_elempack, opt.blob_vkallocator);
            if (top_blob.empty())
                return -100;

            q += slice;
        }

        int out_elempack = top_blobs[0].elempack;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            out_elempack = std::min(out_elempack, top_blobs[i].elempack);
        }

        VkMat bottom_blob_unpacked = bottom_blob;
        if (elempack > out_elempack)
        {
            vkdev->convert_packing(bottom_blob, bottom_blob_unpacked, out_elempack, cmd, opt);
        }

        int hoffset = 0;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            VkMat& top_blob = top_blobs[i];

            std::vector<VkMat> bindings(2);
            bindings[0] = bottom_blob_unpacked;
            bindings[1] = top_blob;

            std::vector<vk_constant_type> constants(13);
            constants[0].i = bottom_blob_unpacked.dims;
            constants[1].i = bottom_blob_unpacked.w;
            constants[2].i = bottom_blob_unpacked.h;
            constants[3].i = bottom_blob_unpacked.d;
            constants[4].i = bottom_blob_unpacked.c;
            constants[5].i = bottom_blob_unpacked.cstep;
            constants[6].i = top_blob.dims;
            constants[7].i = top_blob.w;
            constants[8].i = top_blob.h;
            constants[9].i = top_blob.d;
            constants[10].i = top_blob.c;
            constants[11].i = top_blob.cstep;
            constants[12].i = hoffset;

            const Pipeline* pipeline = 0;
            if (out_elempack == 1 && top_blob.elempack == 1)
            {
                pipeline = pipeline_slice[i % 2];
            }
            else if (out_elempack == 4 && top_blob.elempack == 4)
            {
                pipeline = pipeline_slice_pack4[i % 2];
            }
            else if (out_elempack == 1 && top_blob.elempack == 4)
            {
                pipeline = pipeline_slice_pack1to4[i % 2];
            }
            else if (out_elempack == 8 && top_blob.elempack == 8)
            {
                pipeline = pipeline_slice_pack8[i % 2];
            }
            else if (out_elempack == 1 && top_blob.elempack == 8)
            {
                pipeline = pipeline_slice_pack1to8[i % 2];
            }
            else if (out_elempack == 4 && top_blob.elempack == 8)
            {
                pipeline = pipeline_slice_pack4to8[i % 2];
            }

            cmd.record_pipeline(pipeline, bindings, constants, top_blob);

            hoffset += top_blob.h * top_blob.elempack / out_elempack;
        }

        return 0;
    }

    if (dims == 2 && positive_axis == 1)
    {
        // slice image width
        int w = bottom_blob.w;
        int h = bottom_blob.h;

        int q = 0;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            int slice;
            if (indices_ptr)
            {
                if (i == top_blobs.size() - 1)
                {
                    slice = w - q;
                }
                else
                {
                    int indice = indices_ptr[i];
                    int positive_indice = indice < 0 ? w + indice : indice;
                    slice = positive_indice - q;
                }
            }
            else
            {
                slice = slices_ptr[i];
                if (slice == -233)
                {
                    slice = static_cast<int>((w - q) / (top_blobs.size() - i));
                }
            }

            VkMat& top_blob = top_blobs[i];
            top_blob.create(slice, h, elemsize, elempack, opt.blob_vkallocator);
            if (top_blob.empty())
                return -100;

            q += slice;
        }

        int woffset = 0;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            VkMat& top_blob = top_blobs[i];

            std::vector<VkMat> bindings(2);
            bindings[0] = bottom_blob;
            bindings[1] = top_blob;

            std::vector<vk_constant_type> constants(13);
            constants[0].i = bottom_blob.dims;
            constants[1].i = bottom_blob.w;
            constants[2].i = bottom_blob.h;
            constants[3].i = bottom_blob.d;
            constants[4].i = bottom_blob.c;
            constants[5].i = bottom_blob.cstep;
            constants[6].i = top_blob.dims;
            constants[7].i = top_blob.w;
            constants[8].i = top_blob.h;
            constants[9].i = top_blob.d;
            constants[10].i = top_blob.c;
            constants[11].i = top_blob.cstep;
            constants[12].i = woffset;

            const Pipeline* pipeline = elempack == 8 ? pipeline_slice_pack8[i % 2]
                                       : elempack == 4 ? pipeline_slice_pack4[i % 2]
                                       : pipeline_slice[i % 2];

            cmd.record_pipeline(pipeline, bindings, constants, top_blob);

            woffset += top_blob.w;
        }

        return 0;
    }

    if (dims == 3 && positive_axis == 0)
    {
        // slice dim channel
        int w = bottom_blob.w;
        int h = bottom_blob.h;
        int channels = bottom_blob.c * elempack;

        int q = 0;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            int slice;
            if (indices_ptr)
            {
                if (i == top_blobs.size() - 1)
                {
                    slice = channels - q;
                }
                else
                {
                    int indice = indices_ptr[i];
                    int positive_indice = indice < 0 ? channels + indice : indice;
                    slice = positive_indice - q;
                }
            }
            else
            {
                slice = slices_ptr[i];
                if (slice == -233)
                {
                    slice = static_cast<int>((channels - q) / (top_blobs.size() - i));
                }
            }

            int out_elempack = opt.use_shader_pack8 && slice % 8 == 0 ? 8 : slice % 4 == 0 ? 4 : 1;
            size_t out_elemsize = elemsize / elempack * out_elempack;

            VkMat& top_blob = top_blobs[i];
            top_blob.create(w, h, slice / out_elempack, out_elemsize, out_elempack, opt.blob_vkallocator);
            if (top_blob.empty())
                return -100;

            q += slice;
        }

        int out_elempack = top_blobs[0].elempack;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            out_elempack = std::min(out_elempack, top_blobs[i].elempack);
        }

        VkMat bottom_blob_unpacked = bottom_blob;
        if (elempack > out_elempack)
        {
            vkdev->convert_packing(bottom_blob, bottom_blob_unpacked, out_elempack, cmd, opt);
        }

        int coffset = 0;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            VkMat& top_blob = top_blobs[i];

            std::vector<VkMat> bindings(2);
            bindings[0] = bottom_blob_unpacked;
            bindings[1] = top_blob;

            std::vector<vk_constant_type> constants(13);
            constants[0].i = bottom_blob_unpacked.dims;
            constants[1].i = bottom_blob_unpacked.w;
            constants[2].i = bottom_blob_unpacked.h;
            constants[3].i = bottom_blob_unpacked.d;
            constants[4].i = bottom_blob_unpacked.c;
            constants[5].i = bottom_blob_unpacked.cstep;
            constants[6].i = top_blob.dims;
            constants[7].i = top_blob.w;
            constants[8].i = top_blob.h;
            constants[9].i = top_blob.d;
            constants[10].i = top_blob.c;
            constants[11].i = top_blob.cstep;
            constants[12].i = coffset;

            const Pipeline* pipeline = 0;
            if (out_elempack == 1 && top_blob.elempack == 1)
            {
                pipeline = pipeline_slice[i % 2];
            }
            else if (out_elempack == 4 && top_blob.elempack == 4)
            {
                pipeline = pipeline_slice_pack4[i % 2];
            }
            else if (out_elempack == 1 && top_blob.elempack == 4)
            {
                pipeline = pipeline_slice_pack1to4[i % 2];
            }
            else if (out_elempack == 8 && top_blob.elempack == 8)
            {
                pipeline = pipeline_slice_pack8[i % 2];
            }
            else if (out_elempack == 1 && top_blob.elempack == 8)
            {
                pipeline = pipeline_slice_pack1to8[i % 2];
            }
            else if (out_elempack == 4 && top_blob.elempack == 8)
            {
                pipeline = pipeline_slice_pack4to8[i % 2];
            }

            cmd.record_pipeline(pipeline, bindings, constants, top_blob);

            coffset += top_blob.c * top_blob.elempack / out_elempack;
        }

        return 0;
    }

    if (dims == 3 && positive_axis == 1)
    {
        // slice dim height
        int w = bottom_blob.w;
        int h = bottom_blob.h;
        int channels = bottom_blob.c;

        int q = 0;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            int slice;
            if (indices_ptr)
            {
                if (i == top_blobs.size() - 1)
                {
                    slice = h - q;
                }
                else
                {
                    int indice = indices_ptr[i];
                    int positive_indice = indice < 0 ? h + indice : indice;
                    slice = positive_indice - q;
                }
            }
            else
            {
                slice = slices_ptr[i];
                if (slice == -233)
                {
                    slice = static_cast<int>((h - q) / (top_blobs.size() - i));
                }
            }

            VkMat& top_blob = top_blobs[i];
            top_blob.create(w, slice, channels, elemsize, elempack, opt.blob_vkallocator);
            if (top_blob.empty())
                return -100;

            q += slice;
        }

        int hoffset = 0;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            VkMat& top_blob = top_blobs[i];

            std::vector<VkMat> bindings(2);
            bindings[0] = bottom_blob;
            bindings[1] = top_blob;

            std::vector<vk_constant_type> constants(13);
            constants[0].i = bottom_blob.dims;
            constants[1].i = bottom_blob.w;
            constants[2].i = bottom_blob.h;
            constants[3].i = bottom_blob.d;
            constants[4].i = bottom_blob.c;
            constants[5].i = bottom_blob.cstep;
            constants[6].i = top_blob.dims;
            constants[7].i = top_blob.w;
            constants[8].i = top_blob.h;
            constants[9].i = top_blob.d;
            constants[10].i = top_blob.c;
            constants[11].i = top_blob.cstep;
            constants[12].i = hoffset;

            const Pipeline* pipeline = elempack == 8 ? pipeline_slice_pack8[i % 2]
                                       : elempack == 4 ? pipeline_slice_pack4[i % 2]
                                       : pipeline_slice[i % 2];

            cmd.record_pipeline(pipeline, bindings, constants, top_blob);

            hoffset += top_blob.h;
        }

        return 0;
    }

    if (dims == 3 && positive_axis == 2)
    {
        // slice dim width
        int w = bottom_blob.w;
        int h = bottom_blob.h;
        int channels = bottom_blob.c;

        int q = 0;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            int slice;
            if (indices_ptr)
            {
                if (i == top_blobs.size() - 1)
                {
                    slice = w - q;
                }
                else
                {
                    int indice = indices_ptr[i];
                    int positive_indice = indice < 0 ? w + indice : indice;
                    slice = positive_indice - q;
                }
            }
            else
            {
                slice = slices_ptr[i];
                if (slice == -233)
                {
                    slice = static_cast<int>((w - q) / (top_blobs.size() - i));
                }
            }

            VkMat& top_blob = top_blobs[i];
            top_blob.create(slice, h, channels, elemsize, elempack, opt.blob_vkallocator);
            if (top_blob.empty())
                return -100;

            q += slice;
        }

        int woffset = 0;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            VkMat& top_blob = top_blobs[i];

            std::vector<VkMat> bindings(2);
            bindings[0] = bottom_blob;
            bindings[1] = top_blob;

            std::vector<vk_constant_type> constants(13);
            constants[0].i = bottom_blob.dims;
            constants[1].i = bottom_blob.w;
            constants[2].i = bottom_blob.h;
            constants[3].i = bottom_blob.d;
            constants[4].i = bottom_blob.c;
            constants[5].i = bottom_blob.cstep;
            constants[6].i = top_blob.dims;
            constants[7].i = top_blob.w;
            constants[8].i = top_blob.h;
            constants[9].i = top_blob.d;
            constants[10].i = top_blob.c;
            constants[11].i = top_blob.cstep;
            constants[12].i = woffset;

            const Pipeline* pipeline = elempack == 8 ? pipeline_slice_pack8[i % 2]
                                       : elempack == 4 ? pipeline_slice_pack4[i % 2]
                                       : pipeline_slice[i % 2];

            cmd.record_pipeline(pipeline, bindings, constants, top_blob);

            woffset += top_blob.w;
        }

        return 0;
    }

    if (dims == 4 && positive_axis == 0)
    {
        int w = bottom_blob.w;
        int h = bottom_blob.h;
        int d = bottom_blob.d;
        int channels = bottom_blob.c * elempack;

        int q = 0;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            int slice;
            if (indices_ptr)
            {
                if (i == top_blobs.size() - 1)
                {
                    slice = channels - q;
                }
                else
                {
                    int indice = indices_ptr[i];
                    int positive_indice = indice < 0 ? channels + indice : indice;
                    slice = positive_indice - q;
                }
            }
            else
            {
                slice = slices_ptr[i];
                if (slice == -233)
                {
                    slice = static_cast<int>((channels - q) / (top_blobs.size() - i));
                }
            }

            int out_elempack = opt.use_shader_pack8 && slice % 8 == 0 ? 8 : slice % 4 == 0 ? 4 : 1;
            size_t out_elemsize = elemsize / elempack * out_elempack;

            VkMat& top_blob = top_blobs[i];
            top_blob.create(w, h, d, slice / out_elempack, out_elemsize, out_elempack, opt.blob_vkallocator);
            if (top_blob.empty())
                return -100;

            q += slice;
        }

        int out_elempack = top_blobs[0].elempack;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            out_elempack = std::min(out_elempack, top_blobs[i].elempack);
        }

        VkMat bottom_blob_unpacked = bottom_blob;
        if (elempack > out_elempack)
        {
            vkdev->convert_packing(bottom_blob, bottom_blob_unpacked, out_elempack, cmd, opt);
        }

        int coffset = 0;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            VkMat& top_blob = top_blobs[i];

            std::vector<VkMat> bindings(2);
            bindings[0] = bottom_blob_unpacked;
            bindings[1] = top_blob;

            std::vector<vk_constant_type> constants(13);
            constants[0].i = bottom_blob_unpacked.dims;
            constants[1].i = bottom_blob_unpacked.w;
            constants[2].i = bottom_blob_unpacked.h;
            constants[3].i = bottom_blob_unpacked.d;
            constants[4].i = bottom_blob_unpacked.c;
            constants[5].i = bottom_blob_unpacked.cstep;
            constants[6].i = top_blob.dims;
            constants[7].i = top_blob.w;
            constants[8].i = top_blob.h;
            constants[9].i = top_blob.d;
            constants[10].i = top_blob.c;
            constants[11].i = top_blob.cstep;
            constants[12].i = coffset;

            const Pipeline* pipeline = 0;
            if (out_elempack == 1 && top_blob.elempack == 1)
            {
                pipeline = pipeline_slice[i % 2];
            }
            else if (out_elempack == 4 && top_blob.elempack == 4)
            {
                pipeline = pipeline_slice_pack4[i % 2];
            }
            else if (out_elempack == 1 && top_blob.elempack == 4)
            {
                pipeline = pipeline_slice_pack1to4[i % 2];
            }
            else if (out_elempack == 8 && top_blob.elempack == 8)
            {
                pipeline = pipeline_slice_pack8[i % 2];
            }
            else if (out_elempack == 1 && top_blob.elempack == 8)
            {
                pipeline = pipeline_slice_pack1to8[i % 2];
            }
            else if (out_elempack == 4 && top_blob.elempack == 8)
            {
                pipeline = pipeline_slice_pack4to8[i % 2];
            }

            cmd.record_pipeline(pipeline, bindings, constants, top_blob);

            coffset += top_blob.c * top_blob.elempack / out_elempack;
        }

        return 0;
    }

    if (dims == 4 && positive_axis == 1)
    {
        int w = bottom_blob.w;
        int h = bottom_blob.h;
        int d = bottom_blob.d;
        int channels = bottom_blob.c;

        int q = 0;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            int slice;
            if (indices_ptr)
            {
                if (i == top_blobs.size() - 1)
                {
                    slice = d - q;
                }
                else
                {
                    int indice = indices_ptr[i];
                    int positive_indice = indice < 0 ? d + indice : indice;
                    slice = positive_indice - q;
                }
            }
            else
            {
                slice = slices_ptr[i];
                if (slice == -233)
                {
                    slice = static_cast<int>((d - q) / (top_blobs.size() - i));
                }
            }

            VkMat& top_blob = top_blobs[i];
            top_blob.create(w, h, slice, channels, elemsize, elempack, opt.blob_vkallocator);
            if (top_blob.empty())
                return -100;

            q += slice;
        }

        int doffset = 0;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            VkMat& top_blob = top_blobs[i];

            std::vector<VkMat> bindings(2);
            bindings[0] = bottom_blob;
            bindings[1] = top_blob;

            std::vector<vk_constant_type> constants(13);
            constants[0].i = bottom_blob.dims;
            constants[1].i = bottom_blob.w;
            constants[2].i = bottom_blob.h;
            constants[3].i = bottom_blob.d;
            constants[4].i = bottom_blob.c;
            constants[5].i = bottom_blob.cstep;
            constants[6].i = top_blob.dims;
            constants[7].i = top_blob.w;
            constants[8].i = top_blob.h;
            constants[9].i = top_blob.d;
            constants[10].i = top_blob.c;
            constants[11].i = top_blob.cstep;
            constants[12].i = doffset;

            const Pipeline* pipeline = elempack == 8 ? pipeline_slice_pack8[i % 2]
                                       : elempack == 4 ? pipeline_slice_pack4[i % 2]
                                       : pipeline_slice[i % 2];

            cmd.record_pipeline(pipeline, bindings, constants, top_blob);

            doffset += top_blob.d;
        }

        return 0;
    }

    if (dims == 4 && positive_axis == 2)
    {
        int w = bottom_blob.w;
        int h = bottom_blob.h;
        int d = bottom_blob.d;
        int channels = bottom_blob.c;

        int q = 0;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            int slice;
            if (indices_ptr)
            {
                if (i == top_blobs.size() - 1)
                {
                    slice = h - q;
                }
                else
                {
                    int indice = indices_ptr[i];
                    int positive_indice = indice < 0 ? h + indice : indice;
                    slice = positive_indice - q;
                }
            }
            else
            {
                slice = slices_ptr[i];
                if (slice == -233)
                {
                    slice = static_cast<int>((h - q) / (top_blobs.size() - i));
                }
            }

            VkMat& top_blob = top_blobs[i];
            top_blob.create(w, slice, d, channels, elemsize, elempack, opt.blob_vkallocator);
            if (top_blob.empty())
                return -100;

            q += slice;
        }

        int hoffset = 0;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            VkMat& top_blob = top_blobs[i];

            std::vector<VkMat> bindings(2);
            bindings[0] = bottom_blob;
            bindings[1] = top_blob;

            std::vector<vk_constant_type> constants(13);
            constants[0].i = bottom_blob.dims;
            constants[1].i = bottom_blob.w;
            constants[2].i = bottom_blob.h;
            constants[3].i = bottom_blob.d;
            constants[4].i = bottom_blob.c;
            constants[5].i = bottom_blob.cstep;
            constants[6].i = top_blob.dims;
            constants[7].i = top_blob.w;
            constants[8].i = top_blob.h;
            constants[9].i = top_blob.d;
            constants[10].i = top_blob.c;
            constants[11].i = top_blob.cstep;
            constants[12].i = hoffset;

            const Pipeline* pipeline = elempack == 8 ? pipeline_slice_pack8[i % 2]
                                       : elempack == 4 ? pipeline_slice_pack4[i % 2]
                                       : pipeline_slice[i % 2];

            cmd.record_pipeline(pipeline, bindings, constants, top_blob);

            hoffset += top_blob.h;
        }

        return 0;
    }

    if (dims == 4 && positive_axis == 3)
    {
        int w = bottom_blob.w;
        int h = bottom_blob.h;
        int d = bottom_blob.d;
        int channels = bottom_blob.c;

        int q = 0;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            int slice;
            if (indices_ptr)
            {
                if (i == top_blobs.size() - 1)
                {
                    slice = w - q;
                }
                else
                {
                    int indice = indices_ptr[i];
                    int positive_indice = indice < 0 ? w + indice : indice;
                    slice = positive_indice - q;
                }
            }
            else
            {
                slice = slices_ptr[i];
                if (slice == -233)
                {
                    slice = static_cast<int>((w - q) / (top_blobs.size() - i));
                }
            }

            VkMat& top_blob = top_blobs[i];
            top_blob.create(slice, h, d, channels, elemsize, elempack, opt.blob_vkallocator);
            if (top_blob.empty())
                return -100;

            q += slice;
        }

        int woffset = 0;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            VkMat& top_blob = top_blobs[i];

            std::vector<VkMat> bindings(2);
            bindings[0] = bottom_blob;
            bindings[1] = top_blob;

            std::vector<vk_constant_type> constants(13);
            constants[0].i = bottom_blob.dims;
            constants[1].i = bottom_blob.w;
            constants[2].i = bottom_blob.h;
            constants[3].i = bottom_blob.d;
            constants[4].i = bottom_blob.c;
            constants[5].i = bottom_blob.cstep;
            constants[6].i = top_blob.dims;
            constants[7].i = top_blob.w;
            constants[8].i = top_blob.h;
            constants[9].i = top_blob.d;
            constants[10].i = top_blob.c;
            constants[11].i = top_blob.cstep;
            constants[12].i = woffset;

            const Pipeline* pipeline = elempack == 8 ? pipeline_slice_pack8[i % 2]
                                       : elempack == 4 ? pipeline_slice_pack4[i % 2]
                                       : pipeline_slice[i % 2];

            cmd.record_pipeline(pipeline, bindings, constants, top_blob);

            woffset += top_blob.w;
        }

        return 0;
    }

    return 0;
}

} // namespace ncnn
