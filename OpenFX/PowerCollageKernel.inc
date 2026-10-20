// Per-pixel part of Power Collage. This file is compiled twice: as C++ for the
// CPU path and, embedded as a string, as Metal Shading Language for the GPU path.
// The including code defines PC_PARAMS (reference to the params) and PC_SRC
// (pointer to the source pixels) with the right address spaces.
// Coordinates are pixels relative to the frame center, y pointing up.

#define PC_MAX_TILES 8
#define PC_MAX_VERTS 8

struct PCParams
{
	int width;
	int height;
	int mode;
	int preview;
	int tiles;
	int active;
	int fill;
	int useColor;
	int edges;
	float soft;
	float rOuter;
	float rContent;
	float outline;
	float ccx;
	float ccy;
	float scale;
	float digitSize;
	float bc[4];
	float fc[4];
	int nO[PC_MAX_TILES];
	float polyO[PC_MAX_TILES * PC_MAX_VERTS * 2];
	int nC;
	float polyC[PC_MAX_VERTS * 2];
	float tcx[PC_MAX_TILES];
	float tcy[PC_MAX_TILES];
};

struct PCColor
{
	float r;
	float g;
	float b;
	float a;
};

static inline PCColor pcColor(float r, float g, float b, float a)
{
	PCColor c;
	c.r = r;
	c.g = g;
	c.b = b;
	c.a = a;
	return c;
}

static inline PCColor pcScale(PCColor c, float s)
{
	return pcColor(c.r * s, c.g * s, c.b * s, c.a * s);
}

static inline PCColor pcAdd(PCColor a, PCColor b)
{
	return pcColor(a.r + b.r, a.g + b.g, a.b + b.b, a.a + b.a);
}

static inline float pcSat(float v)
{
	return fmin(fmax(v, 0.0f), 1.0f);
}

// Coverage of an inside-positive distance, antialiased over "soft" pixels.
static inline float pcCoverage(float sd, float soft)
{
	return pcSat(sd / soft + 0.5f);
}

// Signed distance to a polygon, negative inside (after Inigo Quilez).
// "offset" is the index of the first coordinate in the selected array.
static inline float pcSdPoly(PC_PARAMS p, bool content, int offset, int n, float px, float py)
{
	if (n < 3) return 1.0e7f;
	float vx0 = content ? p.polyC[offset] : p.polyO[offset];
	float vy0 = content ? p.polyC[offset + 1] : p.polyO[offset + 1];
	float d = (px - vx0) * (px - vx0) + (py - vy0) * (py - vy0);
	float s = 1.0f;
	int j = n - 1;
	for (int i = 0; i < n; i++)
	{
		int ii = offset + i * 2;
		int jj = offset + j * 2;
		float vix = content ? p.polyC[ii] : p.polyO[ii];
		float viy = content ? p.polyC[ii + 1] : p.polyO[ii + 1];
		float vjx = content ? p.polyC[jj] : p.polyO[jj];
		float vjy = content ? p.polyC[jj + 1] : p.polyO[jj + 1];
		float ex = vjx - vix;
		float ey = vjy - viy;
		float wx = px - vix;
		float wy = py - viy;
		float h = pcSat((wx * ex + wy * ey) / (ex * ex + ey * ey));
		float bx = wx - ex * h;
		float by = wy - ey * h;
		d = fmin(d, bx * bx + by * by);
		bool c1 = py >= viy;
		bool c2 = py < vjy;
		bool c3 = ex * wy > ey * wx;
		if ((c1 && c2 && c3) || (!c1 && !c2 && !c3)) s = -s;
		j = i;
	}
	return s * sqrt(d);
}

static inline float pcTileCoverage(PC_PARAMS p, int t, float px, float py)
{
	float sd = p.rOuter - pcSdPoly(p, false, t * PC_MAX_VERTS * 2, p.nO[t], px, py);
	return pcCoverage(sd, p.soft);
}

// 3x5 bitmap digits 1..8, rows top to bottom, 3 bits per row.
static inline int pcDigitBits(int n)
{
	if (n == 1) return 11415;
	if (n == 2) return 29671;
	if (n == 3) return 29647;
	if (n == 4) return 23497;
	if (n == 5) return 31183;
	if (n == 6) return 31215;
	if (n == 7) return 29257;
	return 31727;
}

static inline float pcDigitMask(int n, float px, float py, float cx, float cy, float size)
{
	float unit = size / 5.0f;
	float gx = (px - cx) / unit + 1.5f;
	float gy = (cy - py) / unit + 2.5f;
	if (gx < 0.0f || gx >= 3.0f || gy < 0.0f || gy >= 5.0f) return 0.0f;
	int col = (int)gx;
	int row = (int)gy;
	return (float)((pcDigitBits(n) >> ((4 - row) * 3 + (2 - col))) & 1);
}

