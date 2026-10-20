// Power Collage OpenFX plugin: diagonal split-screen collage for DaVinci Resolve.
// Works like Resolve FX Video Collage (Create Tile / Create Background, Active
// Tile, Preview Layout), but the tiles are separated by parallel dividers at an
// adjustable angle. Geometry is computed once per frame in PowerCollageGeometry.h,
// the per-pixel work in PowerCollageKernel.inc runs on Metal or the CPU.

#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "ofxsImageEffect.h"
#include "ofxsInteract.h"
#include "ofxsMultiThread.h"
#include "ofxsProcessing.h"
#include "ofxDrawSuite.h"
#include "ofxsSupportPrivate.h"

#include "PowerCollageGeometry.h"

#define kPluginName "Power Collage"
#define kPluginGrouping "Power Collage"
#define kPluginDescription "Diagonal split-screen collage. Apply it to every clip of the collage and give each clip its own Active Tile."
#define kPluginIdentifier "de.podstars.PowerCollage"
#define kPluginVersionMajor 1
#define kPluginVersionMinor 0

static const char* kParamWorkflow = "workflow";
static const char* kParamPreview = "previewLayout";
static const char* kParamActiveTile = "activeTile";
static const char* kParamTiles = "tiles";
static const char* kParamAngle = "angle";
static const char* kParamCenterX = "centerX";
static const char* kParamCenterY = "centerY";
static const char* kParamSpacing = "spacing";
static const char* kParamMargins = "margins";
static const char* kParamRounding = "rounding";
static const char* kParamSoftness = "softness";
static const char* kParamMoveWithTile = "moveWithTile";
static const char* kParamZoom = "zoom";
static const char* kParamOffsetX = "offsetX";
static const char* kParamOffsetY = "offsetY";
static const char* kParamContentEdges = "contentEdges";
static const char* kParamBorderWidth = "borderWidth";
static const char* kParamBorderOnFrameEdges = "borderOnFrameEdges";
static const char* kParamBorderColor = "borderColor";
static const char* kParamBackgroundUsesBorderColor = "backgroundUsesBorderColor";
static const char* kParamDrawFill = "drawFill";
static const char* kParamFillColor = "fillColor";

#ifdef __APPLE__
extern void RunMetalKernel(void* p_CmdQ, const PCParams& p_Params, const float* p_Input, float* p_Output);
#endif

////////////////////////////////////////////////////////////////////////////////

class CollageProcessor : public OFX::ImageProcessor
{
public:
	explicit CollageProcessor(OFX::ImageEffect& p_Instance)
		: OFX::ImageProcessor(p_Instance)
		, m_Src(nullptr)
	{
	}

	void setSrcImg(OFX::Image* p_Src, const float* p_PackedSrc)
	{
		m_SrcImg = p_Src;
		m_Src = p_PackedSrc;
	}

	void setParams(const PCParams& p_Params)
	{
		m_Params = p_Params;
	}

	virtual void processImagesMetal()
	{
#ifdef __APPLE__
		RunMetalKernel(_pMetalCmdQ, m_Params, static_cast<const float*>(m_SrcImg->getPixelData()),
			static_cast<float*>(_dstImg->getPixelData()));
#endif
	}

	virtual void multiThreadProcessImages(OfxRectI p_ProcWindow)
	{
		const OfxRectI& bounds = _dstImg->getBounds();
		for (int y = p_ProcWindow.y1; y < p_ProcWindow.y2; ++y)
		{
			if (_effect.abort()) break;
			float* dstPix = static_cast<float*>(_dstImg->getPixelAddress(p_ProcWindow.x1, y));
			for (int x = p_ProcWindow.x1; x < p_ProcWindow.x2; ++x)
			{
				const PCColor c = pcShade(m_Params, m_Src, x - bounds.x1, y - bounds.y1);
				dstPix[0] = c.r;
				dstPix[1] = c.g;
				dstPix[2] = c.b;
				dstPix[3] = c.a;
				dstPix += 4;
			}
		}
	}

private:
	OFX::Image* m_SrcImg = nullptr;
	const float* m_Src;
	PCParams m_Params;
};

