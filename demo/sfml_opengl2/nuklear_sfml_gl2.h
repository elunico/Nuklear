/*
 * Nuklear - 1.32.0 - public domain
 * no warrenty implied; use at your own risk.
 * authored from 2015-2016 by Micha Mettke
 */
/*
 * ==============================================================
 *
 *                              API
 *
 * ===============================================================
 */
#ifndef NK_SFML_GL2_H_
#define NK_SFML_GL2_H_

#include <SFML/Window.hpp>

NK_API struct nk_context*	nk_sfml_init(sf::Window* window);
NK_API void					nk_sfml_font_stash_begin(struct nk_font_atlas** atlas);
NK_API void					nk_sfml_font_stash_end(void);
NK_API int 					nk_sfml_handle_event(sf::Event* event);
NK_API void 				nk_sfml_render(enum nk_anti_aliasing, int max_vertex_buffer, int max_element_buffer);
NK_API void 				nk_sfml_shutdown(void);

#endif
/*
 * ==============================================================
 *
 *                          IMPLEMENTATION
 *
 * ===============================================================
 */
 #ifdef NK_SFML_GL2_IMPLEMENTATION

struct nk_sfml_device
{
	struct nk_buffer cmds;
	struct nk_draw_null_texture null;
	GLuint font_tex;
};

struct nk_sfml_vertex
{
	float position[2];
	float uv[2];
	nk_byte col[4];
};

static struct nk_sfml
{
	sf::Window* window;
	struct nk_sfml_device ogl;
	struct nk_context ctx;
	struct nk_font_atlas atlas;
} sfml;

NK_INTERN void
nk_sfml_device_upload_atlas(const void* image, int width, int height)
{
	struct nk_sfml_device* dev = &sfml.ogl;
	glGenTextures(1, &dev->font_tex);
	glBindTexture(GL_TEXTURE_2D, dev->font_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)width, (GLsizei)height, 0, 
				GL_RGBA, GL_UNSIGNED_BYTE, image);
}

NK_API void
nk_sfml_render(enum nk_anti_aliasing AA, int max_vertex_buffer, int max_element_buffer)
{
	/* setup global state */
	struct nk_sfml_device* dev = &sfml.ogl;
	struct nk_vec2 scale;

	sf::Vector2u window_size = sfml.window->getSize();
	sf::Vector2u view_size = sfml.window->getSize();

	scale.x = (float)view_size.x / (float)window_size.x;
	scale.y = (float)view_size.y / (float)window_size.y;

	glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_TRANSFORM_BIT);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_SCISSOR_TEST);
	glEnable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glViewport(0, 0, (GLsizei)view_size.x, (GLsizei)view_size.y);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0.0f, window_size.x, window_size.y, 0.0f, -1.0f, 1.0f);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	{
		GLsizei vs = sizeof(struct nk_sfml_vertex);
		size_t vp = offsetof(struct nk_sfml_vertex, position);
		size_t vt = offsetof(struct nk_sfml_vertex, uv);
		size_t vc = offsetof(struct nk_sfml_vertex, col);

		/* convert from command queue into draw  list and draw to screen */
		const struct nk_draw_command* cmd;
		const nk_draw_index* offset = NULL;
		struct nk_buffer vbuf, ebuf;

		/* fill converting configuration */
		struct nk_convert_config config;
		static const struct nk_draw_vertex_layout_element vertex_layout[] = 
		{
			{NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(struct nk_sfml_vertex, position)},
			{NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(struct nk_sfml_vertex, uv)},
			{NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, NK_OFFSETOF(struct nk_sfml_vertex, col)},
			{NK_VERTEX_LAYOUT_END}
		};
		NK_MEMSET(&config, 0, sizeof(config));
		config.vertex_layout = vertex_layout;
		config.vertex_size = sizeof(struct nk_sfml_vertex);
		config.vertex_alignment = NK_ALIGNOF(struct nk_sfml_vertex);
		config.null = dev->null;
		config.circle_segment_count = 22;
		config.curve_segment_count = 22;
		config.arc_segment_count = 22;
		config.global_alpha = 1.0f;
		config.shape_AA = AA;
		config.line_AA = AA;

		/* convert shapes into vertices */
		nk_buffer_init_default(&vbuf);
		nk_buffer_init_default(&ebuf);
		nk_convert(&sfml.ctx, &dev->cmds, &vbuf, &ebuf, &config);

		/* setup vertex buffer pointer */
		{
			const void* vertices = nk_buffer_memory_const(&vbuf);
			glVertexPointer(2, GL_FLOAT, vs, (const void*)((const nk_byte*)vertices + vp));
			glTexCoordPointer(2, GL_FLOAT, vs, (const void*)((const nk_byte*)vertices + vt));
			glColorPointer(4, GL_UNSIGNED_BYTE, vs, (const void*)((const nk_byte*)vertices + vc));
		}

		/* iterate over and execute each draw command */
		offset = (const nk_draw_index*)nk_buffer_memory_const(&ebuf);
		nk_draw_foreach(cmd, &sfml.ctx, &dev->cmds)
		{
			if(!cmd->elem_count) continue;

			glBindTexture(GL_TEXTURE_2D, (GLuint)cmd->texture.id);
			glScissor(
				(GLint)(cmd->clip_rect.x * scale.x),
				(GLint)((window_size.y - (GLint)(cmd->clip_rect.y + cmd->clip_rect.h)) * scale.y),
				(GLint)(cmd->clip_rect.w * scale.x),
				(GLint)(cmd->clip_rect.h * scale.y));
			glDrawElements(GL_TRIANGLES, (GLsizei)cmd->elem_count, GL_UNSIGNED_SHORT, offset);
			offset += cmd->elem_count;
		}
		nk_clear(&sfml.ctx);
		nk_buffer_free(&vbuf);
		nk_buffer_free(&ebuf);
	}

	/* default OpenGL state */
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, 0);
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glPopAttrib();
}