// Source pixel with the content edge mode: 0 transparent, 1 mirror, 2 duplicate.
static inline PCColor pcFetch(PC_SRC src, int w, int h, int edges, int x, int y)
{
	if (edges == 0)
	{
		if (x < 0 || y < 0 || x >= w || y >= h) return pcColor(0.0f, 0.0f, 0.0f, 0.0f);
	}
	else if (edges == 1)
	{
		int pw = 2 * w;
		int ph = 2 * h;
		x = x % pw;
		y = y % ph;
		if (x < 0) x += pw;
		if (y < 0) y += ph;
		if (x >= w) x = pw - 1 - x;
		if (y >= h) y = ph - 1 - y;
	}
	else
	{
		x = x < 0 ? 0 : (x >= w ? w - 1 : x);
		y = y < 0 ? 0 : (y >= h ? h - 1 : y);
	}
	int i = (y * w + x) * 4;
	return pcColor(src[i], src[i + 1], src[i + 2], src[i + 3]);
}

// Bilinear sample at a position in pixels, pixel centers at i + 0.5.
static inline PCColor pcSample(PC_SRC src, int w, int h, int edges, float sx, float sy)
{
	float fx = sx - 0.5f;
	float fy = sy - 0.5f;
	float x0f = floor(fx);
	float y0f = floor(fy);
	float tx = fx - x0f;
	float ty = fy - y0f;
	int x0 = (int)x0f;
	int y0 = (int)y0f;
	PCColor c00 = pcFetch(src, w, h, edges, x0, y0);
	PCColor c10 = pcFetch(src, w, h, edges, x0 + 1, y0);
	PCColor c01 = pcFetch(src, w, h, edges, x0, y0 + 1);
	PCColor c11 = pcFetch(src, w, h, edges, x0 + 1, y0 + 1);
	PCColor bottom = pcAdd(pcScale(c00, 1.0f - tx), pcScale(c10, tx));
	PCColor top = pcAdd(pcScale(c01, 1.0f - tx), pcScale(c11, tx));
	return pcAdd(pcScale(bottom, 1.0f - ty), pcScale(top, ty));
}

static inline PCColor pcShade(PC_PARAMS p, PC_SRC src, int x, int y)
{
	float W = (float)p.width;
	float H = (float)p.height;
	float px = (float)x + 0.5f - 0.5f * W;
	float py = (float)y + 0.5f - 0.5f * H;

	if (p.preview != 0)
	{
		PCColor outc = pcColor(0.08f, 0.08f, 0.08f, 1.0f);
		for (int t = 0; t < p.tiles; t++)
		{
			float sd = p.rOuter - pcSdPoly(p, false, t * PC_MAX_VERTS * 2, p.nO[t], px, py);
			float a = pcCoverage(sd, p.soft);
			if (a > 0.0f)
			{
				float g = (t % 2 == 0) ? 0.34f : 0.26f;
				PCColor tc = pcColor(g, g, g, 1.0f);
				if (t == p.active) tc = pcColor(0.42f, 0.37f, 0.10f, 1.0f);
				float ring = a - pcCoverage(sd - p.outline, p.soft);
				float dm = pcDigitMask(t + 1, px, py, p.tcx[t], p.tcy[t], p.digitSize);
				float hl = fmax(ring, dm);
				tc = pcAdd(pcScale(tc, 1.0f - hl), pcScale(pcColor(1.0f, 0.85f, 0.1f, 1.0f), hl));
				outc = pcAdd(pcScale(outc, 1.0f - a), pcScale(tc, a));
			}
		}
		return outc;
	}

	if (p.mode == 1)
	{
		// Create Background: the clip (or the border color) with holes where the tiles are.
		float holes = 0.0f;
		for (int t = 0; t < p.tiles; t++)
		{
			holes = fmax(holes, pcTileCoverage(p, t, px, py));
		}
		PCColor c;
		if (p.useColor != 0)
		{
			c = pcColor(p.bc[0] * p.bc[3], p.bc[1] * p.bc[3], p.bc[2] * p.bc[3], p.bc[3]);
		}
		else
		{
			c = pcFetch(src, p.width, p.height, 2, x, y);
		}
		return pcScale(c, 1.0f - holes);
	}

	// Create Tile: the clip placed into the active tile, with an optional border ring.
	float tm = pcTileCoverage(p, p.active, px, py);
	if (tm <= 0.0f) return pcColor(0.0f, 0.0f, 0.0f, 0.0f);
	float cm = pcCoverage(p.rContent - pcSdPoly(p, true, 0, p.nC, px, py), p.soft);
	cm = fmin(cm, tm);
	float sx = (px - p.ccx) / p.scale + 0.5f * W;
	float sy = (py - p.ccy) / p.scale + 0.5f * H;
	PCColor c = pcSample(src, p.width, p.height, p.edges, sx, sy);
	if (p.fill != 0)
	{
		float fa = p.fc[3];
		c = pcAdd(c, pcScale(pcColor(p.fc[0] * fa, p.fc[1] * fa, p.fc[2] * fa, fa), 1.0f - c.a));
	}
	float ba = p.bc[3];
	PCColor b = pcColor(p.bc[0] * ba, p.bc[1] * ba, p.bc[2] * ba, ba);
	return pcAdd(pcScale(c, cm), pcScale(b, tm - cm));
}