////////////////////////////////////////////////////////////////////////////////

class PowerCollagePlugin : public OFX::ImageEffect
{
public:
	explicit PowerCollagePlugin(OfxImageEffectHandle p_Handle)
		: ImageEffect(p_Handle)
	{
		m_DstClip = fetchClip(kOfxImageEffectOutputClipName);
		m_SrcClip = fetchClip(kOfxImageEffectSimpleSourceClipName);

		m_Workflow = fetchChoiceParam(kParamWorkflow);
		m_Preview = fetchBooleanParam(kParamPreview);
		m_ActiveTile = fetchChoiceParam(kParamActiveTile);
		m_Tiles = fetchIntParam(kParamTiles);
		m_Angle = fetchDoubleParam(kParamAngle);
		m_CenterX = fetchDoubleParam(kParamCenterX);
		m_CenterY = fetchDoubleParam(kParamCenterY);
		m_Spacing = fetchDoubleParam(kParamSpacing);
		m_Margins = fetchDoubleParam(kParamMargins);
		m_Rounding = fetchDoubleParam(kParamRounding);
		m_Softness = fetchDoubleParam(kParamSoftness);
		m_MoveWithTile = fetchChoiceParam(kParamMoveWithTile);
		m_Zoom = fetchDoubleParam(kParamZoom);
		m_OffsetX = fetchDoubleParam(kParamOffsetX);
		m_OffsetY = fetchDoubleParam(kParamOffsetY);
		m_ContentEdges = fetchChoiceParam(kParamContentEdges);
		m_BorderWidth = fetchDoubleParam(kParamBorderWidth);
		m_BorderOnFrameEdges = fetchBooleanParam(kParamBorderOnFrameEdges);
		m_BorderColor = fetchRGBAParam(kParamBorderColor);
		m_BackgroundUsesBorderColor = fetchBooleanParam(kParamBackgroundUsesBorderColor);
		m_DrawFill = fetchBooleanParam(kParamDrawFill);
		m_FillColor = fetchRGBAParam(kParamFillColor);
	}

	PCSettings settingsAt(double p_Time)
	{
		PCSettings s;
		int choice = 0;
		m_Workflow->getValueAtTime(p_Time, choice);
		s.mode = choice;
		s.preview = m_Preview->getValueAtTime(p_Time);
		m_ActiveTile->getValueAtTime(p_Time, choice);
		s.activeTile = choice + 1;
		s.tiles = m_Tiles->getValueAtTime(p_Time);
		s.angle = m_Angle->getValueAtTime(p_Time);
		s.centerX = 0.5 + m_CenterX->getValueAtTime(p_Time);
		s.centerY = 0.5 + m_CenterY->getValueAtTime(p_Time);
		s.spacing = m_Spacing->getValueAtTime(p_Time);
		s.margins = m_Margins->getValueAtTime(p_Time);
		s.rounding = m_Rounding->getValueAtTime(p_Time);
		s.softness = m_Softness->getValueAtTime(p_Time);
		m_MoveWithTile->getValueAtTime(p_Time, choice);
		s.moveWithTile = choice;
		s.zoom = m_Zoom->getValueAtTime(p_Time);
		s.offsetX = m_OffsetX->getValueAtTime(p_Time);
		s.offsetY = m_OffsetY->getValueAtTime(p_Time);
		m_ContentEdges->getValueAtTime(p_Time, choice);
		s.contentEdges = choice;
		s.borderWidth = m_BorderWidth->getValueAtTime(p_Time);
		s.borderOnFrameEdges = m_BorderOnFrameEdges->getValueAtTime(p_Time);
		m_BorderColor->getValueAtTime(p_Time, s.borderColor[0], s.borderColor[1], s.borderColor[2], s.borderColor[3]);
		s.backgroundUsesBorderColor = m_BackgroundUsesBorderColor->getValueAtTime(p_Time);
		s.drawFill = m_DrawFill->getValueAtTime(p_Time);
		m_FillColor->getValueAtTime(p_Time, s.fillColor[0], s.fillColor[1], s.fillColor[2], s.fillColor[3]);
		return s;
	}

