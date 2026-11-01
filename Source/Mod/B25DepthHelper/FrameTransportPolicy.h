#pragma once
#include <d3d11.h>

inline bool NativeColorCopyCompatible(const D3D11_TEXTURE2D_DESC& source,const D3D11_TEXTURE2D_DESC& transport){
 return source.Width==transport.Width&&source.Height==transport.Height&&source.Format==transport.Format&&source.MipLevels==transport.MipLevels&&source.ArraySize==transport.ArraySize&&source.SampleDesc.Count==transport.SampleDesc.Count&&source.SampleDesc.Quality==transport.SampleDesc.Quality;
}

// Both decisions use the same compatibility predicate. Skipping capture motion
// is safe only when the colour path will also bypass temporal reconstruction.
inline bool NeedsCaptureMotion(const D3D11_TEXTURE2D_DESC& source,const D3D11_TEXTURE2D_DESC& transport){return !NativeColorCopyCompatible(source,transport);}
