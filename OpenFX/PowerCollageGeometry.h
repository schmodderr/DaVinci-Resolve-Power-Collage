#pragma once

// Per-frame geometry of Power Collage: splits the frame into strips between
// parallel dividers and turns each tile into a convex polygon for the kernel.
// Mirrors the Lua part of Fuses/PowerCollage.fuse.

#include <algorithm>
#include <cmath>
#include <vector>

#define PC_PARAMS const PCParams&
#define PC_SRC const float*
#include "PowerCollageKernel.inc"
#undef PC_PARAMS
#undef PC_SRC

struct PCSettings
{
	int mode = 0;            // 0 Create Tile, 1 Create Background
	bool preview = false;
	int activeTile = 1;      // 1-based
	int tiles = 2;
	double angle = 45.0;     // degrees, 0 = horizontal divider
	double centerX = 0.5;    // divider center, 0..1 of the frame
	double centerY = 0.5;
	double spacing = 0.0;    // lengths are fractions of the shorter frame side
	double margins = 0.0;
	double rounding = 0.0;
	double softness = 0.0;
	int moveWithTile = 2;    // 0 Off, 1 Position Only, 2 Position and Scale
	double zoom = 1.0;
	double offsetX = 0.0;    // fraction of the frame width
	double offsetY = 0.0;    // fraction of the frame height
	int contentEdges = 0;    // 0 Transparent, 1 Mirror, 2 Duplicate
	double borderWidth = 0.0;
	bool borderOnFrameEdges = false;
	double borderColor[4] = { 1.0, 1.0, 1.0, 1.0 };
	bool backgroundUsesBorderColor = false;
	bool drawFill = false;
	double fillColor[4] = { 0.0, 0.0, 0.0, 1.0 };
};

namespace PCGeometry
{
	const double kBig = 1.0e7;

	struct Point
	{
		double x;
		double y;
	};
	typedef std::vector<Point> Polygon;

	struct Layout
	{
		double nx;
		double ny;
		double hw;
		double hh;
		double lo[PC_MAX_TILES];
		double hi[PC_MAX_TILES];
	};

	// Keep the polygon side of the line n.p = c where sign * (n.p - c) >= 0.
	inline Polygon clipHalfPlane(const Polygon& poly, double nx, double ny, double c, double sign)
	{
		Polygon out;
		const size_t n = poly.size();
		for (size_t i = 0; i < n; ++i)
		{
			const Point& a = poly[i];
			const Point& b = poly[(i + 1) % n];
			const double da = sign * (a.x * nx + a.y * ny - c);
			const double db = sign * (b.x * nx + b.y * ny - c);
			if (da >= 0) out.push_back(a);
			if ((da >= 0) != (db >= 0))
			{
				const double k = da / (da - db);
				out.push_back({ a.x + (b.x - a.x) * k, a.y + (b.y - a.y) * k });
			}
		}
		return out;
	}

	// Tile t (0 = highest along the normal) as a convex polygon in centered pixel
	// coordinates. stripInset moves the divider edges inward, frameInset the frame edges.
	inline Polygon tilePolygon(const Layout& g, int t, double stripInset, double frameInset)
	{
		const double hw = g.hw - frameInset;
		const double hh = g.hh - frameInset;
		if (hw <= 0 || hh <= 0) return Polygon();
		Polygon poly = { { -hw, -hh }, { hw, -hh }, { hw, hh }, { -hw, hh } };
		if (g.lo[t] > -kBig) poly = clipHalfPlane(poly, g.nx, g.ny, g.lo[t] + stripInset, 1);
		if (g.hi[t] < kBig) poly = clipHalfPlane(poly, g.nx, g.ny, g.hi[t] - stripInset, -1);
		if (poly.size() > PC_MAX_VERTS) poly.resize(PC_MAX_VERTS);
		return poly;
	}

	inline Point centroid(const Polygon& poly)
	{
		double a = 0, cx = 0, cy = 0;
		const size_t n = poly.size();
		for (size_t i = 0; i < n; ++i)
		{
			const Point& p = poly[i];
			const Point& q = poly[(i + 1) % n];
			const double cross = p.x * q.y - q.x * p.y;
			a += cross;
			cx += (p.x + q.x) * cross;
			cy += (p.y + q.y) * cross;
		}
		if (std::fabs(a) < 1e-6) return { 0, 0 };
		return { cx / (3 * a), cy / (3 * a) };
	}

	// Smallest scale at which a frame-sized clip centered on c still covers
	// every vertex of the convex tile, and therefore the whole tile.
	inline double coverScale(const Polygon& poly, const Point& c, double hw, double hh)
	{
		double s = 0;
		for (const Point& p : poly)
		{
			s = std::max(s, std::max(std::fabs(p.x - c.x) / hw, std::fabs(p.y - c.y) / hh));
		}
		return s;
	}

