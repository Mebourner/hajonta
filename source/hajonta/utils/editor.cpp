#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#if defined(NEEDS_EGL)
#include <EGL/egl.h>
#elif defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif

#include "hajonta/platform/common.h"
#include "hajonta/math.cpp"
#include "hajonta/image.cpp"
#include "hajonta/bmp.cpp"
#include "hajonta/font.cpp"

#if defined(_MSC_VER)
#pragma warning(push, 4)
#pragma warning(disable: 4365 4312 4505)
#endif
#define STB_RECT_PACK_IMPLEMENTATION
#include "hajonta/thirdparty/stb_rect_pack.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "hajonta/thirdparty/stb_truetype.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include "hajonta/programs/b.h"
#include "hajonta/programs/ui2d.h"

static float pi = 3.14159265358979f;

struct vertex
{
    float position[4];
    float color[4];
};

struct editor_vertex_format
{
    vertex v;
    float style[4];
    v4 normal;
    v4 tangent;
};

struct face_index
{
    uint32_t vertex;
    uint32_t texture_coord;
    uint32_t normal;
};

struct ui2d_vertex_format
{
    float position[2];
    float tex_coord[2];
    uint32_t texid;
    uint32_t options;
    v3 channel_color;
};

struct ui2d_push_context
{
    ui2d_vertex_format *vertices;
    uint32_t num_vertices;
    uint32_t max_vertices;
    GLuint *elements;
    uint32_t num_elements;
    uint32_t max_elements;
    uint32_t *textures;
    uint32_t num_textures;
    uint32_t max_textures;

    uint32_t seen_textures[1024];
};

struct face
{
    face_index indices[3];
    int texture_offset;
    int bump_texture_offset;
    int emit_texture_offset;
    int ao_texture_offset;
};

struct material
{
    char name[100];
    int32_t texture_offset;
    int32_t bump_texture_offset;
    int32_t emit_texture_offset;
    int32_t ao_texture_offset;
};

struct kenpixel_future_14
{
    uint8_t zfi[5159];
    uint8_t bmp[131210];
    font_data font;
};

struct stb_font_data
{
    stbtt_packedchar chardata[128];
    GLuint font_tex;
    uint32_t vbo;
    uint32_t ibo;
};

struct kenney_ui_data
{
    GLuint panel_tex;
    GLuint ui_pack_tex;
    uint32_t vbo;
    uint32_t ibo;
};

struct game_state
{
    uint32_t vao;
    uint32_t vbo;
    uint32_t ibo;
    uint32_t line_ibo;
    int32_t sampler_ids[16];
    uint32_t texture_ids[20];
    uint32_t num_texture_ids;
    uint32_t aabb_cube_vbo;
    uint32_t aabb_cube_ibo;
    uint32_t bounding_sphere_vbo;
    uint32_t bounding_sphere_ibo;
    uint32_t mouse_vbo;
    uint32_t mouse_texture;

    uint32_t num_bounding_sphere_elements;

    float near_;
    float far_;
    float delta_t;

    loaded_file model_file;
    loaded_file mtl_file;

    char *objfile;
    uint32_t objfile_size;

    v3 vertices[100000];
    uint32_t num_vertices;
    v3 texture_coords[100000];
    uint32_t num_texture_coords;
    v3 normals[100000];
    uint32_t num_normals;
    face faces[100000];
    uint32_t num_faces;

    material materials[10];
    uint32_t num_materials;

    GLushort faces_array[300000];
    uint32_t num_faces_array;
    GLushort line_elements[600000];
    uint32_t num_line_elements;
    editor_vertex_format vbo_vertices[300000];
    uint32_t num_vbo_vertices;

    b_program_struct program_b;
    ui2d_program_struct program_ui2d;

    v3 model_max;
    v3 model_min;

    char bitmap_scratch[4096 * 4096 * 4];

    bool hide_lines;
    int model_mode;
    int shading_mode;

    int x_rotation;
    int y_rotation;

    uint8_t mouse_bitmap[4096];

    uint32_t debug_texture_id;
    uint32_t debug_vbo;
    kenpixel_future_14 debug_font;
    draw_buffer debug_draw_buffer;
#define debug_buffer_width 960
#define debug_buffer_height 14
    uint8_t debug_buffer[4 * debug_buffer_width * debug_buffer_height];

    stb_font_data stb_font;
    kenney_ui_data kenney_ui;
};

void load_fonts(hajonta_thread_context *ctx, platform_memory *memory)
{
    game_state *state = (game_state *)memory->memory;

    uint8_t ttf_buffer[158080];
    unsigned char temp_bitmap[512][512];
    char *filename = "fonts/AnonymousPro-1.002.001/Anonymous Pro.ttf";

    if (!memory->platform_load_asset(ctx, filename, sizeof(ttf_buffer), ttf_buffer)) {
        char msg[1024];
        sprintf(msg, "Could not load %s\n", filename);
        memory->platform_fail(ctx, msg);
        memory->quit = true;
        return;
    }

    stbtt_pack_context pc;

    stbtt_PackBegin(&pc, (unsigned char *)temp_bitmap[0], 512, 512, 0, 1, NULL);
    stbtt_PackSetOversampling(&pc, 1, 1);
    stbtt_PackFontRange(&pc, ttf_buffer, 0, 12.0f, 32, 95, state->stb_font.chardata+32);
    stbtt_PackEnd(&pc);

    glGenTextures(1, &state->stb_font.font_tex);
    glErrorAssert();
    glBindTexture(GL_TEXTURE_2D, state->stb_font.font_tex);
    glErrorAssert();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 512, 512, 0, GL_RED, GL_UNSIGNED_BYTE, temp_bitmap);
    glErrorAssert();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glErrorAssert();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glErrorAssert();
    glGenBuffers(1, &state->stb_font.vbo);
    glGenBuffers(1, &state->stb_font.ibo);
}

bool
gl_setup(hajonta_thread_context *ctx, platform_memory *memory)
{
    glErrorAssert();
    game_state *state = (game_state *)memory->memory;

#if !defined(NEEDS_EGL)
    if (glGenVertexArrays != 0)
    {
        glGenVertexArrays(1, &state->vao);
        glBindVertexArray(state->vao);
        glErrorAssert();
    }
#endif

    glErrorAssert();

    bool loaded;

    loaded = b_program(&state->program_b, ctx, memory);
    if (!loaded)
    {
        return loaded;
    }

    loaded = ui2d_program(&state->program_ui2d, ctx, memory);
    if (!loaded)
    {
        return loaded;
    }

    return true;
}

char *
next_newline(char *str, uint32_t str_length)
{
    for (uint32_t idx = 0;
            idx < str_length;
            ++idx)
    {
        switch (str[idx]) {
            case '\n':
            case '\r':
            case '\0':
                return str + idx;
        }
    }
    return str + str_length;
}

#define starts_with(line, s) (strncmp(line, s, sizeof(s) - 1) == 0)

