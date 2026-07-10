#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include "GLLoader.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef va_copy
#define va_copy(dst, src) ((dst) = (src))
#endif
namespace Enola2
{
	namespace xyprintf_detail
	{
		static void DebugMessage(const char* message)
		{
			if (message)
				OutputDebugStringA(message);
		}

		static std::wstring StringToWide(const char* text)
		{
			if (!text)
				return {};

			// 优先把输入当作 UTF-8。
			int length = MultiByteToWideChar(
				CP_UTF8,
				MB_ERR_INVALID_CHARS,
				text,
				-1,
				nullptr,
				0
			);

			UINT codePage = CP_UTF8;
			DWORD flags = MB_ERR_INVALID_CHARS;

			// 非法 UTF-8 时回退到系统 ANSI 代码页。
			if (length <= 0)
			{
				codePage = CP_ACP;
				flags = 0;

				length = MultiByteToWideChar(
					codePage,
					flags,
					text,
					-1,
					nullptr,
					0
				);
			}

			if (length <= 0)
				return {};

			std::wstring result(static_cast<size_t>(length), L'\0');

			MultiByteToWideChar(
				codePage,
				flags,
				text,
				-1,
				&result[0],
				length
			);

			if (!result.empty() && result.back() == L'\0')
				result.pop_back();

			return result;
		}

		static bool FormatString(
			const char* format,
			va_list arguments,
			std::string& result)
		{
			if (!format)
				return false;

			va_list sizeArguments;
			va_copy(sizeArguments, arguments);

#if defined(_MSC_VER)
			const int requiredLength = _vscprintf(format, sizeArguments);
#else
			const int requiredLength = vsnprintf(
				nullptr,
				0,
				format,
				sizeArguments
			);
#endif

			va_end(sizeArguments);

			if (requiredLength < 0)
				return false;

			if (requiredLength == 0)
			{
				result.clear();
				return true;
			}

			std::vector<char> buffer(
				static_cast<size_t>(requiredLength) + 1u
			);

			va_list writeArguments;
			va_copy(writeArguments, arguments);

			const int written = vsnprintf(
				buffer.data(),
				buffer.size(),
				format,
				writeArguments
			);

			va_end(writeArguments);

			if (written < 0)
				return false;

			result.assign(buffer.data(), static_cast<size_t>(written));
			return true;
		}

		struct TextBitmap
		{
			int width = 0;
			int height = 0;
			std::vector<std::uint8_t> alpha;
		};