static void
nk_sfml_clipboard_paste(nk_handle usr, struct nk_text_edit* edit)
{
	/* Not Implemented in SFML 
	sf::Clipboard clipboard(sfml.window);
	const char* text = clipboard.getText();

	if(text)
		nk_textedit_paste(edit, text, nk_strlen(text));
		(void)usr;
	*/
}

static void
nk_sfml_clipboard_copy(nk_handle usr, const char* text, int len)
{
	char* str = 0;
	(void)usr;
	if(!len)
		return;
	str = (char*)malloc((size_t)len+1);
	if(!str)
		return;
	memcpy(str, text, (size_t)len);
	str[len] = '\0';

	/* Not Implemented in SFML
	sf::Clipboard clipboard(sfml.window);
	clipboard.setText(str);
	*/

	free(str);
}

NK_API struct nk_context*
nk_sfml_init(sf::Window* window)
{
	sfml.window = window;
	nk_init_default(&sfml.ctx, 0);
	sfml.ctx.clip.copy = nk_sfml_clipboard_copy;
	sfml.ctx.clip.paste = nk_sfml_clipboard_paste;
	sfml.ctx.clip.userdata = nk_handle_ptr(0);
	nk_buffer_init_default(&sfml.ogl.cmds);
	return &sfml.ctx;
}

NK_API void
nk_sfml_font_stash_begin(struct nk_font_atlas** atlas)
{
	nk_font_atlas_init_default(&sfml.atlas);
	nk_font_atlas_begin(&sfml.atlas);
	*atlas = &sfml.atlas;
}

NK_API void
nk_sfml_font_stash_end(void)
{
	const void* image;
	int w, h;
	image = nk_font_atlas_bake(&sfml.atlas, &w, &h, NK_FONT_ATLAS_RGBA32);
	nk_sfml_device_upload_atlas(image, w, h);
	nk_font_atlas_end(&sfml.atlas, nk_handle_id((int)sfml.ogl.font_tex), &sfml.ogl.null);
	if(sfml.atlas.default_font)
		nk_style_set_font(&sfml.ctx, &sfml.atlas.default_font->handle);
}