	OfxRectD frameRect(double p_Time)
	{
		return m_SrcClip->getRegionOfDefinition(p_Time);
	}

	OFX::DoubleParam* angleParam() { return m_Angle; }
	OFX::DoubleParam* centerXParam() { return m_CenterX; }
	OFX::DoubleParam* centerYParam() { return m_CenterY; }

	virtual void getClipPreferences(OFX::ClipPreferencesSetter& p_Prefs)
	{
		p_Prefs.setOutputPremultiplication(OFX::eImagePreMultiplied);
	}

	virtual bool isIdentity(const OFX::IsIdentityArguments& /*p_Args*/, OFX::Clip*& /*p_IdentityClip*/, double& /*p_IdentityTime*/)
	{
		return false;
	}

	virtual void render(const OFX::RenderArguments& p_Args)
	{
		if ((m_DstClip->getPixelDepth() != OFX::eBitDepthFloat) || (m_DstClip->getPixelComponents() != OFX::ePixelComponentRGBA))
		{
			OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
		}

		std::unique_ptr<OFX::Image> dst(m_DstClip->fetchImage(p_Args.time));
		std::unique_ptr<OFX::Image> src(m_SrcClip->fetchImage(p_Args.time));
		if (!dst || !src) OFX::throwSuiteStatusException(kOfxStatFailed);
		if ((src->getPixelDepth() != dst->getPixelDepth()) || (src->getPixelComponents() != dst->getPixelComponents()))
		{
			OFX::throwSuiteStatusException(kOfxStatErrValue);
		}

		const OfxRectI& bounds = dst->getBounds();
		const int width = bounds.x2 - bounds.x1;
		const int height = bounds.y2 - bounds.y1;
		const PCParams params = PCGeometry::buildParams(settingsAt(p_Args.time), width, height);

		// The kernel indexes the source as a packed width x height buffer.
		const float* packed = static_cast<const float*>(src->getPixelData());
		std::vector<float> copy;
		const OfxRectI& srcBounds = src->getBounds();
		const bool isPacked = (srcBounds.x1 == bounds.x1) && (srcBounds.y1 == bounds.y1) && (srcBounds.x2 == bounds.x2)
			&& (srcBounds.y2 == bounds.y2) && (src->getRowBytes() == width * 4 * static_cast<int>(sizeof(float)));
		if (!isPacked && !p_Args.isEnabledMetalRender)
		{
			copy.assign(static_cast<size_t>(width) * height * 4, 0.0f);
			for (int y = 0; y < height; ++y)
			{
				for (int x = 0; x < width; ++x)
				{
					const float* pix = static_cast<const float*>(src->getPixelAddress(bounds.x1 + x, bounds.y1 + y));
					if (pix) std::memcpy(&copy[(static_cast<size_t>(y) * width + x) * 4], pix, 4 * sizeof(float));
				}
			}
			packed = copy.data();
		}

		CollageProcessor processor(*this);
		processor.setDstImg(dst.get());
		processor.setSrcImg(src.get(), packed);
		processor.setGPURenderArgs(p_Args);
		processor.setRenderWindow(p_Args.renderWindow);
		processor.setParams(params);
		processor.process();
	}

private:
	OFX::Clip* m_DstClip;
	OFX::Clip* m_SrcClip;