		static bool RasterizeText(
			const std::wstring& text,
			const std::wstring& fontName,
			int fontWidth,
			int fontHeight,
			TextBitmap& result)
		{
			result = {};

			if (text.empty() || fontHeight == 0)
				return false;

			HDC dc = CreateCompatibleDC(nullptr);
			if (!dc)
				return false;

			const int requestedHeight = -(std::max)(1, std::abs(fontHeight));
			const int requestedWidth = fontWidth > 0 ? fontWidth : 0;

			HFONT font = CreateFontW(
				requestedHeight,
				requestedWidth,
				0,
				0,
				FW_NORMAL,
				FALSE,
				FALSE,
				FALSE,
				DEFAULT_CHARSET,
				OUT_DEFAULT_PRECIS,
				CLIP_DEFAULT_PRECIS,

				// 不使用 CLEARTYPE_QUALITY，避免彩色子像素边缘。
				ANTIALIASED_QUALITY,

				DEFAULT_PITCH | FF_DONTCARE,
				fontName.empty() ? L"Segoe UI" : fontName.c_str()
			);

			if (!font)
			{
				DeleteDC(dc);
				return false;
			}

			HGDIOBJ oldFont = SelectObject(dc, font);

			SetBkMode(dc, TRANSPARENT);
			SetTextColor(dc, RGB(255, 255, 255));
			SetTextCharacterExtra(dc, 0);

			const UINT drawFlags =
				DT_LEFT |
				DT_TOP |
				DT_NOPREFIX |
				DT_EXPANDTABS;

			RECT measureRect = { 0, 0, 0, 0 };

			const int measuredHeight = DrawTextW(
				dc,
				text.c_str(),
				static_cast<int>(text.size()),
				&measureRect,
				drawFlags | DT_CALCRECT
			);

			if (measuredHeight <= 0)
			{
				SelectObject(dc, oldFont);
				DeleteObject(font);
				DeleteDC(dc);
				return false;
			}

			int width = measureRect.right - measureRect.left;
			int height = measureRect.bottom - measureRect.top;
			if (width < 1)width = 1;
			if (height < 1)height = 1;

			BITMAPINFO bitmapInfo = {};
			bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bitmapInfo.bmiHeader.biWidth = width;

			// 负高度表示图像第一行就是顶部，不需要手动垂直翻转。
			bitmapInfo.bmiHeader.biHeight = -height;

			bitmapInfo.bmiHeader.biPlanes = 1;
			bitmapInfo.bmiHeader.biBitCount = 32;
			bitmapInfo.bmiHeader.biCompression = BI_RGB;

			void* pixels = nullptr;

			HBITMAP bitmap = CreateDIBSection(
				dc,
				&bitmapInfo,
				DIB_RGB_COLORS,
				&pixels,
				nullptr,
				0
			);

			if (!bitmap || !pixels)
			{
				if (bitmap)
					DeleteObject(bitmap);

				SelectObject(dc, oldFont);
				DeleteObject(font);
				DeleteDC(dc);
				return false;
			}

			HGDIOBJ oldBitmap = SelectObject(dc, bitmap);

			std::memset(
				pixels,
				0,
				static_cast<size_t>(width) *
				static_cast<size_t>(height) *
				4u
			);

			RECT drawRect = { 0, 0, width, height };

			DrawTextW(
				dc,
				text.c_str(),
				static_cast<int>(text.size()),
				&drawRect,
				drawFlags
			);

			result.width = width;
			result.height = height;
			result.alpha.resize(
				static_cast<size_t>(width) *
				static_cast<size_t>(height)
			);

			// 32 位 DIB 数据顺序为 B、G、R、未使用字节。
			// GDI 不会给出有效 alpha，因此从 RGB 灰度提取覆盖率。
			const auto* source =
				static_cast<const std::uint8_t*>(pixels);

			const size_t pixelCount =
				static_cast<size_t>(width) *
				static_cast<size_t>(height);

			for (size_t i = 0; i < pixelCount; ++i)
			{
				const std::uint8_t blue = source[i * 4u + 0u];
				const std::uint8_t green = source[i * 4u + 1u];
				const std::uint8_t red = source[i * 4u + 2u];

				result.alpha[i] = (std::max)(
					red,
					(std::max)(green, blue)
					);
			}

			SelectObject(dc, oldBitmap);
			SelectObject(dc, oldFont);

			DeleteObject(bitmap);
			DeleteObject(font);
			DeleteDC(dc);

			return true;
		}

		static GLuint CompileShader(
			GLenum type,
			const char* source)
		{
			GLuint shader = glCreateShader(type);
			if (!shader)
				return 0;

			glShaderSource(shader, 1, &source, nullptr);
			glCompileShader(shader);

			GLint compiled = GL_FALSE;
			glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

			if (compiled != GL_TRUE)
			{
				GLint logLength = 0;
				glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

				std::vector<char> log(
					static_cast<size_t>((std::max)(logLength, 1))
				);

				glGetShaderInfoLog(
					shader,
					static_cast<GLsizei>(log.size()),
					nullptr,
					log.data()
				);

				DebugMessage("xyprintf shader compilation failed:\n");
				DebugMessage(log.data());
				DebugMessage("\n");

				glDeleteShader(shader);
				return 0;
			}

			return shader;
		}

		struct Renderer
		{
			GLuint program = 0;
			GLuint vertexArray = 0;
			GLuint vertexBuffer = 0;
			GLuint texture = 0;

			GLint screenUniform = -1;
			GLint textureUniform = -1;
			GLint colorUniform = -1;

