#include "liumos.h"

LiumOS* liumos;

GDT gdt_;
IDT idt_;
KeyboardController keyboard_ctrl_;
LiumOS liumos_;
Sheet virtual_vram_;
Sheet virtual_screen_;
Console virtual_console_;
LocalAPIC bsp_local_apic_;
CPUFeatureSet cpu_features_;
SerialPort com1_;

void InitializeVRAMForKernel() {
  constexpr uint64_t kernel_virtual_vram_base = 0xFFFF'FFFF'8000'0000ULL;
  const int xsize = liumos->vram_sheet->GetXSize();
  const int ysize = liumos->vram_sheet->GetYSize();
  const int ppsl = liumos->vram_sheet->GetPixelsPerScanLine();
  CreatePageMapping(
      *liumos->dram_allocator, GetKernelPML4(), kernel_virtual_vram_base,
      reinterpret_cast<uint64_t>(liumos->vram_sheet->GetBuf()),
      liumos->vram_sheet->GetBufSize(), kPageAttrPresent | kPageAttrWritable);
  virtual_vram_.Init(reinterpret_cast<uint8_t*>(kernel_virtual_vram_base),
                     xsize, ysize, ppsl);

  constexpr uint64_t kernel_virtual_screen_base = 0xFFFF'FFFF'8800'0000ULL;
  CreatePageMapping(
      *liumos->dram_allocator, GetKernelPML4(), kernel_virtual_screen_base,
      reinterpret_cast<uint64_t>(liumos->screen_sheet->GetBuf()),
      liumos->screen_sheet->GetBufSize(), kPageAttrPresent | kPageAttrWritable);
  virtual_screen_.Init(reinterpret_cast<uint8_t*>(kernel_virtual_screen_base),
                       xsize, ysize, ppsl);
  virtual_screen_.SetParent(&virtual_vram_);

  liumos->screen_sheet = &virtual_screen_;
  liumos->screen_sheet->DrawRect(0, 0, xsize, ysize, 0xc6c6c6);
}

void SubTask();  // @subtask.cc

void LaunchSubTask(KernelVirtualHeapAllocator& kernel_heap_allocator) {
  const int kNumOfStackPages = 3;
  void* sub_context_stack_base = kernel_heap_allocator.AllocPages<void*>(
      kNumOfStackPages, kPageAttrPresent | kPageAttrWritable);
  void* sub_context_rsp = reinterpret_cast<void*>(
      reinterpret_cast<uint64_t>(sub_context_stack_base) +
      (kNumOfStackPages << kPageSizeExponent));
  PutStringAndHex("SubTask stack base", sub_context_stack_base);

  ExecutionContext* sub_context = liumos->exec_ctx_ctrl->Create(
      SubTask, GDT::kKernelCSSelector, sub_context_rsp, GDT::kKernelDSSelector,
      reinterpret_cast<uint64_t>(&GetKernelPML4()), kRFlagsInterruptEnable, 0);
  liumos->scheduler->RegisterExecutionContext(sub_context);
}

extern "C" void KernelEntry(LiumOS* liumos_passed) {
  liumos_ = *liumos_passed;
  liumos = &liumos_;

  cpu_features_ = *liumos->cpu_features;
  liumos->cpu_features = &cpu_features_;

  KernelVirtualHeapAllocator kernel_heap_allocator(GetKernelPML4(),
                                                   *liumos->dram_allocator);
  liumos->kernel_heap_allocator = &kernel_heap_allocator;

  InitializeVRAMForKernel();

  new (&virtual_console_) Console();
  virtual_console_.SetSheet(liumos->screen_sheet);
  liumos->main_console = &virtual_console_;

  com1_.Init(kPortCOM1);
  liumos->com1 = &com1_;
  liumos->main_console->SetSerial(&com1_);

  bsp_local_apic_.Init();

  ExecutionContextController exec_ctx_ctrl_(kernel_heap_allocator);
  liumos->exec_ctx_ctrl = &exec_ctx_ctrl_;

  ExecutionContext* root_context =
      liumos->exec_ctx_ctrl->Create(nullptr, 0, nullptr, 0, ReadCR3(), 0, 0);

  Scheduler scheduler_(root_context);
  liumos->scheduler = &scheduler_;

  PutString("Hello from kernel!\n");
  PutStringAndHex("RSP: ", ReadRSP());

  ClearIntFlag();

  constexpr uint64_t kNumOfKernelStackPages = 128;
  uint64_t kernel_stack_physical_base =
      liumos->dram_allocator->AllocPages<uint64_t>(kNumOfKernelStackPages);
  uint64_t kernel_stack_virtual_base = 0xFFFF'FFFF'2000'0000ULL;
  CreatePageMapping(*liumos->dram_allocator, GetKernelPML4(),
                    kernel_stack_virtual_base, kernel_stack_physical_base,
                    kNumOfKernelStackPages << kPageSizeExponent,
                    kPageAttrPresent | kPageAttrWritable);
  uint64_t kernel_stack_pointer =
      kernel_stack_virtual_base + (kNumOfKernelStackPages << kPageSizeExponent);

  uint64_t ist1_virt_base = kernel_heap_allocator.AllocPages<uint64_t>(
      kNumOfKernelStackPages, kPageAttrPresent | kPageAttrWritable);

  gdt_.Init(kernel_stack_pointer,
            ist1_virt_base + (kNumOfKernelStackPages << kPageSizeExponent));
  idt_.Init();
  keyboard_ctrl_.Init();

  StoreIntFlag();

  GetKernelPML4().Print();

  LaunchSubTask(kernel_heap_allocator);

  EnableSyscall();

  TextBox console_text_box;
  while (1) {
    ConsoleCommand::WaitAndProcess(console_text_box);
  }
}
