#include "RHICommandList.h"
#include "DynamicRHI.h"

void FRHICommandListImmediate::EndDrawingViewport(
    FRHIViewport* Viewport, const FRHIPresentArgs& PresentArgs)
{
    GDynamicRHI->RHIEndDrawingViewport(*this, Viewport, PresentArgs);
}
