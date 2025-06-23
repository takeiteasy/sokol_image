/* sokol_image.h -- https://github.com/takeiteasy/sokol_image

 sokol_image Copyright (C) 2025 George Watson

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <https://www.gnu.org/licenses/>. */

#ifndef SOKOL_IMAGE_HEADER
#define SOKOL_IMAGE_HEADER
#if defined(__cplusplus)
extern "C" {
#endif

#ifndef __has_include
#define __has_include(x) 1
#endif

#ifndef SOKOL_GFX_INCLUDED
#if __has_include("sokol_gfx.h")
#include "sokol_gfx.h"
#else
#error Please include sokol_gfx.h before sokol_image.h
#endif
#endif

#ifndef SOKOL_COLOR_INCLUDED
#if __has_include("sokol_color.h")
#include "sokol_color.h"
#else
#ifdef __cplusplus
#define SOKOL_COLOR_CONSTEXPR constexpr
#else
#define SOKOL_COLOR_CONSTEXPR const
#endif
static SOKOL_COLOR_CONSTEXPR sg_color sg_black = { 0.0f, 0.0f, 0.0f, 1.0f };
#endif
#endif

#include <stdint.h>

typedef struct image_buffer {
    unsigned int width, height;
    int32_t *buffer;
} simage_buffer;

bool simage_empty(unsigned int w, unsigned int h, sg_color color, simage_buffer *dst);
bool simage_load_from_path(const char *path, simage_buffer *dst);
bool simage_load_from_memory(const void *data, size_t length, simage_buffer *dst);
void simage_destroy_buffer(simage_buffer *img);
void simage_pset(simage_buffer *img, int x, int y, sg_color color);
sg_color simage_pget(simage_buffer *img, int x, int y);

void simage_fill(simage_buffer *img, sg_color color);
void simage_flood(simage_buffer *img, int x, int y, sg_color color);
void simage_paste(simage_buffer *dst, simage_buffer *src, int x, int y);
void simage_clipped_paste(simage_buffer *dst, simage_buffer *src, int x, int y, int rx, int ry, int rw, int rh);
void simage_resize(simage_buffer *src, int nw, int nh);
void simage_rotate(simage_buffer *src, float angle);
void simage_clip(simage_buffer *src, int rx, int ry, int rw, int rh);

bool simage_dupe(simage_buffer *src, simage_buffer *dst);
bool simage_resized(simage_buffer *src, int nw, int nh, simage_buffer *dst);
bool simage_rotated(simage_buffer *src, float angle, simage_buffer *dst);
bool simage_clipped(simage_buffer *src, int rx, int ry, int rw, int rh, simage_buffer *dst);

void simage_draw_line(simage_buffer *img, int x0, int y0, int x1, int y1, sg_color color);
void simage_draw_circle(simage_buffer *img, int xc, int yc, int r, sg_color color, int fill);
void simage_draw_rectangle(simage_buffer *img, int x, int y, int w, int h, sg_color color, int fill);
void simage_draw_triangle(simage_buffer *img, int x0, int y0, int x1, int y1, int x2, int y2, sg_color color, int fill);

/* These functions were adapted from https://github.com/mattiasgustavsson/libs/blob/main/img.h
   Copyright Mattias Gustavsson (C) 2019 [MIT/Public Domain] */
void simage_brightness(simage_buffer *img, float value);
void simage_contrast(simage_buffer *img, float value);
void simage_saturation(simage_buffer *img, float value);

sg_image sg_empty_texture(unsigned int width, unsigned int height);
sg_image sg_load_texture_path(const char *path, unsigned int *width, unsigned int *height);
sg_image sg_load_texture_from_memory(unsigned char *data, size_t data_size, unsigned int *width, unsigned int *height);
sg_image sg_load_texture_from_buffer(simage_buffer *img);
void sg_update_texture_from_buffer(sg_image texture, simage_buffer *img);

#if defined(__cplusplus)
}
#endif
#endif // SOKOL_IMAGE_HEADER

#ifdef SOKOL_IMAGE_IMPL
#ifdef _WIN32
#include <io.h>
#include <dirent.h>
#define F_OK 0
#define access _access
#else
#include <unistd.h>
#endif

// INCLUDES
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define QOI_IMPLEMENTATION
#include "qoi.h"

