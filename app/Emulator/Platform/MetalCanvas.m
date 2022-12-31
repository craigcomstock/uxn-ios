#import "MetalCanvas.h"
#import "MetalShaders.h"
#import "Platform.h"
#import "emu_platform.h"


@interface MetalCanvas () {
    vector_uint2 _viewportSize;
}

@property (strong, nonatomic) id<MTLCommandQueue> commandQueue;
@property (strong, nonatomic) id<MTLRenderPipelineState> pipelineState;
@property (strong, nonatomic) id<MTLBuffer> vertices;
@property (assign, nonatomic) NSUInteger vertexCount;
@property (strong, nonatomic) id<MTLTexture> backgroundTexture;
@property (strong, nonatomic) id<MTLTexture> foregroundTexture;

@end


@implementation MetalCanvas

// from https://developer.apple.com/documentation/uikit/uiview which is a parent of MtkView seen in MetalCanvas.h
/*
 Configuring the bounds and frame rectangles
 var frame: CGRect
 The frame rectangle, which describes the view’s location and size in its superview’s coordinate system.
 var bounds: CGRect
 The bounds rectangle, which describes the view’s location and size in its own coordinate system.
 */
// TODO how to change the frame/view/size when screen_resize() is called? How did it work before?
- (void)setFrame:(CGRect)frame {
    [super setFrame:frame];
    
    CGSize size = self.bounds.size;
    _viewportSize.x = size.width;
    _viewportSize.y = size.height;
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer *)gestureRecognizer {
    return NO;
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [super touchesBegan:touches withEvent:event];
    for (UITouch* touch in touches) {
        CGPoint point = [touch locationInView:self];
        [Platform.sharedPlatform setMousePosition:point];
        [Platform.sharedPlatform handleMouseButton:PlatformMouseLeftButton isDown:YES];
        break;
    }
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [super touchesMoved:touches withEvent:event];
    for (UITouch* touch in touches) {
        CGPoint point = [touch locationInView:self];
        [Platform.sharedPlatform setMousePosition:point];
        break;
    }
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [super touchesEnded:touches withEvent:event];
    for (UITouch* touch in touches) {
        CGPoint point = [touch locationInView:self];
        [Platform.sharedPlatform setMousePosition:point];
        [Platform.sharedPlatform handleMouseButton:PlatformMouseLeftButton isDown:NO];
        break;
    }
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [super touchesCancelled:touches withEvent:event];
    for (UITouch* touch in touches) {
        CGPoint point = [touch locationInView:self];
        [Platform.sharedPlatform setMousePosition:point];
        [Platform.sharedPlatform handleMouseButton:PlatformMouseLeftButton isDown:NO];
        break;
    }
}

- (void)drawRect:(CGRect)rect {
    [Platform.sharedPlatform renderLoop:^{
        PlatformDelegateAppRunloop();
    }];

    [self updateTextures];
    
    MTLRenderPassDescriptor* renderPass = [self currentRenderPassDescriptor];
    if (!renderPass) {
        return;
    }

    id<MTLCommandBuffer> commandBuffer = [self.commandQueue commandBuffer];
    
    id<MTLRenderCommandEncoder> commandEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPass];
    [commandEncoder setRenderPipelineState:self.pipelineState];
    [commandEncoder setVertexBuffer:self.vertices offset:0 atIndex:VertexInputIndexVertices];
    [commandEncoder setVertexBytes:&_viewportSize length:sizeof(_viewportSize) atIndex:VertexInputIndexViewportSize];
    [commandEncoder setFragmentTexture:self.backgroundTexture atIndex:TextureIndexBaseColor];
    [commandEncoder setFragmentTexture:self.foregroundTexture atIndex:TextureIndexForeColor];
    [commandEncoder drawPrimitives:(MTLPrimitiveTypeTriangle) vertexStart:0 vertexCount:self.vertexCount];
    [commandEncoder endEncoding];
    
    [commandBuffer presentDrawable:self.currentDrawable];
    [commandBuffer commit];
}

- (void)setDevice:(id<MTLDevice>)device {
    [super setDevice:device];
    u16 width, height;
    // TODO, here, viewportsize is set, maketexture is called, but how to get size from platform canvas size?
    PlatformGetScreenSize(&width, &height);
    if (_viewportSize.x != width || _viewportSize.y != height) {
        _viewportSize.x = width;
        _viewportSize.y = height;
    }
    
    self.backgroundTexture = [self makeTexture];
    self.foregroundTexture = [self makeTexture];
    
    float hx = _viewportSize.x / 2;
    float hy = _viewportSize.y / 2;

    const Vertex quadVertices[] = {
        { {  hx,  -hy },  { 1.f, 1.f } },
        { { -hx,  -hy },  { 0.f, 1.f } },
        { { -hx,   hy },  { 0.f, 0.f } },

        { {  hx,  -hy },  { 1.f, 1.f } },
        { { -hx,   hy },  { 0.f, 0.f } },
        { {  hx,   hy },  { 1.f, 0.f } },
    };
    
    self.vertices = [device newBufferWithBytes:quadVertices length:sizeof(quadVertices) options:(MTLResourceStorageModeShared)];
    self.vertexCount = sizeof(quadVertices) / sizeof(quadVertices[0]);

    id<MTLLibrary> library = [device newDefaultLibrary];
    id<MTLFunction> vertexFunction = [library newFunctionWithName:@"vertexShader"];
    id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"samplingShader"];
    
    MTLRenderPipelineDescriptor* pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDescriptor.vertexFunction = vertexFunction;
    pipelineDescriptor.fragmentFunction = fragmentFunction;
    pipelineDescriptor.colorAttachments[0].pixelFormat = self.colorPixelFormat;
    
    NSError* error = nil;
    self.pipelineState = [device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
    if (error) {
        NSLog(@"Failed to create pipeline state: %@", error);
    }

    self.commandQueue = [device newCommandQueue];
}

- (id<MTLTexture>)makeTexture {
    MTLTextureDescriptor* textureDescriptor = [[MTLTextureDescriptor alloc] init];
    /*
     Ordinary format with four 8-bit normalized unsigned integer components in BGRA order.
     https://developer.apple.com/documentation/metal/mtlpixelformat/mtlpixelformatbgra8unorm
     */
    textureDescriptor.pixelFormat = MTLPixelFormatBGRA8Unorm;
    textureDescriptor.width = _viewportSize.x;
    textureDescriptor.height = _viewportSize.y;

    return [self.device newTextureWithDescriptor:textureDescriptor];
}

- (void)updateTextures {
    [self updateTexture:self.backgroundTexture withData:Platform.sharedPlatform.bgPixels];
    [self updateTexture:self.foregroundTexture withData:Platform.sharedPlatform.fgPixels];
}

- (void)updateTexture:(id<MTLTexture>)texture withData:(NSData*)data {
    MTLRegion region = {
        {0, 0, 0},
        {_viewportSize.x, _viewportSize.y, 1}
    };

    NSUInteger bytesPerRow = 4 * _viewportSize.x;
    const void* pixels = data.bytes;
    // EXC_BAD_ACCESS here because the viewportsize is bigger than changed canvas size in Platform.mm
    [texture replaceRegion:region mipmapLevel:0 withBytes:pixels bytesPerRow:bytesPerRow];
}

@end