	inline Layout layout(const PCSettings& s, int width, int height, int tiles)
	{
		const double unit = std::min(width, height);
		const double margin = s.margins * unit;
		const double angle = s.angle * M_PI / 180.0;

		Layout g;
		g.nx = -std::sin(angle);
		g.ny = std::cos(angle);
		g.hw = 0.5 * width - margin;
		g.hh = 0.5 * height - margin;

		// The frame (inside the margins) is split into equal strips along the
		// divider normal, shifted so the middle passes through the center.
		const double extent = std::fabs(g.nx) * g.hw + std::fabs(g.ny) * g.hh;
		const double shift = g.nx * (s.centerX - 0.5) * width + g.ny * (s.centerY - 0.5) * height;
		for (int t = 0; t < tiles; ++t)
		{
			const int j = tiles - 1 - t;
			g.lo[t] = (j == 0) ? -kBig : -extent + j * 2 * extent / tiles + shift;
			g.hi[t] = (j == tiles - 1) ? kBig : -extent + (j + 1) * 2 * extent / tiles + shift;
		}
		return g;
	}

	// Positions of the interior dividers along the normal, for the overlay.
	inline std::vector<double> dividers(const Layout& g, int tiles)
	{
		std::vector<double> d;
		for (int t = 1; t < tiles; ++t) d.push_back(g.lo[t - 1]);
		return d;
	}

	inline void storePolygon(const Polygon& poly, float* dst)
	{
		for (size_t i = 0; i < poly.size(); ++i)
		{
			dst[i * 2] = static_cast<float>(poly[i].x);
			dst[i * 2 + 1] = static_cast<float>(poly[i].y);
		}
	}

	inline PCParams buildParams(const PCSettings& s, int width, int height)
	{
		PCParams p = {};
		const int tiles = std::max(1, std::min(PC_MAX_TILES, s.tiles));
		const int active = std::max(1, std::min(tiles, s.activeTile)) - 1;
		const double unit = std::min(width, height);
		const double halfGap = 0.5 * s.spacing * unit;
		const double rounding = s.rounding * unit;
		const double soft = std::max(1.0, s.softness * unit);
		const double border = s.borderWidth * unit;
		const Layout g = layout(s, width, height, tiles);

		// Without spacing the tiles overlap slightly, so that two antialiased edges
		// stacked on different tracks do not leave a dark seam.
		const double seam = (halfGap <= 0) ? (0.5 * soft + 1.0) : 0.0;
		const double rContent = std::max(0.0, rounding - border);

		p.width = width;
		p.height = height;
		p.mode = s.mode;
		p.preview = s.preview ? 1 : 0;
		p.tiles = tiles;
		p.active = active;
		p.fill = s.drawFill ? 1 : 0;
		p.useColor = s.backgroundUsesBorderColor ? 1 : 0;
		p.edges = s.contentEdges;
		p.soft = static_cast<float>(soft);
		p.rOuter = static_cast<float>(rounding);
		p.rContent = static_cast<float>(rContent);
		p.outline = static_cast<float>(std::max(2.0, unit * 0.004));
		p.digitSize = static_cast<float>(unit * 0.12);
		for (int i = 0; i < 4; ++i)
		{
			p.bc[i] = static_cast<float>(s.borderColor[i]);
			p.fc[i] = static_cast<float>(s.fillColor[i]);
		}

		for (int t = 0; t < tiles; ++t)
		{
			const Polygon outer = tilePolygon(g, t, halfGap - seam + rounding, rounding);
			p.nO[t] = static_cast<int>(outer.size());
			storePolygon(outer, &p.polyO[t * PC_MAX_VERTS * 2]);
			const Point c = centroid(tilePolygon(g, t, halfGap, 0));
			p.tcx[t] = static_cast<float>(c.x);
			p.tcy[t] = static_cast<float>(c.y);
		}

		const double frameBorder = s.borderOnFrameEdges ? border : 0.0;
		const Polygon content = tilePolygon(g, active, halfGap - seam + border + rContent, frameBorder + rContent);
		p.nC = static_cast<int>(content.size());
		storePolygon(content, p.polyC);

		// Content placement inside the active tile.
		const Polygon tile = tilePolygon(g, active, halfGap, 0);
		Point cc = { 0, 0 };
		double scale = 1.0;
		if (s.moveWithTile >= 1) cc = centroid(tile);
		if (s.moveWithTile == 2)
		{
			scale = coverScale(tile, cc, 0.5 * width, 0.5 * height);
			if (scale <= 0) scale = 1.0;
		}
		p.ccx = static_cast<float>(cc.x + s.offsetX * width);
		p.ccy = static_cast<float>(cc.y + s.offsetY * height);
		p.scale = static_cast<float>(scale * std::max(0.01, s.zoom));
		return p;
	}
}