#define _RGBA(R, G, B, A) (((unsigned int)(R) << 24) | ((unsigned int)(B) << 16) | ((unsigned int)(G) << 8) | (A))
#define _F2I(F) (int)((F) * 255.f)
#define _I2F(I) (float)((float)(I) / 255.f)

static uint32_t sg_color_to_int(sg_color color) {
    return _RGBA(_F2I(color.r), _F2I(color.g), _F2I(color.b), _F2I(color.a));
}

static sg_color int_to_sg_color(int32_t color) {
    return (sg_color) {
        .r = _I2F((color >> 24) & 0xFF),
        .g = _I2F((color >> 16) & 0xFF),
        .b = _I2F((color >> 8) & 0xFF),
        .a = _I2F(color & 0xFF)
    };
}

bool simage_empty(unsigned int w, unsigned int h, sg_color color, simage_buffer *dst) {
    if (w <= 0 || h <= 0)
        return false;
    dst->width = w;
    dst->height = h;
    if (!(dst->buffer = (int32_t*)malloc(w * h * sizeof(int32_t))))
        return false;
    simage_fill(dst, color);
    return true;
}

bool simage_load_from_path(const char *path, simage_buffer *dst) {
    bool result = false;
    unsigned char *data = NULL;
    if (access(path, F_OK))
        return false;
    size_t sz = -1;
    FILE *fh = fopen(path, "rb");
    if (!fh)
        return false;
    fseek(fh, 0, SEEK_END);
    if (!(sz = ftell(fh)))
        goto BAIL;
    fseek(fh, 0, SEEK_SET);
    if (!(data = malloc(sz * sizeof(unsigned char))))
        goto BAIL;
    if (fread(data, sz, 1, fh) != 1)
        goto BAIL;
    result = simage_load_from_memory(data, (int)sz, dst);
BAIL:
    if (fh)
        fclose(fh);
    if (data)
        free(data);
    return result;
}

static int check_if_qoi(unsigned char *data) {
    return _RGBA(data[0], data[0], data[0], data[0]) ==  _RGBA('q', 'o', 'i', 'f');
}

bool simage_load_from_memory(const void *data, size_t data_size, simage_buffer *dst) {
    if (!data || data_size <= 0)
        return false;
    int _w, _h, c = 0;
    unsigned char *img_data = NULL;
    if (check_if_qoi((unsigned char*)data)) {
        qoi_desc desc;
        if (!(img_data = qoi_decode(data, (int)data_size, &desc, 4)))
            return false;
        _w = desc.width;
        _h = desc.height;
    } else
        if (!(img_data = stbi_load_from_memory(data, (int)data_size, &_w, &_h, &c, 4)))
            return false;
    if (_w <= 0 || _h <= 0 || c < 3) {
        free(img_data);
        return false;
    }

    dst->width = _w;
    dst->height = _h;
    if (!(dst->buffer = malloc(_w * _h * sizeof(int)))) {
        free(img_data);
        return false;
    }
    for (int x = 0; x < _w; x++)
        for (int y = 0; y < _h; y++) {
            unsigned char *p = img_data + (x + _w * y) * 4;
            dst->buffer[y * _w + x] = _RGBA(p[0], p[1], p[2], p[3]);
        }
    free(img_data);
    return true;
}

void simage_destroy_buffer(simage_buffer *img) {
    if (img && img->buffer) {
        free(img->buffer);
        memset(img, 0, sizeof(simage_buffer));
    }
}

void simage_pset(simage_buffer *img, int x, int y, sg_color color) {
    if (img->buffer && x >= 0 && y >= 0 && x <= img->width && y <= img->height)
        img->buffer[y * img->width + x] = sg_color_to_int(color);
}

sg_color simage_pget(simage_buffer *img, int x, int y) {
    int32_t color = 0;
    if (img->buffer && x >= 0 && y >= 0 && x <= img->width && y <= img->height)
        color = img->buffer[y * img->width + x];
    return int_to_sg_color(color);
}

void simage_fill(simage_buffer *img, sg_color color) {
    for (int i = 0; i < img->width * img->height; ++i)
        img->buffer[i] = sg_color_to_int(color);
}

void _pset(simage_buffer *img, int x, int y, int32_t color) {
    if (img->buffer && x >= 0 && y >= 0 && x <= img->width && y <= img->height)
        img->buffer[y * img->width + x] = color;
}

int32_t _pget(simage_buffer *img, int x, int y) {
    int32_t color = 0;
    if (img->buffer && x >= 0 && y >= 0 && x <= img->width && y <= img->height)
        color = img->buffer[y * img->width + x];
    return color;
}