			bool Initialize()
			{
				static const char* vertexShaderSource = R"GLSL(
#version 330 core

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

uniform vec2 uScreenSize;

out vec2 fragmentTexCoord;

void main()
{
    vec2 normalizedPosition;

    normalizedPosition.x =
        inPosition.x / uScreenSize.x * 2.0 - 1.0;

    normalizedPosition.y =
        1.0 - inPosition.y / uScreenSize.y * 2.0;

    gl_Position = vec4(
        normalizedPosition,
        0.0,
        1.0
    );

    fragmentTexCoord = inTexCoord;
}
)GLSL";

				static const char* fragmentShaderSource = R"GLSL(
#version 330 core

in vec2 fragmentTexCoord;

uniform sampler2D uTextTexture;
uniform vec4 uTextColor;

out vec4 outColor;

void main()
{
    float coverage =
        texture(uTextTexture, fragmentTexCoord).r;

    outColor = vec4(
        uTextColor.rgb,
        uTextColor.a * coverage
    );
}
)GLSL";

				GLuint vertexShader = CompileShader(
					GL_VERTEX_SHADER,
					vertexShaderSource
				);

				if (!vertexShader)
					return false;

				GLuint fragmentShader = CompileShader(
					GL_FRAGMENT_SHADER,
					fragmentShaderSource
				);

				if (!fragmentShader)
				{
					glDeleteShader(vertexShader);
					return false;
				}

				program = glCreateProgram();

				glAttachShader(program, vertexShader);
				glAttachShader(program, fragmentShader);
				glLinkProgram(program);

				glDeleteShader(vertexShader);
				glDeleteShader(fragmentShader);

				GLint linked = GL_FALSE;
				glGetProgramiv(program, GL_LINK_STATUS, &linked);

				if (linked != GL_TRUE)
				{
					GLint logLength = 0;
					glGetProgramiv(
						program,
						GL_INFO_LOG_LENGTH,
						&logLength
					);

					std::vector<char> log(
						static_cast<size_t>((std::max)(logLength, 1))
					);

					glGetProgramInfoLog(
						program,
						static_cast<GLsizei>(log.size()),
						nullptr,
						log.data()
					);

					DebugMessage("xyprintf program linking failed:\n");
					DebugMessage(log.data());
					DebugMessage("\n");

					Destroy();
					return false;
				}

				screenUniform = glGetUniformLocation(
					program,
					"uScreenSize"
				);

				textureUniform = glGetUniformLocation(
					program,
					"uTextTexture"
				);

				colorUniform = glGetUniformLocation(
					program,
					"uTextColor"
				);

				glGenVertexArrays(1, &vertexArray);
				glGenBuffers(1, &vertexBuffer);
				glGenTextures(1, &texture);

				if (!vertexArray || !vertexBuffer || !texture)
				{
					Destroy();
					return false;
				}

				glBindVertexArray(vertexArray);
				glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);

				// 6 个顶点，每个顶点包含：
				// position.x, position.y, texcoord.x, texcoord.y
				glBufferData(
					GL_ARRAY_BUFFER,
					sizeof(float) * 6u * 4u,
					nullptr,
					GL_DYNAMIC_DRAW
				);

				glEnableVertexAttribArray(0);
				glVertexAttribPointer(
					0,
					2,
					GL_FLOAT,
					GL_FALSE,
					sizeof(float) * 4,
					reinterpret_cast<void*>(0)
				);

				glEnableVertexAttribArray(1);
				glVertexAttribPointer(
					1,
					2,
					GL_FLOAT,
					GL_FALSE,
					sizeof(float) * 4,
					reinterpret_cast<void*>(sizeof(float) * 2)
				);

				glBindTexture(GL_TEXTURE_2D, texture);

				glTexParameteri(
					GL_TEXTURE_2D,
					GL_TEXTURE_MIN_FILTER,
					GL_LINEAR
				);

				glTexParameteri(
					GL_TEXTURE_2D,
					GL_TEXTURE_MAG_FILTER,
					GL_LINEAR
				);

				glTexParameteri(
					GL_TEXTURE_2D,
					GL_TEXTURE_WRAP_S,
					GL_CLAMP_TO_EDGE
				);