	OFX::ChoiceParam* m_Workflow;
	OFX::BooleanParam* m_Preview;
	OFX::ChoiceParam* m_ActiveTile;
	OFX::IntParam* m_Tiles;
	OFX::DoubleParam* m_Angle;
	OFX::DoubleParam* m_CenterX;
	OFX::DoubleParam* m_CenterY;
	OFX::DoubleParam* m_Spacing;
	OFX::DoubleParam* m_Margins;
	OFX::DoubleParam* m_Rounding;
	OFX::DoubleParam* m_Softness;
	OFX::ChoiceParam* m_MoveWithTile;
	OFX::DoubleParam* m_Zoom;
	OFX::DoubleParam* m_OffsetX;
	OFX::DoubleParam* m_OffsetY;
	OFX::ChoiceParam* m_ContentEdges;
	OFX::DoubleParam* m_BorderWidth;
	OFX::BooleanParam* m_BorderOnFrameEdges;
	OFX::RGBAParam* m_BorderColor;
	OFX::BooleanParam* m_BackgroundUsesBorderColor;
	OFX::BooleanParam* m_DrawFill;
	OFX::RGBAParam* m_FillColor;
};

////////////////////////////////////////////////////////////////////////////////

// Viewer overlay: the dividers, a crosshair to drag the split center and a
// handle on the divider to drag the angle.
class PowerCollageInteract : public OFX::OverlayInteract
{
public:
	PowerCollageInteract(OfxInteractHandle p_Handle, OFX::ImageEffect* p_Effect)
		: OFX::OverlayInteract(p_Handle)
		, m_Plugin(static_cast<PowerCollagePlugin*>(p_Effect))
	{
	}

	virtual bool draw(const OFX::DrawArgs& p_Args)
	{
		OfxDrawContextHandle ctx = OFX::Private::gDrawSuite ? p_Args.context : nullptr;
		if (!ctx) return false;

		const Geometry g = geometry(p_Args.time);
		const double px = p_Args.pixelScale.x;
		const OfxRGBAColourF lineColor = { 1.0f, 0.85f, 0.1f, 0.9f };
		const OfxRGBAColourF handleColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		const OfxRGBAColourF activeColor = { 1.0f, 0.85f, 0.1f, 1.0f };

		OFX::Private::gDrawSuite->setColour(ctx, &lineColor);
		OFX::Private::gDrawSuite->setLineWidth(ctx, 1.5f);
		for (double d : g.dividers)
		{
			OfxPointD a, b;
			if (dividerSegment(g, d, a, b))
			{
				const OfxPointD points[] = { a, b };
				OFX::Private::gDrawSuite->draw(ctx, kOfxDrawPrimitiveLines, points, 2);
			}
		}

		const double r = kHandleRadius * px;
		const OfxPointD c = g.center;
		const OfxPointD h = g.handle;
		{
			const OfxPointD points[] = { c, h };
			OFX::Private::gDrawSuite->setColour(ctx, &handleColor);
			OFX::Private::gDrawSuite->setLineStipple(ctx, kOfxDrawLineStipplePatternDot);
			OFX::Private::gDrawSuite->draw(ctx, kOfxDrawPrimitiveLines, points, 2);
			OFX::Private::gDrawSuite->setLineStipple(ctx, kOfxDrawLineStipplePatternSolid);
		}
		{
			OFX::Private::gDrawSuite->setColour(ctx, m_Drag == DragCenter ? &activeColor : &handleColor);
			const OfxPointD points[] = { { c.x - 2 * r, c.y }, { c.x + 2 * r, c.y }, { c.x, c.y - 2 * r }, { c.x, c.y + 2 * r } };
			OFX::Private::gDrawSuite->draw(ctx, kOfxDrawPrimitiveLines, points, 4);
			const OfxPointD box[] = { { c.x - r, c.y - r }, { c.x + r, c.y + r } };
			OFX::Private::gDrawSuite->draw(ctx, kOfxDrawPrimitiveEllipse, box, 2);
		}
		{
			OFX::Private::gDrawSuite->setColour(ctx, m_Drag == DragAngle ? &activeColor : &handleColor);
			const OfxPointD box[] = { { h.x - r, h.y - r }, { h.x + r, h.y + r } };
			OFX::Private::gDrawSuite->draw(ctx, kOfxDrawPrimitiveEllipse, box, 2);
		}
		return true;
	}