bool
load_mtl(hajonta_thread_context *ctx, platform_memory *memory)
{
    game_state *state = (game_state *)memory->memory;


    char *start_position = (char *)state->mtl_file.contents;
    char *eof = start_position + state->mtl_file.size;
    char *position = start_position;
    uint32_t max_lines = 100000;
    uint32_t counter = 0;
    material *current_material = 0;
    while (position < eof)
    {
        uint32_t remainder = state->mtl_file.size - (uint32_t)(position - start_position);
        char *eol = next_newline(position, remainder);
        char _line[1024];
        strncpy(_line, position, (size_t)(eol - position));
        _line[eol - position] = '\0';
        char *line = _line;

        for(;;)
        {
            if (line[0] == '\t')
            {
                line++;
                continue;
            }
            if (line[0] == ' ')
            {
                line++;
                continue;
            }
            break;
        }

        if (line[0] == '\0')
        {

        }
        else if (line[0] == '#')
        {

        }
        else if (starts_with(line, "newmtl "))
        {
            current_material = state->materials + state->num_materials++;
            strncpy(current_material->name, line + 7, (size_t)(eol - position - 7));
            current_material->texture_offset = -1;
            current_material->bump_texture_offset = -1;
            current_material->emit_texture_offset = -1;
            current_material->ao_texture_offset = -1;
        }
        else if (starts_with(line, "Ns "))
        {
        }
        else if (starts_with(line, "Ka "))
        {
        }
        else if (starts_with(line, "Kd "))
        {
        }
        else if (starts_with(line, "Ke "))
        {
        }
        else if (starts_with(line, "Ks "))
        {
        }
        else if (starts_with(line, "Ni "))
        {
        }
        else if (starts_with(line, "Ns "))
        {
        }
        else if (starts_with(line, "Tr "))
        {
        }
        else if (starts_with(line, "Tf "))
        {
        }
        else if (starts_with(line, "d "))
        {
        }
        else if (starts_with(line, "illum "))
        {
        }
        else if (starts_with(line, "map_Bump ") || starts_with(line, "map_bump "))
        {
            char *filename = line + sizeof("map_Bump ") - 1;
            hassert(strlen(filename) > 0);
            if (filename[0] == '.')
            {

            }
            else
            {
                loaded_file texture;
                bool loaded = memory->platform_editor_load_nearby_file(ctx, &texture, state->mtl_file, filename);
                hassert(loaded);
                int32_t x, y, size;
                loaded = load_image((uint8_t *)texture.contents, texture.size, (uint8_t *)state->bitmap_scratch, sizeof(state->bitmap_scratch), &x, &y, &size, false);
                hassert(loaded);

                current_material->bump_texture_offset = (int32_t)(state->num_texture_ids++);
                glErrorAssert();
                glBindTexture(GL_TEXTURE_2D, state->texture_ids[current_material->bump_texture_offset]);
                glErrorAssert();
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                    x, y, 0,
                    GL_RGBA, GL_UNSIGNED_BYTE, state->bitmap_scratch);
                glErrorAssert();
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glErrorAssert();
            }
        }
        else if (starts_with(line, "map_d "))
        {
        }
        else if (starts_with(line, "map_Kd "))
        {
            char *filename = line + sizeof("map_Kd ") - 1;
            hassert(strlen(filename) > 0);
            if (filename[0] == '.')
            {

            }
            else
            {
                loaded_file texture;
                bool loaded = memory->platform_editor_load_nearby_file(ctx, &texture, state->mtl_file, filename);
                hassert(loaded);
                int32_t x, y, size;
                loaded = load_image((uint8_t *)texture.contents, texture.size, (uint8_t *)state->bitmap_scratch, sizeof(state->bitmap_scratch), &x, &y, &size, false);
                hassert(loaded);

                current_material->texture_offset = (int32_t)(state->num_texture_ids++);
                glErrorAssert();
                glBindTexture(GL_TEXTURE_2D, state->texture_ids[current_material->texture_offset]);
                glErrorAssert();
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                    x, y, 0,
                    GL_RGBA, GL_UNSIGNED_BYTE, state->bitmap_scratch);
                glErrorAssert();
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glErrorAssert();
            }
        }
        else if (starts_with(line, "map_Ke "))
        {
            char *filename = line + sizeof("map_Ke ") - 1;
            hassert(strlen(filename) > 0);
            if (filename[0] == '.')
            {

            }
            else
            {
                loaded_file texture;
                bool loaded = memory->platform_editor_load_nearby_file(ctx, &texture, state->mtl_file, filename);
                hassert(loaded);
                int32_t x, y, size;
                loaded = load_image((uint8_t *)texture.contents, texture.size, (uint8_t *)state->bitmap_scratch, sizeof(state->bitmap_scratch), &x, &y, &size, false);
                hassert(loaded);

                current_material->emit_texture_offset = (int32_t)(state->num_texture_ids++);
                glErrorAssert();
                glBindTexture(GL_TEXTURE_2D, state->texture_ids[current_material->emit_texture_offset]);
                glErrorAssert();
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                    x, y, 0,
                    GL_RGBA, GL_UNSIGNED_BYTE, state->bitmap_scratch);
                glErrorAssert();
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glErrorAssert();
            }
        }
        else if (starts_with(line, "map_ao "))
        {
            char *filename = line + sizeof("map_ao ") - 1;
            hassert(strlen(filename) > 0);
            if (filename[0] == '.')
            {

            }
            else
            {
                loaded_file texture;
                bool loaded = memory->platform_editor_load_nearby_file(ctx, &texture, state->mtl_file, filename);
                hassert(loaded);
                int32_t x, y, size;
                loaded = load_image((uint8_t *)texture.contents, texture.size, (uint8_t *)state->bitmap_scratch, sizeof(state->bitmap_scratch), &x, &y, &size, false);
                hassert(loaded);

                current_material->ao_texture_offset = (int32_t)(state->num_texture_ids++);
                glErrorAssert();
                glBindTexture(GL_TEXTURE_2D, state->texture_ids[current_material->ao_texture_offset]);
                glErrorAssert();
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                    x, y, 0,
                    GL_RGBA, GL_UNSIGNED_BYTE, state->bitmap_scratch);
                glErrorAssert();
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glErrorAssert();
            }
        }
        else if (starts_with(line, "map_Ks "))
        {
        }
        else if (starts_with(line, "refl "))
        {
        }
        else if (starts_with(line, "bump "))
        {
        }
        else if (starts_with(line, "map_Ns "))
        {
        }
        else
        {
            hassert(!"Invalid code path");
        }
        if (counter++ > max_lines)
        {
            hassert(!"Too many lines in mtl file");
        }

        if (*eol == '\0')
        {
            break;
        }
        position = eol + 1;
        while((*position == '\r') && (*position == '\n'))
        {
            position++;
        }
        glErrorAssert();
    }
    return true;
}

void
load_aabb_buffer_objects(game_state *state, v3 model_min, v3 model_max)
{
    editor_vertex_format vertices[] = {
        {
            {model_min.x, model_min.y, model_min.z, 1.0},
        },
        {
            {model_max.x, model_min.y, model_min.z, 1.0},
        },
        {
            {model_max.x, model_max.y, model_min.z, 1.0},
        },
        {
            {model_min.x, model_max.y, model_min.z, 1.0},
        },
        {
            {model_min.x, model_min.y, model_max.z, 1.0},
        },
        {
            {model_max.x, model_min.y, model_max.z, 1.0},
        },
        {
            {model_max.x, model_max.y, model_max.z, 1.0},
        },
        {
            {model_min.x, model_max.y, model_max.z, 1.0},
        },
    };
    glBindBuffer(GL_ARRAY_BUFFER, state->aabb_cube_vbo);

    glBufferData(GL_ARRAY_BUFFER,
            (GLsizeiptr)sizeof(vertices),
            vertices,
            GL_STATIC_DRAW);
    glErrorAssert();

    GLushort elements[] = {
        0, 1,
        1, 2,
        2, 3,
        3, 0,

        4, 5,
        5, 6,
        6, 7,
        7, 4,

        0, 4,
        1, 5,
        2, 6,
        3, 7,
    };

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->aabb_cube_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            (GLsizeiptr)sizeof(elements),
            elements,
            GL_STATIC_DRAW);
    glErrorAssert();
}

void
load_bounding_sphere(game_state *state, v3 model_min, v3 model_max)
{
    v3 center = v3div(v3add(state->model_max, state->model_min), 2.0f);
    v3 diff = v3sub(center, state->model_min);
    float size = v3length(diff);

    editor_vertex_format vertices[64 * 3] = {};
    for (uint32_t idx = 0;
            idx < harray_count(vertices);
            ++idx)
    {
        editor_vertex_format *v = vertices + idx;
        if (idx < 64)
        {
            float a = idx * (2.0f * pi) / (harray_count(vertices) / 3 - 1);
            *v = {
                {
                    {center.x + sinf(a) * size, center.y + cosf(a) * size, center.z, 1.0f},
                    {1.0f, 1.0f, 1.0f, 0.5f},
                },
                {0.0f, 0.0f, 0.0f, 0.0f},
            };
        }
        else if (idx < 128)
        {
            float a = (idx - 64) * (2.0f * pi) / (harray_count(vertices) / 3 - 1);
            *v = {
                {
                    {center.x, center.y + sinf(a) * size, center.z + cosf(a) * size, 1.0f},
                    {1.0f, 1.0f, 1.0f, 0.5f},
                },
                {0.0f, 0.0f, 0.0f, 0.0f},
            };
        }
        else
        {
            float a = (idx - 128) * (2.0f * pi) / (harray_count(vertices) / 3 - 1);
            *v = {
                {
                    {center.x + cosf(a) * size, center.y, center.z + sinf(a) * size, 1.0f},
                    {1.0f, 1.0f, 1.0f, 0.5f},
                },
                {0.0f, 0.0f, 0.0f, 0.0f},
            };
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, state->bounding_sphere_vbo);

    glBufferData(GL_ARRAY_BUFFER,
            (GLsizeiptr)sizeof(vertices),
            vertices,
            GL_STATIC_DRAW);
    glErrorAssert();

    GLushort elements[harray_count(vertices)];
    for (GLushort idx = 0;
            idx < harray_count(vertices);
            ++idx)
    {
        elements[idx] = idx;
        if ((idx % 64) == 63)
        {
            elements[idx] = 65535;
        }
    }

    state->num_bounding_sphere_elements = harray_count(elements);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->bounding_sphere_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            (GLsizeiptr)sizeof(elements),
            elements,
            GL_STATIC_DRAW);
    glErrorAssert();
}

