// LVGL heap in PSRAM (LV_USE_STDLIB_MALLOC = LV_STDLIB_CUSTOM, PLAN.md §5.5).
// Falls back to internal RAM if PSRAM runs out. The draw buffers are allocated
// separately in internal DMA-capable RAM (ui/LvglPort.cpp).

#include <esp_heap_caps.h>
#include <lvgl.h>

extern "C" {

void lv_mem_init(void) {}
void lv_mem_deinit(void) {}

lv_mem_pool_t lv_mem_add_pool(void *mem, size_t bytes) {
  LV_UNUSED(mem);
  LV_UNUSED(bytes);
  return nullptr;
}

void lv_mem_remove_pool(lv_mem_pool_t pool) { LV_UNUSED(pool); }

void *lv_malloc_core(size_t size) {
  return heap_caps_malloc_prefer(size, 2, MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT);
}

void *lv_realloc_core(void *p, size_t new_size) {
  return heap_caps_realloc_prefer(p, new_size, 2, MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT);
}

void lv_free_core(void *p) { heap_caps_free(p); }

void lv_mem_monitor_core(lv_mem_monitor_t *mon) {
  multi_heap_info_t info;
  heap_caps_get_info(&info, MALLOC_CAP_SPIRAM);
  mon->total_size = info.total_free_bytes + info.total_allocated_bytes;
  mon->free_size = info.total_free_bytes;
  mon->free_biggest_size = info.largest_free_block;
  mon->free_cnt = info.free_blocks;
  mon->used_cnt = info.allocated_blocks;
  mon->max_used = mon->total_size - info.minimum_free_bytes;
  mon->used_pct = mon->total_size ? 100 - (100 * mon->free_size) / mon->total_size : 0;
  mon->frag_pct = mon->free_size ? 100 - (100 * mon->free_biggest_size) / mon->free_size : 0;
}

lv_result_t lv_mem_test_core(void) { return LV_RESULT_OK; }

}  // extern "C"
