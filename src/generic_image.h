/* generic_image.h -- https://github.com/takeiteasy/generic_image

 Copyright (C) 2025  George Watson

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

#ifndef GENERIC_IMAGE_HEADER
#define GENERIC_IMAGE_HEADER
#if defined(__cplusplus)
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

int32_t RGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
int32_t RGB(uint8_t r, uint8_t g, uint8_t b);
int32_t RGBA1(uint8_t c, uint8_t a);
int32_t RGB1(uint8_t c);

uint8_t Rgba(int32_t c);
uint8_t rGba(int32_t c);
uint8_t rgBa(int32_t c);
uint8_t rgbA(int32_t c);

int32_t rGBA(int32_t c, uint8_t r);
int32_t RgBA(int32_t c, uint8_t g);
int32_t RGbA(int32_t c, uint8_t b);
int32_t RGBa(int32_t c, uint8_t a);

typedef struct image_t {
    unsigned int width, height;
    int32_t *buffer;
} generic_image;

bool image_empty(int w, int h, int32_t color, generic_image* dst);
bool image_load_from_path(const char *path, generic_image* dst);
bool image_load_from_memory(const void *data, size_t length, generic_image* dst);
bool image_save(generic_image *img, const char *path);
void image_destroy(generic_image *img);

void image_fill(generic_image *img, int32_t color);
void image_flood(generic_image *img, int x, int y, int32_t color);
void image_pset(generic_image *img, int x, int y, int32_t color);
int image_pget(generic_image *img, int x, int y);
void image_paste(generic_image *dst, generic_image *src, int x, int y);
void image_clipped_paste(generic_image *dst, generic_image *src, int x, int y, int rx, int ry, int rw, int rh);
void image_resize(generic_image *src, int nw, int nh);
void image_rotate(generic_image *src, float angle);
void image_clip(generic_image *src, int rx, int ry, int rw, int rh);

#ifdef GENERIC_USE_BLOCKS
typedef int(^image_callback_t)(int x, int y, int32_t color);
#else
typedef int(*image_callback_t)(int x, int y, int32_t color);
#endif
void image_pass_thru(generic_image *img, image_callback_t fn);

bool image_dupe(generic_image *src, generic_image *dst);
bool image_resized(generic_image *src, int nw, int nh, generic_image *dst);
bool image_rotated(generic_image *src, float angle, generic_image *dst);
bool image_clipped(generic_image *src, int rx, int ry, int rw, int rh, generic_image *dst);

void image_draw_line(generic_image *img, int x0, int y0, int x1, int y1, int32_t color);
void image_draw_circle(generic_image *img, int xc, int yc, int r, int32_t color, int fill);
void image_draw_rectangle(generic_image *img, int x, int y, int w, int h, int32_t color, int fill);
void image_draw_triangle(generic_image *img, int x0, int y0, int x1, int y1, int x2, int y2, int32_t color, int fill);

#if defined(__cplusplus)
}
#endif
#endif // GENERIC_IMAGE_HEADER

#if defined(GENERIC_IMPL) ||defined(GENERIC_IMAGE_IMPL)
// INCLUDES
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define QOI_IMPLEMENTATION
#include "qoi.h"

#ifdef _WIN32
#include <io.h>
#include <dirent.h>
#define F_OK 0
#define access _access
#else
#include <unistd.h>
#endif

#define _SWAP(a, b)   \
    do                \
    {                 \
        int temp = a; \
        a = b;        \
        b = temp;     \
    } while (0)
#define _MIN(a, b) (((a) < (b)) ? (a) : (b))
#define _MAX(a, b) (((a) > (b)) ? (a) : (b))
#define _CLAMP(v, low, high)  ((v) < (low) ? (low) : ((v) > (high) ? (high) : (v)))
#define _RADIANS(a) ((a) * M_PI / 180.0)

int32_t RGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint8_t)r << 24) | ((uint8_t)g << 16) | ((uint8_t)b << 8) | a;
}

int32_t RGB(uint8_t r, uint8_t g, uint8_t b) {
    return RGBA(r, g, b, 255);
}

int32_t RGBA1(uint8_t c, uint8_t a) {
    return RGBA(c, c, c, a);
}

int32_t RGB1(uint8_t c) {
    return RGB(c, c, c);
}

uint8_t Rgba(int32_t c) {
    return (uint8_t)((c >> 24) & 0xFF);
}

uint8_t rGba(int32_t c) {
    return (uint8_t)((c >>  16) & 0xFF);
}

uint8_t rgBa(int32_t c) {
    return (uint8_t)((c >> 8) & 0xFF);
}

uint8_t rgbA(int32_t c) {
    return (uint8_t)(c & 0xFF);
}

int32_t rGBA(int32_t c, uint8_t r) {
    return (c & ~0x00FF0000) | (r << 24);
}

int32_t RgBA(int32_t c, uint8_t g) {
    return (c & ~0x0000FF00) | (g << 16);
}

int32_t RGbA(int32_t c, uint8_t b) {
    return (c & ~0x000000FF) | (b << 8);
}

int32_t RGBa(int32_t c, uint8_t a) {
    return (c & ~0x00FF0000) | a;
}

bool image_empty(int w, int h, int32_t color, generic_image* dst) {
    if (w <= 0 || h <= 0)
        return false;
    generic_image result = (generic_image) {
        .width = w,
        .height = h,
        .buffer = (int32_t*)malloc(w * h * sizeof(generic_image))
    };
    if (!color)
        memset(dst->buffer, 0, w * h * sizeof(int32_t));
    else
        image_fill(dst, color);
    return true;
}

bool image_load_from_path(const char *path, generic_image *dst) {
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
    result = image_load_from_memory(data, (int)sz, dst);
BAIL:
    if (fh)
        fclose(fh);
    if (data)
        free(data);
    return result;
}

#define _SHIFT(A, B, C, D) (((unsigned int)(A) << 24) | \
                            ((unsigned int)(B) << 16) | \
                            ((unsigned int)(C) << 8)  | \
                            (D))

static int check_if_qoi(unsigned char *data) {
    return _SHIFT(data[0], data[0], data[0], data[0]) == _SHIFT('q', 'o', 'i', 'f');
}

bool image_load_from_memory(const void *data, size_t data_size, generic_image* dst) {
    if (!data || data_size <= 0)
        return false;
    int _w, _h, c;
    unsigned char *img_data = NULL;
    if (check_if_qoi((unsigned char*)data)) {
        qoi_desc desc;
        if (!(img_data = qoi_decode(data, data_size, &desc, 4)))
            return false;
        _w = desc.width;
        _h = desc.height;
    } else
        if (!(img_data = stbi_load_from_memory(data, data_size, &_w, &_h, &c, 4)))
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
            dst->buffer[y * _w + x] = RGBA(p[0], p[1], p[2], p[3]);
        }
    free(img_data);
    return true;
}

bool image_save(generic_image *img, const char *path) {
    // TODO: Integrate stb_image_write.h
    return false;
}

void image_destroy(generic_image *img) {
    if (img && img->buffer)
        free(img->buffer);
}

void image_fill(generic_image *img, int32_t color) {
    for (int i = 0; i < img->width * img->height; ++i)
        img->buffer[i] = color;
}

static inline void flood_fn(generic_image *img, int x, int y, int _new, int _old) {
    if (_new == _old || image_pget(img, x, y) != _old)
        return;

    int x1 = x;
    while (x1 < img->width && image_pget(img, x1, y) == _old) {
        image_pset(img, x1, y, _new);
        x1++;
    }

    x1 = x - 1;
    while (x1 >= 0 && image_pget(img, x1, y) == _old) {
        image_pset(img, x1, y, _new);
        x1--;
    }

    x1 = x;
    while (x1 < img->width && image_pget(img, x1, y) == _old) {
        if(y > 0 && image_pget(img, x1, y - 1) == _old)
            flood_fn(img, x1, y - 1, _new, _old);
        x1++;
    }

    x1 = x - 1;
    while(x1 >= 0 && image_pget(img, x1, y) == _old) {
        if(y > 0 && image_pget(img, x1, y - 1) == _old)
            flood_fn(img, x1, y - 1, _new, _old);
        x1--;
    }

    x1 = x;
    while(x1 < img->width && image_pget(img, x1, y) == _old) {
        if(y < img->height - 1 && image_pget(img, x1, y + 1) == _old)
            flood_fn(img, x1, y + 1, _new, _old);
        x1++;
    }

    x1 = x - 1;
    while(x1 >= 0 && image_pget(img, x1, y) == _old) {
        if(y < img->height - 1 && image_pget(img, x1, y + 1) == _old)
            flood_fn(img, x1, y + 1, _new, _old);
        x1--;
    }
}

void image_flood(generic_image *img, int x, int y, int32_t color) {
    if (x < 0 || y < 0 || x >= img->width || y >= img->height)
        return;
    flood_fn(img, x, y, color, image_pget(img, x, y));
}

void image_pset(generic_image *img, int x, int y, int32_t color) {
    if (x >= 0 && y >= 0 && x < img->width && y < img->height) {
        int a = rgbA(color);
        img->buffer[y * img->width + x] = color;
    }
}

int image_pget(generic_image *img, int x, int y) {
    return (x >= 0 && y >= 0 && x < img->width && y < img->height) ? img->buffer[y * img->width + x] : 0;
}

void image_paste(generic_image *dst, generic_image *src, int x, int y) {
    for (int ox = 0; ox < src->width; ++ox) {
        for (int oy = 0; oy < src->height; ++oy) {
            if (oy > dst->height)
                break;
            image_pset(dst, x + ox, y + oy, image_pget(src, ox, oy));
        }
        if (ox > dst->width)
            break;
    }
}

void image_clipped_paste(generic_image *dst, generic_image *src, int x, int y, int rx, int ry, int rw, int rh) {
    for (int ox = 0; ox < rw; ++ox)
        for (int oy = 0; oy < rh; ++oy)
            image_pset(dst, ox + x, oy + y, image_pget(src, ox + rx, oy + ry));
}

bool image_dupe(generic_image *src, generic_image *dst) {
    if (!image_empty(src->width, src->height, 0, dst))
        return false;
    memcpy(dst->buffer, src->buffer, src->width * src->height * sizeof(uint32_t));
    return true;
}

void image_pass_thru(generic_image *img, image_callback_t fn) {
    int x, y;
    for (x = 0; x < img->width; ++x)
        for (y = 0; y < img->height; ++y)
            img->buffer[y * img->width + x] = fn(x, y, image_pget(img, x, y));
}

bool image_resized(generic_image *src, int nw, int nh, generic_image *dst) {
    if (!image_empty(nw, nh, 0, dst))
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

void image_resize(generic_image *src, int nw, int nh) {
    generic_image result;
    if (!image_resized(src, nw, nh, &result))
        return;
    free(src->buffer);
    memcpy(src, &result, sizeof(generic_image));
}

bool image_rotated(generic_image *src, float angle, generic_image *dst) {
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
    if (!image_empty(dw, dh, 0, dst))
        return false;
    int x, y, sx, sy;
    for (x = 0; x < dw; ++x)
        for (y = 0; y < dh; ++y) {
            sx = ((x + mm[0][0]) * c + (y + mm[0][1]) * s);
            sy = ((y + mm[0][1]) * c - (x + mm[0][0]) * s);
            if (sx < 0 || sx >= src->width || sy < 0 || sy >= src->height)
                continue;
            image_pset(dst, x, y, image_pget(src, sx, sy));
        }
    return true;
}

void image_rotate(generic_image *src, float angle) {
    generic_image result;
    if (!image_rotated(src, angle, &result))
        return;
    free(src->buffer);
    memcpy(src, &result, sizeof(generic_image));
}

bool image_clipped(generic_image *src, int rx, int ry, int rw, int rh, generic_image *dst) {
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
    if (!image_empty(iw, ih, 0, dst))
        return false;
    for (int px = 0; px < iw; px++)
        for (int py = 0; py < ih; py++) {
            int cx = ox + px;
            int cy = oy + py;
            image_pset(dst, px, py, image_pget(src, cx, cy));
        }
    return true;
}

void image_clip(generic_image *src, int rx, int ry, int rw, int rh) {
    generic_image result;
    if (!image_clipped(src, rx, ry, rw, rh, &result))
        return;
    free(src->buffer);
    memcpy(src, &result, sizeof(generic_image));
}

static inline void vline(generic_image *img, int x, int y0, int y1, int32_t color) {
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
        image_pset(img, x, y, color);
}

static inline void hline(generic_image *img, int y, int x0, int x1, int32_t color) {
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
        image_pset(img, x, y, color);
}

void image_draw_line(generic_image *img, int x0, int y0, int x1, int y1, int32_t color) {
    if (x0 == x1)
        vline(img, x0, y0, y1, color);
    else if (y0 == y1)
        hline(img, y0, x0, x1, color);
    else {
        int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = (dx > dy ? dx : -dy) / 2;

        while (image_pset(img, x0, y0, color), x0 != x1 || y0 != y1) {
            int e2 = err;
            if (e2 > -dx) { err -= dy; x0 += sx; }
            if (e2 <  dy) { err += dx; y0 += sy; }
        }
    }
}

void image_draw_circle(generic_image *img, int xc, int yc, int r, int32_t color, int fill) {
    int x = -r, y = 0, err = 2 - 2 * r; /* II. Quadrant */
    do {
        image_pset(img, xc - x, yc + y, color);    /*   I. Quadrant */
        image_pset(img, xc - y, yc - x, color);    /*  II. Quadrant */
        image_pset(img, xc + x, yc - y, color);    /* III. Quadrant */
        image_pset(img, xc + y, yc + x, color);    /*  IV. Quadrant */

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

void image_draw_rectangle(generic_image *img, int x, int y, int w, int h, int32_t color, int fill) {
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

void image_draw_triangle(generic_image *img, int x0, int y0, int x1, int y1, int x2, int y2, int32_t color, int fill) {
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
                image_pset(img, j, y0 + i, color);
        }
    } else {
        image_draw_line(img, x0, y0, x1, y1, color);
        image_draw_line(img, x1, y1, x2, y2, color);
        image_draw_line(img, x2, y2, x0, y0, color);
    }
}

#endif // GENERIC_IMPL
