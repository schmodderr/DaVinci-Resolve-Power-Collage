#import <Metal/Metal.h>

#include <mutex>
#include <string>
#include <unordered_map>

#include "PowerCollageGeometry.h"
#include "PowerCollageKernelSource.h"

// The shared kernel (PowerCollageKernel.inc) wrapped as a Metal compute kernel.
static std::string MetalSource()
{
	std::string source =
		"#include <metal_stdlib>\n"
		"using namespace metal;\n"
		"#define PC_PARAMS constant PCParams&\n"
		"#define PC_SRC const device float*\n";
	source += kPowerCollageKernelSource;
	source +=
		"\nkernel void PowerCollageKernel(constant PCParams& p [[buffer(1)]],\n"
		"                               const device float* src [[buffer(0)]],\n"
		"                               device float* dst [[buffer(8)]],\n"
		"                               uint2 id [[thread_position_in_grid]])\n"
		"{\n"
		"    if ((int)id.x < p.width && (int)id.y < p.height)\n"
		"    {\n"
		"        PCColor c = pcShade(p, src, (int)id.x, (int)id.y);\n"
		"        int i = ((int)id.y * p.width + (int)id.x) * 4;\n"
		"        dst[i] = c.r;\n"
		"        dst[i + 1] = c.g;\n"
		"        dst[i + 2] = c.b;\n"
		"        dst[i + 3] = c.a;\n"
		"    }\n"
		"}\n";
	return source;
}

static std::mutex s_PipelineMutex;
static std::unordered_map<id<MTLCommandQueue>, id<MTLComputePipelineState>> s_Pipelines;

static id<MTLComputePipelineState> Pipeline(id<MTLCommandQueue> p_Queue)
{
	std::lock_guard<std::mutex> lock(s_PipelineMutex);
	const auto it = s_Pipelines.find(p_Queue);
	if (it != s_Pipelines.end()) return it->second;

	id<MTLDevice> device = p_Queue.device;
	NSError* err = nil;
	MTLCompileOptions* options = [MTLCompileOptions new];
	options.fastMathEnabled = YES;
	id<MTLLibrary> library = [device newLibraryWithSource:@(MetalSource().c_str()) options:options error:&err];
	[options release];
	if (!library)
	{
		fprintf(stderr, "Power Collage: failed to compile Metal kernel, %s\n", err.localizedDescription.UTF8String);
		return nil;
	}
	id<MTLFunction> function = [library newFunctionWithName:@"PowerCollageKernel"];
	id<MTLComputePipelineState> pipeline = function ? [device newComputePipelineStateWithFunction:function error:&err] : nil;
	[function release];
	[library release];
	if (!pipeline)
	{
		fprintf(stderr, "Power Collage: failed to create Metal pipeline\n");
		return nil;
	}
	s_Pipelines[p_Queue] = pipeline;
	return pipeline;
}

void RunMetalKernel(void* p_CmdQ, const PCParams& p_Params, const float* p_Input, float* p_Output)
{
	id<MTLCommandQueue> queue = static_cast<id<MTLCommandQueue>>(p_CmdQ);
	id<MTLComputePipelineState> pipeline = Pipeline(queue);
	if (!pipeline) return;

	id<MTLBuffer> srcBuf = reinterpret_cast<id<MTLBuffer>>(const_cast<float*>(p_Input));
	id<MTLBuffer> dstBuf = reinterpret_cast<id<MTLBuffer>>(p_Output);

	id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
	commandBuffer.label = @"PowerCollageKernel";
	id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
	[encoder setComputePipelineState:pipeline];
	[encoder setBuffer:srcBuf offset:0 atIndex:0];
	[encoder setBuffer:dstBuf offset:0 atIndex:8];
	[encoder setBytes:&p_Params length:sizeof(PCParams) atIndex:1];

	const NSUInteger exeWidth = [pipeline threadExecutionWidth];
	const MTLSize threadsPerGroup = MTLSizeMake(exeWidth, 1, 1);
	const MTLSize groups = MTLSizeMake((p_Params.width + exeWidth - 1) / exeWidth, p_Params.height, 1);
	[encoder dispatchThreadgroups:groups threadsPerThreadgroup:threadsPerGroup];
	[encoder endEncoding];
	[commandBuffer commit];
}
