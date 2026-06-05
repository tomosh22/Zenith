#pragma once

// ============================================================================
// CB_ToolIconGen — procedural toolbar-icon drawing (tools-build only). Pure CPU
// pixel code, zero engine deps: it renders a recognisable white-on-transparent
// glyph (with a dark outline) for each of the 20 tools into an RGBA8 buffer.
// CityBuilder.cpp wraps the buffer into a .ztxtr at tools-build. Glyphs are drawn
// supersampled (4x) and box-downsampled for clean antialiased edges.
//
// RenderToolIcon(index, outSize, outRGBA) — index matches CB_ToolIcons::All().
// ============================================================================

#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace CB_ToolIconGen
{
	// White glyph + dark outline colours.
	static constexpr uint8_t IW_R = 245, IW_G = 248, IW_B = 252;
	static constexpr uint8_t ID_R = 26,  ID_G = 28,  ID_B = 34;

	// Supersampled RGBA canvas. Primitives take coords in a fixed 256x256 design
	// space (scaled by k to the real supersample resolution), so glyphs are written
	// once and render at any output size.
	struct Canvas
	{
		int S; double k; std::vector<uint8_t> a;
		explicit Canvas(int s) : S(s), k(s / 256.0), a(static_cast<size_t>(s) * s * 4, 0) {}

		inline void set(int x, int y, uint8_t r, uint8_t g, uint8_t b)
		{
			if (static_cast<unsigned>(x) < static_cast<unsigned>(S) && static_cast<unsigned>(y) < static_cast<unsigned>(S))
			{
				size_t i = (static_cast<size_t>(y) * S + x) * 4;
				a[i] = r; a[i + 1] = g; a[i + 2] = b; a[i + 3] = 255;
			}
		}
		void rect(double x0, double y0, double x1, double y1, uint8_t r, uint8_t g, uint8_t b)
		{
			int ix0 = (int)std::floor(x0 * k), iy0 = (int)std::floor(y0 * k);
			int ix1 = (int)std::ceil(x1 * k),  iy1 = (int)std::ceil(y1 * k);
			for (int y = iy0; y < iy1; ++y) for (int x = ix0; x < ix1; ++x) set(x, y, r, g, b);
		}
		void disc(double cx, double cy, double rad, uint8_t r, uint8_t g, uint8_t b)
		{
			double CX = cx * k, CY = cy * k, RD = rad * k;
			int x0 = (int)(CX - RD - 1), x1 = (int)(CX + RD + 1), y0 = (int)(CY - RD - 1), y1 = (int)(CY + RD + 1);
			for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x)
			{
				double dx = x - CX, dy = y - CY; if (dx * dx + dy * dy <= RD * RD) set(x, y, r, g, b);
			}
		}
		void seg(double x0, double y0, double x1, double y1, double halfw, uint8_t r, uint8_t g, uint8_t b)
		{
			double X0 = x0 * k, Y0 = y0 * k, X1 = x1 * k, Y1 = y1 * k, HW = halfw * k;
			int bx0 = (int)(std::min(X0, X1) - HW - 1), bx1 = (int)(std::max(X0, X1) + HW + 1);
			int by0 = (int)(std::min(Y0, Y1) - HW - 1), by1 = (int)(std::max(Y0, Y1) + HW + 1);
			double vx = X1 - X0, vy = Y1 - Y0, len2 = vx * vx + vy * vy;
			for (int y = by0; y <= by1; ++y) for (int x = bx0; x <= bx1; ++x)
			{
				double t = len2 > 0 ? ((x - X0) * vx + (y - Y0) * vy) / len2 : 0.0; t = t < 0 ? 0 : (t > 1 ? 1 : t);
				double px = X0 + t * vx, py = Y0 + t * vy, dx = x - px, dy = y - py;
				if (dx * dx + dy * dy <= HW * HW) set(x, y, r, g, b);
			}
		}
		void poly(const double* xs, const double* ys, int n, uint8_t r, uint8_t g, uint8_t b)
		{
			double minx = 1e9, maxx = -1e9, miny = 1e9, maxy = -1e9;
			for (int i = 0; i < n; ++i) { double X = xs[i] * k, Y = ys[i] * k; minx = std::min(minx, X); maxx = std::max(maxx, X); miny = std::min(miny, Y); maxy = std::max(maxy, Y); }
			int x0 = (int)std::floor(minx), x1 = (int)std::ceil(maxx), y0 = (int)std::floor(miny), y1 = (int)std::ceil(maxy);
			for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x)
			{
				bool in = false; double px = x + 0.5, py = y + 0.5;
				for (int i = 0, j = n - 1; i < n; j = i++)
				{
					double xi = xs[i] * k, yi = ys[i] * k, xj = xs[j] * k, yj = ys[j] * k;
					if (((yi > py) != (yj > py)) && (px < (xj - xi) * (py - yi) / (yj - yi) + xi)) in = !in;
				}
				if (in) set(x, y, r, g, b);
			}
		}
		void tri(double ax, double ay, double bx, double by, double cx, double cy, uint8_t r, uint8_t g, uint8_t b)
		{
			double xs[3] = { ax, bx, cx }, ys[3] = { ay, by, cy }; poly(xs, ys, 3, r, g, b);
		}
		void star(double cx, double cy, double rO, double rI, int pts, uint8_t r, uint8_t g, uint8_t b)
		{
			const int n = pts * 2; double xs[24], ys[24]; const double PI = 3.14159265358979;
			for (int i = 0; i < n && i < 24; ++i) { double ang = -PI / 2 + i * PI / pts; double rr = (i & 1) ? rI : rO; xs[i] = cx + rr * std::cos(ang); ys[i] = cy + rr * std::sin(ang); }
			poly(xs, ys, n, r, g, b);
		}
	};

	inline void DrawGlyph(int idx, Canvas& c)
	{
		#define IW IW_R, IW_G, IW_B
		#define ID ID_R, ID_G, ID_B
		switch (idx)
		{
		case 0: // bulldoze: bold X
			c.seg(66, 66, 190, 190, 16, IW); c.seg(190, 66, 66, 190, 16, IW); break;
		case 1: // road: surface bar + centre dashes
			c.rect(34, 110, 222, 146, IW);
			c.rect(56, 124, 92, 132, ID); c.rect(110, 124, 146, 132, ID); c.rect(164, 124, 200, 132, ID); break;
		case 2: // residential: house
			c.tri(128, 50, 54, 118, 202, 118, IW); c.rect(74, 118, 182, 196, IW); c.rect(116, 150, 140, 196, ID); break;
		case 3: // commercial: tower with windows
		{
			c.rect(82, 44, 174, 210, IW);
			for (int r = 0; r < 4; ++r) for (int cc = 0; cc < 3; ++cc) { double x = 96 + cc * 30, y = 60 + r * 36; c.rect(x, y, x + 18, y + 22, ID); }
			break;
		}
		case 4: // industrial: factory + chimneys + smoke
			c.rect(54, 118, 202, 202, IW); c.rect(72, 74, 98, 118, IW); c.rect(118, 90, 144, 118, IW);
			c.disc(86, 60, 15, IW); c.disc(110, 46, 12, IW); break;
		case 5: // park: tree
			c.disc(128, 100, 50, IW); c.disc(96, 124, 32, IW); c.disc(160, 124, 32, IW); c.rect(118, 150, 138, 204, IW); break;
		case 6: // power: lightning bolt
		{
			double bx[] = { 150, 96, 128, 104, 172, 140, 160 }, by[] = { 42, 138, 138, 214, 106, 106, 42 };
			c.poly(bx, by, 7, IW); break;
		}
		case 7: // water: droplet
			c.disc(128, 152, 46, IW); c.tri(128, 50, 88, 150, 168, 150, IW); break;
		case 8: // police: shield + star
		{
			double sx[] = { 70, 186, 186, 128, 70 }, sy[] = { 62, 62, 138, 210, 138 };
			c.poly(sx, sy, 5, IW); c.star(128, 120, 34, 15, 5, ID); break;
		}
		case 9: // fire: two-tone flame
			c.disc(128, 160, 42, IW); c.tri(128, 48, 92, 160, 164, 160, IW);
			c.disc(128, 172, 20, ID); c.tri(128, 98, 110, 172, 146, 172, ID); break;
		case 10: // health: medical cross
			c.rect(112, 54, 144, 202, IW); c.rect(62, 108, 194, 140, IW); break;
		case 11: // school: graduation cap
		{
			double dx[] = { 128, 200, 128, 56 }, dy[] = { 66, 104, 142, 104 };
			c.poly(dx, dy, 4, IW); c.rect(104, 104, 152, 150, IW); c.seg(188, 104, 188, 152, 4, IW); c.disc(188, 158, 9, IW); break;
		}
		case 12: // garbage: trash can
			c.rect(78, 90, 178, 102, IW); c.rect(116, 76, 140, 90, IW);
			{ double bx2[] = { 86, 170, 160, 96 }, by2[] = { 104, 104, 200, 200 }; c.poly(bx2, by2, 4, IW); }
			c.rect(108, 118, 116, 188, ID); c.rect(124, 118, 132, 188, ID); c.rect(140, 118, 148, 188, ID); break;
		case 13: // sewage: pipe section
			c.rect(46, 112, 210, 156, IW); c.rect(46, 100, 72, 168, IW); c.rect(184, 100, 210, 168, IW); c.disc(128, 134, 9, ID); break;
		case 14: // transit: bus
			c.rect(46, 70, 210, 158, IW); c.rect(58, 86, 198, 118, ID);
			c.disc(86, 166, 18, IW); c.disc(170, 166, 18, IW); c.disc(86, 166, 9, ID); c.disc(170, 166, 9, ID); break;
		case 15: // mail: envelope
			c.rect(46, 82, 210, 174, IW); c.seg(46, 82, 128, 134, 7, ID); c.seg(210, 82, 128, 134, 7, ID); break;
		case 16: // district: flag
			c.rect(70, 46, 84, 210, IW); c.tri(84, 56, 186, 84, 84, 112, IW); break;
		case 17: // busline: route with stops
			c.seg(50, 172, 104, 92, 12, IW); c.seg(104, 92, 158, 150, 12, IW); c.seg(158, 150, 210, 80, 12, IW);
			c.disc(50, 172, 17, IW); c.disc(50, 172, 8, ID); c.disc(158, 150, 17, IW); c.disc(158, 150, 8, ID); c.disc(210, 80, 17, IW); c.disc(210, 80, 8, ID); break;
		case 18: // conduit: plug
			c.rect(96, 72, 160, 150, IW); c.rect(108, 48, 120, 72, IW); c.rect(136, 48, 148, 72, IW); c.rect(122, 150, 134, 198, IW); break;
		case 19: // terrain: mountains
			c.tri(34, 200, 108, 76, 168, 200, IW); c.tri(118, 200, 182, 106, 226, 200, IW); break;
		default: break;
		}
		#undef IW
		#undef ID
	}

	// Add a ~1.25px dark outline (at output scale) around the glyph silhouette:
	// any transparent pixel within R of an opaque pixel becomes the outline colour.
	inline void Outline(Canvas& c)
	{
		const int R = std::max(3, c.S / 64 + 1);
		std::vector<uint8_t> mark(static_cast<size_t>(c.S) * c.S, 0);
		for (int y = 0; y < c.S; ++y) for (int x = 0; x < c.S; ++x)
		{
			if (c.a[(static_cast<size_t>(y) * c.S + x) * 4 + 3] != 0) continue;
			bool bNear = false;
			for (int dy = -R; dy <= R && !bNear; ++dy) for (int dx = -R; dx <= R; ++dx)
			{
				if (dx * dx + dy * dy > R * R) continue;
				int nx = x + dx, ny = y + dy;
				if (static_cast<unsigned>(nx) >= static_cast<unsigned>(c.S) || static_cast<unsigned>(ny) >= static_cast<unsigned>(c.S)) continue;
				if (c.a[(static_cast<size_t>(ny) * c.S + nx) * 4 + 3] != 0) { bNear = true; break; }
			}
			if (bNear) mark[static_cast<size_t>(y) * c.S + x] = 1;
		}
		for (size_t p = 0; p < mark.size(); ++p) if (mark[p]) { size_t i = p * 4; c.a[i] = ID_R; c.a[i + 1] = ID_G; c.a[i + 2] = ID_B; c.a[i + 3] = 255; }
	}

	// 4x box-downsample with alpha-weighted RGB (no dark fringing on edges).
	inline void Downsample(const Canvas& c, int outSize, std::vector<uint8_t>& out)
	{
		out.assign(static_cast<size_t>(outSize) * outSize * 4, 0);
		const int f = c.S / outSize;
		for (int oy = 0; oy < outSize; ++oy) for (int ox = 0; ox < outSize; ++ox)
		{
			long sr = 0, sg = 0, sb = 0, sa = 0, wsum = 0;
			for (int yy = 0; yy < f; ++yy) for (int xx = 0; xx < f; ++xx)
			{
				size_t i = ((static_cast<size_t>(oy * f + yy)) * c.S + (ox * f + xx)) * 4;
				int al = c.a[i + 3]; sa += al; sr += static_cast<long>(c.a[i]) * al; sg += static_cast<long>(c.a[i + 1]) * al; sb += static_cast<long>(c.a[i + 2]) * al; wsum += al;
			}
			size_t o = (static_cast<size_t>(oy) * outSize + ox) * 4;
			out[o + 3] = static_cast<uint8_t>(sa / (f * f));
			if (wsum > 0) { out[o] = static_cast<uint8_t>(sr / wsum); out[o + 1] = static_cast<uint8_t>(sg / wsum); out[o + 2] = static_cast<uint8_t>(sb / wsum); }
		}
	}

	// Render tool icon `idx` into `out` as outSize*outSize RGBA8 (premultiply-safe).
	inline void RenderToolIcon(int idx, int outSize, std::vector<uint8_t>& out)
	{
		Canvas c(outSize * 4);
		DrawGlyph(idx, c);
		Outline(c);
		Downsample(c, outSize, out);
	}
}