	virtual bool penDown(const OFX::PenArgs& p_Args)
	{
		const Geometry g = geometry(p_Args.time);
		const double grab = 2.0 * kHandleRadius * p_Args.pixelScale.x;
		const OfxPointD& pos = p_Args.penPosition;
		if (distance(pos, g.handle) <= grab)
		{
			m_Drag = DragAngle;
		}
		else if (distance(pos, g.center) <= grab)
		{
			m_Drag = DragCenter;
		}
		else
		{
			return false;
		}
		m_Plugin->beginEditBlock("Power Collage overlay");
		_effect->redrawOverlays();
		return true;
	}

	virtual bool penMotion(const OFX::PenArgs& p_Args)
	{
		if (m_Drag == DragNone) return false;
		const Geometry g = geometry(p_Args.time);
		const OfxPointD& pos = p_Args.penPosition;
		if (m_Drag == DragCenter)
		{
			m_Plugin->centerXParam()->setValue((pos.x - g.rect.x1) / g.width - 0.5);
			m_Plugin->centerYParam()->setValue((pos.y - g.rect.y1) / g.height - 0.5);
		}
		else
		{
			double angle = std::atan2(pos.y - g.center.y, pos.x - g.center.x) * 180.0 / M_PI;
			// A divider has no direction, keep the angle in -90..90.
			if (angle > 90.0) angle -= 180.0;
			if (angle < -90.0) angle += 180.0;
			m_Plugin->angleParam()->setValue(angle);
		}
		_effect->redrawOverlays();
		return true;
	}

	virtual bool penUp(const OFX::PenArgs& /*p_Args*/)
	{
		if (m_Drag == DragNone) return false;
		m_Drag = DragNone;
		m_Plugin->endEditBlock();
		_effect->redrawOverlays();
		return true;
	}

private:
	enum DragMode { DragNone, DragCenter, DragAngle };

	struct Geometry
	{
		OfxRectD rect;
		double width;
		double height;
		double ux;
		double uy;
		double nx;
		double ny;
		OfxPointD center;
		OfxPointD handle;
		OfxPointD frameCenter;
		std::vector<double> dividers;
	};

	static constexpr double kHandleRadius = 6.0;

	static double distance(const OfxPointD& a, const OfxPointD& b)
	{
		return std::hypot(a.x - b.x, a.y - b.y);
	}

	Geometry geometry(double p_Time)
	{
		Geometry g;
		const PCSettings s = m_Plugin->settingsAt(p_Time);
		g.rect = m_Plugin->frameRect(p_Time);
		g.width = std::max(1.0, g.rect.x2 - g.rect.x1);
		g.height = std::max(1.0, g.rect.y2 - g.rect.y1);
		const double a = s.angle * M_PI / 180.0;
		g.ux = std::cos(a);
		g.uy = std::sin(a);
		g.nx = -g.uy;
		g.ny = g.ux;
		g.frameCenter = { g.rect.x1 + 0.5 * g.width, g.rect.y1 + 0.5 * g.height };
		g.center = { g.rect.x1 + s.centerX * g.width, g.rect.y1 + s.centerY * g.height };
		const double arm = 0.25 * std::min(g.width, g.height);
		g.handle = { g.center.x + g.ux * arm, g.center.y + g.uy * arm };
		const int tiles = std::max(1, std::min(PC_MAX_TILES, s.tiles));
		g.dividers = PCGeometry::dividers(PCGeometry::layout(s, static_cast<int>(g.width), static_cast<int>(g.height), tiles), tiles);
		return g;
	}