void
setup_vertex_attrib_array(game_state *state)
{
    glEnableVertexAttribArray((GLuint)state->program_b.a_pos_id);
    glEnableVertexAttribArray((GLuint)state->program_b.a_color_id);
    glEnableVertexAttribArray((GLuint)state->program_b.a_style_id);
    glEnableVertexAttribArray((GLuint)state->program_b.a_normal_id);
    glVertexAttribPointer((GLuint)state->program_b.a_pos_id, 4, GL_FLOAT, GL_FALSE, sizeof(editor_vertex_format), 0);
    glVertexAttribPointer((GLuint)state->program_b.a_color_id, 4, GL_FLOAT, GL_FALSE, sizeof(editor_vertex_format), (void *)offsetof(vertex, color));
    glVertexAttribPointer((GLuint)state->program_b.a_style_id, 4, GL_FLOAT, GL_FALSE, sizeof(editor_vertex_format), (void *)offsetof(editor_vertex_format, style));
    glVertexAttribPointer((GLuint)state->program_b.a_normal_id, 4, GL_FLOAT, GL_FALSE, sizeof(editor_vertex_format), (void *)offsetof(editor_vertex_format, normal));
    glVertexAttribPointer((GLuint)state->program_b.a_tangent_id, 4, GL_FLOAT, GL_FALSE, sizeof(editor_vertex_format), (void *)offsetof(editor_vertex_format, tangent));

    glErrorAssert();
}

uint32_t
push_texture(ui2d_push_context *pushctx, uint32_t tex)
{
    hassert(tex < sizeof(pushctx->seen_textures));

    if (pushctx->seen_textures[tex] != 0)
    {
        return pushctx->seen_textures[tex];
    }

    pushctx->textures[pushctx->num_textures++] = tex;

    // Yes, this is post-increment.  It is decremented in the shader.
    pushctx->seen_textures[tex] = pushctx->num_textures;
    return pushctx->num_textures;
}

void
push_quad(ui2d_push_context *pushctx, stbtt_aligned_quad q, uint32_t tex, uint32_t options)
{
        ui2d_vertex_format *vertices = pushctx->vertices;
        uint32_t *num_vertices = &pushctx->num_vertices;
        uint32_t *elements = pushctx->elements;
        uint32_t *num_elements = &pushctx->num_elements;

        uint32_t tex_handle = push_texture(pushctx, tex);

        uint32_t bl_vertex = (*num_vertices)++;
        vertices[bl_vertex] =
        {
            { q.x0, q.y0 },
            { q.s0, q.t0 },
            tex_handle,
            options,
            { 1, 1, 1 },
        };
        uint32_t br_vertex = (*num_vertices)++;
        vertices[br_vertex] =
        {
            { q.x1, q.y0 },
            { q.s1, q.t0 },
            tex_handle,
            options,
            { 1, 1, 0 },
        };
        uint32_t tr_vertex = (*num_vertices)++;
        vertices[tr_vertex] =
        {
            { q.x1, q.y1 },
            { q.s1, q.t1 },
            tex_handle,
            options,
            { 1, 1, 0 },
        };
        uint32_t tl_vertex = (*num_vertices)++;
        vertices[tl_vertex] =
        {
            { q.x0, q.y1 },
            { q.s0, q.t1 },
            tex_handle,
            options,
            { 1, 1, 0 },
        };
        elements[(*num_elements)++] = bl_vertex;
        elements[(*num_elements)++] = br_vertex;
        elements[(*num_elements)++] = tr_vertex;
        elements[(*num_elements)++] = tr_vertex;
        elements[(*num_elements)++] = tl_vertex;
        elements[(*num_elements)++] = bl_vertex;
}

void
push_panel(game_state *state, ui2d_push_context *pushctx, rectangle2 rect)
{
    uint32_t tex = state->kenney_ui.panel_tex;
    stbtt_aligned_quad q;

    // BL
    q.x0 = rect.position.x;
    q.x1 = rect.position.x + 8.0f;
    q.y0 = rect.position.y;
    q.y1 = rect.position.y + 8.0f;
    q.s0 = 0.00f;
    q.s1 = 0.08f;
    q.t0 = 0.00f;
    q.t1 = 0.08f;
    push_quad(pushctx, q, tex, 0);

    // TL
    q.y0 = rect.position.y + rect.dimension.y;
    q.y1 = q.y0 - 8.0f;
    q.t0 = 1.00f;
    q.t1 = 0.92f;
    push_quad(pushctx, q, tex, 0);

    // TR
    q.x0 = rect.position.x + rect.dimension.x;
    q.x1 = q.x0 - 8.0f;
    q.s0 = 1.00f;
    q.s1 = 0.92f;
    push_quad(pushctx, q, tex, 0);

    // BR
    q.y0 = rect.position.y;
    q.y1 = rect.position.y + 8.0f;
    q.t0 = 0.00f;
    q.t1 = 0.08f;
    push_quad(pushctx, q, tex, 0);

    // TR-BR
    q.y0 = rect.position.y + 8.0f;
    q.y1 = rect.position.y + rect.dimension.y - 8.0f;
    q.t0 = 0.08f;
    q.t1 = 0.92f;
    push_quad(pushctx, q, tex, 0);

    // TL-BL
    q.x0 = rect.position.x;
    q.x1 = rect.position.x + 8.0f;
    q.s0 = 0.00f;
    q.s1 = 0.08f;
    push_quad(pushctx, q, tex, 0);

    // TL-TR
    q.x0 = rect.position.x + 8.0f;
    q.x1 = rect.position.x + rect.dimension.x - 8.0f;
    q.y0 = rect.position.y + rect.dimension.y;
    q.y1 = rect.position.y + rect.dimension.y - 8.0f;
    q.s0 = 0.08f;
    q.s1 = 0.92f;
    q.t0 = 1.00f;
    q.t1 = 0.92f;
    push_quad(pushctx, q, tex, 0);

    // BL-BR
    q.y0 = rect.position.y;
    q.y1 = rect.position.y + 8.0f;
    q.t0 = 0.00f;
    q.t1 = 0.08f;
    push_quad(pushctx, q, tex, 0);

    // CENTER
    q.x0 = rect.position.x + 8.0f;
    q.x1 = rect.position.x + rect.dimension.x - 8.0f;
    q.y0 = rect.position.y + 8.0f;
    q.y1 = rect.position.y + rect.dimension.y - 8.0f;
    q.s0 = 0.08f;
    q.s1 = 0.92f;
    q.t0 = 0.08f;
    q.t1 = 0.92f;
    push_quad(pushctx, q, tex, 0);
}