NK_API int
nk_sfml_handle_event(sf::Event* event)
{
	struct nk_context* ctx = &sfml.ctx;

	/* optional grabbing behavior */
	if(ctx->input.mouse.grab)
	{
		sfml.window->setMouseCursorGrabbed(true);
		ctx->input.mouse.grab = 0;
	}
	else if(ctx->input.mouse.ungrab)
	{
		int x = (int)ctx->input.mouse.prev.x;
		int y = (int)ctx->input.mouse.prev.y;
		sfml.window->setMouseCursorGrabbed(false);
		sf::Mouse::setPosition(sf::Vector2i(x, y));
		ctx->input.mouse.ungrab = 0;
	}

	if(event->type == sf::Event::KeyReleased || event->type == sf::Event::KeyPressed)
	{
		int down = event->type == sf::Event::KeyPressed;
		sf::Keyboard::Key key = event->key.code;

		if(key == sf::Keyboard::RShift || key == sf::Keyboard::LShift)
			nk_input_key(ctx, NK_KEY_SHIFT, down);
		else if(key == sf::Keyboard::Delete)
			nk_input_key(ctx, NK_KEY_DEL, down);
		else if(key == sf::Keyboard::Return)
			nk_input_key(ctx, NK_KEY_ENTER, down);
		else if(key == sf::Keyboard::Tab)
			nk_input_key(ctx, NK_KEY_TAB, down);
		else if(key == sf::Keyboard::BackSpace)
			nk_input_key(ctx, NK_KEY_BACKSPACE, down);
		else if(key == sf::Keyboard::Home)
		{
			nk_input_key(ctx, NK_KEY_TEXT_START, down);
			nk_input_key(ctx, NK_KEY_SCROLL_START, down);
		}
		else if(key == sf::Keyboard::End)
		{
			nk_input_key(ctx, NK_KEY_TEXT_END, down);
			nk_input_key(ctx, NK_KEY_SCROLL_END, down);
		}
		else if(key == sf::Keyboard::PageDown)
			nk_input_key(ctx, NK_KEY_SCROLL_DOWN, down);
		else if(key == sf::Keyboard::PageUp)
			nk_input_key(ctx, NK_KEY_SCROLL_DOWN, down);
		else if(key == sf::Keyboard::Z)
			nk_input_key(ctx, NK_KEY_TEXT_UNDO, down && sf::Keyboard::isKeyPressed(sf::Keyboard::LControl));
		else if(key == sf::Keyboard::R)
			nk_input_key(ctx, NK_KEY_TEXT_REDO, down && sf::Keyboard::isKeyPressed(sf::Keyboard::LControl));
		else if(key == sf::Keyboard::C)
			nk_input_key(ctx, NK_KEY_COPY, down && sf::Keyboard::isKeyPressed(sf::Keyboard::LControl));
		else if(key == sf::Keyboard::V)
			nk_input_key(ctx, NK_KEY_PASTE, down && sf::Keyboard::isKeyPressed(sf::Keyboard::LControl));
		else if(key == sf::Keyboard::X)
			nk_input_key(ctx, NK_KEY_CUT, down && sf::Keyboard::isKeyPressed(sf::Keyboard::LControl));
		else if(key == sf::Keyboard::B)
			nk_input_key(ctx, NK_KEY_TEXT_LINE_START, down && sf::Keyboard::isKeyPressed(sf::Keyboard::LControl));
		else if(key == sf::Keyboard::E)
			nk_input_key(ctx, NK_KEY_TEXT_LINE_END, down && sf::Keyboard::isKeyPressed(sf::Keyboard::LControl));
		else if(key == sf::Keyboard::Up)
			nk_input_key(ctx, NK_KEY_UP, down);
		else if(key == sf::Keyboard::Down)
			nk_input_key(ctx, NK_KEY_DOWN, down);
		else if(key == sf::Keyboard::Left)
		{
			if(sf::Keyboard::isKeyPressed(sf::Keyboard::LControl))
				nk_input_key(ctx, NK_KEY_TEXT_WORD_LEFT, down);
			else
				nk_input_key(ctx, NK_KEY_LEFT, down);
		}
		else if(key == sf::Keyboard::Right)
		{
			if(sf::Keyboard::isKeyPressed(sf::Keyboard::LControl))
				nk_input_key(ctx, NK_KEY_TEXT_WORD_RIGHT, down);
			else
				nk_input_key(ctx, NK_KEY_RIGHT, down);
		}
		else return 0;

		return 1;
	}
	else if(event->type == sf::Event::MouseButtonPressed || event->type == sf::Event::MouseButtonReleased)
	{
		int down = event->type == sf::Event::MouseButtonPressed;
		const int x = event->mouseButton.x;
		const int y = event->mouseButton.y;

		if(event->mouseButton.button == sf::Mouse::Left)
			nk_input_button(ctx, NK_BUTTON_LEFT, x, y, down);
		if(event->mouseButton.button == sf::Mouse::Middle)
			nk_input_button(ctx, NK_BUTTON_MIDDLE, x, y, down);
		if(event->mouseButton.button == sf::Mouse::Right)
			nk_input_button(ctx, NK_BUTTON_RIGHT, x, y, down);
		else
			return 0;

		return 1;
	}
	else if(event->type == sf::Event::MouseMoved)
	{
		if(ctx->input.mouse.grabbed)
		{
			int x = (int)ctx->input.mouse.prev.x + event->mouseMove.x;
			int y = (int)ctx->input.mouse.prev.y + event->mouseMove.y;

			nk_input_motion(ctx, x, y);
		}
		else
			nk_input_motion(ctx, event->mouseMove.x, event->mouseMove.y);

		return 1;
	}
	/* For Android*/
	else if(event->type == sf::Event::TouchBegan || event->type == sf::Event::TouchEnded)
	{
		int down = event->type == sf::Event::TouchBegan;
		const int x = event->touch.x;
		const int y = event->touch.y;

		nk_input_button(ctx, NK_BUTTON_LEFT, x, y, down);

		return 1;
	}
	else if(event->type == sf::Event::TouchMoved)
	{
		if(ctx->input.mouse.grabbed)
		{
			int x = (int)ctx->input.mouse.prev.x;
			int y = (int)ctx->input.mouse.prev.y;

			nk_input_motion(ctx, x + event->touch.x, y + event->touch.y);
		}
		else
			nk_input_motion(ctx, event->touch.x, event->touch.y);

		return 1;
	}
	else if(event->type == sf::Event::TextEntered)
	{
		nk_glyph glyph;
		memcpy(glyph, (const void*)event->text.unicode, NK_UTF_SIZE);
		nk_input_glyph(ctx, glyph);

		return 1;
	}
	else if(event->type == sf::Event::MouseWheelScrolled)
	{
		nk_input_scroll(ctx, event->mouseWheelScroll.delta);

		return 1;
	}

	return 0;
}

NK_API
void nk_sfml_shutdown(void)
{
	struct nk_sfml_device* dev = &sfml.ogl;
	nk_font_atlas_clear(&sfml.atlas);
	nk_free(&sfml.ctx);
	glDeleteTextures(1, &dev->font_tex);
	nk_buffer_free(&dev->cmds);
	memset(&sfml, 0, sizeof(sfml));
}

#endif