	// The divider n.p = d (relative to the frame center) clipped to the frame.
	static bool dividerSegment(const Geometry& g, double d, OfxPointD& a, OfxPointD& b)
	{
		const double hw = 0.5 * g.width;
		const double hh = 0.5 * g.height;
		const double ox = d * g.nx;
		const double oy = d * g.ny;
		double t0 = -1e9, t1 = 1e9;
		const double dirs[2] = { g.ux, g.uy };
		const double origin[2] = { ox, oy };
		const double half[2] = { hw, hh };
		for (int i = 0; i < 2; ++i)
		{
			if (std::fabs(dirs[i]) < 1e-9)
			{
				if (std::fabs(origin[i]) > half[i]) return false;
				continue;
			}
			double ta = (-half[i] - origin[i]) / dirs[i];
			double tb = (half[i] - origin[i]) / dirs[i];
			if (ta > tb) std::swap(ta, tb);
			t0 = std::max(t0, ta);
			t1 = std::min(t1, tb);
		}
		if (t0 >= t1) return false;
		a = { g.frameCenter.x + ox + g.ux * t0, g.frameCenter.y + oy + g.uy * t0 };
		b = { g.frameCenter.x + ox + g.ux * t1, g.frameCenter.y + oy + g.uy * t1 };
		return true;
	}

	PowerCollagePlugin* m_Plugin;
	DragMode m_Drag = DragNone;
};

class PowerCollageOverlayDescriptor : public OFX::DefaultEffectOverlayDescriptor<PowerCollageOverlayDescriptor, PowerCollageInteract>
{
};

////////////////////////////////////////////////////////////////////////////////

using namespace OFX;

class PowerCollagePluginFactory : public OFX::PluginFactoryHelper<PowerCollagePluginFactory>
{
public:
	PowerCollagePluginFactory()
		: OFX::PluginFactoryHelper<PowerCollagePluginFactory>(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor)
	{
	}

	virtual void load() {}
	virtual void unload() {}

	virtual void describe(OFX::ImageEffectDescriptor& p_Desc)
	{
		p_Desc.setLabels(kPluginName, kPluginName, kPluginName);
		p_Desc.setPluginGrouping(kPluginGrouping);
		p_Desc.setPluginDescription(kPluginDescription);

		p_Desc.addSupportedContext(eContextFilter);
		p_Desc.addSupportedContext(eContextGeneral);
		p_Desc.addSupportedBitDepth(eBitDepthFloat);

		p_Desc.setSingleInstance(false);
		p_Desc.setHostFrameThreading(false);
		p_Desc.setSupportsMultiResolution(false);
		p_Desc.setSupportsTiles(false);
		p_Desc.setTemporalClipAccess(false);
		p_Desc.setRenderTwiceAlways(false);
		p_Desc.setSupportsMultipleClipPARs(false);
#ifdef __APPLE__
		p_Desc.setSupportsMetalRender(true);
#endif
		p_Desc.setOverlayInteractDescriptor(new PowerCollageOverlayDescriptor());
	}