static inline void flood_fn(simage_buffer *img, int x, int y, int _new, int _old) {
    if (_new == _old || _pget(img, x, y) != _old)
        return;

    int x1 = x;
    while (x1 < img->width && _pget(img, x1, y) == _old) {
        _pset(img, x1, y, _new);
        x1++;
    }

    x1 = x - 1;
    while (x1 >= 0 && _pget(img, x1, y) == _old) {
        _pset(img, x1, y, _new);
        x1--;
    }

    x1 = x;
    while (x1 < img->width && _pget(img, x1, y) == _old) {
        if(y > 0 && _pget(img, x1, y - 1) == _old)
            flood_fn(img, x1, y - 1, _new, _old);
        x1++;
    }

    x1 = x - 1;
    while(x1 >= 0 && _pget(img, x1, y) == _old) {
        if(y > 0 && _pget(img, x1, y - 1) == _old)
            flood_fn(img, x1, y - 1, _new, _old);
        x1--;
    }

    x1 = x;
    while(x1 < img->width && _pget(img, x1, y) == _old) {
        if(y < img->height - 1 && _pget(img, x1, y + 1) == _old)
            flood_fn(img, x1, y + 1, _new, _old);
        x1++;
    }

    x1 = x - 1;
    while(x1 >= 0 && _pget(img, x1, y) == _old) {
        if(y < img->height - 1 && _pget(img, x1, y + 1) == _old)
            flood_fn(img, x1, y + 1, _new, _old);
        x1--;
    }
}

void simage_flood(simage_buffer *img, int x, int y, sg_color color) {
    if (x < 0 || y < 0 || x >= img->width || y >= img->height)
        return;
    flood_fn(img, x, y, sg_color_to_int(color), _pget(img, x, y));
}

void simage_paste(simage_buffer *dst, simage_buffer *src, int x, int y) {
    for (int ox = 0; ox < src->width; ++ox) {
        for (int oy = 0; oy < src->height; ++oy) {
            if (oy > dst->height)
                break;
            simage_pset(dst, x + ox, y + oy, simage_pget(src, ox, oy));
        }
        if (ox > dst->width)
            break;
    }
}

void simage_clipped_paste(simage_buffer *dst, simage_buffer *src, int x, int y, int rx, int ry, int rw, int rh) {
    for (int ox = 0; ox < rw; ++ox)
        for (int oy = 0; oy < rh; ++oy)
            simage_pset(dst, ox + x, oy + y, simage_pget(src, ox + rx, oy + ry));
}

bool simage_dupe(simage_buffer *src, simage_buffer *dst) {
    if (!simage_empty(src->width, src->height, sg_black, dst))
        return false;
    memcpy(dst->buffer, src->buffer, src->width * src->height * sizeof(uint32_t));
    return true;
}

bool simage_resized(simage_buffer *src, int nw, int nh, simage_buffer *dst) {
    if (!simage_empty(nw, nh, sg_black, dst))
        return false;
    int x_ratio = (int)((src->width << 16) / dst->width) + 1;
    int y_ratio = (int)((src->height << 16) / dst->height) + 1;
    int x2, y2, i, j;
    for (i = 0; i < dst->height; ++i) {
        int *t = dst->buffer + i * dst->width;
        y2 = ((i * y_ratio) >> 16);
        int *p = src->buffer + y2 * src->width;
        int rat = 0;
        for (j = 0; j < dst->width; ++j) {
            x2 = (rat >> 16);
            *t++ = p[x2];
            rat += x_ratio;
        }
    }
    return true;
}

void simage_resize(simage_buffer *src, int nw, int nh) {
    simage_buffer result;
    if (!simage_resized(src, nw, nh, &result))
        return;
    free(src->buffer);
    memcpy(src, &result, sizeof(simage_buffer));
}

