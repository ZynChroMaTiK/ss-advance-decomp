/* All renderers present through the same composed GDI window. DwmFlush
 * synchronizes that presentation with DWM; this is not a Vulkan swapchain.
 * Loaded optionally so missing/disabled composition never prevents startup. */
static void presentation_sync_refresh(App *a)
{
    BOOL enabled=FALSE;
    a->vsync_available=a->dwm_flush && a->dwm_composition &&
        SUCCEEDED(a->dwm_composition(&enabled)) && enabled;
    if(a->menu)a->menu->vsync_available=a->vsync_available;
}
static void presentation_sync_init(App *a)
{
    a->dwm=LoadLibraryExW(L"dwmapi.dll",NULL,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(a->dwm){
        a->dwm_flush=(HRESULT (WINAPI *)(void))GetProcAddress(a->dwm,"DwmFlush");
        a->dwm_composition=(HRESULT (WINAPI *)(BOOL *))GetProcAddress(a->dwm,"DwmIsCompositionEnabled");
    }
    presentation_sync_refresh(a);
}
static void presentation_sync_wait(App *a)
{
    if(!a->settings.vsync || !a->vsync_available)return;
    if(FAILED(a->dwm_flush())){
        /* Preserve the saved choice, disable the unavailable action, and
         * continue unsynchronized. A display/composition change can retry. */
        a->vsync_available=0;
        if(a->menu)a->menu->vsync_available=0;
        fprintf(stderr,"VSync synchronization unavailable; continuing without it.\n");
    }
}