	virtual void describeInContext(OFX::ImageEffectDescriptor& p_Desc, OFX::ContextEnum /*p_Context*/)
	{
		ClipDescriptor* srcClip = p_Desc.defineClip(kOfxImageEffectSimpleSourceClipName);
		srcClip->addSupportedComponent(ePixelComponentRGBA);
		srcClip->setTemporalClipAccess(false);
		srcClip->setSupportsTiles(false);
		srcClip->setIsMask(false);

		ClipDescriptor* dstClip = p_Desc.defineClip(kOfxImageEffectOutputClipName);
		dstClip->addSupportedComponent(ePixelComponentRGBA);
		dstClip->setSupportsTiles(false);

		PageParamDescriptor* page = p_Desc.definePageParam("Controls");

		ChoiceParamDescriptor* workflow = defineChoice(p_Desc, page, nullptr, kParamWorkflow, "Workflow",
			"Create Tile places the clip into the active tile. Create Background turns the clip into a frame with holes where the tiles are.",
			{ "Create Tile", "Create Background" }, 0);
		(void)workflow;
		defineBool(p_Desc, page, nullptr, kParamPreview, "Preview Layout", "Show all tiles with their numbers instead of the image", false);
		std::vector<std::string> tileNames;
		for (int i = 1; i <= PC_MAX_TILES; ++i) tileNames.push_back("Tile " + std::to_string(i));
		defineChoice(p_Desc, page, nullptr, kParamActiveTile, "Active Tile", "Tile this clip is placed into. Tile 1 is at the top or on the left.", tileNames, 0);

		GroupParamDescriptor* layout = defineGroup(p_Desc, page, "layoutGroup", "Layout");
		IntParamDescriptor* tiles = p_Desc.defineIntParam(kParamTiles);
		tiles->setLabels("Tiles", "Tiles", "Tiles");
		tiles->setHint("Number of tiles, separated by parallel dividers");
		tiles->setDefault(2);
		tiles->setRange(1, PC_MAX_TILES);
		tiles->setDisplayRange(2, PC_MAX_TILES);
		tiles->setParent(*layout);
		page->addChild(*tiles);
		DoubleParamDescriptor* angle = defineDouble(p_Desc, page, layout, kParamAngle, "Angle",
			"Angle of the dividers in degrees. 0 splits top/bottom, 90 splits left/right.", 45.0, -90.0, 90.0, -90.0, 90.0);
		angle->setDoubleType(eDoubleTypeAngle);
		defineDouble(p_Desc, page, layout, kParamCenterX, "Split Center X", "Moves the dividers left/right", 0.0, -1.0, 1.0, -0.5, 0.5);
		defineDouble(p_Desc, page, layout, kParamCenterY, "Split Center Y", "Moves the dividers up/down", 0.0, -1.0, 1.0, -0.5, 0.5);
		defineDouble(p_Desc, page, layout, kParamSpacing, "Spacing", "Space between the tiles", 0.0, 0.0, 1.0, 0.0, 0.1);
		defineDouble(p_Desc, page, layout, kParamMargins, "Margins", "Space between the tiles and the frame edges", 0.0, 0.0, 0.5, 0.0, 0.2);
		defineDouble(p_Desc, page, layout, kParamRounding, "Rounding", "Rounding of the tile corners", 0.0, 0.0, 0.5, 0.0, 0.2);
		defineDouble(p_Desc, page, layout, kParamSoftness, "Edge Softness", "Soft edge along the tile borders", 0.0, 0.0, 0.5, 0.0, 0.05);

		GroupParamDescriptor* content = defineGroup(p_Desc, page, "contentGroup", "Resize Content");
		defineChoice(p_Desc, page, content, kParamMoveWithTile, "Move With Tile",
			"Position and Scale centers the clip on the tile and scales it so the tile is always filled",
			{ "Off", "Position Only", "Position and Scale" }, 2);
		defineDouble(p_Desc, page, content, kParamZoom, "Zoom", "Additional zoom on the clip", 1.0, 0.01, 10.0, 0.25, 4.0);
		defineDouble(p_Desc, page, content, kParamOffsetX, "Left/Right Offset", "Moves the clip inside the tile", 0.0, -2.0, 2.0, -1.0, 1.0);
		defineDouble(p_Desc, page, content, kParamOffsetY, "Up/Down Offset", "Moves the clip inside the tile", 0.0, -2.0, 2.0, -1.0, 1.0);
		defineChoice(p_Desc, page, content, kParamContentEdges, "Content Edges", "What is shown outside the clip",
			{ "Transparent", "Mirror", "Duplicate" }, 0);

		GroupParamDescriptor* styling = defineGroup(p_Desc, page, "stylingGroup", "Tile Styling");
		defineDouble(p_Desc, page, styling, kParamBorderWidth, "Border Width", "Border inside the tile along the dividers", 0.0, 0.0, 0.5, 0.0, 0.05);
		defineBool(p_Desc, page, styling, kParamBorderOnFrameEdges, "Border On Frame Edges", "Also draw the border along the frame edges", false);
		defineRGBA(p_Desc, page, styling, kParamBorderColor, "Border Color", 1.0, 1.0, 1.0, 1.0);
		defineBool(p_Desc, page, styling, kParamBackgroundUsesBorderColor, "Background Uses Border Color",
			"In Create Background, use the border color instead of the clip", false);
		defineBool(p_Desc, page, styling, kParamDrawFill, "Draw Fill", "Draw a color behind transparent parts of the clip", false);
		defineRGBA(p_Desc, page, styling, kParamFillColor, "Fill Color", 0.0, 0.0, 0.0, 1.0);
	}