bool simage_rotated(simage_buffer *src, float angle, simage_buffer *dst) {
    float theta = _RADIANS(angle);
    float c = cosf(theta), s = sinf(theta);
    float r[3][2] = {
        { -src->height * s, src->height * c },
        {  src->width * c - src->height * s, src->height * c + src->width * s },
        {  src->width * c, src->width * s }
    };

    float mm[2][2] = {{
        _MIN(0, _MIN(r[0][0], _MIN(r[1][0], r[2][0]))),
        _MIN(0, _MIN(r[0][1], _MIN(r[1][1], r[2][1])))
    }, {
        (theta > 1.5708  && theta < 3.14159 ? 0.f : _MAX(r[0][0], _MAX(r[1][0], r[2][0]))),
        (theta > 3.14159 && theta < 4.71239 ? 0.f : _MAX(r[0][1], _MAX(r[1][1], r[2][1])))
    }};

    int dw = (int)ceil(fabsf(mm[1][0]) - mm[0][0]);
    int dh = (int)ceil(fabsf(mm[1][1]) - mm[0][1]);
    if (!simage_empty(dw, dh, sg_black, dst))
        return false;
    int x, y, sx, sy;
    for (x = 0; x < dw; ++x)
        for (y = 0; y < dh; ++y) {
            sx = ((x + mm[0][0]) * c + (y + mm[0][1]) * s);
            sy = ((y + mm[0][1]) * c - (x + mm[0][0]) * s);
            if (sx < 0 || sx >= src->width || sy < 0 || sy >= src->height)
                continue;
            simage_pset(dst, x, y, simage_pget(src, sx, sy));
        }
    return true;
}

void simage_rotate(simage_buffer *src, float angle) {
    simage_buffer result;
    if (!simage_rotated(src, angle, &result))
        return;
    free(src->buffer);
    memcpy(src, &result, sizeof(simage_buffer));
}

bool simage_clipped(simage_buffer *src, int rx, int ry, int rw, int rh, simage_buffer *dst) {
    int ox = _CLAMP(rx, 0, src->width);
    int oy = _CLAMP(ry, 0, src->height);
    if (ox >= src->width || oy >= src->height)
        return false;
    int mx = _MIN(ox + rw, src->width);
    int my = _MIN(oy + rh, src->height);
    int iw = mx - ox;
    int ih = my - oy;
    if (iw <= 0 || ih <= 0)
        return false;
    if (!simage_empty(iw, ih, sg_black, dst))
        return false;
    for (int px = 0; px < iw; px++)
        for (int py = 0; py < ih; py++) {
            int cx = ox + px;
            int cy = oy + py;
            simage_pset(dst, px, py, simage_pget(src, cx, cy));
        }
    return true;
}

void simage_clip(simage_buffer *src, int rx, int ry, int rw, int rh) {
    simage_buffer result;
    if (!simage_clipped(src, rx, ry, rw, rh, &result))
        return;
    free(src->buffer);
    memcpy(src, &result, sizeof(simage_buffer));
}

static inline void vline(simage_buffer *img, int x, int y0, int y1, sg_color color) {
    if (y1 < y0) {
        y0 += y1;
        y1  = y0 - y1;
        y0 -= y1;
    }

    if (x < 0 || x >= img->width || y0 >= img->height)
        return;

    if (y0 < 0)
        y0 = 0;
    if (y1 >= img->height)
        y1 = img->height - 1;

    for(int y = y0; y <= y1; y++)
        simage_pset(img, x, y, color);
}

static inline void hline(simage_buffer *img, int y, int x0, int x1, sg_color color) {
    if (x1 < x0) {
        x0 += x1;
        x1  = x0 - x1;
        x0 -= x1;
    }

    if (y < 0 || y >= img->height || x0 >= img->width)
        return;

    if (x0 < 0)
        x0 = 0;
    if (x1 >= img->width)
        x1 = img->width - 1;

    for(int x = x0; x <= x1; x++)
        simage_pset(img, x, y, color);
}

void simage_draw_line(simage_buffer *img, int x0, int y0, int x1, int y1, sg_color color) {
    if (x0 == x1)
        vline(img, x0, y0, y1, color);
    else if (y0 == y1)
        hline(img, y0, x0, x1, color);
    else {
        int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = (dx > dy ? dx : -dy) / 2;

        while (simage_pset(img, x0, y0, color), x0 != x1 || y0 != y1) {
            int e2 = err;
            if (e2 > -dx) { err -= dy; x0 += sx; }
            if (e2 <  dy) { err += dx; y0 += sy; }
        }
    }
}

