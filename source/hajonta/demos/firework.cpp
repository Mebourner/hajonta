#pragma once

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

#include "hajonta/programs/ui2d.h"
#include "hajonta/ui/ui2d.cpp"

enum struct shader_mode
{
    standard,
    diffuse_texture,
    normal_texture,
    ao_texture,
    emit_texture,
    blinn_phong,
    specular_exponent_texture,
};

DEMO(demo_firework)
{
    glErrorAssert();

    game_state *state = (game_state *)memory->memory;
    demo_firework_state *demo_state = &state->demos.firework;

    if (!demo_state->firework_vbo)
    {
        glErrorAssert();
        glGenBuffers(1, &demo_state->firework_vbo);

        struct
        {
            float position[4];
        } vertices[] = {
            { 0.000f,  0.000f,  1.000f, 1.000f},
            { 0.894f,  0.000f,  0.447f, 1.000f},
            { 0.276f,  0.851f,  0.447f, 1.000f},
            {-0.724f,  0.526f,  0.447f, 1.000f},
            {-0.724f, -0.526f,  0.447f, 1.000f},
            { 0.276f, -0.851f,  0.447f, 1.000f},
            { 0.724f,  0.526f, -0.447f, 1.000f},
            {-0.276f,  0.851f, -0.447f, 1.000f},
            {-0.894f,  0.000f, -0.447f, 1.000f},
            {-0.276f, -0.851f, -0.447f, 1.000f},
            { 0.724f, -0.526f, -0.447f, 1.000f},
            { 0.000f,  0.000f, -1.000f, 1.000f},
        };
        glBindBuffer(GL_ARRAY_BUFFER, demo_state->firework_vbo);
        glBufferData(GL_ARRAY_BUFFER,
                sizeof(vertices),
                vertices,
                GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        struct
        {
            uint8_t vertex_ids[3];
        } faces[] =
        {
            { 0, 1, 2},
            { 0, 2, 3},
            { 0, 3, 4},
            { 0, 4, 5},
            { 0, 5, 1},
            {11, 6, 7},
            {11, 7, 8},
            {11, 8, 9},
            {11, 9,10},
            {11,10, 6},
            { 1, 2, 6},
            { 2, 3, 7},
            { 3, 4, 8},
            { 4, 5, 9},
            { 5, 1,10},
            { 6, 7, 2},
            { 7, 8, 3},
            { 8, 9, 4},
            { 9,10, 5},
            {10, 6, 1},
        };
        demo_state->firework_num_faces = harray_count(faces);
        glGenBuffers(1, &demo_state->firework_ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, demo_state->firework_ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                sizeof(faces),
                faces,
                GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        glGenTextures(1, &demo_state->firework_texture);
        glBindTexture(GL_TEXTURE_2D, demo_state->firework_texture);
        uint32_t color = 0xffff00ff;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &color);

        glErrorAssert();

        {
            glGenBuffers(1, &demo_state->ground_vbo);
            struct
            {
                float position[4];
            } vertices[] = {
                {-200.000f, 0.000f,-100.000f, 1.000f},
                {-200.000f, 0.000f, 100.000f, 1.000f},
                { 200.000f, 0.000f,-100.000f, 1.000f},
                { 200.000f, 0.000f, 100.000f, 1.000f},
            };
            glBindBuffer(GL_ARRAY_BUFFER, demo_state->ground_vbo);
            glBufferData(GL_ARRAY_BUFFER,
                    sizeof(vertices),
                    vertices,
                    GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            struct
            {
                uint8_t vertex_ids[3];
            } faces[] =
            {
                { 0, 1, 2},
                { 0, 2, 3},
            };
            demo_state->ground_num_faces = harray_count(faces);
            glGenBuffers(1, &demo_state->ground_ibo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, demo_state->ground_ibo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                    sizeof(faces),
                    faces,
                    GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

            glGenTextures(1, &demo_state->ground_texture);
            glBindTexture(GL_TEXTURE_2D, demo_state->ground_texture);
            uint32_t color = 0xff202020;
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &color);

            glErrorAssert();
        }

        firework_particle particles[] =
        {
            {
                { 0.0f, 10.0f,-26.0f},
                { 0.0f, 15.0f, -5.0f},
                { 0.0f, -9.8f,  0.0f},
                { 0.0f,  0.0f,  0.0f},
            },
            {
                { 5.0f, 20.0f,-38.0f},
                { 3.0f, 10.0f,  0.0f},
                { 0.0f, -9.8f,  0.0f},
                { 0.0f,  0.0f,  0.0f},
            },
            {
                {-5.0f,  5.0f,-50.0f},
                {10.0f, 20.0f,  7.0f},
                { 0.0f, -9.8f,  0.0f},
                { 0.0f,  0.0f,  0.0f},
            },
        };
        uint32_t num_particles = harray_count(particles);
        for (uint32_t idx = 0; idx < num_particles; ++idx)
        {
            demo_state->particles[idx] = particles[idx];
        }
        demo_state->num_particles = num_particles;
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
        if (controller->buttons.back.ended_down)
        {
            state->demos.active_demo = 0;
        }
    }

    glErrorAssert();

    glDepthFunc(GL_LESS);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.0f, 0.0f, 0.1f, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glErrorAssert();

    demo_state->delta_t += input->delta_t;

    glUseProgram(state->program_c.program);
    glUniform1i(state->program_c.u_shader_mode_id, (int)shader_mode::diffuse_texture);

    glBindBuffer(GL_ARRAY_BUFFER, demo_state->firework_vbo);
    glEnableVertexAttribArray((GLuint)state->program_c.a_pos_id);
    glVertexAttribPointer((GLuint)state->program_c.a_pos_id, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, demo_state->firework_ibo);

    glUniform1i(
        glGetUniformLocation(state->program_c.program, "tex"),
        0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, demo_state->firework_texture);

    glErrorAssert();

    m4 view = m4identity();
    view.cols[3] = v4{0,-6,0,1};
    float ratio = (float)input->window.width / (float)input->window.height;
    m4 projection = m4frustumprojection(1.0f, 100.0f, {-ratio, -1.0f}, {ratio, 1.0f});

    glUniformMatrix4fv(state->program_c.u_view_id, 1, false, (float *)&view);
    glUniformMatrix4fv(state->program_c.u_projection_id, 1, false, (float *)&projection);

    glErrorAssert();

    for (uint32_t idx = 0; idx < demo_state->num_particles; ++idx)
    {
        firework_particle *fp = demo_state->particles + idx;

        v3 acceleration = fp->constant_acceleration;
        acceleration = v3sub(acceleration, v3mul(fp->velocity, 0.05f));
        v3 movement = v3add(
            v3mul(acceleration, 0.5f * (input->delta_t * input->delta_t)),
            v3mul(fp->velocity, input->delta_t)
        );
        fp->position = v3add(fp->position, movement);
        fp->velocity = v3add(
            v3mul(acceleration, input->delta_t),
            fp->velocity
        );

        if (fp->position.y < 0.0f)
        {
            firework_particle *last_particle = demo_state->particles + demo_state->num_particles - 1;
            *fp = *last_particle;
            demo_state->num_particles--;
            idx--;
            continue;
        }

        m4 model = m4identity();
        m4 translate = m4identity();
        translate.cols[3] = v4{fp->position.x, fp->position.y, fp->position.z, 1.000f};
        model = m4mul(model, translate);

        glUniformMatrix4fv(state->program_c.u_model_id, 1, false, (float *)&model);
        glDrawElements(GL_TRIANGLES, (GLsizei)(demo_state->firework_num_faces * 3), GL_UNSIGNED_BYTE, 0);
    }

    glErrorAssert();

    glBindBuffer(GL_ARRAY_BUFFER, demo_state->ground_vbo);
    glEnableVertexAttribArray((GLuint)state->program_c.a_pos_id);
    glVertexAttribPointer((GLuint)state->program_c.a_pos_id, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, demo_state->ground_ibo);

    glUniform1i(
        glGetUniformLocation(state->program_c.program, "tex"),
        0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, demo_state->ground_texture);

    {
        m4 model = m4identity();
        glUniformMatrix4fv(state->program_c.u_model_id, 1, false, (float *)&model);
        glDrawElements(GL_TRIANGLES, (GLsizei)(demo_state->firework_num_faces * 3), GL_UNSIGNED_BYTE, 0);
    }

    glErrorAssert();

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    glErrorAssert();

    if (!demo_state->num_particles)
    {
        memory->quit = true;
    }
}