				glTexParameteri(
					GL_TEXTURE_2D,
					GL_TEXTURE_WRAP_T,
					GL_CLAMP_TO_EDGE
				);

				return true;
			}

			void Destroy()
			{
				if (texture)
					glDeleteTextures(1, &texture);

				if (vertexBuffer)
					glDeleteBuffers(1, &vertexBuffer);

				if (vertexArray)
					glDeleteVertexArrays(1, &vertexArray);

				if (program)
					glDeleteProgram(program);

				program = 0;
				vertexArray = 0;
				vertexBuffer = 0;
				texture = 0;

				screenUniform = -1;
				textureUniform = -1;
				colorUniform = -1;
			}
		};

		static std::unordered_map<HGLRC, Renderer>& GetRenderers()
		{
			// OpenGL 对象属于创建它们的上下文。
			// 每个 HGLRC 分别保存一套渲染资源。
			static std::unordered_map<HGLRC, Renderer> renderers;
			return renderers;
		}

		static Renderer* GetCurrentRenderer()
		{
			HGLRC context = wglGetCurrentContext();
			if (!context)
			{
				DebugMessage(
					"xyprintf: no current OpenGL context.\n"
				);

				return nullptr;
			}

			auto& renderers = GetRenderers();
			auto iterator = renderers.find(context);

			if (iterator != renderers.end())
				return &iterator->second;

			Renderer renderer;

			if (!renderer.Initialize())
				return nullptr;

			auto inserted = renderers.emplace(
				context,
				std::move(renderer)
			);

			return &inserted.first->second;
		}

		static void RestoreCapability(
			GLenum capability,
			GLboolean enabled)
		{
			if (enabled)
				glEnable(capability);
			else
				glDisable(capability);
		}

		struct SavedGLState
		{
			GLint drawFramebuffer = 0;
			GLint program = 0;
			GLint vertexArray = 0;
			GLint arrayBuffer = 0;

			GLint activeTexture = GL_TEXTURE0;
			GLint texture2D = 0;
			GLint sampler = 0;

			GLint pixelUnpackBuffer = 0;
			GLint unpackAlignment = 4;
			GLint unpackRowLength = 0;
			GLint unpackSkipPixels = 0;
			GLint unpackSkipRows = 0;

			GLint blendSourceRGB = GL_ONE;
			GLint blendDestinationRGB = GL_ZERO;
			GLint blendSourceAlpha = GL_ONE;
			GLint blendDestinationAlpha = GL_ZERO;

			GLint blendEquationRGB = GL_FUNC_ADD;
			GLint blendEquationAlpha = GL_FUNC_ADD;

			GLint polygonMode[2] = {
				GL_FILL,
				GL_FILL
			};

			GLboolean blendEnabled = GL_FALSE;
			GLboolean depthEnabled = GL_FALSE;
			GLboolean cullEnabled = GL_FALSE;
			GLboolean stencilEnabled = GL_FALSE;

			void Save()
			{
				glGetIntegerv(
					GL_DRAW_FRAMEBUFFER_BINDING,
					&drawFramebuffer
				);

				glGetIntegerv(
					GL_CURRENT_PROGRAM,
					&program
				);

				glGetIntegerv(
					GL_VERTEX_ARRAY_BINDING,
					&vertexArray
				);

				glGetIntegerv(
					GL_ARRAY_BUFFER_BINDING,
					&arrayBuffer
				);

				glGetIntegerv(
					GL_ACTIVE_TEXTURE,
					&activeTexture
				);

				// 单独保存纹理单元 0 的状态。
				glActiveTexture(GL_TEXTURE0);

				glGetIntegerv(
					GL_TEXTURE_BINDING_2D,
					&texture2D
				);

				glGetIntegerv(
					GL_SAMPLER_BINDING,
					&sampler
				);

				glGetIntegerv(
					GL_PIXEL_UNPACK_BUFFER_BINDING,
					&pixelUnpackBuffer
				);

				glGetIntegerv(
					GL_UNPACK_ALIGNMENT,
					&unpackAlignment
				);

				glGetIntegerv(
					GL_UNPACK_ROW_LENGTH,
					&unpackRowLength
				);

				glGetIntegerv(
					GL_UNPACK_SKIP_PIXELS,
					&unpackSkipPixels
				);

				glGetIntegerv(
					GL_UNPACK_SKIP_ROWS,
					&unpackSkipRows
				);

				blendEnabled = glIsEnabled(GL_BLEND);
				depthEnabled = glIsEnabled(GL_DEPTH_TEST);
				cullEnabled = glIsEnabled(GL_CULL_FACE);
				stencilEnabled = glIsEnabled(GL_STENCIL_TEST);

				glGetIntegerv(
					GL_BLEND_SRC_RGB,
					&blendSourceRGB
				);

				glGetIntegerv(
					GL_BLEND_DST_RGB,
					&blendDestinationRGB
				);

				glGetIntegerv(
					GL_BLEND_SRC_ALPHA,
					&blendSourceAlpha
				);

				glGetIntegerv(
					GL_BLEND_DST_ALPHA,
					&blendDestinationAlpha
				);

				glGetIntegerv(
					GL_BLEND_EQUATION_RGB,
					&blendEquationRGB
				);

				glGetIntegerv(
					GL_BLEND_EQUATION_ALPHA,
					&blendEquationAlpha
				);

				glGetIntegerv(
					GL_POLYGON_MODE,
					polygonMode
				);
			}

			void Restore()
			{
				glBindFramebuffer(
					GL_DRAW_FRAMEBUFFER,
					static_cast<GLuint>(drawFramebuffer)
				);

				glUseProgram(static_cast<GLuint>(program));

				glBindVertexArray(
					static_cast<GLuint>(vertexArray)
				);

				glBindBuffer(
					GL_ARRAY_BUFFER,
					static_cast<GLuint>(arrayBuffer)
				);

				glBindBuffer(
					GL_PIXEL_UNPACK_BUFFER,
					static_cast<GLuint>(pixelUnpackBuffer)
				);

				glPixelStorei(
					GL_UNPACK_ALIGNMENT,
					unpackAlignment
				);

				glPixelStorei(
					GL_UNPACK_ROW_LENGTH,
					unpackRowLength
				);

				glPixelStorei(
					GL_UNPACK_SKIP_PIXELS,
					unpackSkipPixels
				);

				glPixelStorei(
					GL_UNPACK_SKIP_ROWS,
					unpackSkipRows
				);

				glActiveTexture(GL_TEXTURE0);

				glBindTexture(
					GL_TEXTURE_2D,
					static_cast<GLuint>(texture2D)
				);

				glBindSampler(
					0,
					static_cast<GLuint>(sampler)
				);

				glActiveTexture(
					static_cast<GLenum>(activeTexture)
				);

				glBlendFuncSeparate(
					static_cast<GLenum>(blendSourceRGB),
					static_cast<GLenum>(blendDestinationRGB),
					static_cast<GLenum>(blendSourceAlpha),
					static_cast<GLenum>(blendDestinationAlpha)
				);

				glBlendEquationSeparate(
					static_cast<GLenum>(blendEquationRGB),
					static_cast<GLenum>(blendEquationAlpha)
				);

				RestoreCapability(GL_BLEND, blendEnabled);
				RestoreCapability(GL_DEPTH_TEST, depthEnabled);
				RestoreCapability(GL_CULL_FACE, cullEnabled);
				RestoreCapability(GL_STENCIL_TEST, stencilEnabled);

				glPolygonMode(
					GL_FRONT,
					static_cast<GLenum>(polygonMode[0])
				);

				glPolygonMode(
					GL_BACK,
					static_cast<GLenum>(polygonMode[1])
				);
			}
		};

	} // namespace xyprintf_detail


	void xyprintf(
		GLuint fbo,
		const char* fontName,
		int fontWidth,
		int fontHeight,
		int x,
		int y,
		const char* fmt,
		...)
	{
		using namespace xyprintf_detail;

		if (!fmt || fontHeight == 0)
			return;

		if (!wglGetCurrentContext())
		{
			DebugMessage(
				"xyprintf: no current OpenGL context.\n"
			);

			return;
		}

		std::string formattedText;

		va_list arguments;
		va_start(arguments, fmt);

		const bool formatSucceeded = FormatString(
			fmt,
			arguments,
			formattedText
		);

		va_end(arguments);

		if (!formatSucceeded || formattedText.empty())
			return;

		const std::wstring wideText =
			StringToWide(formattedText.c_str());

		const std::wstring wideFontName =
			StringToWide(fontName ? fontName : "Segoe UI");

		if (wideText.empty())
			return;

		TextBitmap bitmap;

		if (!RasterizeText(
			wideText,
			wideFontName,
			fontWidth,
			fontHeight,
			bitmap))
		{
			return;
		}

		GLint viewport[4] = { 0, 0, 0, 0 };
		glGetIntegerv(GL_VIEWPORT, viewport);

		const int viewportWidth = viewport[2];
		const int viewportHeight = viewport[3];

		if (viewportWidth <= 0 || viewportHeight <= 0)
			return;

		SavedGLState oldState;
		oldState.Save();

		Renderer* renderer = GetCurrentRenderer();

		if (!renderer)
		{
			oldState.Restore();
			return;
		}

		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);

		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glDisable(GL_STENCIL_TEST);

		glEnable(GL_BLEND);

		glBlendEquationSeparate(
			GL_FUNC_ADD,
			GL_FUNC_ADD
		);

		glBlendFuncSeparate(
			GL_SRC_ALPHA,
			GL_ONE_MINUS_SRC_ALPHA,
			GL_ONE,
			GL_ONE_MINUS_SRC_ALPHA
		);

		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		glUseProgram(renderer->program);
		glBindVertexArray(renderer->vertexArray);
		glBindBuffer(GL_ARRAY_BUFFER, renderer->vertexBuffer);

		glActiveTexture(GL_TEXTURE0);
		glBindSampler(0, 0);
		glBindTexture(GL_TEXTURE_2D, renderer->texture);

		// 防止调用者绑定的 PBO 影响 glTexImage2D 的指针含义。
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
		glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GL_R8,
			bitmap.width,
			bitmap.height,
			0,
			GL_RED,
			GL_UNSIGNED_BYTE,
			bitmap.alpha.data()
		);

		const float left = static_cast<float>(x);
		const float top = static_cast<float>(y);

		const float right =
			static_cast<float>(x + bitmap.width);

		const float bottom =
			static_cast<float>(y + bitmap.height);

		// 左上、右上、右下
		// 左上、右下、左下
		const float vertices[] = {
			left,  top,    0.0f, 0.0f,
			right, top,    1.0f, 0.0f,
			right, bottom, 1.0f, 1.0f,

			left,  top,    0.0f, 0.0f,
			right, bottom, 1.0f, 1.0f,
			left,  bottom, 0.0f, 1.0f
		};

		glBufferSubData(
			GL_ARRAY_BUFFER,
			0,
			sizeof(vertices),
			vertices
		);

		glUniform2f(
			renderer->screenUniform,
			static_cast<float>(viewportWidth),
			static_cast<float>(viewportHeight)
		);

		glUniform1i(
			renderer->textureUniform,
			0
		);

		// 当前固定为白色。
		glUniform4f(
			renderer->colorUniform,
			1.0f,
			1.0f,
			1.0f,
			1.0f
		);

		glDrawArrays(GL_TRIANGLES, 0, 6);

		oldState.Restore();
	}


	// 在销毁当前 OpenGL 上下文之前调用。
	// 每个 OpenGL 上下文分别调用一次。
	void xyprintfShutdown()
	{
		using namespace xyprintf_detail;

		HGLRC context = wglGetCurrentContext();
		if (!context)
			return;

		auto& renderers = GetRenderers();
		auto iterator = renderers.find(context);

		if (iterator == renderers.end())
			return;

		iterator->second.Destroy();
		renderers.erase(iterator);
	}
}