void simage_draw_circle(simage_buffer *img, int xc, int yc, int r, sg_color color, int fill) {
    int x = -r, y = 0, err = 2 - 2 * r; /* II. Quadrant */
    do {
        simage_pset(img, xc - x, yc + y, color);    /*   I. Quadrant */
        simage_pset(img, xc - y, yc - x, color);    /*  II. Quadrant */
        simage_pset(img, xc + x, yc - y, color);    /* III. Quadrant */
        simage_pset(img, xc + y, yc + x, color);    /*  IV. Quadrant */

        if (fill) {
            hline(img, yc - y, xc - x, xc + x, color);
            hline(img, yc + y, xc - x, xc + x, color);
        }

        r = err;
        if (r <= y)
            err += ++y * 2 + 1; /* e_xy+e_y < 0 */
        if (r > x || err > y)
            err += ++x * 2 + 1; /* e_xy+e_x > 0 or no 2nd y-step */
    } while (x < 0);
}

void simage_draw_rectangle(simage_buffer *img, int x, int y, int w, int h, sg_color color, int fill) {
    if (x < 0) {
        w += x;
        x  = 0;
    }
    if (y < 0) {
        h += y;
        y  = 0;
    }

    w += x;
    h += y;
    if (w < 0 || h < 0 || x > img->width || y > img->height)
        return;

    if (w > img->width)
        w = img->width;
    if (h > img->height)
        h = img->height;

    if (fill) {
        for (; y < h; ++y)
            hline(img, y, x, w, color);
    } else {
        hline(img, y, x, w, color);
        hline(img, h, x, w, color);
        vline(img, x, y, h, color);
        vline(img, w, y, h, color);
    }
}

void simage_draw_triangle(simage_buffer *img, int x0, int y0, int x1, int y1, int x2, int y2, sg_color color, int fill) {
    if (y0 ==  y1 && y0 ==  y2)
        return;
    if (fill) {
        if (y0 > y1) {
            _SWAP(x0, x1);
            _SWAP(y0, y1);
        }
        if (y0 > y2) {
            _SWAP(x0, x2);
            _SWAP(y0, y2);
        }
        if (y1 > y2) {
            _SWAP(x1, x2);
            _SWAP(y1, y2);
        }

        int total_height = y2 - y0, i, j;
        for (i = 0; i < total_height; ++i) {
            int second_half = i > y1 - y0 || y1 == y0;
            int segment_height = second_half ? y2 - y1 : y1 - y0;
            float alpha = (float)i / total_height;
            float beta  = (float)(i - (second_half ? y1 - y0 : 0)) / segment_height;
            int ax = x0 + (x2 - x0) * alpha;
            int ay = y0 + (y2 - y0) * alpha;
            int bx = second_half ? x1 + (x2 - x1) : x0 + (x1 - x0) * beta;
            int by = second_half ? y1 + (y2 - y1) : y0 + (y1 - y0) * beta;
            if (ax > bx) {
                _SWAP(ax, bx);
                _SWAP(ay, by);
            }
            for (j = ax; j <= bx; ++j)
                simage_pset(img, j, y0 + i, color);
        }
    } else {
        simage_draw_line(img, x0, y0, x1, y1, color);
        simage_draw_line(img, x1, y1, x2, y2, color);
        simage_draw_line(img, x2, y2, x0, y0, color);
    }
}

void simage_brightness(simage_buffer *img, float value) {
    for (int x = 0; x < img->width; x++)
        for (int y =  0; y < img->height; y++) {
            sg_color c = simage_pget(img, x, y);
            simage_pset(img, x, y, (sg_color) {
                c.r + value,
                c.g + value,
                c.b + value,
                c.a + value
            });
        }
}

void simage_contrast(simage_buffer *img, float value) {
    for (int x = 0; x < img->width; x++)
        for (int y =  0; y < img->height; y++) {
            sg_color c = simage_pget(img, x, y);
            simage_pset(img, x, y, (sg_color) {
                (c.r - .5f) * value + .5f,
                (c.g - .5f) * value + .5f,
                (c.r - .5f) * value + .5f,
                (c.a - .5f) * value + .5f
            });
        }
}

typedef struct hsva {
    float h;
    float s;
    float v;
    float a;
} _hsva_t;