	virtual OFX::ImageEffect* createInstance(OfxImageEffectHandle p_Handle, OFX::ContextEnum /*p_Context*/)
	{
		return new PowerCollagePlugin(p_Handle);
	}

private:
	static GroupParamDescriptor* defineGroup(ImageEffectDescriptor& p_Desc, PageParamDescriptor* p_Page, const char* p_Name, const char* p_Label)
	{
		GroupParamDescriptor* group = p_Desc.defineGroupParam(p_Name);
		group->setLabels(p_Label, p_Label, p_Label);
		group->setOpen(true);
		p_Page->addChild(*group);
		return group;
	}

	static DoubleParamDescriptor* defineDouble(ImageEffectDescriptor& p_Desc, PageParamDescriptor* p_Page, GroupParamDescriptor* p_Group,
		const char* p_Name, const char* p_Label, const char* p_Hint, double p_Default, double p_Min, double p_Max,
		double p_DisplayMin, double p_DisplayMax)
	{
		DoubleParamDescriptor* param = p_Desc.defineDoubleParam(p_Name);
		param->setLabels(p_Label, p_Label, p_Label);
		param->setScriptName(p_Name);
		param->setHint(p_Hint);
		param->setDefault(p_Default);
		param->setRange(p_Min, p_Max);
		param->setDisplayRange(p_DisplayMin, p_DisplayMax);
		param->setIncrement((p_DisplayMax - p_DisplayMin) / 200.0);
		param->setDoubleType(eDoubleTypePlain);
		if (p_Group) param->setParent(*p_Group);
		p_Page->addChild(*param);
		return param;
	}

	static void defineBool(ImageEffectDescriptor& p_Desc, PageParamDescriptor* p_Page, GroupParamDescriptor* p_Group,
		const char* p_Name, const char* p_Label, const char* p_Hint, bool p_Default)
	{
		BooleanParamDescriptor* param = p_Desc.defineBooleanParam(p_Name);
		param->setLabels(p_Label, p_Label, p_Label);
		param->setHint(p_Hint);
		param->setDefault(p_Default);
		if (p_Group) param->setParent(*p_Group);
		p_Page->addChild(*param);
	}

	static ChoiceParamDescriptor* defineChoice(ImageEffectDescriptor& p_Desc, PageParamDescriptor* p_Page, GroupParamDescriptor* p_Group,
		const char* p_Name, const char* p_Label, const char* p_Hint, const std::vector<std::string>& p_Options, int p_Default)
	{
		ChoiceParamDescriptor* param = p_Desc.defineChoiceParam(p_Name);
		param->setLabels(p_Label, p_Label, p_Label);
		param->setHint(p_Hint);
		for (const std::string& option : p_Options) param->appendOption(option);
		param->setDefault(p_Default);
		if (p_Group) param->setParent(*p_Group);
		p_Page->addChild(*param);
		return param;
	}

	static void defineRGBA(ImageEffectDescriptor& p_Desc, PageParamDescriptor* p_Page, GroupParamDescriptor* p_Group,
		const char* p_Name, const char* p_Label, double p_R, double p_G, double p_B, double p_A)
	{
		RGBAParamDescriptor* param = p_Desc.defineRGBAParam(p_Name);
		param->setLabels(p_Label, p_Label, p_Label);
		param->setDefault(p_R, p_G, p_B, p_A);
		if (p_Group) param->setParent(*p_Group);
		p_Page->addChild(*param);
	}
};

void OFX::Plugin::getPluginIDs(PluginFactoryArray& p_FactoryArray)
{
	static PowerCollagePluginFactory powerCollage;
	p_FactoryArray.push_back(&powerCollage);
}