void
ui2d_render_elements(game_state *state, ui2d_push_context *pushctx)
{
    glBindBuffer(GL_ARRAY_BUFFER, state->stb_font.vbo);
    glBufferData(GL_ARRAY_BUFFER,
            (GLsizei)(pushctx->num_vertices * sizeof(pushctx->vertices[0])),
            pushctx->vertices,
            GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->stb_font.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            (GLsizei)(pushctx->num_elements * sizeof(pushctx->elements[0])),
            pushctx->elements,
            GL_STATIC_DRAW);

    glEnableVertexAttribArray((GLuint)state->program_ui2d.a_pos_id);
    glEnableVertexAttribArray((GLuint)state->program_ui2d.a_tex_coord_id);
    glEnableVertexAttribArray((GLuint)state->program_ui2d.a_texid_id);
    glEnableVertexAttribArray((GLuint)state->program_ui2d.a_options_id);
    glEnableVertexAttribArray((GLuint)state->program_ui2d.a_channel_color_id);
    glVertexAttribPointer((GLuint)state->program_ui2d.a_pos_id, 2, GL_FLOAT, GL_FALSE, sizeof(ui2d_vertex_format), 0);
    glVertexAttribPointer((GLuint)state->program_ui2d.a_tex_coord_id, 2, GL_FLOAT, GL_FALSE, sizeof(ui2d_vertex_format), (void *)offsetof(ui2d_vertex_format, tex_coord));
    glVertexAttribPointer((GLuint)state->program_ui2d.a_texid_id, 1, GL_FLOAT, GL_FALSE, sizeof(ui2d_vertex_format), (void *)offsetof(ui2d_vertex_format, texid));
    glVertexAttribPointer((GLuint)state->program_ui2d.a_options_id, 1, GL_FLOAT, GL_FALSE, sizeof(ui2d_vertex_format), (void *)offsetof(ui2d_vertex_format, options));
    glVertexAttribPointer((GLuint)state->program_ui2d.a_channel_color_id, 3, GL_FLOAT, GL_FALSE, sizeof(ui2d_vertex_format), (void *)offsetof(ui2d_vertex_format, channel_color));

    GLint uniform_locations[10] = {};
    char msg[] = "tex[xx]";
    for (int idx = 0; idx < harray_count(uniform_locations); ++idx)
    {
        sprintf(msg, "tex[%d]", idx);
        uniform_locations[idx] = glGetUniformLocation(state->program_ui2d.program, msg);

    }
    for (uint32_t i = 0; i < pushctx->num_textures; ++i)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, pushctx->textures[i]);
        glUniform1i(
            uniform_locations[i],
            (GLint)i);
    }

    glDrawElements(GL_TRIANGLES, (GLsizei)pushctx->num_elements, GL_UNSIGNED_INT, 0);
    glErrorAssert();
}

bool
load_texture_asset(
    hajonta_thread_context *ctx,
    platform_memory *memory,
    char *filename,
    uint8_t *image_buffer,
    uint32_t image_size,
    int32_t *x,
    int32_t *y,
    GLuint *tex
)
{
    game_state *state = (game_state *)memory->memory;
    if (!memory->platform_load_asset(ctx, filename, image_size, image_buffer)) {
        return false;
    }

    int32_t actual_size;
    load_image(image_buffer, image_size, (uint8_t *)state->bitmap_scratch, sizeof(state->bitmap_scratch),
            x, y, &actual_size);

    glGenTextures(1, tex);
    glBindTexture(GL_TEXTURE_2D, *tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
        *x, *y, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, state->bitmap_scratch);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    return true;
}

void
load_texture_asset_failed(
    hajonta_thread_context *ctx,
    platform_memory *memory,
    char *filename
)
{
    char msg[1024];
    sprintf(msg, "Could not load %s\n", filename);
    memory->platform_fail(ctx, msg);
    memory->quit = true;
}

