#include <iostream>
#include <vector>
#include <cstring>

#include <openexr.h>
#include <openexr_errors.h>

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: reader <file.exr>\n";
        return 1;
    }

    const char *filename = argv[1];
    const char *wanted[4] = { "R", "G", "B", "Y" };

    exr_context_t ctx = NULL;
    exr_context_initializer_t cinit = EXR_DEFAULT_CONTEXT_INITIALIZER;
    exr_decode_pipeline_t decode = EXR_DECODE_PIPELINE_INITIALIZER;
    exr_chunk_info_t chunk{};
    bool decode_initialized = false;
    int exit_code = 0;
    std::vector<float> channel_buffers[4];

    // --- Open EXR file ---
    exr_result_t rv = exr_start_read(&ctx, filename, &cinit);
    if (rv != EXR_ERR_SUCCESS)
    {
        std::cerr << "Failed to open EXR: " << exr_get_default_error_message(rv) << "\n";
        return 1;
    }

    do
    {
        // --- Basic header info ---
        exr_attr_box2i_t dw;
        rv = exr_get_data_window(ctx, 0, &dw);
        if (rv != EXR_ERR_SUCCESS)
        {
            std::cerr << "Failed to read data window\n";
            exit_code = 1;
            break;
        }

        int width = dw.max.x - dw.min.x + 1;
        int height = dw.max.y - dw.min.y + 1;

        std::cout << "Resolution: " << width << " x " << height << "\n";

        // --- Initialize decode pipeline for the first scanline chunk ---
        rv = exr_read_scanline_chunk_info(ctx, 0, dw.min.y, &chunk);
        if (rv != EXR_ERR_SUCCESS)
        {
            std::cerr << "Failed to read first chunk info\n";
            exit_code = 1;
            break;
        }

        rv = exr_decoding_initialize(ctx, 0, &chunk, &decode);
        if (rv != EXR_ERR_SUCCESS)
        {
            std::cerr << "decode init failed\n";
            exit_code = 1;
            break;
        }

        decode_initialized = true;

        if (decode.channel_count <= 0 || decode.channels == nullptr)
        {
            std::cerr << "No channels found.\n";
            exit_code = 1;
            break;
        }

        int selected_indices[4] = { -1, -1, -1, -1 };
        int chan_count = 0;
        int example_slot = -1;
        for (int i = 0; i < 4; ++i)
        {
            for (int c = 0; c < decode.channel_count; ++c)
            {
                if (decode.channels[c].channel_name != nullptr && std::strcmp(decode.channels[c].channel_name, wanted[i]) == 0)
                {
                    selected_indices[i] = c;
                    ++chan_count;
                    if (example_slot < 0)
                    {
                        example_slot = i;
                    }
                    break;
                }
            }
        }

        if (chan_count == 0)
        {
            std::cerr << "No channels found.\n";
            exit_code = 1;
            break;
        }

        std::cout << "Channels found: " << chan_count << "\n";

        // --- Allocate buffers (float per pixel per channel) ---
        for (int i = 0; i < 4; ++i)
        {
            if (selected_indices[i] < 0)
            {
                continue;
            }

            exr_coding_channel_info_t &pc = decode.channels[selected_indices[i]];
            channel_buffers[i].resize(static_cast<size_t>(pc.width) * static_cast<size_t>(pc.height));
            pc.user_data_type = EXR_PIXEL_FLOAT;
            pc.user_bytes_per_element = sizeof(float);
            pc.user_pixel_stride = sizeof(float);
            pc.user_line_stride = sizeof(float) * pc.width;
            pc.decode_to_ptr = reinterpret_cast<unsigned char *>(channel_buffers[i].data());
        }

        rv = exr_decoding_choose_default_routines(ctx, 0, &decode);
        if (rv != EXR_ERR_SUCCESS)
        {
            std::cerr << "Failed to choose decode routines\n";
            exit_code = 1;
            break;
        }

        int scanlines_per_chunk = 1;
        if (exr_get_scanlines_per_chunk(ctx, 0, &scanlines_per_chunk) != EXR_ERR_SUCCESS || scanlines_per_chunk <= 0)
        {
            scanlines_per_chunk = 1;
        }

        // --- Decode all chunks ---
        for (int y = dw.min.y; y <= dw.max.y; y += scanlines_per_chunk)
        {
            rv = exr_read_scanline_chunk_info(ctx, 0, y, &chunk);
            if (rv != EXR_ERR_SUCCESS) continue;

            rv = exr_decoding_update(ctx, 0, &chunk, &decode);
            if (rv != EXR_ERR_SUCCESS) continue;

            rv = exr_decoding_run(ctx, 0, &decode);
            if (rv != EXR_ERR_SUCCESS) continue;
        }

        std::cout << "Loaded EXR successfully.\n";
        if (example_slot >= 0)
        {
            std::cout << "Example pixel " << wanted[example_slot] << " at (0,0): "
                      << channel_buffers[example_slot][0] << "\n";
        }
    } while (false);

    if (decode_initialized)
    {
        exr_decoding_destroy(ctx, &decode);
    }

    if (ctx != NULL)
    {
        exr_finish(&ctx);
    }

    return exit_code;
}