static _hsva_t _rgba_to_hsva(sg_color rgb) {
    float cmin = _MIN(rgb.r, _MIN(rgb.g, rgb.b)); //Min. value of RGB
    float cmax = _MAX(rgb.r, _MAX(rgb.g, rgb.b)); //Max. value of RGB
    float cdel = cmax - cmin;   //Delta RGB value
    _hsva_t hsv = {
        .v = cmax,
        .a = rgb.a
    };
    if (cdel == 0) { // This is a gray, no chroma...
        hsv.h = 0;  // HSV results from 0 to 1
        hsv.s = 0;
    } else { // Chromatic data...
        hsv.s = cdel / cmax;
        float rdel = (((cmax - rgb.r) / 6.0f) + (cdel / 2.0f)) / cdel;
        float gdel = (((cmax - rgb.g) / 6.0f) + (cdel / 2.0f)) / cdel;
        float bdel = (((cmax - rgb.b) / 6.0f) + (cdel / 2.0f)) / cdel;
        if (rgb.r == cmax)
            hsv.h = bdel - gdel;
        else if (rgb.g == cmax)
            hsv.h = ( 1.0f / 3.0f ) + rdel - bdel;
        else
            hsv.h = ( 2.0f / 3.0f ) + gdel - rdel;
        if (hsv.h < 0.0f)
            hsv.h += 1.0f;
        if (hsv.h > 1.0f)
            hsv.h -= 1.0f;
    }
    return hsv;
}

static sg_color _hsva_to_rgba(_hsva_t hsv) {
    sg_color rgb = {.a = hsv.a};
    if (hsv.s == 0.0f) { // HSV from 0 to 1
        rgb.r = hsv.v;
        rgb.g = hsv.v;
        rgb.b = hsv.v;
    } else {
        float h = hsv.h * 6.0f;
        if (h == 6.0f)
            h = 0.0f; // H must be < 1
        float i = floorf(h);
        float v1 = hsv.v * (1.0f - hsv.s);
        float v2 = hsv.v * (1.0f - hsv.s * (h - i));
        float v3 = hsv.v * (1.0f - hsv.s * (1.0f - (h - i)));

        switch ((int)i) {
            case 0: { rgb.r = hsv.v; rgb.g = v3   ; rgb.b = v1   ; } break;
            case 1: { rgb.r = v2   ; rgb.g = hsv.v; rgb.b = v1   ; } break;
            case 2: { rgb.r = v1   ; rgb.g = hsv.v; rgb.b = v3   ; } break;
            case 3: { rgb.r = v1   ; rgb.g = v2   ; rgb.b = hsv.v; } break;
            case 4: { rgb.r = v3   ; rgb.g = v1   ; rgb.b = hsv.v; } break;
            default:{ rgb.r = hsv.v; rgb.g = v1   ; rgb.b = v2   ; } break;
        }
    }
    return rgb;
}

void simage_saturation(simage_buffer *img, float value) {
    for (int x = 0; x < img->width; x++)
        for (int y =  0; y < img->height; y++) {
            sg_color c = simage_pget(img, x, y);
            _hsva_t hsv = _rgba_to_hsva(c);
            hsv.s = _CLAMP(hsv.s + value, 0.0f, 1.0f);
            simage_pset(img, x, y, _hsva_to_rgba(hsv));
        }
}

sg_image sg_empty_texture(unsigned int width, unsigned int height) {
    if (width <= 0 || height <= 0)
        return (sg_image){.id=SG_INVALID_ID};
    sg_image_desc desc = {
        .width = width,
        .height = height,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .usage.stream_update = true
    };
    return sg_make_image(&desc);
}

static sg_image image_to_sg(simage_buffer *img) {
    sg_image texture = sg_empty_texture(img->width, img->height);
    sg_update_texture_from_buffer(texture, img);
    return texture;
}

sg_image sg_load_texture_path(const char *path, unsigned int *width, unsigned int *height) {
    simage_buffer tmp;
    if (!simage_load_from_path(path, &tmp))
        return (sg_image){.id=SG_INVALID_ID};
    if (width)
        *width = tmp.width;
    if (height)
        *height = tmp.height;
    return image_to_sg(&tmp);
}

sg_image sg_load_texture_from_memory(unsigned char *data, size_t data_size, unsigned int *width, unsigned int *height) {
    simage_buffer tmp;
    if (!simage_load_from_memory(data, data_size, &tmp))
        return (sg_image){.id=SG_INVALID_ID};
    if (width)
        *width = tmp.width;
    if (height)
        *height = tmp.height;
    return image_to_sg(&tmp);
}

void sg_update_texture_from_buffer(sg_image texture, simage_buffer *img) {
    if (texture.id == SG_INVALID_ID)
        return;
    sg_update_image(texture, &(sg_image_data) {
        .subimage[0][0] = (sg_range) {
            .ptr = img->buffer,
            .size = img->width * img->height * sizeof(int)
        }
    });
}
#endif
