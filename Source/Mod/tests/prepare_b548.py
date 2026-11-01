from pathlib import Path
p=Path('B25DepthHelper/B140aDirectPresentSharedOpen.cpp');s=p.read_text();needle=' LARGE_INTEGER begin{},afterCopy{},end{};QueryPerformanceCounter(&begin);context->CopyResource(slot.xessInput.Get(),internalColor);'
replacement=''' // B548: already full-size SDR colour needs no second temporal reconstruction.
 // Preserve source pixels (including moving objects and UI) for XeFG. Copy only
 // compatible resources; actual upscaling/format conversion retains XeSS below.
 D3D11_TEXTURE2D_DESC transport{};slot.shared->GetDesc(&transport);
 if(source.Width==transport.Width&&source.Height==transport.Height&&source.Format==transport.Format&&source.MipLevels==transport.MipLevels&&source.ArraySize==transport.ArraySize&&source.SampleDesc.Count==transport.SampleDesc.Count&&source.SampleDesc.Quality==transport.SampleDesc.Quality){
  context->CopyResource(slot.shared.Get(),internalColor);
  GhostingInvalidateXess(); // A later return to reconstruction must start fresh.
  GhostingRequestXessReset();
  g_b171SecondColorResource=slot.output;g_b171SecondColorHash=(unsigned long long)g_b172GtaFrame+1ull;g_b171SecondColorReady.store(true,std::memory_order_release);
  if(b240_log_sample(g_b172GtaFrame))out<<"B548: NATIVE_COLOR_DIRECT source_frame_id="<<g_b172GtaFrame<<", size="<<source.Width<<"x"<<source.Height<<", bit_exact_copy=1, xess_execute=0, xefg_enabled=1, camera_only_temporal_color_accumulation=0\\n";
  return S_OK;
 }
'''+needle
assert needle in s;s=s.replace(needle,replacement);p.write_text(s)
p=Path('Fast-Build.ps1');p.write_text(p.read_text().replace('B547','B548'))