extern "C" GAME_UPDATE_AND_RENDER(game_update_and_render)
{
    game_state *state = (game_state *)memory->memory;

#if !defined(NEEDS_EGL) && !defined(__APPLE__)
    if (!glCreateProgram)
    {
        load_glfuncs(ctx, memory->platform_glgetprocaddress);
    }
#endif

    if (!memory->initialized)
    {
        if(!gl_setup(ctx, memory))
        {
            return;
        }
        load_fonts(ctx, memory);
        memory->initialized = 1;

        state->near_ = {5.0f};
        state->far_ = {50.0f};


        glErrorAssert();
        glGenBuffers(1, &state->vbo);
        glGenBuffers(1, &state->aabb_cube_vbo);
        glGenBuffers(1, &state->bounding_sphere_vbo);
        glGenBuffers(1, &state->ibo);
        glGenBuffers(1, &state->line_ibo);
        glGenBuffers(1, &state->aabb_cube_ibo);
        glGenBuffers(1, &state->bounding_sphere_ibo);
        glErrorAssert();
        glGenTextures(harray_count(state->texture_ids), state->texture_ids);
        glGenBuffers(1, &state->mouse_vbo);
        glGenTextures(1, &state->mouse_texture);

        {
            if (!memory->platform_load_asset(ctx, "fonts/kenpixel_future/kenpixel_future_regular_14.zfi", sizeof(state->debug_font.zfi), state->debug_font.zfi))
            {
                memory->platform_fail(ctx, "Failed to open zfi file");
                return;
            }
            if (!memory->platform_load_asset(ctx, "fonts/kenpixel_future/kenpixel_future_regular_14.bmp", sizeof(state->debug_font.bmp), state->debug_font.bmp))
            {
                memory->platform_fail(ctx, "Failed to open bmp file");
                return;
            }
            load_font(state->debug_font.zfi, state->debug_font.bmp, &state->debug_font.font, ctx, memory);

            state->debug_draw_buffer.memory = state->debug_buffer;
            state->debug_draw_buffer.width = debug_buffer_width;
            state->debug_draw_buffer.height = debug_buffer_height;
            state->debug_draw_buffer.pitch = 4 * debug_buffer_width;

            glGenBuffers(1, &state->debug_vbo);
            glGenTextures(1, &state->debug_texture_id);
            glBindTexture(GL_TEXTURE_2D, state->debug_texture_id);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                debug_buffer_width, debug_buffer_height, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, state->debug_buffer);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        }

        {
            uint8_t image[256];
            int32_t x, y;
            char filename[] = "ui/slick_arrows/slick_arrow-delta.png";
            bool loaded = load_texture_asset(ctx, memory, filename, image, sizeof(image), &x, &y, &state->mouse_texture);
            if (!loaded)
            {
                return load_texture_asset_failed(ctx, memory, filename);
            }
        }

        {
            uint8_t image[636];
            int32_t x, y;
            char filename[] = "ui/kenney/glassPanel.png";
            bool loaded = load_texture_asset(ctx, memory, filename, image, sizeof(image), &x, &y, &state->kenney_ui.panel_tex);
            if (!loaded)
            {
                return load_texture_asset_failed(ctx, memory, filename);
            }
        }

        {
            uint8_t image[25459];
            int32_t x, y;
            char filename[] = "ui/kenney/UIpackSheet_transparent.png";
            bool loaded = load_texture_asset(ctx, memory, filename, image, sizeof(image), &x, &y, &state->kenney_ui.ui_pack_tex);
            if (!loaded)
            {
                return load_texture_asset_failed(ctx, memory, filename);
            }
        }

        glErrorAssert();
        state->sampler_ids[0] = glGetUniformLocation(state->program_b.program, "tex");
        state->sampler_ids[1] = glGetUniformLocation(state->program_b.program, "tex1");
        state->sampler_ids[2] = glGetUniformLocation(state->program_b.program, "tex2");
        state->sampler_ids[3] = glGetUniformLocation(state->program_b.program, "tex3");
        state->sampler_ids[4] = glGetUniformLocation(state->program_b.program, "tex4");
        state->sampler_ids[5] = glGetUniformLocation(state->program_b.program, "tex5");
        state->sampler_ids[6] = glGetUniformLocation(state->program_b.program, "tex6");
        state->sampler_ids[7] = glGetUniformLocation(state->program_b.program, "tex7");
        state->sampler_ids[8] = glGetUniformLocation(state->program_b.program, "tex8");
        state->sampler_ids[9] = glGetUniformLocation(state->program_b.program, "tex9");
        state->sampler_ids[10] = glGetUniformLocation(state->program_b.program, "tex10");
        state->sampler_ids[11] = glGetUniformLocation(state->program_b.program, "tex11");
        state->sampler_ids[12] = glGetUniformLocation(state->program_b.program, "tex12");
        state->sampler_ids[13] = glGetUniformLocation(state->program_b.program, "tex13");
        state->sampler_ids[14] = glGetUniformLocation(state->program_b.program, "tex14");
        state->sampler_ids[15] = glGetUniformLocation(state->program_b.program, "tex15");
        glErrorAssert();

        while (!memory->platform_editor_load_file(ctx, &state->model_file))
        {
        }
        glErrorAssert();

        char *start_position = (char *)state->model_file.contents;
        char *eof = start_position + state->model_file.size;
        char *position = start_position;
        uint32_t max_lines = 200000;
        uint32_t counter = 0;
        material null_material = {};
        null_material.texture_offset = -1;
        null_material.bump_texture_offset = -1;
        null_material.emit_texture_offset = -1;
        null_material.ao_texture_offset = -1;
        material *current_material = &null_material;
        while (position < eof)
        {
            uint32_t remainder = state->model_file.size - (uint32_t)(position - start_position);
            char *eol = next_newline(position, remainder);
            char line[1024];
            strncpy(line, position, (size_t)(eol - position));
            line[eol - position] = '\0';

            if (line[0] == '\0')
            {
            }
            else if (line[0] == 'v')
            {
                if (line[1] == 't')
                {
                    float a, b, c;
                    int num_found = sscanf(position + 2, "%f %f %f", &a, &b, &c);
                    if (num_found == 3)
                    {
                        v3 texture_coord = {a,b,c};
                        state->texture_coords[state->num_texture_coords++] = texture_coord;
                    }
                    else if (num_found == 2)
                    {
                        v3 texture_coord = {a, b, 0.0f};
                        state->texture_coords[state->num_texture_coords++] = texture_coord;
                    }
                    else
                    {
                        hassert(!"Invalid code path");
                    }
                }
                else if (line[1] == 'n')
                {
                    float a, b, c;
                    int num_found = sscanf(position + 2, "%f %f %f", &a, &b, &c);
                    if (num_found == 3)
                    {
                        v3 normal = {a,b,c};
                        state->normals[state->num_normals++] = normal;
                    }
                    else
                    {
                        hassert(!"Invalid code path");
                    }
                }
                else if (line[1] == ' ')
                {
                    float a, b, c;
                    if (sscanf(line + 2, "%f %f %f", &a, &b, &c) == 3)
                    {
                        if (state->num_vertices == 0)
                        {
                            state->model_max = {a, b, c};
                            state->model_min = {a, b, c};
                        }
                        else
                        {
                            if (a > state->model_max.x)
                            {
                                state->model_max.x = a;
                            }
                            if (b > state->model_max.y)
                            {
                                state->model_max.y = b;
                            }
                            if (c > state->model_max.z)
                            {
                                state->model_max.z = c;
                            }
                            if (a < state->model_min.x)
                            {
                                state->model_min.x = a;
                            }
                            if (b < state->model_min.y)
                            {
                                state->model_min.y = b;
                            }
                            if (c < state->model_min.z)
                            {
                                state->model_min.z = c;
                            }
                        }
                        v3 vertex = {a,b,c};
                        state->vertices[state->num_vertices++] = vertex;
                    }
                }
                else
                {
                    hassert(!"Invalid code path");
                }
            }
            else if (line[0] == 'f')
            {
                char a[100], b[100], c[100], d[100];
                int num_found = sscanf(line + 2, "%s %s %s %s", (char *)&a, (char *)&b, (char *)&c, (char *)&d);
                hassert((num_found == 4) || (num_found == 3));
                if (num_found == 4)
                {
                    uint32_t t1, t2;
                    int num_found2 = sscanf(a, "%d/%d", &t1, &t2);
                    if (num_found2 == 1)
                    {

                    }
                    else if (num_found2 == 2)
                    {
                        uint32_t a_vertex_id, a_texture_coord_id;
                        uint32_t b_vertex_id, b_texture_coord_id;
                        uint32_t c_vertex_id, c_texture_coord_id;
                        uint32_t d_vertex_id, d_texture_coord_id;
                        int num_found2;
                        num_found2 = sscanf(a, "%d/%d", &a_vertex_id, &a_texture_coord_id);
                        hassert(num_found2 == 2);
                        num_found2 = sscanf(b, "%d/%d", &b_vertex_id, &b_texture_coord_id);
                        hassert(num_found2 == 2);
                        num_found2 = sscanf(c, "%d/%d", &c_vertex_id, &c_texture_coord_id);
                        hassert(num_found2 == 2);
                        num_found2 = sscanf(d, "%d/%d", &d_vertex_id, &d_texture_coord_id);
                        hassert(num_found2 == 2);
                        face face1 = {
                            {
                                {a_vertex_id, a_texture_coord_id},
                                {b_vertex_id, b_texture_coord_id},
                                {c_vertex_id, c_texture_coord_id},
                            },
                            current_material->texture_offset,
                            current_material->bump_texture_offset,
                            current_material->emit_texture_offset,
                            current_material->ao_texture_offset,
                        };
                        face face2 = {
                            {
                                {c_vertex_id, c_texture_coord_id},
                                {d_vertex_id, d_texture_coord_id},
                                {a_vertex_id, a_texture_coord_id},
                            },
                            current_material->texture_offset,
                            current_material->bump_texture_offset,
                            current_material->emit_texture_offset,
                            current_material->ao_texture_offset,
                        };
                        state->faces[state->num_faces++] = face1;
                        state->faces[state->num_faces++] = face2;
                    }
                    else
                    {
                        hassert(!"Invalid number of face attributes");
                    }
                }
                else if (num_found == 3)
                {
                    uint32_t t1, t2, t3;
                    int num_found2 = sscanf(a, "%d/%d/%d", &t1, &t2, &t3);
                    if (num_found2 == 1)
                    {

                    }
                    else if (num_found2 == 2)
                    {
                        uint32_t a_vertex_id, a_texture_coord_id;
                        uint32_t b_vertex_id, b_texture_coord_id;
                        uint32_t c_vertex_id, c_texture_coord_id;
                        int num_found2;
                        num_found2 = sscanf(a, "%d/%d", &a_vertex_id, &a_texture_coord_id);
                        hassert(num_found2 == 2);
                        num_found2 = sscanf(b, "%d/%d", &b_vertex_id, &b_texture_coord_id);
                        hassert(num_found2 == 2);
                        num_found2 = sscanf(c, "%d/%d", &c_vertex_id, &c_texture_coord_id);
                        hassert(num_found2 == 2);
                        face face1 = {
                            {
                                {a_vertex_id, a_texture_coord_id},
                                {b_vertex_id, b_texture_coord_id},
                                {c_vertex_id, c_texture_coord_id},
                            },
                            current_material->texture_offset,
                            current_material->bump_texture_offset,
                            current_material->emit_texture_offset,
                            current_material->ao_texture_offset,
                        };
                        state->faces[state->num_faces++] = face1;
                    }
                    else if (num_found2 == 3)
                    {
                        uint32_t a_vertex_id, a_texture_coord_id, a_normal_id;
                        uint32_t b_vertex_id, b_texture_coord_id, b_normal_id;
                        uint32_t c_vertex_id, c_texture_coord_id, c_normal_id;
                        int num_found2;
                        num_found2 = sscanf(a, "%d/%d/%d", &a_vertex_id, &a_texture_coord_id, &a_normal_id);
                        hassert(num_found2 == 3);
                        num_found2 = sscanf(b, "%d/%d/%d", &b_vertex_id, &b_texture_coord_id, &b_normal_id);
                        hassert(num_found2 == 3);
                        num_found2 = sscanf(c, "%d/%d/%d", &c_vertex_id, &c_texture_coord_id, &c_normal_id);
                        hassert(num_found2 == 3);
                        face face1 = {
                            {
                                {a_vertex_id, a_texture_coord_id, a_normal_id},
                                {b_vertex_id, b_texture_coord_id, b_normal_id},
                                {c_vertex_id, c_texture_coord_id, c_normal_id},
                            },
                            current_material->texture_offset,
                            current_material->bump_texture_offset,
                            current_material->emit_texture_offset,
                            current_material->ao_texture_offset,
                        };
                        state->faces[state->num_faces++] = face1;
                    }
                    else
                    {
                        hassert(!"Invalid number of face attributes");
                    }
                }
            }
            else if (line[0] == '#')
            {
            }
            else if (line[0] == 'g')
            {
            }
            else if (line[0] == 'o')
            {
            }
            else if (line[0] == 's')
            {
            }
            else if (starts_with(line, "mtllib"))
            {
                char *filename = line + 7;
                bool loaded = memory->platform_editor_load_nearby_file(ctx, &state->mtl_file, state->model_file, filename);
                hassert(loaded);
                load_mtl(ctx, memory);
            }
            else if (starts_with(line, "usemtl"))
            {
                material *tm;
                char material_name[100];
                auto material_name_length = eol - position - 7;
                strncpy(material_name, line + 7, (size_t)material_name_length + 1);

                current_material = 0;
                for (tm = state->materials;
                        tm < state->materials + state->num_materials;
                        tm++)
                {
                    if (strcmp(tm->name, material_name) == 0)
                    {
                        current_material = tm;
                        break;
                    }
                }
                hassert(current_material);
            }
            else
            {
                hassert(!"Invalid code path");
            }

            if (*eol == '\0')
            {
                break;
            }
            position = eol + 1;
            while((*position == '\r') && (*position == '\n'))
            {
                position++;
            }
            if (counter++ >= max_lines)
            {
                hassert(!"Counter too high!");
                break;
            }
        }

        glErrorAssert();

        for (uint32_t face_idx = 0;
                face_idx < state->num_faces * 3;
                ++face_idx)
        {
            state->num_faces_array++;
            state->faces_array[face_idx] = (GLushort)face_idx;
        }


        for (GLushort face_array_idx = 0;
                face_array_idx < state->num_faces_array;
                face_array_idx += 3)
        {
            state->line_elements[state->num_line_elements++] = face_array_idx;
            state->line_elements[state->num_line_elements++] = (GLushort)(face_array_idx + 1);
            state->line_elements[state->num_line_elements++] = (GLushort)(face_array_idx + 1);
            state->line_elements[state->num_line_elements++] = (GLushort)(face_array_idx + 2);
            state->line_elements[state->num_line_elements++] = (GLushort)(face_array_idx + 2);
            state->line_elements[state->num_line_elements++] = face_array_idx;
        }

        glErrorAssert();
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->ibo);
        glErrorAssert();
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                (GLsizeiptr)(state->num_faces_array * sizeof(state->faces_array[0])),
                state->faces_array,
                GL_STATIC_DRAW);
        glErrorAssert();

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->line_ibo);
        glErrorAssert();
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                (GLsizeiptr)(state->num_line_elements * sizeof(state->line_elements[0])),
                state->line_elements,
                GL_STATIC_DRAW);
        glErrorAssert();

        for (uint32_t face_idx = 0;
                face_idx < state->num_faces;
                ++face_idx)
        {
            state->num_vbo_vertices += 3;
            editor_vertex_format *vbo_v1 = state->vbo_vertices + (3 * face_idx);
            editor_vertex_format *vbo_v2 = vbo_v1 + 1;
            editor_vertex_format *vbo_v3 = vbo_v2 + 1;
            face *f = state->faces + face_idx;
            v3 *face_v1 = state->vertices + (f->indices[0].vertex - 1);
            v3 *face_v2 = state->vertices + (f->indices[1].vertex - 1);
            v3 *face_v3 = state->vertices + (f->indices[2].vertex - 1);

            triangle3 t = {
                {face_v1->x, face_v1->y, face_v1->z},
                {face_v2->x, face_v2->y, face_v2->z},
                {face_v3->x, face_v3->y, face_v3->z},
            };

            v3 face_n1_v3;
            v4 face_n1_v4;
            v3 face_n2_v3;
            v4 face_n2_v4;
            v3 face_n3_v3;
            v4 face_n3_v4;

            if (f->indices[0].normal == 0)
            {
                v3 normal3 = winded_triangle_normal(t);
                v4 normal = {normal3.x, normal3.y, normal3.z, 1.0f};
                face_n1_v3 = normal3;
                face_n2_v3 = normal3;
                face_n3_v3 = normal3;
                face_n1_v4 = normal;
                face_n2_v4 = normal;
                face_n3_v4 = normal;
            }
            else
            {
                face_n1_v3 = state->normals[f->indices[0].normal - 1];
                face_n2_v3 = state->normals[f->indices[1].normal - 1];
                face_n3_v3 = state->normals[f->indices[2].normal - 1];
                face_n1_v4 = { face_n1_v3.x, face_n1_v3.y, face_n1_v3.z, 1.0f };
                face_n2_v4 = { face_n2_v3.x, face_n2_v3.y, face_n2_v3.z, 1.0f };
                face_n3_v4 = { face_n3_v3.x, face_n3_v3.y, face_n3_v3.z, 1.0f };
            }


            v3 *face_vt1 = state->texture_coords + (f->indices[0].texture_coord - 1);
            v3 *face_vt2 = state->texture_coords + (f->indices[1].texture_coord - 1);
            v3 *face_vt3 = state->texture_coords + (f->indices[2].texture_coord - 1);

            v4 face_t1;
            v4 face_t2;
            v4 face_t3;

            {
                v3 q1 = v3sub(t.p1, t.p0);
                v3 q2 = v3sub(t.p2, t.p0);
                float x1 = q1.x;
                float x2 = q2.x;
                float y1 = q1.y;
                float y2 = q2.y;
                float z1 = q1.z;
                float z2 = q2.z;

                float s1 = face_vt2->x - face_vt1->x;
                float t1 = face_vt2->y - face_vt1->y;
                float s2 = face_vt3->x - face_vt1->x;
                float t2 = face_vt3->y - face_vt1->y;

                float r = 1 / (s1 * t2 - s2 * t1);

                v3 sdir = {
                    r * (t2 * x1 - t1 * x2),
                    r * (t2 * y1 - t1 * y2),
                    r * (t2 * z1 - t1 * z2),
                };
                v3 tdir = {
                    r * (s1 * x2 - s1 * x1),
                    r * (s1 * y2 - s1 * y1),
                    r * (s1 * z2 - s1 * z1),
                };

                {
                    v3 normal3 = face_n1_v3;
                    v3 tangent3 = v3normalize(v3sub(sdir, v3mul(normal3, (v3dot(normal3, sdir)))));
                    float w = v3dot(v3cross(normal3, sdir), tdir) < 0.0f ? -1.0f : 1.0f;
                    face_t1 = {
                        tangent3.x,
                        tangent3.y,
                        tangent3.z,
                        w,
                    };
                }
                {
                    v3 normal3 = face_n2_v3;
                    v3 tangent3 = v3normalize(v3sub(sdir, v3mul(normal3, (v3dot(normal3, sdir)))));
                    float w = v3dot(v3cross(normal3, sdir), tdir) < 0.0f ? -1.0f : 1.0f;
                    face_t2 = {
                        tangent3.x,
                        tangent3.y,
                        tangent3.z,
                        w,
                    };
                }
                {
                    v3 normal3 = face_n3_v3;
                    v3 tangent3 = v3normalize(v3sub(sdir, v3mul(normal3, (v3dot(normal3, sdir)))));
                    float w = v3dot(v3cross(normal3, sdir), tdir) < 0.0f ? -1.0f : 1.0f;
                    face_t3 = {
                        tangent3.x,
                        tangent3.y,
                        tangent3.z,
                        w,
                    };
                }
            }

            vbo_v1->v.position[0] = face_v1->x;
            vbo_v1->v.position[1] = face_v1->y;
            vbo_v1->v.position[2] = face_v1->z;
            vbo_v1->v.position[3] = 1.0f;
            vbo_v1->normal = face_n1_v4;
            vbo_v1->tangent = face_t1;

            vbo_v2->v.position[0] = face_v2->x;
            vbo_v2->v.position[1] = face_v2->y;
            vbo_v2->v.position[2] = face_v2->z;
            vbo_v2->v.position[3] = 1.0f;
            vbo_v2->normal = face_n2_v4;
            vbo_v2->tangent = face_t2;

            vbo_v3->v.position[0] = face_v3->x;
            vbo_v3->v.position[1] = face_v3->y;
            vbo_v3->v.position[2] = face_v3->z;
            vbo_v3->v.position[3] = 1.0f;
            vbo_v3->normal = face_n3_v4;
            vbo_v3->tangent = face_t3;

            vbo_v1->v.color[0] = face_vt1->x;
            vbo_v1->v.color[1] = 1 - face_vt1->y;
            vbo_v1->v.color[2] = 0.0f;
            vbo_v1->v.color[3] = 1.0f;

            vbo_v2->v.color[0] = face_vt2->x;
            vbo_v2->v.color[1] = 1 - face_vt2->y;
            vbo_v2->v.color[2] = 0.0f;
            vbo_v2->v.color[3] = 1.0f;

            vbo_v3->v.color[0] = face_vt3->x;
            vbo_v3->v.color[1] = 1 - face_vt3->y;
            vbo_v3->v.color[2] = 0.0f;
            vbo_v3->v.color[3] = 1.0f;

            vbo_v1->style[0] = 3.0f;
            vbo_v1->style[1] = (float)f->texture_offset;
            vbo_v1->style[2] = (float)f->bump_texture_offset;
            vbo_v1->style[3] = (100.0f * (f->ao_texture_offset + 1)) + ((float)f->emit_texture_offset + 1);
            vbo_v2->style[0] = 3.0f;
            vbo_v2->style[1] = (float)f->texture_offset;
            vbo_v2->style[2] = (float)f->bump_texture_offset;
            vbo_v2->style[3] = (100.0f * (f->ao_texture_offset + 1)) + ((float)f->emit_texture_offset + 1);
            vbo_v3->style[0] = 3.0f;
            vbo_v3->style[1] = (float)f->texture_offset;
            vbo_v3->style[2] = (float)f->bump_texture_offset;
            vbo_v3->style[3] = (100.0f * (f->ao_texture_offset + 1)) + ((float)f->emit_texture_offset + 1);
        }

        glBindBuffer(GL_ARRAY_BUFFER, state->vbo);
        glErrorAssert();

        glBufferData(GL_ARRAY_BUFFER,
                (GLsizeiptr)(sizeof(state->vbo_vertices[0]) * state->num_faces * 3),
                state->vbo_vertices,
                GL_STATIC_DRAW);
        glErrorAssert();

        load_aabb_buffer_objects(state, state->model_min, state->model_max);
        load_bounding_sphere(state, state->model_min, state->model_max);
    }

    for (uint32_t i = 0;
            i < harray_count(input->controllers);
            ++i)
    {
        if (!input->controllers[i].is_active)
        {
            continue;
        }
        game_controller_state *controller = &input->controllers[i];
        if (controller->buttons.back.ended_down && !controller->buttons.back.repeat)
        {
            memory->quit = true;
        }
        if (controller->buttons.start.ended_down && !controller->buttons.start.repeat)
        {
            state->hide_lines ^= true;
        }
        if (controller->buttons.move_up.ended_down && !controller->buttons.move_up.repeat)
        {
            state->model_mode = (state->model_mode + 1) % 9;
        }
        if (controller->buttons.move_right.ended_down && !controller->buttons.move_right.repeat)
        {
            state->shading_mode = (state->shading_mode + 1) % 5;
        }
        if (controller->buttons.move_down.ended_down && !controller->buttons.move_down.repeat)
        {
            state->x_rotation = (state->x_rotation + 1) % 4;
        }
        if (controller->buttons.move_left.ended_down && !controller->buttons.move_left.repeat)
        {
            state->y_rotation = (state->y_rotation + 1) % 4;
        }
    }

    v2 mouse_loc = {
        (float)input->mouse.x / ((float)input->window.width / 2.0f) - 1.0f,
        ((float)input->mouse.y / ((float)input->window.height / 2.0f) - 1.0f) * -1.0f,
    };

    {
        glBindBuffer(GL_ARRAY_BUFFER, state->debug_vbo);
        float height = (float)debug_buffer_height / ((float)input->window.height / 2.0f);
        float top = -(1-height);
        float width = (float)debug_buffer_width / ((float)input->window.width / 2.0f);
        float left = 1 - width;
        editor_vertex_format font_vertices[4] = {
            {
                {
                    {left, top, 0.0, 1.0},
                    { 0.0, 1.0, 1.0, 1.0},
                },
                {2.0, 0.0, 0.0, 0.0},
            },
            {
                {
                    { 1.0, top, 0.0, 1.0},
                    { 1.0, 1.0, 1.0, 1.0},
                },
                {2.0, 0.0, 0.0, 0.0},
            },
            {
                {
                    { 1.0,-1.0, 0.0, 1.0},
                    {1.0, 0.0, 1.0, 1.0},
                },
                {2.0, 0.0, 0.0, 0.0},
            },
            {
                {
                    {left,-1.0, 0.0, 1.0},
                    {0.0, 0.0, 1.0, 1.0},
                },
                {2.0, 0.0, 0.0, 0.0},
            },
        };
        glBufferData(GL_ARRAY_BUFFER, sizeof(font_vertices), font_vertices, GL_STATIC_DRAW);
    }

    glBindVertexArray(state->vao);

    glErrorAssert();

    // Revert to something resembling defaults
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthFunc(GL_ALWAYS);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_PRIMITIVE_RESTART);

    glErrorAssert();

    glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glErrorAssert();

    state->delta_t += input->delta_t;

    glUseProgram(state->program_b.program);
    glBindBuffer(GL_ARRAY_BUFFER, state->vbo);

    glErrorAssert();

    setup_vertex_attrib_array(state);

    for (uint32_t idx = 0;
            idx < state->num_texture_ids;
            ++idx)
    {
        glUniform1i(state->sampler_ids[idx], (GLint)idx);
        glActiveTexture(GL_TEXTURE0 + idx);
        glErrorAssert();
        glBindTexture(GL_TEXTURE_2D, state->texture_ids[idx]);
        glErrorAssert();
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->ibo);
    glErrorAssert();

    v3 center = v3div(v3add(state->model_max, state->model_min), 2.0f);
    m4 center_translate = m4identity();
    center_translate.cols[3] = {-center.x, -center.y, -center.z, 1.0f};

    v3 dimension = v3sub(state->model_max, state->model_min);
    float max_dimension = dimension.x;
    if (dimension.y > max_dimension)
    {
        max_dimension = dimension.y;
    }
    if (dimension.z > max_dimension)
    {
        max_dimension = dimension.z;
    }

    v3 axis = {0.0f, 1.0f, 0.0f};
    m4 rotate = m4rotation(axis, state->delta_t);
    v3 x_axis = {1.0f, 0.0f, 0.0f};
    v3 y_axis = {0.0f, 0.0f, 1.0f};
    m4 x_rotate = m4rotation(x_axis, (pi / 2) * state->x_rotation);
    m4 y_rotate = m4rotation(y_axis, (pi / 2) * state->y_rotation);
    rotate = m4mul(rotate, x_rotate);
    rotate = m4mul(rotate, y_rotate);

    m4 scale = m4identity();
    scale.cols[0].E[0] = 2.0f / max_dimension;
    scale.cols[1].E[1] = 2.0f / max_dimension;
    scale.cols[2].E[2] = 2.0f / max_dimension;
    m4 translate = m4identity();
    translate.cols[3] = v4{0.0f, 0.0f, -10.0f, 1.0f};

    m4 a = center_translate;
    m4 b = scale;
    m4 c = rotate;
    m4 d = translate;

    m4 u_model = m4mul(d, m4mul(c, m4mul(b, a)));

    v4 light_position = {-5.0f, -3.0f, -12.0f, 1.0f};
    glUniform4fv(state->program_b.u_w_lightPosition_id, 1, (float *)&light_position);
    v4 u_mvp_enabled = {1.0f, 0.0f, 0.0f, 0.0f};
    glUniform4fv(state->program_b.u_mvp_enabled_id, 1, (float *)&u_mvp_enabled);
    glUniformMatrix4fv(state->program_b.u_model_id, 1, false, (float *)&u_model);
    m4 u_view = m4identity();
    glUniformMatrix4fv(state->program_b.u_view_id, 1, false, (float *)&u_view);
    float ratio = (float)input->window.width / (float)input->window.height;
    m4 u_perspective = m4frustumprojection(state->near_, state->far_, {-ratio, -1.0f}, {ratio, 1.0f});
    glUniformMatrix4fv(state->program_b.u_perspective_id, 1, false, (float *)&u_perspective);
    glUniform1i(state->program_b.u_model_mode_id, state->model_mode);
    glUniform1i(state->program_b.u_shading_mode_id, state->shading_mode);

    glDrawElements(GL_TRIANGLES, (GLsizei)state->num_faces_array, GL_UNSIGNED_SHORT, 0);
    glErrorAssert();

    if (!state->hide_lines)
    {
        glUniform1i(state->program_b.u_model_mode_id, 0);
        glUniform1i(state->program_b.u_shading_mode_id, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->line_ibo);
        glDepthFunc(GL_LEQUAL);
        u_mvp_enabled = {1.0f, 0.0f, 0.0f, 1.0f};
        glUniform4fv(state->program_b.u_mvp_enabled_id, 1, (float *)&u_mvp_enabled);
        glDrawElements(GL_LINES, (GLsizei)state->num_line_elements, GL_UNSIGNED_SHORT, 0);
    }

    {
        glUniform1i(state->program_b.u_model_mode_id, 0);
        glUniform1i(state->program_b.u_shading_mode_id, 0);
        glBindBuffer(GL_ARRAY_BUFFER, state->aabb_cube_vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->aabb_cube_ibo);
        setup_vertex_attrib_array(state);
        glDepthFunc(GL_LEQUAL);
        u_mvp_enabled = {1.0f, 0.0f, 0.0f, 1.0f};
        glUniform4fv(state->program_b.u_mvp_enabled_id, 1, (float *)&u_mvp_enabled);
        glDrawElements(GL_LINES, 24, GL_UNSIGNED_SHORT, 0);
        glErrorAssert();
    }

    {
        glUniform1i(state->program_b.u_model_mode_id, 0);
        glUniform1i(state->program_b.u_shading_mode_id, 0);
        glBindBuffer(GL_ARRAY_BUFFER, state->bounding_sphere_vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->bounding_sphere_ibo);
        setup_vertex_attrib_array(state);
        u_mvp_enabled = {1.0f, 0.0f, 0.0f, 1.0f};
        glUniform4fv(state->program_b.u_mvp_enabled_id, 1, (float *)&u_mvp_enabled);
        glPrimitiveRestartIndex(65535);
        glDrawElements(GL_LINE_LOOP, (GLsizei)state->num_bounding_sphere_elements, GL_UNSIGNED_SHORT, 0);
        glErrorAssert();
    }

    {

        memset(state->debug_buffer, 0, sizeof(state->debug_buffer));
        char msg[1024] = {};
        sprintf(msg + strlen(msg), "MODEL MODE:...");
        switch (state->model_mode)
        {
            case 0:
            {
                sprintf(msg + strlen(msg), "STD");
            } break;
            case 1:
            {
                sprintf(msg + strlen(msg), "DISCARD");
            } break;
            case 2:
            {
                sprintf(msg + strlen(msg), "NORMAL");
            } break;
            case 3:
            {
                sprintf(msg + strlen(msg), "LIGHT.REFLECT");
            } break;
            case 4:
            {
                sprintf(msg + strlen(msg), "BUMP.MAP");
            } break;
            case 5:
            {
                sprintf(msg + strlen(msg), "BUMP.NORMAL");
            } break;
            case 6:
            {
                sprintf(msg + strlen(msg), "NORMAL.CAMERA");
            } break;
            case 7:
            {
                sprintf(msg + strlen(msg), "EMIT");
            } break;
            case 8:
            {
                sprintf(msg + strlen(msg), "AMBIENT.OCCLUSION");
            } break;
            default:
            {
                sprintf(msg + strlen(msg), "UNKNOWN");
            } break;
        }
        sprintf(msg + strlen(msg), "...");

        sprintf(msg + strlen(msg), "SHADING MODE...");
        switch (state->shading_mode)
        {
            case 0:
            {
                sprintf(msg + strlen(msg), "DIFFUSE");
            } break;
            case 1:
            {
                sprintf(msg + strlen(msg), "LIGHTING.NOBUMP");
            } break;
            case 2:
            {
                sprintf(msg + strlen(msg), "LIGHTING.WITHBUMP");
            } break;
            case 3:
            {
                sprintf(msg + strlen(msg), "LIGHTING.WITHBUMP.EMIT");
            } break;
            case 4:
            {
                sprintf(msg + strlen(msg), "LIGHTING.WITHBUMP.EMIT.AO");
            } break;
            default:
            {
                sprintf(msg + strlen(msg), "UNKNOWN");
            } break;
        }
        sprintf(msg + strlen(msg), "...");
        sprintf(msg + strlen(msg), "%d x %d", input->window.width, input->window.height);
        sprintf(msg + strlen(msg), "(%0.4f)", ratio);
        write_to_buffer(&state->debug_draw_buffer, &state->debug_font.font, msg);

        glDisable(GL_DEPTH_TEST);
        glDepthFunc(GL_ALWAYS);
        glUniform1i(state->program_b.u_model_mode_id, 0);
        glUniform1i(state->program_b.u_shading_mode_id, 0);

        glBindBuffer(GL_ARRAY_BUFFER, state->debug_vbo);
        setup_vertex_attrib_array(state);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, state->debug_texture_id);
        glUniform1i(
            glGetUniformLocation(state->program_b.program, "tex"),
            0);
        glTexSubImage2D(GL_TEXTURE_2D,
            0,
            0,
            0,
            (GLsizei)debug_buffer_width,
            (GLsizei)debug_buffer_height,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            state->debug_buffer);
        u_mvp_enabled = {0.0f, 0.0f, 0.0f, 0.0f};
        glUniform4fv(state->program_b.u_mvp_enabled_id, 1, (float *)&u_mvp_enabled);
        v2 position = {0, 0};
        glUniform2fv(state->program_b.u_offset_id, 1, (float *)&position);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }

    {
        glDisable(GL_DEPTH_TEST);
        glDepthFunc(GL_ALWAYS);
        glUniform1i(state->program_b.u_model_mode_id, 0);
        glUniform1i(state->program_b.u_shading_mode_id, 0);
        float mouse_width = 16.0f / ((float)input->window.width / 2.0f);
        float mouse_height = 16.0f / ((float)input->window.height / 2.0f);
        editor_vertex_format vertices[] =
        {
            {
                {
                    {mouse_loc.x, mouse_loc.y, 0.0, 1.0},
                    {0.0, 0.0, 1.0, 1.0},
                },
                {2.0, 0.0, 0.0, 0.0},
            },
            {
                {
                    {mouse_loc.x + mouse_width, mouse_loc.y, 0.0, 1.0},
                    {1.0, 0.0, 1.0, 1.0},
                },
                {2.0, 0.0, 0.0, 0.0},
            },
            {
                {
                    {mouse_loc.x + mouse_width, mouse_loc.y - mouse_height, 0.0, 1.0},
                    {1.0, 1.0, 1.0, 1.0},
                },
                {2.0, 0.0, 0.0, 0.0},
            },
            {
                {
                    {mouse_loc.x, mouse_loc.y - mouse_height, 0.0, 1.0},
                    {0.0, 1.0, 1.0, 1.0},
                },
                {2.0, 0.0, 0.0, 0.0},
            },
        };
        glBindBuffer(GL_ARRAY_BUFFER, state->mouse_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        setup_vertex_attrib_array(state);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, state->mouse_texture);
        glUniform1i(
            glGetUniformLocation(state->program_b.program, "tex"),
            0);
        u_mvp_enabled = {0.0f, 0.0f, 0.0f, 0.0f};
        glUniform4fv(state->program_b.u_mvp_enabled_id, 1, (float *)&u_mvp_enabled);
        v2 position = {0, 0};
        glUniform2fv(state->program_b.u_offset_id, 1, (float *)&position);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glErrorAssert();
    }

    {
        glDisable(GL_DEPTH_TEST);
        glDepthFunc(GL_ALWAYS);
        glUseProgram(state->program_ui2d.program);

        float screen_pixel_size[] =
        {
            (float)input->window.width,
            (float)input->window.height,
        };
        glUniform2fv(state->program_ui2d.screen_pixel_size_id, 1, (float *)&screen_pixel_size);

        ui2d_push_context pushctx = {};
        ui2d_vertex_format vertices[100];
        uint32_t elements[200];
        uint32_t textures[10];
        pushctx.vertices = vertices;
        pushctx.max_vertices = harray_count(vertices);
        pushctx.elements = elements;
        pushctx.max_elements = harray_count(elements);
        pushctx.textures = textures;
        pushctx.max_textures = harray_count(textures);

        rectangle2 rect = {
            { 35, 35},
            {105, 40},
        };
        push_panel(state, &pushctx, rect);

        char full_text[] = "hello world";
        char *text = (char *)full_text;
        float x = 50;
        float y = input->window.height - 50.0f;
        while (*text) {
            stbtt_aligned_quad q;
            stbtt_GetPackedQuad(state->stb_font.chardata, 512, 512, *text++, &x, &y, &q, 0);
            q.y0 = input->window.height - q.y0;
            q.y1 = input->window.height - q.y1;
            push_quad(&pushctx, q, state->stb_font.font_tex, 1);
        }

        ui2d_render_elements(state, &pushctx);
    }
}

